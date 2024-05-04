
#ifndef _MAIN_H_
#define _MAIN_H_

/*****************************************************************************/

typedef unsigned  char u8;
typedef unsigned short u16;
typedef unsigned   int u32;
typedef unsigned long long u64;


#define REG(x)   (*(volatile unsigned   int*)(x))
#define REG32(x) (*(volatile unsigned   int*)(x))
#define REG16(x) (*(volatile unsigned short*)(x))
#define REG8(x)  (*(volatile unsigned  char*)(x))

#define NULL ((void*)0)


#define SS_ID    REG16(0x25807000)
#define SS_VER   REG16(0x25807002)
#define SS_CTRL  REG16(0x25897004)
#define SS_STAT  REG16(0x25897008)
#define SS_TIMER REG32(0x2589700c)
#define SS_CMD   REG16(0x25897010)
#define SS_ARG   REG16(0x25897014)

// SS_CTRL
#define SAROO_EN       0x8000
#define CS0_NONE       0x0000
#define CS0_DATA       0x1000
#define CS0_RAM1M      0x2000
#define CS0_RAM4M      0x3000
#define SAROO_LED      0x0100

// SS_CMD
#define SSCMD_PRINTF   0x0001
#define SSCMD_LISTDISC 0x0002
#define SSCMD_LOADDISC 0x0003
#define SSCMD_CHECK    0x0004
#define SSCMD_UPDATE   0x0005
#define SSCMD_FILERD   0x0006
#define SSCMD_FILEWR   0x0007
#define SSCMD_LISTBINS 0x0008
#define SSCMD_SSAVE    0x0009
#define SSCMD_LSAVE    0x000a
#define SSCMD_LMEMS    0x000b
#define SSCMD_SMEMS    0x000c


#define IMGINFO_ADDR   0x22080000
#define SYSINFO_ADDR   0x220a0000
#define TMPBUFF_ADDR   0x220c0000


/*****************************************************************************/
// cdblock


typedef struct {
	u16 cr1;
	u16 cr2;
	u16 cr3;
	u16 cr4;
}CDCMD;

typedef struct {
	int fad;
	int size;
	u8  unit;
	u8  gap;
	u8  fn;
	u8  attr;
}CdcFile;

#define CR1     REG16(0x25890018)
#define CR2     REG16(0x2589001c)
#define CR3     REG16(0x25890020)
#define CR4     REG16(0x25890024)
#define HIRQ    REG16(0x25890008)
#define HMSK    REG16(0x2589000c)

#define HIRQ_CMOK   0x0001
#define HIRQ_DRDY   0x0002
#define HIRQ_CSCT   0x0004
#define HIRQ_BFUL   0x0008
#define HIRQ_PEND   0x0010
#define HIRQ_DCHG   0x0020
#define HIRQ_ESEL   0x0040
#define HIRQ_EHST   0x0080
#define HIRQ_ECPY   0x0100
#define HIRQ_EFLS   0x0200
#define HIRQ_SCDQ   0x0400


#define STATUS_BUSY             0x00
#define STATUS_PAUSE            0x01
#define STATUS_STANDBY          0x02
#define STATUS_PLAY             0x03
#define STATUS_SEEK             0x04
#define STATUS_SCAN             0x05
#define STATUS_OPEN             0x06
#define STATUS_NODISC           0x07
#define STATUS_RETRY            0x08
#define STATUS_ERROR            0x09
#define STATUS_FATAL            0x0a
#define STATUS_PERIODIC         0x20
#define STATUS_TRANSFER         0x40
#define STATUS_WAIT             0x80
#define STATUS_REJECT           0xff


#define INDIRECT_CALL(addr, return_type, ...) (**(return_type(**)(__VA_ARGS__)) addr)

#define bios_run_cd_player  INDIRECT_CALL(0x0600026C, void, void)
#define bios_loadcd_init1   INDIRECT_CALL(0x060002dc, int,  int)  // 00002650
#define bios_loadcd_init    INDIRECT_CALL(0x0600029c, int,  int)  // 00001904
#define bios_loadcd_read    INDIRECT_CALL(0x060002cc, int,  void) // 00001912
#define bios_loadcd_boot    INDIRECT_CALL(0x06000288, int,  void)  // 000018A8

