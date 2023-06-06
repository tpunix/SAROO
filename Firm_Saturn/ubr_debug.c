
#include "main.h"
#include "smpc.h"

/**********************************************************/



void vbi_isr_entry(void);
void ubr_isr_entry(void);
void debug_shell(void *arg);


static int auto_break;
int game_break_pc;
void (*game_break_handle)(REGS *reg);


/**********************************************************/

static inline u32 get_sr(void)
{
	u32 sr;

	__asm volatile ( "stc\tsr,%0": "=r" (sr) );
	return	sr;
}

static inline void set_sr(u32 sr)
{
	__asm volatile ( "ldc\t%0,sr":: "r" (sr) );
}


void show_regs(REGS *reg)
{
	printk("SH2 Registers:\n");
	printk("R0 =%08x R1 =%08x R2 =%08x R3 =%08x\n", reg->r0, reg->r1, reg->r2, reg->r3);
	printk("R4 =%08x R5 =%08x R6 =%08x R7 =%08x\n", reg->r4, reg->r5, reg->r6, reg->r7);
	printk("R8 =%08x R9 =%08x R10=%08x R11=%08x\n", reg->r8, reg->r9, reg->r10,reg->r11);
	printk("R12=%08x R13=%08x R14=%08x R15=%08x\n", reg->r12,reg->r13,reg->r14,reg->r15);
	printk("PC =%08X PR =%08x SR =%08x GBR=%08x\n", reg->pc, reg->pr, reg->sr, reg->gbr);
}


/**********************************************************/


void set_break_pc(u32 addr, u32 mask)
{
	if(addr){
		BARA = addr;
		BAMRA = mask;
		BBRA = 0x0054;
		BRCR = 0x1400;
	}else{
		BBRA = 0;
	}
}


void set_break_rw(u32 addr, u32 mask, int rw)
{
	if(addr){
		BARB = addr;
		BAMRB = mask;
		BBRB = (0x0060|rw);
		BRCR = 0x1400;
	}else{
		BBRB = 0;
	}
}


void ubr_handle(REGS *reg)
{
	BRCR = 0x1400;

	if(auto_break){
		// 修改设置SR时的参数, 打开ubr中断
		if(reg->pc==0x0000044c){
			reg->r0 = 0xe0;
			set_break_pc(0x000002b0, 0);
		}

		// memset
		if(reg->pc==0x000002b2){
			// 避免LRAM被BIOS清0
			if(reg->r3==0x00200000){
				reg->r6 = 0x00020000;
				set_break_pc(0x000002c2, 0);
			}
		}

		// memcpy
		if(reg->pc==0x000002c4){
			// 避免向量表被BIOS重设
			if(reg->r3==0x06000000){
				reg->r6 = 0x0c;
				set_break_pc(0x000002a4, 0);
				//set_break_pc(0x060011bc, 0); // --> bios:000011bc
				//0x060011bc处会重设cdplayer的地址。
				//设置到这里，有可能断不下来，游戏也不启动了。
			}
		}

		// last
		if(reg->pc==0x000002a6){
		//if(reg->pc==0x060011be){
		//	*(u32*)(0x0600026c) = 0x02000f00;
			if(game_break_pc){
				set_break_pc(game_break_pc, 0);
				printk("Set game break at %08x\n", game_break_pc);
			}
			auto_break = 0;
		}
		return;
	}


	sci_init();
	printk("\nubr_handle! BRCR=%04x\n", BRCR);
	show_regs(reg);

	if(reg->pc==game_break_pc+2 && game_break_handle){
		game_break_handle(reg);
	}else{
		debug_shell(reg);
	}
}


void vbi_handle(REGS *reg)
{
	sci_init();
	printk("\nvbi_handle!\n");
	show_regs(reg);
	return;
}


void break_in_game(int break_pc, void *handle)
{
	auto_break = 1;
	game_break_pc = break_pc;
	game_break_handle = handle;

	set_break_pc(0x0000044a, 0);
	install_ubr_isr();
}


void break_in_game_next(int break_pc, void *handle)
{
	game_break_pc = break_pc;
	game_break_handle = handle;
	set_break_pc(break_pc, 0);
}


void install_ubr_isr(void)
{
	*(u32*)(0x06000030) = (u32)ubr_isr_entry;

	u32 sr = get_sr();
	sr &= 0xffffffef;
	set_sr(sr);
}


/**********************************************************/


#define CMD_START if(0){}
#define CMD(x) else if(strcmp(cmd, #x)==0)

void debug_shell(void *ctx)
{
	char cmd[128];
	u32 argc, arg[4];
	REGS *reg = (REGS*)ctx;

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

		CMD(r0){ reg->r0 = arg[0];}
		CMD(r6){ reg->r6 = arg[0];}

		CMD(b ){
			u32 addr = 0, mask = 0;
			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_pc(addr, mask);
			break;
		}
		CMD(br) {
			u32 addr = 0, mask = 0;

			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_rw(addr, mask, BRK_READ);
		}
		CMD(bw) {
			u32 addr = 0, mask = 0;

			if(argc>0) addr = arg[0];
			if(argc>1) mask = arg[1];
			set_break_rw(addr, mask, BRK_WRITE);
		}

		CMD(c ){ break; }
	}

}


/**********************************************************/



