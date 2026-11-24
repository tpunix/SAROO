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

typedef struct
{
    unsigned int        base;
    unsigned int        size;
}                       bank_t;

enum { KIND_NONE, KIND_1MB, KIND_4MB, KIND_UNKNOWN };
enum { T_PROBE, T_ALIAS, T_ADDR, T_CACHED, T_RANDOM, T_INV, T_CHK16, T_RETAIN, T_DMA, T_COUNT };
enum { S_PENDING, S_RUNNING, S_PASS, S_FAIL };
enum { PH_START, PH_FILL, PH_WAIT, PH_VERIFY, PH_RANDOM, PH_DONE, PH_NOCART };

static const char *const test_name[T_COUNT] =
{
    "PROBE 16/32", "ALIASING", "ADDRESS", "CACHED READ", "RANDOM W/R", "INVERTED", "CHECKER 16BIT",
    "RETENTION 3S", "DMA SH2"
};

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
volatile unsigned int   cart_res[16];

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
    cd_kick();
    if (phase == PH_DONE || phase == PH_NOCART)
    {
        if (jo_is_pad1_key_down(JO_KEY_START) || (auto_loop && phase == PH_DONE))
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
    jo_printf(1, 1, "RAM CART STRESS 1.1        RUN %u", run_count);
    switch (cart_kind)
    {
    case KIND_4MB:
        jo_printf(1, 2, "CART ID %02X  EXTENDED RAM 4MB (32MBIT)", cart_id);
        jo_printf(1, 3, "RAM 4096 KB AT %08X", banks[0].base);
        break;
    case KIND_1MB:
        jo_printf(1, 2, "CART ID %02X  EXTENDED RAM 1MB (8MBIT)", cart_id);
        jo_printf(1, 3, "RAM 2 X 512 KB AT %08X %08X", banks[0].base, banks[1].base);
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
        jo_printf_with_color(1, 4, JO_COLOR_INDEX_Yellow, "TIMING THIS RUN: SEGA ASR0 %08X ", SEGA_ASR0);
    else
        jo_printf_with_color(1, 4, JO_COLOR_INDEX_White, "TIMING THIS RUN: UNCHANGED (NO SETUP)");

    for (i = 0; i < T_COUNT; i++)
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_White);
        jo_printf(1, 5 + i, "%d %-14s", i + 1, test_name[i]);
        switch (test_status[i])
        {
        case S_RUNNING:
            jo_printf_with_color(18, 5 + i, JO_COLOR_INDEX_Yellow, "RUNNING         ");
            break;
        case S_PASS:
            jo_printf_with_color(18, 5 + i, JO_COLOR_INDEX_Green, "PASS            ");
            break;
        case S_FAIL:
            jo_printf_with_color(18, 5 + i, JO_COLOR_INDEX_Red, "FAIL %-10u ", test_errors[i]);
            break;
        default:
            jo_printf(18, 5 + i, "-               ");
            break;
        }
    }

    /* CD load */
    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    jo_printf(1, 14, "CD LOAD %s  LOADS %-6u", cd_load_on ? "ON " : "OFF", cd_loads);
    if (cd_errors || cd_start_fail)
        jo_printf_with_color(26, 14, JO_COLOR_INDEX_Red, "BAD %u/%u   ", cd_errors, cd_start_fail);
    else
        jo_printf_with_color(26, 14, JO_COLOR_INDEX_Green, "DATA OK   ");

    /* Progress of the current test */
    jo_set_printf_color_index(JO_COLOR_INDEX_White);
    percent = total_bytes ? ((pos >> 10) * 100u) / (total_bytes >> 10) : 0;
    if (percent > 100)
        percent = 100;
    if (cur_test == T_DMA && phase != PH_DONE)
    {
        jo_printf(1, 15, "%-14s TRANSFER %u OF %u   ", test_name[cur_test],
                  pos < DMA_LOCATIONS ? pos + 1 : DMA_LOCATIONS, DMA_LOCATIONS);
        draw_bar(16, (pos * 100u) / DMA_LOCATIONS);
    }
    else if (phase == PH_RANDOM)
    {
        jo_printf(1, 15, "%-14s ACCESSING %4u   ", test_name[cur_test], pos);
        draw_bar(16, (pos * 100u) / RANDOM_FRAMES);
    }
    else
    {
        switch (phase)
        {
        case PH_FILL:
            jo_printf(1, 15, "%-14s WRITING   %4u KB", test_name[cur_test], pos >> 10);
            break;
        case PH_VERIFY:
            jo_printf(1, 15, "%-14s VERIFYING %4u KB", test_name[cur_test], pos >> 10);
            break;
        case PH_WAIT:
            {
                const int   left = (int)(wait_until - jo_core_get_vblank_count());

                jo_printf(1, 15, "%-14s WAITING   %4d   ", test_name[cur_test], left > 0 ? left : 0);
                percent = left > 0 ? 100u - ((unsigned int)left * 100u) / RETAIN_FRAMES : 100u;
            }
            break;
        case PH_DONE:
            jo_printf(1, 15, "ALL TESTS DONE                    ");
            percent = 100;
            break;
        default:
            jo_printf(1, 15, "%-14s                   ", test_name[cur_test]);
            break;
        }
        draw_bar(16, percent);
    }

    /* Errors */
    if (total_errors)
    {
        jo_set_printf_color_index(JO_COLOR_INDEX_Red);
        jo_printf(1, 18, "ERRORS %u", total_errors);
        jo_printf(1, 19, "FIRST AT %08X  TEST %d", first_addr, first_test + 1);
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
        jo_printf(1, 18, "ERRORS 0");
    }

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
    jo_printf(1, 23, "RUNS PASSED %u OF %u   ", pass_count, phase == PH_DONE ? run_count : run_count - 1);
    jo_printf(1, 25, "START: RUN AGAIN  A: LOOP %s  C: CD", auto_loop ? "ON " : "OFF");
    jo_printf(1, 26, "B: SETUP NEXT RUN %s",
              init_mode == INIT_TB47 ? "SEGA TB47" : init_mode == INIT_AUTO ? "AUTO     " : "NONE     ");
}

void                    jo_main(void)
{
    jo_core_init(JO_COLOR_Black);
    start_run();
    jo_core_add_callback(step);
    jo_core_add_callback(draw);
    jo_core_run();
}
