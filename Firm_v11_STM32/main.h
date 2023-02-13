
#include "stm32f10x.h"
#include "rtl.h"
#include "string.h"


typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         s32;
typedef unsigned  long long u64;
typedef long long s64;


#define REG8(x)  ( *(volatile unsigned char*) (x) )
#define REG16(x) ( *(volatile unsigned short*)(x) )
#define REG(x)   ( *(volatile unsigned int*)  (x) )


#define in_isr()        __get_IPSR()
#define disable_irq()   __disable_irq()
#define restore_irq(pm) __set_PRIMASK(pm)


void hex_dump(char *str, void *buf, int size);

void usart1_init(void);
void usart1_puts(char *str, int len);
void _putc(u8 byte);
void _puts(char *str);
int  _getc(void);



void memcpy32(void *dst, void *src, int len);

int printk(char *fmt, ...);
int eprint(char *fmt, ...);
int sprintk(char *buf, const char *fmt, ...);

void simple_shell(void);



int sdio_init(void);

int scan_dir(char *dirname, int level, int (*func)(char *, char*, int));

void spi2_init(void);
void spi2_set_notify(void *handle);
void spi2_trans(u32 data);
int fill_audio_buffer(u8 *data);

int epcs_readid(void);
int epcs_write(int addr, int len, u8 *buf);
int epcs_read(int addr, int len, u8 *buf);
void fpga_config(void);

void saturn_config(void);


