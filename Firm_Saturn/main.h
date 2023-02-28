
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


#define SS_ID    REG16(0x25897000)
#define SS_VER   REG16(0x25897002)
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


/*****************************************************************************/


#define INDIRECT_CALL(addr, return_type, ...) (**(return_type(**)(__VA_ARGS__)) addr)
#define bios_run_cd_player  INDIRECT_CALL(0x0600026C, void, void)

int bios_cd_cmd(void);


/*****************************************************************************/

extern const int HZ;

void reset_timer(void);
u32 get_timer(void);
void usleep(u32 us);
void msleep(u32 ms);



/*****************************************************************************/

#define PAD_LT      (1<<12)
#define PAD_START   (1<<11)
#define PAD_A       (1<<10)
#define PAD_C       (1<<9)
#define PAD_B       (1<<8)
#define PAD_RIGHT   (1<<7)
#define PAD_LEFT    (1<<6)
#define PAD_DOWN    (1<<5)
#define PAD_UP      (1<<4)
#define PAD_RT      (1<<3)
#define PAD_X       (1<<2)
#define PAD_Y       (1<<1)
#define PAD_Z       (1<<0)

u32 pad_read(void);

int smpc_cmd(int cmd);

/*****************************************************************************/


void conio_init(void);
void conio_put_char(int x, int y, int color, int ch);
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

/*****************************************************************************/


void sci_init(void);
void sci_putc(int ch);
int sci_getc(int timeout);


/*****************************************************************************/

extern void (*printk_putc)(int ch);
int printk(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);

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


/*****************************************************************************/

void sci_shell(void);
int gets(char *buf, int len);
int str2hex(char *str, int *hex);
void dump(int argc, int *args, int width);
void mem_dump(char *str, void *addr, int size);

/*****************************************************************************/


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

void break_in_game(int break_pc, void *handle);
void break_in_game_next(int break_pc, void *handle);
void install_ubr_isr(void);
void set_break_pc(u32 addr, u32 mask);
void set_break_rw(u32 addr, u32 mask, int rw);

#define BRK_READ  0x04
#define BRK_WRITE 0x08
#define BRK_RW    0x0c


/*****************************************************************************/


typedef struct menu_desc_t {
	int num;
	int current;
	char title[32];
	char items[11][64];
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


/*****************************************************************************/
// game patch

extern int skip_patch;

void patch_game(char *id);


/*****************************************************************************/

#endif

