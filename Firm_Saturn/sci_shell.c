
#include "main.h"
#include "smpc.h"

/**********************************************************/

extern int gets_from_stm32;	/* [MENU_WRAM] defined in res_util.c */
extern int to_stm32;

/* [MENU_WRAM] gets, str2hex, dump, mem_dump: moved to res_util.c (the in-game debugger in
 * ubr_debug.c uses them, so the resident image needs its own copy) */
extern int arg_base;


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



