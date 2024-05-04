
#include "main.h"
#include "smpc.h"

/**********************************************************/



void ubr_handle(REGS *reg);
void debug_shell(void *arg);


int game_break_pc;
void (*game_break_handle)(REGS *reg);


/**********************************************************/

inline u32 get_vbr(void)
{
	u32 vbr;
	__asm volatile ( "stc\tvbr,%0": "=r" (vbr) );
	return	vbr;
}

inline void set_vbr(u32 vbr)
{
	__asm volatile ( "ldc\t%0,vbr":: "r" (vbr) );
}


inline u32 get_sr(void)
{
	u32 sr;
	__asm volatile ( "stc\tsr,%0": "=r" (sr) );
	return	sr;
}

inline void set_sr(u32 sr)
{
	__asm volatile ( "ldc\t%0,sr":: "r" (sr) );
}

inline void set_imask(u32 imask)
{
	u32 sr = get_sr();
	sr &= 0xffffff0f;
	sr |= (imask<<4);
	set_sr(sr);
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

void m_ginst_isr_entry(void);
void m_sinst_isr_entry(void);
void m_caddr_isr_entry(void);
void m_gaddr_isr_entry(void);
void m_nmi_isr_entry(void);
void m_ubr_isr_entry(void);


u32 slave_vbr = 0x06000400;


void exc_handle(REGS *reg, int type)
{
	int slave = type&0x80;
	type &= 0x7f;

	sci_init();
	if(type==ISR_UBR){
		printk("\n--------\n%s UBR!\n", (slave)?"Slave":"Master");
		ubr_handle(reg);
	}else if(type==ISR_NMI){
		printk("\n--------\n%s NMI!\n", (slave)?"Slave":"Master");
		show_regs(reg);
		debug_shell(reg);
	}
}


void install_isr(int type)
{
	u32 mvbr, svbr;

	mvbr = get_vbr() + type*4;
	svbr = slave_vbr + type*4;

	if(type==ISR_NMI){
		printk("install nmi: %08x %08x\n", mvbr, svbr);
		*(u32*)mvbr = (u32)m_nmi_isr_entry;
		*(u32*)svbr = (u32)m_nmi_isr_entry+8;
	}else if(type==ISR_UBR){
		*(u32*)mvbr = (u32)m_ubr_isr_entry;
		*(u32*)svbr = (u32)m_ubr_isr_entry+8;
		set_imask(0x0e);
	}

}


static u32 new_vtable[256];

void reloc_vbr(void)
{
	u32 old_vbr = get_vbr();
	memcpy((void*)new_vtable, (void*)old_vbr, 1024);
	set_vbr((u32)new_vtable);
	printk("New VBR at %08x\n", new_vtable);
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

	printk("\nubr_handle! BRCR=%04x\n", BRCR);
	show_regs(reg);

	if(reg->pc==game_break_pc+2 && game_break_handle){
		game_break_handle(reg);
	}else{
		debug_shell(reg);
	}
}


void break_in_game(int break_pc, void *handle)
{
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
	*(u32*)(0x06000030) = (u32)m_ubr_isr_entry;

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



