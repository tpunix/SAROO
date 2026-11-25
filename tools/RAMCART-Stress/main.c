/*******************************************************************
* RAMCART-Stress: stress test for the Saturn extended RAM cartridge
* at SEGA'S OWN cart timing (based on RAMCART-Test).
*
* Made to test the SAROO FPGA redesign (FPGA_abus_exp): by default every
* run applies Sega's TB#47 cart setup (0x257EFFFE = 1, ASR0 = 0x23301FF0:
* 3 wait states, external WAIT off; AREF = 0x13), so the cart has to keep
* up without stretching the bus. A cart that can't (a SAROO with the stock
* FPGA) fails here; a real Sega/Action Replay cart passes.
*
* Tests (the whole cartridge RAM, one chunk per frame):
*   1 PROBE      16/32-bit write/verify at the start, middle, end of each bank
*   2 ALIASING   every power-of-two offset gets its own value
*   3 ADDRESS    every longword = a value made from its own address
*   4 CACHED     ADDRESS read back through the SH-2 cache (0x02xxxxxx):
*                every 16-byte line is purged, then filled by the CPU,
*                so the cart sees cache-line fills, not single reads
*   5 RANDOM     random addresses: write A, read another address B, read A
*                back (write -> read with no gap, the hardest case for
*                Sega's timing, which has no write precharge); then the
*                whole RAM is checked
*   6 INVERTED   ADDRESS with all bits inverted
*   7 CHECKER16  0x5555 / 0xAAAA, 16-bit accesses
*   8 RETENTION  write, wait 3 seconds, verify
*   9 DMA SH2    SH-2 DMA to the cart and back
*  10 CODE       (1.2) SH-2 code run FROM the cart, like the SAROO firmware
*                (it runs at 0x02000000, in the cart area): each frame a
*                64 KB block of generated code is written to the next part of
*                the cart and called through the cache (cache-line fills,
*                cache purged first) and once cache-through (one read per
*                instruction); the result is checked, then the block is read
*                back as data. A bad instruction fetch can crash the CPU: if
*                the screen stops during test 10, that is the failure.
*  11 XRAM       (1.3, SAROO only) the 8 MB the SAROO maps above the 4 MB cart,
*                0x22800000-0x22FFFFFF (used by YZB's "Saroo only" hacks):
*                fill, verify through the cache, random write/read, check.
*                Skipped ("NO SAROO") when the SAROO ID isn't found.
*
* All runs (1.2): ALL RUNS counts errors over every run, and the first run
* that failed stays on screen until START (START also restarts the counts).
*
* CD LOAD (C button, on by default): LOAD.BIN (128 KB) is read from the
* disc in the background again and again, 16 KB per frame, and checked.
* On a SAROO that keeps the STM32 busy with the same SDRAM while the cart
* is hammered (the case that broke SAROO timing 0x34403440 at boot).
*
* Pad: START = run again, A = loop forever on/off (on by default),
* B = cart setup of the next run: SEGA TB47 (default) / AUTO / NONE,
* C = CD load on/off.
* Results are also in cart_res[] for a debugger. The test DESTROYS the
* cart content. Nothing here writes saves, but on a SAROO a cart timing
* that fails can corrupt anything in its SDRAM, emulated backup RAM included.
*******************************************************************/
#include <jo/jo.h>

/* ---- hardware ------------------------------------------------------ */
#define CART_ID_WORD    (*(volatile unsigned short *)0x24FFFFFEu)
#define CART_INIT       (*(volatile unsigned short *)0x257EFFFEu)
#define SCU_ASR0        (*(volatile unsigned int *)0x25FE00B0u)   /* A-Bus set register */
#define SCU_AREF        (*(volatile unsigned int *)0x25FE00B8u)   /* A-Bus refresh register */
#define CART_BANK0      0x22400000u     /* DRAM0, cache-through */
#define CART_BANK1      0x22600000u     /* DRAM1, cache-through */
#define SEGA_ASR0       0x23301FF0u     /* TB#47: 3 waits, no external wait (CS0) */

#define KB(n)           ((unsigned int)(n) * 1024u)
#define CHUNK           KB(64)          /* bytes written or verified per frame */
#define RETAIN_FRAMES   180u            /* 3 s at 60 Hz */
#define DMA_SIZE        4096u
#define RANDOM_FRAMES   120u            /* frames of random accesses */
#define RANDOM_OPS      4096u           /* write/read/read triples per frame */
#define LOAD_SIZE       KB(128)         /* LOAD.BIN */
#define CODE_SIZE       KB(64)          /* code block per frame (the SH-2 cache is 4 KB) */
#define CODE_CALLS      4u              /* cached calls per frame (plus one cache-through call) */
#define SH2_CCR         (*(volatile unsigned char *)0xFFFFFE92u)   /* cache control register */

typedef struct
{
    unsigned int        base;
    unsigned int        size;
}                       bank_t;

enum { KIND_NONE, KIND_1MB, KIND_4MB, KIND_UNKNOWN };
enum { T_PROBE, T_ALIAS, T_ADDR, T_CACHED, T_RANDOM, T_INV, T_CHK16, T_RETAIN, T_DMA, T_CODE, T_XRAM, T_AA24, T_COUNT };
enum { S_PENDING, S_RUNNING, S_PASS, S_FAIL, S_SKIP, S_NOSAROO, S_NOROMCTL, S_MIRROR, S_UNSTABLE };
enum { PH_START, PH_FILL, PH_WAIT, PH_VERIFY, PH_RANDOM, PH_CODE, PH_XRAM, PH_DONE, PH_NOCART };

static const char *const test_name[T_COUNT] =
{
    "PROBE 16/32", "ALIASING", "ADDRESS", "CACHED READ", "RANDOM W/R", "INVERTED", "CHECKER 16BIT",
    "RETENTION 3S", "DMA SH2", "CODE FROM CART", "SAROO XRAM 8MB", "AA24 HIGH 16MB"
};

/* Test 12 (1.4): newer SAROO boards can route cart address line AA24 to the FPGA (its SS_SCLK pin). The MCU
   command ROMCTL (0x41, SS_ARG bit 0) makes the FPGA use it as SDRAM address bit 24, so 0x23000000-0x23FFFFFF is
   another 16 MB instead of a mirror of 0x22000000. On boards where that pin is the CD audio clock it must stay
   off: the test detects this first and switches ROMCTL off again. */
#define SS_CMD_REG      (*(volatile unsigned short *)0x25897010u)
#define SS_ARG_REG      (*(volatile unsigned short *)0x25897014u)
#define SSCMD_ROMCTL    0x0041u
#define AA24_BASE       0x23000000u
#define AA24_SIZE       KB(16384)
#define AA24_PAIRS      64u
static int              aa24_on;        /* 1: this program switched ROMCTL on and must switch it off */
static unsigned int     aa24_seen[3];   /* detection: pairs separate / mirrored / other */
static unsigned int     xr_base;        /* region of tests 11 and 12 */
static unsigned int     xr_size;

