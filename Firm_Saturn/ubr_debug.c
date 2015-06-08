
#include "main.h"
#include "smpc.h"

/**********************************************************/

typedef struct {
	u32     pr;
	u32     gbr;
	u32     vbr;
	u32     mach;
	u32     macl;
	u32     r15;
	u32     r14;
	u32     r13;
	u32     r12;
	u32     r11;
	u32     r10;
	u32     r9;
	u32     r8;
	u32     r7;
	u32     r6;
	u32     r5;
	u32     r4;
	u32     r3;
	u32     r2;
	u32     r1;
	u32     r0;
	u32     pc;
	u32     sr;
}REGS;

void vbi_isr_entry(void);
void ubr_isr_entry(void);
void debug_shell(void);

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

void set_break_pc(u32 addr)
{
	BARA = addr;
	BAMRA = 0;
	BBRA = 0x0054;
	BRCR = 0x1400;
}

void ubr_handle(REGS *reg)
{
	sci_init();
	printk("\nubr_handle! BRCR=%04x\n", BRCR);
	show_regs(reg);
	BRCR = 0x1400;

	// 修改设置SR时的参数, 打开ubr中断
	if(reg->pc==0x0000044c){
		reg->r0 = 0xe0;
		set_break_pc(0x000002c2);
	}

	// memcpy
	if(reg->pc==0x000002c4){
		// 避免向量表被BIOS重设
		if(reg->r3==0x06000000){
			reg->r6 = 0x0c;
			set_break_pc(0x06004000);
		}
	}

	if(reg->pc==0x06006180){
		printk("Game Start!\n");
		debug_shell();
	}
}

void vbi_handle(REGS *reg)
{
	sci_init();
	printk("\nvbi_handle!\n");
	show_regs(reg);
	return;
}


void install_ubr_isr(void)
{
	u32 sr;

	//*(u32*)(0x06000100) = (u32)vbi_isr_entry;
	*(u32*)(0x06000030) = (u32)ubr_isr_entry;

	set_break_pc(0x0000044a);

	sr = get_sr();
	sr &= 0xffffffef;
	set_sr(sr);
}



/**********************************************************/

int gets(char *buf, int len);
int parse_arg(char *str, char *argv[]);
void dump(int argc, char *argv[]);
void mrd(int argc, char *argv[]);
void mwr(int argc, char *argv[]);
void command_go(int go_addr);
void mem_dump(char *str, void *addr, int size);

void debug_shell(void)
{
	int ch;
	char str[128];
	int argc;
	char *argv[16];

	while(1){
		ch = gets(str, 128);
		argc = parse_arg(str, argv);
		if(argv[0][0]=='d')
			dump(argc, argv);
		if(argv[0][0]=='r')
			mrd(argc, argv);
		if(argv[0][0]=='w')
			mwr(argc, argv);
		if(argv[0][0]=='g'){
			int addr = strtoul(argv[1], 0, 0, 0);
			command_go(addr);
		}

		if(argv[0][0]=='b'){
			int addr = strtoul(argv[1], 0, 0, 0);
			set_break_pc(addr);
		}

		if(argv[0][0]=='c'){
			break;
		}
	}

}


/**********************************************************/



