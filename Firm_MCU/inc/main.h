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

u32 get_build_date(void);

void *malloc(uint32_t size);
void free(void *p);
void memcpy32(void *dst, void *src, int len);
int mem_test(u32 addr, int size);


void uart4_init(void);
uint8_t _getc(void);
int  _getc_tmout(int tmout_ms);
void _putc(u8 byte);
void _puts(char *str);
void uart4_puts(char *str);

int printk(char *fmt, ...);
int sprintk(char *sbuf, const char *fmt, ...);
void hex_dump(char *str, void *addr, int size);

void simple_shell(void);

int tiny_xmodem_recv(u8 *dest);



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
int flash_write_config(u8 *cfgbuf);


/******************************************************************************/

#define MAKE_LEDEVENT(freq, times, flag)  (((flag)<<16) | ((times)<<8) | (freq))

#define LEDEV_NONE              0
#define LEDEV_SD_ERROR          MAKE_LEDEVENT(10, 10, 2)
#define LEDEV_FPGA_ERROR        MAKE_LEDEVENT(10, 20, 2)
#define LEDEV_SDRAM_ERROR       MAKE_LEDEVENT(15, 8,  2)
#define LEDEV_NOFIRM            MAKE_LEDEVENT(15, 3,  1)
#define LEDEV_FILE_ERROR        MAKE_LEDEVENT(15, 5,  1)
#define LEDEV_SCFG_ERROR        MAKE_LEDEVENT(15, 4,  1)
#define LEDEV_CSCT              MAKE_LEDEVENT(7,  1,  0)
#define LEDEV_BUSY              MAKE_LEDEVENT(50, 1,  1)
#define LEDEV_OK                MAKE_LEDEVENT(13, 2,  1)


void led_event(int ev);

/******************************************************************************/

#endif