/* Test 11 (1.3): the SAROO maps its SDRAM over the whole 16 MB cart window, so 0x22800000-0x22FFFFFF (8 MB above
   the 4 MB cart) is RAM too. Nothing in the SAROO firmware uses it; YZB's "Saroo only" hacks keep game data there.
   0x22000000-0x223FFFFF is the SAROO's own (firmware, save RAM): never touched. */
#define XRAM_BASE       0x22800000u
#define XRAM_SIZE       KB(8192)
#define SAROO_ID        (*(volatile unsigned short *)0x25807000u)   /* "SR" on a SAROO */
#define XRAM_RND_FRAMES 60u
enum { XR_FILL, XR_VERIFY, XR_RANDOM, XR_FINAL };
static int              xram_stage;

static bank_t           banks[2];
static int              bank_count;
static unsigned int     total_bytes;
static unsigned int     cart_id;
static int              cart_kind;

static int              test_status[T_COUNT];
static unsigned int     test_errors[T_COUNT];
static int              cur_test;
static int              phase;
static unsigned int     pos;            /* bytes (or frames for RANDOM) done in the current phase */
static unsigned int     wait_until;
static unsigned int     run_seed;
static unsigned int     rnd;            /* xorshift state */

static unsigned int     total_errors;
static unsigned int     first_addr;
static unsigned int     first_exp;
static unsigned int     first_got;
static int              first_test;

static unsigned int     run_count;
static unsigned int     pass_count;

/* All runs since power-on or the last START (1.2) */
static unsigned int     all_errors;
static unsigned int     all_read;
static unsigned int     all_stored;
static unsigned int     all_other;
static unsigned int     fail_runs;
static unsigned int     fail_run;       /* first run that failed (0: none yet) */
static int              fail_test;
static unsigned int     fail_addr;
static unsigned int     fail_exp;
static unsigned int     fail_got;

/* Test 10 */
static unsigned int     code_calls;     /* calls made this run */
static unsigned int     run_start_vbl;
static unsigned int     run_frames;
static int              auto_loop = 1;

/* Cartridge setup, changed with the B button: Sega's TB#47 (default), AUTO (only if the RAM doesn't answer), none */
enum { INIT_TB47, INIT_AUTO, INIT_NONE, INIT_MODES };
static int              init_mode = INIT_TB47;
static int              init_applied;   /* 1: the TB#47 setup was written for this run */

#define DMA_LOCATIONS   3u
static int              dma_timeout;    /* 0: none, 1: the write to the cart did not end, 2: the read */
static unsigned int     dma_chcr;
static unsigned int     dma_dmaor;
static unsigned int     dma_tcr;
static unsigned char    dma_src[DMA_SIZE] __attribute__((aligned(16)));
static unsigned char    dma_dst[DMA_SIZE] __attribute__((aligned(16)));

/* CD load */
static unsigned int     load_buf[LOAD_SIZE / 4 + 4] __attribute__((aligned(16)));
static int              cd_load_on = 1;
static volatile int     cd_busy;
static unsigned int     cd_loads;
static unsigned int     cd_errors;      /* loads with wrong data or wrong length */
static unsigned int     cd_start_fail;

/* Read by a debugger: magic, id, kind, total errors, first fail (address, expected, got, test), runs, passed
   runs, done, CD loads, CD errors */
volatile unsigned int   cart_res[20];   /* 1.2: [16] all-run errors, [17] failed runs, [18] first failed run, [19] code calls */

/* ---- helpers ------------------------------------------------------- */
static void             bus_settle(void)
{
    volatile int        d;

    for (d = 0; d < 0x200; d++)
        __asm__ volatile ("nop");
}

/* Re-read check (1.1): every error is read back twice at once, through the cache-through area.
   READ BAD:   both re-reads give a right value: the RAM holds the right data, the first read was wrong.
   STORED BAD: both re-reads give the same wrong value: the wrong data is in the RAM (a write went wrong).
   OTHER:      anything else (the re-reads disagree, or no re-read was possible). */
static unsigned int     cls_read;
static unsigned int     cls_stored;
static unsigned int     cls_other;
static unsigned int     first_stored_addr;
static unsigned int     first_stored_val;

static unsigned int     reread(const unsigned int addr, const int width)
{
    const unsigned int  a = addr | 0x20000000u;

    if (width == 1)
        return (*(volatile unsigned char *)a);
    if (width == 2)
        return (*(volatile unsigned short *)a);
    return (*(volatile unsigned int *)a);
}

/* `expected` and `alt` are the values that are right at `addr` (the same value when only one is). width: 1, 2, 4
   bytes, 0 = no re-read possible */
static void             record_error_x(const unsigned int addr, const unsigned int expected, const unsigned int alt,
                                       const unsigned int got, const int width)
{
    if (total_errors == 0)
    {
        first_addr = addr;
        first_exp = expected;
        first_got = got;
        first_test = cur_test;
    }
    ++total_errors;
    ++test_errors[cur_test];
    if (width)
    {
        const unsigned int  r1 = reread(addr, width);
        const unsigned int  r2 = reread(addr, width);

        if ((r1 == expected || r1 == alt) && (r2 == expected || r2 == alt))
            ++cls_read;
        else if (r1 == got && r2 == got)
        {
            if (cls_stored == 0)
            {
                first_stored_addr = addr;
                first_stored_val = got;
            }
            ++cls_stored;
        }
        else
            ++cls_other;
    }
    else
        ++cls_other;
}

static void             record_error(const unsigned int addr, const unsigned int expected, const unsigned int got)
{
    record_error_x(addr, expected, expected, got, 4);
}

/* Address of the byte `offset` of the cartridge RAM (the banks one after the other) */
static unsigned int     addr_of(unsigned int offset)
{
    int                 i;

    for (i = 0; i < bank_count - 1; i++)
    {
        if (offset < banks[i].size)
            break;
        offset -= banks[i].size;
    }
    return (banks[i].base + offset);
}

static __jo_force_inline unsigned int   xorshift(void)
{
    rnd ^= rnd << 13;
    rnd ^= rnd >> 17;
    rnd ^= rnd << 5;
    return (rnd);
}

/* Value of the longword at `addr`: different at every address and at every run.
   T_CACHED and T_RANDOM start from the ADDRESS pattern, which test 3 leaves in the RAM. */
static __jo_force_inline unsigned int   pattern32(const int test, const unsigned int addr)
{
    unsigned int        v;

    v = addr * 2654435761u + run_seed;
    if (test == T_INV)
        return (~v);
    if (test == T_RETAIN)
        return ((v << 7) | (v >> 25));
    return (v);
}

/* What the RANDOM test writes: different from the ADDRESS pattern at every address */
static __jo_force_inline unsigned int   pattern_rnd(const unsigned int addr)
{
    return (pattern32(T_ADDR, addr) ^ 0x0F0FF0F0u);
}

static __jo_force_inline unsigned short pattern16(const unsigned int addr)
{
    return ((((addr >> 1) ^ run_count) & 1) ? 0xAAAA : 0x5555);
}

