
#include "main.h"
#include "ff.h"
#include "../Saturn/cdc.h"

/******************************************************************************/

static char hbuf[128];
static int hblen = 0;
#define PROMPT "stm32"

int gets(char *buf, int len)
{
	int n, ch, esc;

	printk(PROMPT "> ");
	n = 0;
	esc = 0;
	while(1){
		while((ch=_getc())<0);
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


static int arg_base = 16;

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

static int dump_width = 4;
static int dump_addr = 0;

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
			sbuf[c++] = d;    // Little endian
			sbuf[c++] = d>>8;
			printk("%04x ", d);
		}else{
			d = *(u32*)(addr);
			sbuf[c++] = d;    // Little endian
			sbuf[c++] = d>>8;
			sbuf[c++] = d>>16;
			sbuf[c++] = d>>24;
			printk("%08x ", d);
		}
		p += width;
		addr += width;
	}

	dump_width = width;
	dump_addr = addr;
}


/******************************************************************************/


static u16 mksum(u32 addr)
{
	return (addr>>16) + addr;
}


int mem_test(u32 addr, int size)
{
	u16 rd, sum;
	int i, ecnt=0;
	
	printk("Memory test at %08x ...", addr);
	
	size /= 2;
	for(i=0; i<size; i++){
		*(u16*)(addr) = mksum(addr);
		addr += 2;
	}

	for(i=0; i<size; i++){
		addr -= 2;
		sum = mksum(addr);
		rd  = *(u16*)(addr);
		if(sum!=rd){
			printk("\nMismatch0 at %08x! read=%08x calc=%08x\n", addr, rd, sum);
			ecnt += 1;
		}
		*(u16*)(addr) = ~sum;

	}

	for(i=0; i<size; i++){
		sum = ~mksum(addr);
		rd  = *(u16*)(addr);
		if(sum!=rd){
			printk("\nMismatch2 at %08x! read=%08x calc=%08x\n", addr, rd, sum);
			ecnt += 1;
		}
		addr += 2;
	}

	if(ecnt==0)
		printk("    OK!\n\n");

	return ecnt;
}

/******************************************************************************/


#define CMD_START if(0){}
#define CMD(x) else if(strcmp(cmd, #x)==0)

void simple_shell(void)
{
	char cmd[128];
	int argc, arg[4];

	dump_addr = 0x08000000;

	while(1){
		gets(cmd, 128);
		if(strncmp(cmd, "ss ", 3)==0){
			int len = strlen(cmd+3);
			strcpy((char*)(TMPBUFF_ADDR+0x10), cmd+3);
			*(u8*)TMPBUFF_ADDR = len;
			continue;
		}

		char *sp = strchr(cmd, ' ');
		if(sp){
			*sp = 0;
			sp += 1;
			argc = str2hex(sp, arg);
		}else{
			argc = 0;
		}

		CMD_START
		CMD(base16) { arg_base = 16; }
		CMD(base10) { arg_base = 10; }
		CMD(rb){ printk("%08x: %02x\n", arg[0], *(u8*)arg[0]);  }
		CMD(rw){ printk("%08x: %04x\n", arg[0], *(u16*)arg[0]); }
		CMD(rd){ printk("%08x: %08x\n", arg[0], *(u32*)arg[0]);	}
		CMD(wb){ printk("%08x= %02x\n", arg[0], (u8)arg[1]);  *(u8*)arg[0]  = arg[1]; }
		CMD(ww){ printk("%08x= %04x\n", arg[0], (u16)arg[1]); *(u16*)arg[0] = arg[1]; }
		CMD(wd){ printk("%08x= %08x\n", arg[0], (u32)arg[1]); *(u32*)arg[0] = arg[1]; }
		CMD(db){ dump(argc, arg, 1); }
		CMD(dw){ dump(argc, arg, 2); }
		CMD(dd){ dump(argc, arg, 4); }
		CMD(d) { dump(argc, arg, 0); }
		CMD(go){ void (*go)(void) = (void(*)(void))arg[0]; go(); }
		
		CMD(spt){
			void play_i2s(void);
			play_i2s();
		}
		CMD(fpu){
			fpga_update(0);
		}
		CMD(flu){
			flash_update(0);
		}
		CMD(sdram){
			void sdram_config(int type, int reftime);
			int type=3, reftime=64;
			if(argc>=1){
				type = arg[0];
			}
			if(argc>=2){
				reftime = arg[1];
			}
			sdram_config(type, reftime);
		}

		CMD(mmt){
			u32 addr = 0x61000000;
			int size = 0x00100000;
			if(argc==2){
				addr = arg[0];
				size = arg[1];
			}
			mem_test(addr, size);
		}

		CMD(memset){
			if(argc>=3){
				memset((void*)arg[0], arg[1], arg[2]);
			}else{
				printk("Wrong args!\n");
			}
		}
		CMD(memcpy){
			if(argc>=3){
				memcpy((void*)arg[0], (void*)arg[1], arg[2]);
			}else{
				printk("Wrong args!\n");
			}
		}


		CMD(ls){
			scan_dir("/", 0, NULL);
		}
		CMD(lscue){
			int list_disc(int show);
			list_disc(1);
		}
		CMD(load){
			int load_disc(int index);
			load_disc(arg[0]);
		}
		CMD(cdc){
			void cdc_dump();
			cdc_dump();
		}
		CMD(log){
			extern int log_mask;
			if(argc){
				log_mask = arg[0];
			}
			printk("log_mask: %04x\n", log_mask);
		}
		CMD(fsdly){
			extern int sector_delay_force;
			if(argc){
				sector_delay_force = arg[0];
			}
			printk("sector_delay_force: %d\n", sector_delay_force);
		}
		CMD(sdly){
			extern int sector_delay;
			if(argc){
				sector_delay = arg[0];
			}
			printk("sector_delay: %d\n", sector_delay);
		}
		CMD(fpdly){
			extern int play_delay_force;
			if(argc){
				play_delay_force = arg[0];
			}
			printk("play_delay_force: %d\n", play_delay_force);
		}
		CMD(seram){
			FIL fp;
			int retv = f_open(&fp, "/exram.bin", FA_CREATE_ALWAYS|FA_WRITE);
			if(retv==FR_OK){
				u32 wsize = 0x00400000;
				retv = f_write(&fp, (void*)0x61400000, 0x00400000, &wsize);
				f_close(&fp);
				printk("save to exram.bin: %d %d\n", retv, wsize);
			}else{
				printk("create exram.bin failed! %d\n", retv);
			}
		}
		CMD(pt){
			void show_pt(int id);
			show_pt(arg[0]);
		}

		CMD(q){
			break;
		}
	}
}

