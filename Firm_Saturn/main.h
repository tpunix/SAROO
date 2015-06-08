
#ifndef _MAIN_H_
#define _MAIN_H_

typedef unsigned  char u8;
typedef unsigned short u16;
typedef unsigned   int u32;
typedef unsigned long long u64;

#define REG(x)   (*(volatile unsigned   int*)(x))
#define REG16(x) (*(volatile unsigned short*)(x))
#define REG8(x)  (*(volatile unsigned  char*)(x))

#define NULL ((void*)0)




extern const int HZ;

void reset_timer(void);
u32 get_timer(void);
void usleep(u32 us);
void msleep(u32 ms);


void conio_init(void);
void conio_put_char(int x, int y, int color, char ch);
void conio_putc(int ch);


void sci_init(void);
void sci_putc(int ch);
int sci_getc(int timeout);


extern void (*printk_putc)(int ch);
int printk(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);

int strlen(char *s);
int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, int n);
char *strcpy(char *dst, char *src);
char *strncpy(char *dst, char *src, int n);
u32 strtoul(char *str, char **endptr, int requestedbase, int *ret);

void *memset(void *s, int v, int n);
void *memcpy(void *to, void *from, int n);
int memcmp(void *dst, void *src, int n);




void mem_dump(char *str, void *addr, int size);



u32 crc32(u8 *buf, int len, u32 crc);



int smpc_cmd(int cmd);


#endif

