/*
 * [MENU_WRAM] Helpers used by both the menu (in work RAM) and the resident
 * game-side code (in the cart): moved here unchanged from main.c and
 * sci_shell.c. This file is linked into both images, so each has its own copy
 * and the resident code never calls into the menu (which a game overwrites).
 */

#include "main.h"
#include "smpc.h"
#include "vdp2.h"

int gets_from_stm32 = 0;	// was in sci_shell.c

u32 debug_flag;
int bios_type;

/**********************************************************/


u32 BE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
}


u32 LE16(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[1]<<8) | b[0];
}


u32 LE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[3]<<24) | (b[2]<<16) | (b[1]<<8) | b[0];
}

void LE32W(void *ptr, u32 val)
{
	u8 *b = (u8*)ptr;
	b[0] = (val>> 0)&0xff;
	b[1] = (val>> 8)&0xff;
	b[2] = (val>>16)&0xff;
	b[3] = (val>>24)&0xff;
}

/**********************************************************/

int smpc_cmd(int cmd)
{
	printk("SMPC_CMD(%02x) ...\n", cmd);
	// wait busy state
	while(SF&0x01);

	SF = 0x01;
	COMREG = cmd;
	while(SF&0x01);

	printk("    done. SS=%02x\n", SR);
	return SR;
}

/**********************************************************/


void stm32_puts(char *str)
{
	u32 stm32_base = TMPBUFF_ADDR;
	int len;

	len = strlen(str);
	LE32W((void*)(stm32_base+0), len);

	memcpy((void*)stm32_base+0x10, str, len+1);

	SS_CMD = 0x0001;
	while(SS_CMD);
}


/**********************************************************/

#if 0
const int HZ = 1000000;

void reset_timer(void)
{
	SS_TIMER = 0;
}

u32 get_timer(void)
{
	return SS_TIMER;
}

#else

const int HZ = 208496; /* for NTSC: 320 lines mode */
static int timer_base;

void reset_timer(void)
{
	timer_base = 0;
	TCR = 0x02;
	FRCH = 0;
	FRCL = 0;
}

u32 get_timer(void)
{
	int s1, s2, th, tl;
	while(1){
		s1 = FTCSR&0x02;
		th = FRCH;
		tl = FRCL;
		s2 = FTCSR&0x02;
		if(s1==s2)
			break;
	}

	th = (th<<8) | tl;
	if(s2&0x02){
		timer_base += 65536;
		FTCSR = 0;
	}

	return timer_base+th;
}

#endif


void usleep(u32 us)
{
	u32 now, end;

	if(us==0)
		us = 1;
	end = (us*HZ)/1000000;

	reset_timer();
	while(1){
		now = get_timer();
		if(now>=end)
			break;
	}
}

void msleep(u32 ms)
{
	u32 now, end;

	if(ms==0)
		ms = 1;
	end = (ms*HZ)/1000;

	reset_timer();
	while(1){
		now = get_timer();
		if(now>=end)
			break;
	}
}

/**********************************************************/

// clk = 26874880
// bps = ((clk/32)/n)/(BRR+1)

void sci_init(void)
{
	SCR = 0x00;     /* set TE,RE to 0 */
	SMR = 0x00;     /* set SMR to 8n1 async mode */
//	BRR = 0x15;     /* set BRR to 9600 bps */
//	BRR = 0x01;     /* set BRR to 104980 bps */
	BRR = 0x00;     /* set BRR to 209960 bps */
	SCR = 0xcd;     /* set TIE RIE MPIE TEIE CKE1 CKE0 */
	SCR = 0x31;     /* set TE RE to 1 */

	printk_putc = sci_putc;
}

void sci_putc(int ch)
{
	while((SSR&0x80)==0);
	TDR = ch;
	SSR &= 0x7f;
}

int sci_getc(int timeout)
{
	int ch;

	timeout *= 1500;
	while(timeout){
		if(SSR&0x40){
			ch = RDR;
			SSR &= 0xbf;
			return ch;
		}
		timeout -= 1;
	}

	return -1;
}


/**********************************************************/