//!< mode 0 -> check, 1 -> do auth
#define bios_check_cd_auth  INDIRECT_CALL(0x06000270, int,  int mode)


void clear_hirq(int mask);
int wait_hirq(int mask);
int wait_status(int wait);
int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *cmdname);

int cdc_get_status(int *status);
int cdc_get_hwinfo(int *status);
int cdc_cdb_init(int standby);
int cdc_end_trans(int *cdwnum);
int cdc_get_toc(u8 *buf);

int cdc_play_fad(int mode, int start_fad, int range);

int cdc_cddev_connect(int filter);
int cdc_get_cddev(int *selnum);

int cdc_set_filter_range(int selnum, int start_fad, int num_sector);
int cdc_get_filter_range(int selnum, int *fad, int *num_sector);
int cdc_set_filter_mode(int selnum, int mode);
int cdc_get_filter_mode(int selnum, int *mode);
int cdc_set_filter_connect(int selnum, int c_true, int c_false);
int cdc_get_filter_connect(int selnum, int *c_true, int *c_false);
int cdc_reset_selector(int flag, int selnum);

int cdc_get_buffer_size(int *total, int *pnum, int *free);
int cdc_get_numsector(int selnum, int *snum);
int cdc_calc_actsize(int bufno, int spos, int snum);
int cdc_get_actsize(int *actsize);
int cdc_get_sector_info(int bufno, int secno, int *fad);

int cdc_set_size(int size);
int cdc_get_data(int bufnum, int spos, int snum);
int cdc_get_del_data(int bufnum, int spos, int snum);
void cdc_trans_data(u8 *buf, int length);

int cdc_auth_status(int *status);
int cdc_auth_device(void);

int cdc_change_dir(int selnum, int fid);
int cdc_read_dir(int selnum, int fid);
int cdc_get_file_scope(int *fid, int *fnum, int *drend);
int cdc_get_file_info(int fid, CdcFile *buf);
int cdc_read_file(int selnum, int fid, int offset);
int cdc_abort_file(void);






void cdc_dump(int count);
void show_selector(int id);
void cdblock_on(int wait);
void cdblock_off(void);
int cdblock_check(void);



int bios_cd_cmd(int type);
int cdc_read_sector(int fad, int size, u8 *buf);
void my_cdplayer(void);

void bup_init(u8 *lib_addr, u8 *work_addr, void *cfg);



/*****************************************************************************/

extern const int HZ;

void reset_timer(void);
u32 get_timer(void);
void usleep(u32 us);
void msleep(u32 ms);



/*****************************************************************************/

#define PAD_RIGHT   (1<<15)
#define PAD_LEFT    (1<<14)
#define PAD_DOWN    (1<<13)
#define PAD_UP      (1<<12)
#define PAD_START   (1<<11)
#define PAD_A       (1<<10)
#define PAD_C       (1<<9)
#define PAD_B       (1<<8)
#define PAD_RT      (1<<7)
#define PAD_X       (1<<6)
#define PAD_Y       (1<<5)
#define PAD_Z       (1<<4)
#define PAD_LT      (1<<3)

u32 pad_read(void);

int smpc_cmd(int cmd);

/*****************************************************************************/


void conio_init(void);
int conio_put_char(int x, int y, int color, int ch);
void conio_putc(int ch);
u32 conio_getc(void);

#define BUTTON_DOWN(stat, bt)  ( (stat & ((bt<<16)|bt)) ==  ((bt<<16)|bt) )
#define BUTTON_UP  (stat, bt)  ( ((stat^(~bt)) & ((bt<<16)|bt)) == ((bt<<16)|bt) )


extern int fbw, fbh;