/* ---- CD load ------------------------------------------------------- */
static void             cd_done(char *contents, int length, int token)
{
    /* GFS may have written the buffer behind the cache: check it through the cache-through area */
    const volatile unsigned int *w = (const volatile unsigned int *)((unsigned int)contents | 0x20000000u);
    unsigned int        i;
    bool                ok = (length == (int)LOAD_SIZE);

    (void)token;
    for (i = 0; ok && i < LOAD_SIZE / 4; i++)
    {
        if (w[i] != ((i * 2654435761u) ^ 0x5A5AA5A5u))
            ok = false;
    }
    ++cd_loads;
    if (!ok)
        ++cd_errors;
    cd_busy = 0;
}

static void             cd_kick(void)
{
    volatile unsigned int *w = (volatile unsigned int *)((unsigned int)load_buf | 0x20000000u);
    unsigned int        i;

    if (!cd_load_on || cd_busy)
        return;
    /* Clear first, so a load that leaves the old data behind can't pass */
    for (i = 0; i < LOAD_SIZE / 4; i++)
        w[i] = 0;
    cd_busy = 1;
    if (!jo_fs_read_file_async_ptr("LOAD.BIN", cd_done, 0, load_buf))
    {
        cd_busy = 0;
        ++cd_start_fail;
    }
}

/* ---- detection ----------------------------------------------------- */

/* TB#47 cartridge setup: enable the cart, then Sega's A-Bus timings and the DRAM refresh */
static void             apply_tb47_init(void)
{
    CART_INIT = 0x0001;
    bus_settle();
    SCU_ASR0 = SEGA_ASR0;
    SCU_AREF = 0x00000013u;
    bus_settle();
    init_applied = 1;
}

static bool             quick_probe(const unsigned int base)
{
    volatile unsigned short *a = (volatile unsigned short *)base;
    volatile unsigned short *b = (volatile unsigned short *)(base + 0x1000u);
    bool                ok;

    *a = 0x5AA5;
    *b = 0xA55A;
    ok = (*a == 0x5AA5 && *b == 0xA55A);
    *a = 0xC33C;
    *b = 0x3CC3;
    return (ok && *a == 0xC33C && *b == 0x3CC3);
}

static void             detect_cart(void)
{
    cart_id = CART_ID_WORD & 0xFFu;
    bank_count = 0;
    total_bytes = 0;
    init_applied = 0;
    switch (cart_id)
    {
    case 0x5A:
    case 0x1A:
    case 0xDA:
        cart_kind = KIND_1MB;
        banks[0].base = CART_BANK0;
        banks[0].size = KB(512);
        banks[1].base = CART_BANK1;
        banks[1].size = KB(512);
        bank_count = 2;
        if (init_mode != INIT_NONE)
            apply_tb47_init();
        break;
    case 0x5C:
        cart_kind = KIND_4MB;
        banks[0].base = CART_BANK0;
        banks[0].size = KB(4096);
        bank_count = 1;
        if (init_mode == INIT_TB47 || (init_mode == INIT_AUTO && !quick_probe(CART_BANK0)))
            apply_tb47_init();
        break;
    case 0x00:
    case 0xFF:
        cart_kind = KIND_NONE;
        break;
    default:
        cart_kind = KIND_UNKNOWN;
        break;
    }
    if (bank_count)
        total_bytes = banks[0].size + (bank_count > 1 ? banks[1].size : 0);
}

/* ---- test 1: probes ------------------------------------------------ */
static void             probe16(const unsigned int addr, const unsigned short value)
{
    volatile unsigned short *p = (volatile unsigned short *)addr;
    unsigned short      got;

    *p = value;
    got = *p;
    if (got != value)
        record_error_x(addr, value, value, got, 2);
}

static void             probe32(const unsigned int addr, const unsigned int value)
{
    volatile unsigned int   *p = (volatile unsigned int *)addr;
    unsigned int        got;

    *p = value;
    got = *p;
    if (got != value)
        record_error(addr, value, got);
}

static void             run_probe(void)
{
    int                 i;

    for (i = 0; i < bank_count; i++)
    {
        const unsigned int  base = banks[i].base;
        const unsigned int  size = banks[i].size;

        probe16(base, 0x5555);
        probe16(base, 0xAAAA);
        probe16(base + (size >> 1), 0xAAAA);
        probe16(base + (size >> 1), 0x5555);
        probe16(base + size - 2, 0x5A5A);
        probe16(base + size - 2, 0xA5A5);
        probe32(base, 0xDEADBEEFu);
        probe32(base + (size >> 1), 0xCAFEBABEu);
        probe32(base + size - 4, 0x12345678u);
    }
}

/* ---- test 2: aliasing ---------------------------------------------- */
#define ALIAS_MAX       48

static void             run_alias(void)
{
    unsigned int        addr[ALIAS_MAX];
    int                 count = 0;
    int                 i;
    unsigned int        off;

    for (i = 0; i < bank_count; i++)
    {
        addr[count++] = banks[i].base;
        for (off = 4; off < banks[i].size && count < ALIAS_MAX; off <<= 1)
            addr[count++] = banks[i].base + off;
    }
    for (i = 0; i < count; i++)
        *(volatile unsigned int *)addr[i] = 0xA11A5000u + (unsigned int)i;
    for (i = 0; i < count; i++)
    {
        const unsigned int  got = *(volatile unsigned int *)addr[i];

        if (got != 0xA11A5000u + (unsigned int)i)
            record_error(addr[i], 0xA11A5000u + (unsigned int)i, got);
    }
}

/* ---- whole RAM tests, one chunk per frame -------------------------- */
static void             fill_chunk(const int test, const unsigned int offset)
{
    unsigned int        a = addr_of(offset);
    const unsigned int  end = a + CHUNK;

    if (test == T_CHK16)
    {
        for (; a < end; a += 2)
            *(volatile unsigned short *)a = pattern16(a);
        return;
    }
    for (; a < end; a += 4)
        *(volatile unsigned int *)a = pattern32(test, a);
}

static void             verify_chunk(const int test, const unsigned int offset)
{
    unsigned int        a = addr_of(offset);
    const unsigned int  end = a + CHUNK;

    if (test == T_CHK16)
    {
        for (; a < end; a += 2)
        {
            const unsigned short    expected = pattern16(a);
            const unsigned short    got = *(volatile unsigned short *)a;

            if (got != expected)
                record_error_x(a, expected, expected, got, 2);
        }
        return;
    }
    if (test == T_CACHED)
    {
        /* Each 16-byte line: purge it (associative purge area 0x4xxxxxxx), then read it through the cache:
           the first read makes the SH-2 fill the whole line from the cart */
        for (; a < end; a += 16)
        {
            const unsigned int  cached = a & 0x0FFFFFFFu;
            const volatile unsigned int *c = (const volatile unsigned int *)cached;
            unsigned int        k;

            *(volatile unsigned int *)(0x40000000u | cached) = 0;
            for (k = 0; k < 4; k++)
            {
                const unsigned int  expected = pattern32(T_ADDR, a + k * 4u);
                const unsigned int  got = c[k];

                if (got != expected)
                    record_error(a + k * 4u, expected, got);
            }
        }
        return;
    }
    if (test == T_RANDOM)
    {
        /* After the random accesses every longword is either the ADDRESS pattern or the RANDOM one */
        for (; a < end; a += 4)
        {
            const unsigned int  got = *(volatile unsigned int *)a;

            if (got != pattern32(T_ADDR, a) && got != pattern_rnd(a))
                record_error_x(a, pattern_rnd(a), pattern32(T_ADDR, a), got, 4);
        }
        return;
    }
    for (; a < end; a += 4)
    {
        const unsigned int  expected = pattern32(test, a);
        const unsigned int  got = *(volatile unsigned int *)a;

        if (got != expected)
            record_error(a, expected, got);
    }
}