void cpu_dmemcpy(void *dst, void *src, int size, int ch)
{
	int chcr = 0x5a11;

	if(((u32)src&0xfff8703f)==0x25800000){
		// CDBLOCK的DATA_PORT
		chcr &= 0xcfff;
	}

	while((CHCR0&0x03)==0x01);
	CHCR0 = 0;
	DMAOR = 1;

	SAR0 = (u32)src;
	DAR0 = (u32)dst;
	TCR0 = (size+3)>>2;
	CHCR0 = chcr;
}

#if 1
void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	volatile SCUDMA_REG *sdma = (SCUDMA_REG *)(SCU_DMA_BASE+ch*0x20);
	int mask  = 0x0030;
	int maskh = 0x0001;

	if(ch>0){ mask <<= 4; maskh <<= 1; }
	if(ch>1){ mask <<= 4; maskh   = 0; }
	mask |= maskh<<16;
	while(SDSTAT&mask); // 等待传输结束

	sdma->src = (u32)src;
	sdma->dst = (u32)dst;
	sdma->count = size;
	sdma->addv = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	sdma->mode = 0x07;
	sdma->enable = 0x101;
}

#else

void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	while(SDSTAT&0x10030); // 等待传输结束

	SD0R = (u32)src;
	SD0W = (u32)dst;
	SD0C = size;
	SD0AD = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	SD0MD = 0x07;
	SD0EN = 0x101;
}

#endif

/**********************************************************/

int read_file (char *name, int offset, int size, void *buf)
{
	LE32W((void*)(TMPBUFF_ADDR+0x00), offset);
	LE32W((void*)(TMPBUFF_ADDR+0x04), size);
	strcpy((void*)(TMPBUFF_ADDR+0x10), name);

	u32 bus_addr = (u32)buf & 0x0fffffff;
	if(bus_addr>=0x02000000 && bus_addr<0x03000000){
		// buf位于MCU和SS可以直接访问的空间内。
		LE32W((void*)(TMPBUFF_ADDR+0x08), bus_addr);
	}else{
		// buf不可直接访问，必须中转。
		LE32W((void*)(TMPBUFF_ADDR+0x08), TMPBUFF_ADDR+0x0100);
	}


	SS_ARG = 0;
	SS_CMD = SSCMD_FILERD;
	while(SS_CMD);

	int retv = (signed short)SS_ARG;
	if(retv<0)
		return retv;

	size = LE32((void*)(TMPBUFF_ADDR+0x04));
	if(bus_addr<0x02000000 || bus_addr>=0x03000000){
		memcpy(buf, (void*)(TMPBUFF_ADDR+0x0100), size);
	}

	return size;
}


int write_file(char *name, int offset, int size, void *buf)
{
	LE32W((void*)(TMPBUFF_ADDR), offset);
	LE32W((void*)(TMPBUFF_ADDR+0x04), size);
	strcpy((void*)(TMPBUFF_ADDR+0x10), name);

	u32 bus_addr = (u32)buf & 0x0fffffff;
	if(bus_addr>=0x02000000 && bus_addr<0x03000000){
		// buf位于MCU和SS可以直接访问的空间内。
		LE32W((void*)(TMPBUFF_ADDR+0x08), bus_addr);
	}else{
		// buf不可直接访问，必须中转。
		LE32W((void*)(TMPBUFF_ADDR+0x08), TMPBUFF_ADDR+0x0100);
		memcpy((void*)(TMPBUFF_ADDR+0x0100), buf, size);
	}


	SS_ARG = 0;
	SS_CMD = SSCMD_FILEWR;
	while(SS_CMD);

	int retv = (signed short)SS_ARG;
	if(retv<0)
		return retv;

	size = LE32((void*)(TMPBUFF_ADDR+0x04));
	return size;
}


/**********************************************************/
/* from sci_shell.c */

static char hbuf[128];
static int hblen = 0;
#define PROMPT "SS"

