
#include "stm32f10x.h"
#include "rtl.h"
#include "string.h"


#define REG8(x)  ( *(volatile unsigned char*) (x) )
#define REG16(x) ( *(volatile unsigned short*)(x) )
#define REG(x)   ( *(volatile unsigned int*)  (x) )


int disable_irq(void);
void restore_irq(int stat);
int get_primask(void);
int get_control(void);

#define in_isr()  (__get_IPSR())


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


static void spi1_init(void);

int epcs_readid(void);
int epcs_write(int addr, int len, u8 *buf);
int epcs_read(int addr, int len, u8 *buf);
void fpga_config(void);

void saturn_config(void);