/* ---- test 5: random write / read / read ---------------------------- */
static void             run_random_frame(void)
{
    const unsigned int  mask = (total_bytes - 1u) & ~3u;    /* total_bytes is a power of two */
    unsigned int        i;

    for (i = 0; i < RANDOM_OPS; i++)
    {
        const unsigned int  a = addr_of(xorshift() & mask);
        const unsigned int  b = addr_of(xorshift() & mask);
        unsigned int        got;

        *(volatile unsigned int *)a = pattern_rnd(a);
        /* B, read right after the write: either value is fine, anything else is not */
        got = *(volatile unsigned int *)b;
        if (got != pattern32(T_ADDR, b) && got != pattern_rnd(b))
            record_error_x(b, pattern_rnd(b), pattern32(T_ADDR, b), got, 4);
        /* A again: the write must have landed */
        got = *(volatile unsigned int *)a;
        if (got != pattern_rnd(a))
            record_error(a, pattern_rnd(a), got);
    }
}

/* ---- test 9: SH-2 DMA ---------------------------------------------- */
#define DMA_TIMEOUT     1000000u

static __jo_force_inline bool   dma_busy(void)
{
    return ((JO_SH2_CHCR0 & (JO_SH2_CHCR_ENABLE | JO_SH2_CHCR_TRANSFER_END)) == JO_SH2_CHCR_ENABLE);
}

static bool             dma_transfer(const unsigned int src, const unsigned int dst, const unsigned int size)
{
    unsigned int        sr;
    unsigned int        n;

    for (n = 0; dma_busy(); n++)
    {
        if (n > DMA_TIMEOUT)
            break;
    }
    sr = __jo_core_disable_interrupts();
    JO_SH2_CHCR0 = 0;
    JO_SH2_SAR0 = src;
    JO_SH2_DAR0 = dst;
    JO_SH2_TCR0 = size >> 2;
    if (JO_SH2_DMAOR != JO_SH2_DMAOR_ENABLE)
        JO_SH2_DMAOR = JO_SH2_DMAOR_ENABLE;
    JO_SH2_CHCR0 = JO_SH2_CHCR_DEST_INCREMENT | JO_SH2_CHCR_SRC_INCREMENT | JO_SH2_CHCR_SIZE_32 |
                   JO_SH2_CHCR_AUTO_REQUEST | JO_SH2_CHCR_ENABLE;
    __jo_core_restore_interrupts(sr);
    for (n = 0; dma_busy(); n++)
    {
        if (n > DMA_TIMEOUT)
        {
            dma_chcr = JO_SH2_CHCR0;
            dma_dmaor = JO_SH2_DMAOR;
            dma_tcr = JO_SH2_TCR0;
            JO_SH2_CHCR0 = 0;
            return (false);
        }
    }
    return (true);
}

static void             run_dma_at(const unsigned int cart_addr, const unsigned int salt)
{
    volatile unsigned char  *cart = (volatile unsigned char *)cart_addr;
    volatile unsigned char  *back = (volatile unsigned char *)((unsigned int)dma_dst | 0x20000000u);
    unsigned int        i;

    for (i = 0; i < DMA_SIZE; i++)
    {
        dma_src[i] = (unsigned char)((i * 7u) ^ (i >> 8) ^ salt ^ run_seed);
        back[i] = 0;
        cart[i] = 0;
    }
    if (!dma_transfer((unsigned int)dma_src, cart_addr, DMA_SIZE))
    {
        dma_timeout = 1;
        record_error_x(cart_addr, 0, 0, 0, 0);
        return;
    }
    for (i = 0; i < DMA_SIZE; i++)
    {
        const unsigned char got = cart[i];

        if (got != dma_src[i])
            record_error_x(cart_addr + i, dma_src[i], dma_src[i], got, 1);
    }
    if (!dma_transfer(cart_addr, (unsigned int)dma_dst, DMA_SIZE))
    {
        dma_timeout = 2;
        record_error_x(cart_addr, 0, 0, 0, 0);
        return;
    }
    for (i = 0; i < DMA_SIZE; i++)
    {
        const unsigned char got = back[i];

        if (got != dma_src[i])
            record_error_x(cart_addr + i, dma_src[i], dma_src[i], got, 1);
    }
}

static void             run_dma(const unsigned int index)
{
    if (index == 0)
        run_dma_at(addr_of(0), 0x11);
    else if (index == 1)
        run_dma_at(addr_of(total_bytes >> 1), 0x22);
    else
        run_dma_at(addr_of(total_bytes - DMA_SIZE), 0x33);
}

/* ---- test 10: code run from the cart ------------------------------- */
/* SH-2 opcodes used (all on r0, so a call can't touch anything else) */
#define OP_MOV_R4_R0    0x6043u         /* mov r4,r0 (the argument) */
#define OP_ADD_R0       0x7000u         /* add #imm,r0 */
#define OP_ROTL_R0      0x4004u         /* rotl r0 */
#define OP_NOT_R0       0x6007u         /* not r0,r0 */
#define OP_RTS          0x000Bu
#define OP_NOP          0x0009u

typedef unsigned int    (*code_fn)(unsigned int);

static void             cache_purge(void)
{
    const unsigned int  sr = __jo_core_disable_interrupts();
    const unsigned char ccr = SH2_CCR;

    SH2_CCR = 0x10;                     /* CP: purge every line (and cache off for a moment) */
    SH2_CCR = ccr;
    __jo_core_restore_interrupts(sr);
}

/* Writes the code block at `base` (cache-through address) and returns what it computes for `seed`. Every
   instruction word changes from run to run and from block to block. */
static unsigned int     code_write(const unsigned int base, const unsigned int seed)
{
    volatile unsigned int   *w = (volatile unsigned int *)base;
    const unsigned int  words = CODE_SIZE / 2u;
    unsigned int        h = seed ^ base;
    unsigned int        r0 = seed;
    unsigned int        i;
    unsigned int        hi = OP_MOV_R4_R0;

    for (i = 1; i < words; i++)
    {
        unsigned int        op;

        if (i == words - 2u)
            op = OP_RTS;
        else if (i == words - 1u)
            op = OP_NOP;
        else
        {
            h = h * 1103515245u + 12345u;
            switch ((h >> 16) % 3u)
            {
            case 0:
                op = OP_ADD_R0 | ((h >> 8) & 0xFFu);
                r0 += (unsigned int)(int)(signed char)(op & 0xFFu);
                break;
            case 1:
                op = OP_ROTL_R0;
                r0 = (r0 << 1) | (r0 >> 31);
                break;
            default:
                op = OP_NOT_R0;
                r0 = ~r0;
                break;
            }
        }
        if (i & 1u)
            *w++ = (hi << 16) | op;     /* big-endian: the even word is the high half */
        else
            hi = op;
    }
    return (r0);
}

