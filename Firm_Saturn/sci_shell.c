
#include "main.h"
#include "smpc.h"

/**********************************************************/

int gets_from_stm32 = 0;
extern int to_stm32;

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


/**********************************************************/


#define CMD_START if(0){}
#define CMD(x) else if(strcmp(cmd, #x)==0)


void sci_shell(void)
{
	char cmd[128];
	u32 argc, arg[4];


	while(1){
		gets(cmd, 128);
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
		CMD(rd){ printk("%08x: %08x\n", arg[0], *(u32*)arg[0]); }
		CMD(wb){ printk("%08x= %02x\n", arg[0], (u8)arg[1]);  *(u8*)arg[0]  = arg[1]; }
		CMD(ww){ printk("%08x= %04x\n", arg[0], (u16)arg[1]); *(u16*)arg[0] = arg[1]; }
		CMD(wd){ printk("%08x= %08x\n", arg[0], (u32)arg[1]); *(u32*)arg[0] = arg[1]; }
		CMD(db){ dump(argc, arg, 1); }
		CMD(dw){ dump(argc, arg, 2); }
		CMD(dd){ dump(argc, arg, 4); }
		CMD(d) { dump(argc, arg, 0); }
		CMD(go){ void (*go)(void) = (void(*)(void))arg[0]; go(); }

		CMD(t) {
			void cd_cmd(void);
			cd_cmd();
		}
		CMD(f) {
			void cdc_file_test(void);
			cdc_file_test();
		}
		CMD(m) { bios_run_cd_player(); } /* 运行系统CD播放器 */
		CMD(n) { bios_cd_cmd(0); }       /* 启动游戏 */
		CMD(cds) {
			void cdc_dump(int count);
			int count = 1;
			if(argc>0)
				count = arg[0];
			cdc_dump(count);
		}

		CMD(menu) { menu_init(); }

		CMD(nmi) {
			install_isr(ISR_NMI);
			void reloc_vbr(void);
			reloc_vbr();
		}
		CMD(stm32) {
			to_stm32 = 1;
			gets_from_stm32 = 1;
			*(u32*)(TMPBUFF_ADDR) = 0;
		}
		CMD(saturn) {
			to_stm32 = 0;
			gets_from_stm32 = 0;
		}

		CMD(wtt) {
			// 测试延时计数. 大概9000000是1秒.
			// Test delay counter. Approximately 9000000 is 1 second.
			int cnt = arg[0];
			reset_timer();
			while(cnt){
				cnt -= 1;
			}
			int tm = get_timer();
			printk("counter: %d  timer: %d\n", arg[0], tm);
		}
		CMD(mmt) {
			int mem_test(int size);
			mem_test(0x100000);
		}

		CMD(b) {
			u32 addr = 0, mask = 0;

			install_ubr_isr();

			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_pc(addr, mask);
		}
		CMD(br) {
			u32 addr = 0, mask = 0;

			install_ubr_isr();

			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_rw(addr, mask, BRK_READ);
		}
		CMD(bw) {
			u32 addr = 0, mask = 0;

			install_ubr_isr();

			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_rw(addr, mask, BRK_WRITE);
		}
		CMD(bb) {
			u32 addr = 0;
			if(argc>0) addr = arg[0];
			if(addr){
				game_break_pc = addr;
			}
		}

		CMD(c) {
			/* 打开系统光驱 */
			cdblock_on(1);
		}
		CMD(auth){
			int retv = cdblock_check();
			printk("cdblock_check: %d\n", retv);
		}
		CMD(jhl){
			jhl_auth_hack(arg[0]);
		}
		CMD(s) {
			/* 关闭系统光驱, 打开sarooo */
			cdblock_off();
		}

		CMD(listb){
			SS_CMD = SSCMD_LISTBINS;
			while(SS_CMD);
		}
		CMD(loadb){
			int run_binary(int index, int run);
			run_binary(arg[0], 0);
		}
		CMD(fwt){
			int retv = write_file("/saturn_save.bin", 0, 0x10000, (void*)0x00180000);
			printk("write 0x00180000 to /saturn_save.bin: %d\n", retv);
		}

		CMD(z) {
			/* 下载到ram并运行. 如果指定了地址, 则只下载. */
			int xm_len, xm_addr;
			xm_addr = (argc>0)? arg[0] : 0x06004000;
			xm_len = tiny_xmodem_recv((u8*)xm_addr);
			printk("                            \n");
			printk("                            \n");
			printk("Download to %08x, size %08x\n", xm_addr, xm_len);

			if(argc==0){
				u32 *xbuf = (u32*)xm_addr;
				int offset = 0;
				if(xbuf[0]==0x53454741 && xbuf[1]==0x20534547)
					offset = 0x0f00;
				printk("Jump to %08x ...\n", xm_addr);
				void (*go)(void) = (void(*)(void))(xm_addr+offset);
				go();
			}
		}

		CMD(q) {
			break;
		}
	}
}


/**********************************************************/