int gets(char *buf, int len)
{
	int n, ch, esc;

	if(gets_from_stm32){
		while(1){
			n = *(volatile u8*)TMPBUFF_ADDR;
			if(n)
				break;
		}

		strcpy(buf, (u8*)(TMPBUFF_ADDR+0x10));
		buf[n] = 0;
		return n;
	}

	printk(PROMPT "> ");
	n = 0;
	esc = 0;
	while(1){
		while((ch=sci_getc(1000))<0);
		if(ch==0x0d || ch==0x0a){
			break;
		}

		if(esc==0 && ch==0x1b){
			esc = 1;
			continue;
		}
		if(esc==1){
			if(ch==0x5b){
				esc = 2;
			}else{
				printk("<ESC1 %02x>", ch);
				esc = 0;
			}
			continue;
		}
		if(esc==2){
			esc = 0;
			if(ch==0x41 || ch==0x43){
				// UP:0x41  RIGHT:0x43
				if(hblen){
					int sn = n;
					n = hblen;
					memcpy(buf, hbuf, hblen);
					printk("\r" PROMPT "> %s", hbuf);
					while(hblen<sn){
						printk(" ");
						hblen += 1;
					}
					if(ch==0x43){
						break;
					}
				}
			}else{
				printk("<ESC2 %02x>", ch);
			}
		}else if(ch==0x08){
			if(n){
				n -= 1;
				printk("\b \b");
			}
		}else if(ch>=0x20){
			printk("%c", ch);
			buf[n] = ch;
			n++;
			if(n==len)
				break;
		}else{
			printk("{%02x}", ch);
		}
	}

	if(n){
		memcpy(hbuf, buf, n);
		hbuf[n] = 0;
		hblen = n;
	}

	printk("\r\n");
	buf[n] = 0;
	return n;
}


int arg_base = 16;	/* [MENU_WRAM] not static: sci_shell.c sets it */

int str2hex(char *str, int *hex)
{
	int n, p, ch, base;

	base = arg_base;
	n = 0;
	p = 0;
	hex[0] = 0;
	while(1){
		ch = *str++;
		if(ch==' '){
			if(p){
				n++;
				hex[n] = 0;
				p = 0;
			}
			continue;
		}else if((ch=='0' && *str=='x')){
			str += 1;
			base = 16;
		}else if((ch>='0' && ch<='9')){
			ch -= '0';
		}else if((base==16 && ch>='a' && ch<='f')){
			ch = ch-'a'+10;
		}else if((base==16 && ch>='A' && ch<='F')){
			ch = ch-'A'+10;
		}else{
			if(p)
				n++;
			return n;
		}
		hex[n] = hex[n]*base + ch;
		p++;
	}
}


/**********************************************************/

static int dump_width = 2;
static int dump_addr = 0x00200000;

void dump(int argc, int *args, int width)
{
	int i, c, p, addr, len;
	u32 d;
	u8 sbuf[16];

	addr = (argc>=1)? args[0] : dump_addr;
	len  = (argc>=2)? args[1] : 256;
	if(width==0)
		width = dump_width;

	c = 0;
	p = 0;
	while(1){
		if((p%16)==0 || p>=len){
			if(c){
				printk(" ");
				for(i=0; i<c; i++){
					if(sbuf[i]>0x20 && sbuf[i]<0x80){
						printk("%c", sbuf[i]);
					}else{
						printk(".");
					}
				}
				c = 0;
			}
			if(p>=len){
				printk("\n");
			}else{
				printk("\n%08x:  ", addr);
			}
		}else if((p%8)==0){
			printk("- ");
		}
		if(p>=len)
			break;

		if(width==1){
			d = *(u8*)(addr);
			sbuf[c++] = d;
			printk("%02x ", d);
		}else if(width==2){
			d = *(u16*)(addr);
			sbuf[c++] = d>>8; // big endian
			sbuf[c++] = d;
			printk("%04x ", d);
		}else{
			d = *(u32*)(addr);
			sbuf[c++] = d>>24; // big endian
			sbuf[c++] = d>>16;
			sbuf[c++] = d>>8;
			sbuf[c++] = d;
			printk("%08x ", d);
		}
		p += width;
		addr += width;
	}

	dump_width = width;
	dump_addr = addr;
}


void mem_dump(char *str, void *vaddr, int size)
{
	u8 *addr = (u8*)vaddr;
	int i, j, c;

	if(str){
		printk("%s:\n", str);
	}

	for(i=0; i<size; i+=16){
		printk("%04x: ", i);
		for(j=0; j<16; j++){
			printk("%02x ", addr[j]);
		}
		printk(" ");
		for(j=0; j<16; j++){
			c = addr[j];
			if(c>0x20 && c<0x80){
				printk("%c", c);
			}else{
				printk(".");
			}
		}
		printk("\n");
		addr += 16;
	}
}