/* Reads the block back as data: wrong words here are STORED/READ errors of the code itself */
static void             code_verify(const unsigned int base, const unsigned int seed)
{
    const volatile unsigned int *w = (const volatile unsigned int *)base;
    const unsigned int  words = CODE_SIZE / 2u;
    unsigned int        h = seed ^ base;
    unsigned int        i;
    unsigned int        hi = OP_MOV_R4_R0;

    for (i = 1; i < words; i++)
    {
        unsigned int        op;

        if (i == words - 2u)
            op = OP_RTS;
        else if (i == words - 1u)
            op = OP_NOP;
        else
        {
            h = h * 1103515245u + 12345u;
            switch ((h >> 16) % 3u)
            {
            case 0:
                op = OP_ADD_R0 | ((h >> 8) & 0xFFu);
                break;
            case 1:
                op = OP_ROTL_R0;
                break;
            default:
                op = OP_NOT_R0;
                break;
            }
        }
        if (i & 1u)
        {
            const unsigned int  a = base + (i - 1u) * 2u;
            const unsigned int  expected = (hi << 16) | op;
            const unsigned int  got = w[(i - 1u) / 2u];

            if (got != expected)
                record_error(a, expected, got);
        }
        else
            hi = op;
    }
}

/* Self-test: the same code generated in work RAM and run there. If this works and the cart call doesn't, the
   code is right and the cart (or an emulator that can't run code from the cart) is the problem. */
static unsigned int     code_ram[CODE_SIZE / 4u] __attribute__((aligned(16)));
static int              selftest_done;  /* 0: not yet, 1: passed, 2: failed */

/* Step marker, written straight to the screen: if test 10 hangs, the last one shown is the step that hung */
static void             t10_mark(const char *const step_name, const unsigned int n)
{
    jo_printf_with_color(1, 27, JO_COLOR_INDEX_Yellow, "T10 %-8s %u AT %08X SELF %s ", step_name, n,
                         addr_of(pos * CODE_SIZE), selftest_done == 1 ? "OK " : selftest_done == 2 ? "BAD" : "-  ");
}

/* Z: test 10 on/off (an emulator that can't run code from the cart hangs in test 10) */
static int              code_test_on = 1;

static void             run_selftest(const unsigned int seed)
{
    const unsigned int  ram_through = (unsigned int)code_ram | 0x20000000u;
    const unsigned int  ram_cached = (unsigned int)code_ram & 0x0FFFFFFFu;
    unsigned int        expected;
    unsigned int        got;

    t10_mark("SELFTEST", 0);

    expected = code_write(ram_through, seed);
    cache_purge();
    got = ((code_fn)ram_cached)(seed);
    selftest_done = (got == expected) ? 1 : 2;
    if (got != expected)
        record_error_x(ram_through, expected, expected, got, 0);   /* the test program itself is wrong */
    t10_mark("SELFTEST", 0);
}

static void             run_code_block(const unsigned int index)
{
    const unsigned int  base = addr_of(index * CODE_SIZE);
    const unsigned int  seed = run_seed ^ (index * 0x9E3779B9u);
    unsigned int        expected;

    if (selftest_done == 0)
        run_selftest(seed);

    t10_mark("WRITE", 0);
    expected = code_write(base, seed);
    const code_fn       cached = (code_fn)(base & 0x0FFFFFFFu);
    const code_fn       uncached = (code_fn)base;
    unsigned int        n;
    unsigned int        got;
    unsigned int        bad_runs = 0;
    unsigned int        before;

    /* Cache-through first: one bus read per instruction */
    t10_mark("UNCACHED", 1);
    got = uncached(seed);
    ++code_calls;
    if (got != expected)
    {
        record_error_x(base, expected, expected, got, 0);
        ++bad_runs;
    }
    for (n = 0; n < CODE_CALLS; n++)
    {
        t10_mark("PURGE", n + 1);
        cache_purge();
        t10_mark("CACHED", n + 1);
        got = cached(seed);
        ++code_calls;
        if (got != expected)
        {
            record_error_x(base, expected, expected, got, 0);   /* counted as OTHER for now */
            ++bad_runs;
        }
    }
    before = total_errors;
    t10_mark("VERIFY", 0);
    code_verify(base, seed);
    t10_mark("DONE", 0);
    /* Wrong result but the code in the RAM is right: an instruction was fetched wrong (READ BAD) */
    if (bad_runs && total_errors == before)
    {
        cls_other -= bad_runs;
        cls_read += bad_runs;
    }
}

/* ---- test 11: SAROO extra RAM above the 4 MB cart ------------------ */
static __jo_force_inline unsigned int   pattern_xram(const unsigned int addr)
{
    return (pattern32(T_XRAM, addr) ^ 0x5A0F3C69u);
}

static __jo_force_inline unsigned int   pattern_xram_rnd(const unsigned int addr)
{
    return (pattern_xram(addr) ^ 0x0F0FF0F0u);
}

static bool             is_saroo(void)
{
    return (SAROO_ID == 0x5253u);
}

/* One frame of test 11. Returns true when the test is finished. */
static bool             run_xram_frame(void)
{
    unsigned int        a;
    unsigned int        end;
    unsigned int        i;

    switch (xram_stage)
    {
    case XR_FILL:
        for (a = xr_base + pos, end = a + CHUNK; a < end; a += 4)
            *(volatile unsigned int *)a = pattern_xram(a);
        break;
    case XR_VERIFY:
        /* Through the cache, a 16-byte line at a time (purged first), like game code reading its data */
        for (a = xr_base + pos, end = a + CHUNK; a < end; a += 16)
        {
            const unsigned int  cached = a & 0x0FFFFFFFu;
            unsigned int        k;

            *(volatile unsigned int *)(0x40000000u | cached) = 0;
            for (k = 0; k < 4; k++)
            {
                const unsigned int  expected = pattern_xram(a + k * 4u);
                const unsigned int  got = ((const volatile unsigned int *)cached)[k];

                if (got != expected)
                    record_error(a + k * 4u, expected, got);
            }
        }
        break;
    case XR_RANDOM:
        for (i = 0; i < RANDOM_OPS; i++)
        {
            const unsigned int  ra = xr_base + (xorshift() & (xr_size - 4u));
            const unsigned int  rb = xr_base + (xorshift() & (xr_size - 4u));
            unsigned int        got;

            *(volatile unsigned int *)ra = pattern_xram_rnd(ra);
            got = *(volatile unsigned int *)rb;
            if (got != pattern_xram(rb) && got != pattern_xram_rnd(rb))
                record_error_x(rb, pattern_xram_rnd(rb), pattern_xram(rb), got, 4);
            got = *(volatile unsigned int *)ra;
            if (got != pattern_xram_rnd(ra))
                record_error(ra, pattern_xram_rnd(ra), got);
        }
        if (++pos >= XRAM_RND_FRAMES)
        {
            pos = 0;
            xram_stage = XR_FINAL;
        }
        return (false);
    default:
        for (a = xr_base + pos, end = a + CHUNK; a < end; a += 4)
        {
            const unsigned int  got = *(volatile unsigned int *)a;

            if (got != pattern_xram(a) && got != pattern_xram_rnd(a))
                record_error_x(a, pattern_xram_rnd(a), pattern_xram(a), got, 4);
        }
        break;
    }
    pos += CHUNK;
    if (pos < xr_size)
        return (false);
    pos = 0;
    if (xram_stage == XR_FINAL)
        return (true);
    ++xram_stage;
    return (false);
}

