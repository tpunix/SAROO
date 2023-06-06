#ifndef _MAIN_H_
#define _MAIN_H_


#include <stm32h7xx.h>
#include <cmsis_os2.h>
#include <string.h>

/******************************************************************************/

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         s32;
typedef unsigned  long long u64;
typedef long long s64;



#define in_isr()        __get_IPSR()
#define disable_irq()   __disable_irq()
#define restore_irq(pm) __set_PRIMASK(pm)

#define NOTHING()    __asm volatile("")

/******************************************************************************/


void *malloc(uint32_t size);
void free(void *p);
void memcpy32(void *dst, void *src, int len);

void uart4_init(void);
u8   _getc(void);
void _putc(u8 byte);
void _puts(char *str);
void uart4_puts(char *str);

int printk(char *fmt, ...);
int sprintk(char *sbuf, const char *fmt, ...);
void hex_dump(char *str, void *addr, int size);

void simple_shell(void);



int sdio_init(void);
int sdio_reset(void);

int scan_dir(char *dirname, int level, int (*func)(char *, char*, int));

void spi2_init(void);
void spi2_set_notify(void *handle);
void spi2_trans(u32 data);
int fill_audio_buffer(u8 *data);

void saturn_config(void);


int flash_update(int check);
int fpga_update(int check);


/******************************************************************************/

#endif