void put_pixel(int x, int y, int c);
void put_line (int x1, int y1, int x2, int y2, int c);
void put_hline(int y, int x1, int x2, int c);
void put_vline(int x, int y1, int y2, int c);
void put_rect (int x1, int y1, int x2, int y2, int c);
void put_box(int x1, int y1, int x2, int y2, int c);


void nbg1_on(void);
void nbg1_set_cram(int index, int r, int g, int b);
void nbg1_put_pixel(int x, int y, int color);


/*****************************************************************************/


void sci_init(void);
void sci_putc(int ch);
int sci_getc(int timeout);


/*****************************************************************************/

extern int to_stm32;
extern int gets_from_stm32;


extern void (*printk_putc)(int ch);
int printk(char *fmt, ...);
int snprintf(char *buf, int size, char *fmt, ...);
#define sprintf(buf, fmt, ...) snprintf(buf, 1024, fmt, __VA_ARGS__)


int strlen(char *s);
int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, int n);
char *strcpy(char *dst, char *src);
char *strncpy(char *dst, char *src, int n);
char *strchr(char *s1, int ch);
u32 strtoul(char *str, char **endptr, int requestedbase, int *ret);

void *memset(void *s, int v, int n);
void *memcpy(void *to, void *from, int n);
int memcmp(void *dst, void *src, int n);

int tiny_xmodem_recv(u8 *dest);

/*****************************************************************************/

void sci_shell(void);
int gets(char *buf, int len);
int str2hex(char *str, int *hex);
void dump(int argc, int *args, int width);
void mem_dump(char *str, void *addr, int size);

int read_file (char *name, int offset, int size, void *buf);
int write_file(char *name, int offset, int size, void *buf);

u32 BE32(void *ptr);
u32 LE32(void *ptr);

u32 get_build_date(void);

/*****************************************************************************/


#define ISR_GEN_INST    4
#define ISR_SLOT_INST   6
#define ISR_CPU_ADDR    9
#define ISR_DMA_ADDR    10
#define ISR_NMI         11
#define ISR_UBR         12

#define MASTER_CPU      0x00
#define SLAVE_CPU       0x80



typedef struct {
	u32     pr;
	u32     gbr;
	u32     vbr;
	u32     mach;
	u32     macl;
	u32     r15;
	u32     r14;
	u32     r13;
	u32     r12;
	u32     r11;
	u32     r10;
	u32     r9;
	u32     r8;
	u32     r7;
	u32     r6;
	u32     r5;
	u32     r4;
	u32     r3;
	u32     r2;
	u32     r1;
	u32     r0;
	u32     pc;
	u32     sr;
}REGS;


u32 crc32(u8 *buf, int len, u32 crc);

u32 get_sr(void);
void set_sr(u32 sr);
void install_isr(int type);
void break_in_game(int break_pc, void *handle);
void break_in_game_next(int break_pc, void *handle);
void install_ubr_isr(void);
void set_break_pc(u32 addr, u32 mask);
void set_break_rw(u32 addr, u32 mask, int rw);

extern int game_break_pc;

#define BRK_READ  0x04
#define BRK_WRITE 0x08
#define BRK_RW    0x0c


/*****************************************************************************/


#define MENU_ITEMS    12

typedef struct menu_desc_t {
	int num;
	int current;
	char title[64];
	char items[MENU_ITEMS][96];
	char *version;
	int (*handle)(int index);
}MENU_DESC;

#define MENU_EXIT     99
#define MENU_RESTART  98

void menu_init(void);
void add_menu_item(MENU_DESC *menu, char *item);
int menu_run(MENU_DESC *menu);

void menu_status(MENU_DESC *menu, char *string);
void draw_menu_frame(MENU_DESC *menu);
void menu_update(MENU_DESC *menu);
int menu_default(MENU_DESC *menu, int ctrl);


extern int lang_id;
void lang_init(void);
void lang_next(void);
char *TT(char *str);

/*****************************************************************************/
// game patch

extern int skip_patch;
extern int need_bup;

void patch_game(char *id);


/*****************************************************************************/

extern u32 debug_flag;

/*****************************************************************************/

#endif