/* ---- test 12: AA24, the upper 16 MB ------------------------------- */
/* Sends ROMCTL to the SAROO's MCU. Returns its answer (ST_CTRL bits 15..8), or -1 if it didn't answer: an MCU
   firmware without ROMCTL only logs the command and never clears SS_CMD. */
static int              romctl(const int on)
{
    unsigned int        n;

    SS_ARG_REG = (unsigned short)(on ? 1 : 0);
    SS_CMD_REG = SSCMD_ROMCTL;
    for (n = 0; n < 400000u; n++)
    {
        if (SS_CMD_REG == 0)
        {
            aa24_on = on;
            return (SS_ARG_REG & 0xFFu);
        }
    }
    SS_CMD_REG = 0;                     /* give up: free the command register again */
    return (-1);
}

/* Switches AA24 on and checks that 0x23400000+ is RAM of its own. Only addresses 4 MB and more into either 16 MB
   half are written (cart / extra RAM, or unused SDRAM): never the SAROO's own first 4 MB, whatever the pin does.
   Returns S_PASS (AA24 works, left on), or S_NOROMCTL / S_MIRROR / S_UNSTABLE (left off). */
static int              aa24_detect(void)
{
    const int           answer = romctl(1);
    unsigned int        k;
    unsigned int        r;

    aa24_seen[0] = aa24_seen[1] = aa24_seen[2] = 0;
    if (answer < 0)
        return (S_NOROMCTL);
    if ((answer & 0x10) == 0)           /* the FPGA control bit (st_reg_ctrl[12]) didn't stick */
    {
        romctl(0);
        return (S_NOROMCTL);
    }
    for (k = 0; k < AA24_PAIRS; k++)
    {
        const unsigned int  lo = 0x22400000u + k * 0x10000u;
        const unsigned int  hi = lo + 0x01000000u;
        const unsigned int  vlo = 0xA5000000u | (k << 8) | run_count;
        const unsigned int  vhi = 0x5A000000u | (k << 8) | run_count;

        *(volatile unsigned int *)lo = vlo;
        *(volatile unsigned int *)hi = vhi;
        for (r = 0; r < 4; r++)
        {
            const unsigned int  glo = *(volatile unsigned int *)lo;
            const unsigned int  ghi = *(volatile unsigned int *)hi;

            if (glo == vlo && ghi == vhi)
                ++aa24_seen[0];
            else if (glo == vhi && ghi == vhi)
                ++aa24_seen[1];
            else
                ++aa24_seen[2];
        }
    }
    if (aa24_seen[0] == AA24_PAIRS * 4u)
        return (S_PASS);
    romctl(0);
    return (aa24_seen[1] == AA24_PAIRS * 4u ? S_MIRROR : S_UNSTABLE);
}

/* ---- sequencer ----------------------------------------------------- */
static void             publish(const unsigned int done)
{
    cart_res[0] = 0xCA275757u;
    cart_res[1] = cart_id;
    cart_res[2] = (unsigned int)cart_kind;
    cart_res[3] = total_errors;
    cart_res[4] = first_addr;
    cart_res[5] = first_exp;
    cart_res[6] = first_got;
    cart_res[7] = (unsigned int)first_test;
    cart_res[8] = run_count;
    cart_res[9] = pass_count;
    cart_res[10] = done;
    cart_res[11] = cd_loads;
    cart_res[12] = cd_errors;
    cart_res[13] = cls_read;
    cart_res[14] = cls_stored;
    cart_res[15] = cls_other;
    cart_res[16] = all_errors;
    cart_res[17] = fail_runs;
    cart_res[18] = fail_run;
    cart_res[19] = code_calls;
}

/* START: forget every earlier run */
static void             reset_all_runs(void)
{
    run_count = 0;
    pass_count = 0;
    all_errors = 0;
    all_read = 0;
    all_stored = 0;
    all_other = 0;
    fail_runs = 0;
    fail_run = 0;
}

static void             start_run(void)
{
    int                 i;

    for (i = 0; i < T_COUNT; i++)
    {
        test_status[i] = S_PENDING;
        test_errors[i] = 0;
    }
    total_errors = 0;
    first_addr = 0;
    first_exp = 0;
    first_got = 0;
    first_test = 0;
    cls_read = 0;
    cls_stored = 0;
    cls_other = 0;
    first_stored_addr = 0;
    first_stored_val = 0;
    dma_timeout = 0;
    code_calls = 0;
    if (aa24_on)
        romctl(0);                      /* never leave AA24 on between runs */
    cur_test = 0;
    pos = 0;
    ++run_count;
    run_seed = run_count * 0x01234567u + 0x13579BDFu;
    rnd = run_seed | 1u;
    run_start_vbl = jo_core_get_vblank_count();
    run_frames = 0;
    detect_cart();
    phase = (cart_kind == KIND_1MB || cart_kind == KIND_4MB) ? PH_START : PH_NOCART;
    publish(0);
    jo_clear_screen();
}

static void             finish_test(void)
{
    test_status[cur_test] = test_errors[cur_test] ? S_FAIL : S_PASS;
    ++cur_test;
    pos = 0;
    phase = PH_START;
    if (cur_test >= T_COUNT)
    {
        cur_test = T_COUNT - 1;
        phase = PH_DONE;
        run_frames = jo_core_get_vblank_count() - run_start_vbl;
        if (total_errors == 0)
            ++pass_count;
        else
        {
            ++fail_runs;
            if (fail_run == 0)
            {
                fail_run = run_count;
                fail_test = first_test;
                fail_addr = first_addr;
                fail_exp = first_exp;
                fail_got = first_got;
            }
        }
        all_errors += total_errors;
        all_read += cls_read;
        all_stored += cls_stored;
        all_other += cls_other;
        publish(1);
    }
}

static void             step(void)
{
    if (jo_is_pad1_key_down(JO_KEY_A))
        auto_loop = !auto_loop;
    if (jo_is_pad1_key_down(JO_KEY_B))
        init_mode = (init_mode + 1) % INIT_MODES;
    if (jo_is_pad1_key_down(JO_KEY_C))
        cd_load_on = !cd_load_on;
    if (jo_is_pad1_key_down(JO_KEY_Z))
        code_test_on = !code_test_on;
    cd_kick();
    if (phase == PH_DONE || phase == PH_NOCART)
    {
        if (jo_is_pad1_key_down(JO_KEY_START))
        {
            reset_all_runs();
            start_run();
        }
        else if (auto_loop && phase == PH_DONE)
            start_run();
        return;
    }
    run_frames = jo_core_get_vblank_count() - run_start_vbl;
    test_status[cur_test] = S_RUNNING;
    switch (cur_test)
    {
    case T_PROBE:
        run_probe();
        finish_test();
        return;
    case T_ALIAS:
        run_alias();
        finish_test();
        return;
    case T_DMA:
        if (phase == PH_START)
        {
            pos = 0;
            phase = PH_VERIFY;
            return;
        }
        run_dma(pos);
        if (++pos >= DMA_LOCATIONS || dma_timeout)
            finish_test();
        return;
    case T_CODE:
        if (phase == PH_START)
        {
            if (!code_test_on)
            {
                /* Off (Z): only the work-RAM self-test, nothing runs from the cart */
                if (selftest_done == 0)
                    run_selftest(run_seed);
                finish_test();
                if (test_errors[T_CODE] == 0)
                    test_status[T_CODE] = S_SKIP;
                return;
            }
            pos = 0;
            phase = PH_CODE;
            return;
        }
        run_code_block(pos);
        if (++pos >= total_bytes / CODE_SIZE)
            finish_test();
        return;
    case T_XRAM:
        if (phase == PH_START)
        {
            if (!is_saroo())
            {
                /* Not a SAROO: the extra RAM isn't there, don't touch it */
                finish_test();
                if (test_errors[T_XRAM] == 0)
                    test_status[T_XRAM] = S_NOSAROO;
                return;
            }
            pos = 0;
            xr_base = XRAM_BASE;
            xr_size = XRAM_SIZE;
            xram_stage = XR_FILL;
            phase = PH_XRAM;
            return;
        }
        if (run_xram_frame())
            finish_test();
        return;
    case T_AA24:
        if (phase == PH_START)
        {
            int         result;

            if (!is_saroo())
            {
                finish_test();
                if (test_errors[T_AA24] == 0)
                    test_status[T_AA24] = S_NOSAROO;
                return;
            }
            result = aa24_detect();
            if (result != S_PASS)
            {
                finish_test();
                if (test_errors[T_AA24] == 0)
                    test_status[T_AA24] = result;
                return;
            }
            pos = 0;
            xr_base = AA24_BASE;
            xr_size = AA24_SIZE;
            xram_stage = XR_FILL;
            phase = PH_XRAM;
            return;
        }
        if (run_xram_frame())
        {
            romctl(0);
            finish_test();
        }
        return;
    case T_CACHED:
        /* Only reads: straight to the verify phase */
        if (phase == PH_START)
        {
            pos = 0;
            phase = PH_VERIFY;
        }
        break;
    case T_RANDOM:
        if (phase == PH_START)
        {
            pos = 0;
            phase = PH_RANDOM;
        }
        if (phase == PH_RANDOM)
        {
            run_random_frame();
            if (++pos >= RANDOM_FRAMES)
            {
                pos = 0;
                phase = PH_VERIFY;
            }
            return;
        }
        break;
    default:
        break;
    }
    switch (phase)
    {
    case PH_START:
        pos = 0;
        phase = PH_FILL;
        break;
    case PH_FILL:
        fill_chunk(cur_test, pos);
        pos += CHUNK;
        if (pos >= total_bytes)
        {
            pos = 0;
            if (cur_test == T_RETAIN)
            {
                wait_until = jo_core_get_vblank_count() + RETAIN_FRAMES;
                phase = PH_WAIT;
            }
            else
                phase = PH_VERIFY;
        }
        break;
    case PH_WAIT:
        if ((int)(jo_core_get_vblank_count() - wait_until) >= 0)
            phase = PH_VERIFY;
        break;
    case PH_VERIFY:
        verify_chunk(cur_test, pos);
        pos += CHUNK;
        if (pos >= total_bytes)
            finish_test();
        break;
    default:
        break;
    }
}

/* ---- display ------------------------------------------------------- */
static void             draw_bar(const int y, const unsigned int percent)
{
    char                bar[24];
    unsigned int        i;
    const unsigned int  filled = percent / 5u;

    bar[0] = '[';
    for (i = 0; i < 20; i++)
        bar[1 + i] = i < filled ? '#' : '.';
    bar[21] = ']';
    bar[22] = '\0';
    jo_printf(1, y, "%s %3u%%", bar, percent);
}

static void             draw(void)
{
    int                 i;
    unsigned int        percent;

    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    jo_printf(1, 1, "RAM CART STRESS 1.4        RUN %u", run_count);
    switch (cart_kind)
    {
    case KIND_4MB:
        jo_printf(1, 2, "CART %02X 4MB ", cart_id);
        break;
    case KIND_1MB:
        jo_printf(1, 2, "CART %02X 1MB ", cart_id);
        break;
    case KIND_UNKNOWN:
        jo_printf(1, 2, "CART ID %02X  NOT A SEGA RAM CARTRIDGE", cart_id);
        jo_printf_with_color(1, 5, JO_COLOR_INDEX_Yellow, "NOT TESTED: NOTHING WAS WRITTEN TO IT");
        break;
    default:
        jo_printf(1, 2, "CART ID %02X  NO CARTRIDGE", cart_id);
        jo_printf_with_color(1, 5, JO_COLOR_INDEX_Yellow, "INSERT A RAM CARTRIDGE (POWER OFF)");
        break;
    }
    if (phase == PH_NOCART)
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_White);
        jo_printf(1, 7, "EMULATOR: SELECT THE 4MB RAM CART");
        jo_printf(1, 8, "IN THE CARTRIDGE SETTINGS");
        jo_printf(1, 25, "START: TRY AGAIN");
        return;
    }

    if (init_applied)
        jo_printf_with_color(14, 2, JO_COLOR_INDEX_Yellow, "TIMING: SEGA %08X   ", SEGA_ASR0);
    else
        jo_printf_with_color(14, 2, JO_COLOR_INDEX_White, "TIMING: UNCHANGED      ");

    for (i = 0; i < T_COUNT; i++)
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_White);
        jo_printf(1, 3 + i, "%d %-14s", i + 1, test_name[i]);
        switch (test_status[i])
        {
        case S_RUNNING:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "RUNNING         ");
            break;
        case S_PASS:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Green, "PASS            ");
            break;
        case S_FAIL:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Red, "FAIL %-10u ", test_errors[i]);
            break;
        case S_SKIP:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "OFF (Z)         ");
            break;
        case S_NOSAROO:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "NO SAROO        ");
            break;
        case S_NOROMCTL:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "MCU: NO ROMCTL  ");
            break;
        case S_MIRROR:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "NO AA24 (MIRROR)");
            break;
        case S_UNSTABLE:
            jo_printf_with_color(18, 3 + i, JO_COLOR_INDEX_Yellow, "UNSTABLE %u/%u/%u", aa24_seen[0], aa24_seen[1],
                                 aa24_seen[2]);
            break;
        default:
            jo_printf(18, 3 + i, "-               ");
            break;
        }
    }

    /* CD load */
    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    jo_printf(1, 15, "CD LOAD %s  LOADS %-6u", cd_load_on ? "ON " : "OFF", cd_loads);
    if (cd_errors || cd_start_fail)
        jo_printf_with_color(26, 15, JO_COLOR_INDEX_Red, "BAD %u/%u   ", cd_errors, cd_start_fail);
    else
        jo_printf_with_color(26, 15, JO_COLOR_INDEX_Green, "DATA OK   ");

    /* Progress of the current test */
    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    percent = total_bytes ? ((pos >> 10) * 100u) / (total_bytes >> 10) : 0;
    if (percent > 100)
        percent = 100;
    if (cur_test == T_DMA && phase != PH_DONE)
    {
        jo_printf(1, 16, "%-14s TRANSFER %u OF %u   ", test_name[cur_test],
                  pos < DMA_LOCATIONS ? pos + 1 : DMA_LOCATIONS, DMA_LOCATIONS);
        draw_bar(17, (pos * 100u) / DMA_LOCATIONS);
    }
    else if (phase == PH_RANDOM)
    {
        jo_printf(1, 16, "%-14s ACCESSING %4u   ", test_name[cur_test], pos);
        draw_bar(17, (pos * 100u) / RANDOM_FRAMES);
    }
    else if (phase == PH_CODE)
    {
        const unsigned int  blocks = total_bytes / CODE_SIZE;

        jo_printf(1, 16, "%-14s BLOCK %2u OF %2u  ", test_name[cur_test], pos < blocks ? pos + 1 : blocks, blocks);
        draw_bar(17, (pos * 100u) / blocks);
    }
    else if (phase == PH_XRAM)
    {
        static const char *const stage_name[4] = { "WRITING  ", "VERIFYING", "RANDOM   ", "CHECKING " };

        if (xram_stage == XR_RANDOM)
        {
            jo_printf(1, 16, "%s %08X %s %4u    ", cur_test == T_AA24 ? "AA24" : "XRAM", xr_base,
                      stage_name[xram_stage], pos);
            draw_bar(17, (pos * 100u) / XRAM_RND_FRAMES);
        }
        else
        {
            jo_printf(1, 16, "%s %08X %s %5u KB", cur_test == T_AA24 ? "AA24" : "XRAM", xr_base + pos,
                      stage_name[xram_stage], pos >> 10);
            draw_bar(17, (pos >> 10) * 100u / (xr_size >> 10));
        }
    }
    else
    {
        switch (phase)
        {
        case PH_FILL:
            jo_printf(1, 16, "%-14s WRITING   %4u KB", test_name[cur_test], pos >> 10);
            break;
        case PH_VERIFY:
            jo_printf(1, 16, "%-14s VERIFYING %4u KB", test_name[cur_test], pos >> 10);
            break;
        case PH_WAIT:
            {
                const int   left = (int)(wait_until - jo_core_get_vblank_count());

                jo_printf(1, 16, "%-14s WAITING   %4d   ", test_name[cur_test], left > 0 ? left : 0);
                percent = left > 0 ? 100u - ((unsigned int)left * 100u) / RETAIN_FRAMES : 100u;
            }
            break;
        case PH_DONE:
            jo_printf(1, 16, "ALL TESTS DONE                    ");
            percent = 100;
            break;
        default:
            jo_printf(1, 16, "%-14s                   ", test_name[cur_test]);
            break;
        }
        draw_bar(17, percent);
    }

    /* Errors */
    if (total_errors)
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_Red);
        jo_printf(1, 18, "ERRORS %-8u", total_errors);
        jo_printf(1, 19, "FIRST AT %08X  TEST %d ", first_addr, first_test + 1);
        jo_printf(1, 20, "EXPECTED %08X GOT %08X", first_exp, first_got);
        /* Re-read check: is the wrong value in the RAM (STORED) or was only the read wrong (READ)? */
        jo_printf_with_color(1, 24, JO_COLOR_INDEX_Yellow, "READ BAD %-4u STORED BAD %-4u OTHER %-3u",
                             cls_read, cls_stored, cls_other);
        if (dma_timeout)
            jo_printf(1, 27, "DMA %s STUCK CHCR %08X", dma_timeout == 1 ? "WR" : "RD", dma_chcr);
        else if (cls_stored)
            jo_printf(1, 27, "1ST STORED BAD %08X = %08X", first_stored_addr, first_stored_val);
    }
    else
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_White);
        jo_printf(1, 18, "ERRORS 0       ");
        if (fail_run)
        {
            /* This run is clean, but an earlier one failed: keep its details on screen until START */
            jo_printf_with_color(1, 19, JO_COLOR_INDEX_Yellow, "RUN %u FAILED: TEST %d AT %08X ", fail_run,
                                 fail_test + 1, fail_addr);
            jo_printf_with_color(1, 20, JO_COLOR_INDEX_Yellow, "EXPECTED %08X GOT %08X", fail_exp, fail_got);
            jo_printf_with_color(1, 24, JO_COLOR_INDEX_Yellow, "ALL: READ %-4u STORED %-4u OTHER %-4u",
                                 all_read, all_stored, all_other);
        }
    }
    /* All runs since power-on or START */
    if (all_errors)
        jo_printf_with_color(17, 18, JO_COLOR_INDEX_Red, "ALL RUNS %u IN %u RUNS ", all_errors, fail_runs);
    else
        jo_printf_with_color(17, 18, JO_COLOR_INDEX_Green, "ALL RUNS 0          ");

    if (phase == PH_DONE)
    {
        if (total_errors)
            jo_printf_with_color(1, 21, JO_COLOR_INDEX_Red, "RESULT: FAIL   ");
        else
            jo_printf_with_color(1, 21, JO_COLOR_INDEX_Green, "RESULT: PASS   ");
    }
    else
        jo_printf_with_color(1, 21, JO_COLOR_INDEX_Yellow, "RESULT: TESTING");
    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    jo_printf(1, 22, "TIME %u FRAMES   ", run_frames);
    if (selftest_done == 1)
        jo_printf_with_color(21, 22, JO_COLOR_INDEX_Green, "CODE SELFTEST OK ");
    else if (selftest_done == 2)
        jo_printf_with_color(21, 22, JO_COLOR_INDEX_Red, "CODE SELFTEST BAD");
    jo_printf(1, 23, "RUNS PASSED %u OF %u   ", pass_count, phase == PH_DONE ? run_count : run_count - 1);
    jo_printf(1, 25, "START: RESTART  A: LOOP %s  C: CD", auto_loop ? "ON " : "OFF");
    jo_printf(1, 26, "B: SETUP NEXT RUN %s",
              init_mode == INIT_TB47 ? "SEGA TB47" : init_mode == INIT_AUTO ? "AUTO     " : "NONE     ");
    jo_printf(29, 26, "Z:CODE %s", code_test_on ? "ON " : "OFF");
}

void                    jo_main(void)
{
    jo_core_init(JO_COLOR_Black);
    start_run();
    jo_core_add_callback(step);
    jo_core_add_callback(draw);
    jo_core_run();
}
