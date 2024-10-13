
#include "main.h"
#include "smpc.h"


/**********************************************************/

static int my_bios_loadcd_boot(int r4, int r5);

static int (*cdp_boot_game)(void);
static int sk0, sk1;
static int bios_ver;


void (*sys_bup_init)(u8*, u8*, void*);
int use_sys_load = 0;

static void (*orig_func)(void);

static void cdp_hook(void)
{
	sk0 = *(u16*)0x06020232;
	sk1 = *(u16*)0x06020236;
	orig_func();
}


static void hook_getkey(void)
{
	if(bios_ver==0){
		if(*(u32*)(0x0604cd18) == 0x0604406c){
			orig_func = (void*)0x0604406c;
			*(u32*)(0x0604cd18) = (u32)cdp_hook;
		}
	}else if(bios_ver==1){
		if(*(u32*)(0x0604d8e0) == 0x06044204){
			orig_func = (void*)0x06044204;
			*(u32*)(0x0604d8e0) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604d8b0) == 0x06044204){
			// V-Saturn 1.01
			orig_func = (void*)0x06044204;
			*(u32*)(0x0604d8b0) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ecec) == 0x06044ae0){
			// 1.00a(U)
			orig_func = (void*)0x06044ae0;
			*(u32*)(0x0604ecec) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ed0c) == 0x06044ae0){
			// 1.01a(U)
			orig_func = (void*)0x06044ae0;
			*(u32*)(0x0604ed0c) = (u32)cdp_hook;
		}
	}else{
		printk("Unkonw BIOS ver!\n");
	}
}


static int cdp_boot(void)
{
	int pad = (sk0)? sk0 : sk1;
	printk("cdp_boot! PAD=%04x\n", pad);

	hook_getkey();

	if(pad & PAD_START){
		void (*go)(void) = (void*)(0x02000f00);
		go();
	}else if(pad & PAD_C){
		// SAROO启动，但用系统存档
		bios_cd_cmd(0x80);
	}else if(pad & PAD_A){
		// SAROO启动
		bios_cd_cmd(0);
	}

	return 0;
}


static int cdp_read_ip(void)
{
	if(debug_flag&1)
		sci_init();
	
	int ip_size = my_bios_loadcd_boot(0, 0x1876);
	if((ip_size==-8)||(ip_size==-4)){
		ip_size=0x0;
	}

	printk("\ncdp_read_ip: %d\n", ip_size);

	hook_getkey();

	return ip_size;
}


static void (*orig_0344)(int, int);

static void my_0344(int and, int or)
{
	hook_getkey();
	orig_0344(and, or);
}


// 调用04c8的代码必须在内部RAM中运行。
static void _call_04c8(void)
{
	void (*go)(void) = (void*)(0x04c8);
	go();
	*(u32*)(0x25fe00b0) = 0x38803880;
}

void my_cdplayer(void)
{
	void (*go)(int);

	if(debug_flag&1) sci_init();
	printk("\nLoad my multiPlayer ...\n");


	*(u8*)(0xfffffe92)  =  0x10;
	*(u8*)(0xfffffe92)  =  0x1;

	*(u32*)(0xffffff40) = 0x00;
	*(u32*)(0xffffff44) = 0x00;
	*(u16*)(0xffffff48) = 0x00;
	*(u32*)(0xffffff60) = 0x00;
	*(u32*)(0xffffff64) = 0x00;
	*(u16*)(0xffffff68) = 0x00;	
	*(u32*)(0xffffff70) = 0x00;
	*(u32*)(0xffffff74) = 0x00;
	*(u16*)(0xffffff78) = 0x00;	

	*(u8*)(0xfffffe71)  =  0x0;
	*(u8*)(0xfffffe72)  =  0x0;

	*(u32*)(0xffffff80) = 0x00;	
	*(u32*)(0xffffff84) = 0x00;	
	*(u32*)(0xffffff88) = 0x01;	
	*(u32*)(0xffffff8c) = 0x00;	
	*(u32*)(0xffffff90) = 0x00;	
	*(u32*)(0xffffff94) = 0x00;	
	*(u32*)(0xffffff98) = 0x01;	
	*(u32*)(0xffffff9c) = 0x00;	

	*(u32*)(0xffffff08) = 0x00;
	*(u32*)(0xffffffb0) = 0x00;

	go = (void*)*(u32*)(0x060002dc);
	go(3);


	*(u32*)(0x06000348) = 0xffffffff;

	memcpy((u8*)0x00280000, _call_04c8, 64);
	go = (void*)0x00280000;
	go(0);

	if(debug_flag&1) sci_init();
	*(u32*)(0xffffffb0) = 0;
	*(u8 *)(0x2010001f) = 0x1a;

	memset((u8*)0x0600a000, 0, 0xf6000);
	memset((u8*)0x06000c00, 0, 0x09400);
	memset((u8*)0x06000b00, 0, 0x00100);
	memcpy((u8*)0x06000000, (u8*)0x20000600, 0x0210);
	memcpy((u8*)0x06000220, (u8*)0x20000820, 0x08e0);
	memcpy((u8*)0x06001100, (u8*)0x20001100, 0x0700);
	memcpy((u8*)0x060002c0, (u8*)0x20001100, 0x0020);

	*(u32*)(0x06000358) = (u32)bup_init;

	if(use_sys_load==0){
		if(*(u32*)(0x06001340)==0x18a8){
			// 1.00
			bios_ver = 0;
			cdp_boot_game = (void*)0x18a8;
			*(u32*)(0x06001340) = (u32)cdp_boot;
			*(u32*)(0x06001344) = (u32)cdp_read_ip;
		}else if(*(u32*)(0x0600134c)==0x18a8){
			// 1.01 1.02
			bios_ver = 1;
			cdp_boot_game = (void*)0x18a8;
			*(u32*)(0x0600134c) = (u32)cdp_boot;
			*(u32*)(0x06001350) = (u32)cdp_read_ip;
		}else if(*(u32*)(0x060013d8)==0x19b8){
			// 1.03
			bios_ver = 2;
			cdp_boot_game = (void*)0x19b8;
			*(u32*)(0x060013d8) = (u32)cdp_boot;
			*(u32*)(0x060013dc) = (u32)cdp_read_ip;
		}

		orig_0344 = (void*)*(u32*)(0x06000344);
		*(u32*)(0x06000344) = (u32)my_0344;
	}

	*(u32*)(0x06000234) = 0x02ac;
	*(u32*)(0x06000238) = 0x02bc;
	*(u32*)(0x0600023c) = 0x0350;
	*(u32*)(0x06000328) = 0x04c8;
	*(u32*)(0x0600024c) = 0x4843444d;

	go = (void*)0x06000680;
	go(0);
}


int my_bios_loadcd_init(void)
{
	printk("\nmy_bios_loadcd_init!\n");

	*(u32*)(0x06000278) = 0;
	*(u32*)(0x0600027c) = 0;

	cdc_abort_file();
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);
	cdc_set_size(0);

	// This is needed for Yabause: Clear the diskChange flag.
	int status;
	cdc_get_hwinfo(&status);

	return 0;
}


int my_bios_loadcd_read(void)
{
	int status, tm;
	u8 *sbuf = (u8*)0x06002000;
	char ipstr[64];

	printk("\nmy_bios_loadcd_read!\n");

	cdc_reset_selector(0, 0);
	cdc_cddev_connect(0);
	cdc_play_fad(0, 150, 16);

	HIRQ = 0;
	tm = 10000000;
	while(tm){
		if(HIRQ&HIRQ_PEND)
			break;
		tm -= 1;
	}
	if(tm==0){
		printk("  PLAY timeout!\n");
		return -1;
	}

	cdc_get_data(0, 0, 16);
	cdc_trans_data(sbuf, 2048*16);
	cdc_end_trans(&status);

	*(u16*)(0x060003a0) = 1;

	memcpy(ipstr, (u8*)0x06002020, 16);
	ipstr[16] = 0;
	printk("\nLoad game: %s\n", ipstr);
	memcpy(ipstr, (u8*)0x06002060, 32);
	ipstr[32] = 0;
	printk("  %s\n\n", ipstr);

	return 0;
}


static u16 code_06b8[10] = {
	0xd102, // mov.l #0x25fe00b0, r1
	0xd203, // mov.l #0x38803880, r2
	0x2122, // mov.l r2, @r1
	0xd103, // mov.l next_call, r1
	0x412b, // jmp   @r1
	0x4f26, // lds.l @r15+, pr
	0x25fe,0x00b0,
	0x3880,0x3880,
	// 060006cc: .long next_call
};


static void my_06b0(void)
{
	if(debug_flag&1) sci_init();
	printk("\nbios_set_clock_speed(%d)!\n", *(u32*)(0x06000324));

	void (*go)(void) = (void*)0x1800;
	go();

	*(u32*)(0x25fe00b0) = 0x38803880;

	SF = 1;
	COMREG = 0x19;
	while(SF&1);
}


static void read_1st(void)
{
	printk("Read main ...\n");
	int retv;
	retv = *(u32*)(0x06000284);
	void (*go)(void) = (void(*)(void))retv;
	go();

	patch_game((char*)0x06002020);
	if(need_bup){
		*(u32*)(0x06000358) = (u32)bup_init;
		*(u32*)(0x0600026c) = (u32)my_cdplayer;
	}
	force_no_bup = 0;

	// 0x06000320: 0x060006b0  bios_set_clock_speed
	memcpy((u8*)0x060006b8, code_06b8, sizeof(code_06b8));
	*(u32*)(0x060006cc) = (u32)my_06b0;


	if(game_break_pc){
		set_break_pc(game_break_pc, 0);
		install_ubr_isr();
//		install_isr(ISR_NMI);
//		void reloc_vbr(void);
//		reloc_vbr();
		to_stm32 = 1;
		gets_from_stm32 = 1;
		*(u32*)(0x22820000) = 0;
	}
}


static int my_bios_loadcd_boot(int r4, int r5)
{
	__asm volatile ( "sts.l  pr, @-r15" :: );
	__asm volatile ( "jmp  @%0":: "r" (r5) );
	__asm volatile ( "mov  r4, r0" :: );
	__asm volatile ( "nop" :: );
	return 0;
}


int bios_cd_cmd(int type)
{
	int retv, ip_size;

	use_sys_bup = (type&0x80)>>7;
	use_sys_load = 0;
	type &= 0x7f;

	my_bios_loadcd_init();

#if 1
	my_bios_loadcd_boot(0, 0x1904);

	while(1){
		ip_size = bios_loadcd_read();
		if(ip_size==0x8000)
			break;
	}

	char ipstr[64];

	memcpy(ipstr, (u8*)0x06002020, 16);
	ipstr[16] = 0;
	printk("\nLoad game: %s\n", ipstr);
	memcpy(ipstr, (u8*)0x06002060, 32);
	ipstr[32] = 0;
	printk("  %s\n\n", ipstr);


#else
	retv = my_bios_loadcd_read();
	if(retv)
		return retv;

	// emulate bios_loadcd_boot
	*(u32*)(0x06000290) = 3;
	ip_size = bios_loadcd_read();//1912读取ip文件
#endif

	if(type>0 && use_sys_bup==0){
		// 光盘游戏。需要通知MCU加载SAVE。
		memcpy((void*)(TMPBUFF_ADDR+0x10), (u8*)0x06002020, 16);
		*(u8*)(TMPBUFF_ADDR+0x20) = 0;

		SS_ARG = 0;
		SS_CMD = SSCMD_LSAVE;
		while(SS_CMD);
	}

	*(u32*)(0x06002270) = (u32)read_1st;	
	*(u32*)(0x02000f04) = (u32)cdc_read_sector;
	*(u16*)(0x0600220c) = 9;

	retv = my_bios_loadcd_boot(ip_size, 0x18be);//跳到18be
	if((retv==-8)||(retv==-4)){
		*(u32*)(0x06000254) = 0x6002100;
		retv = my_bios_loadcd_boot(0, 0x18c6);
	}
	printk("bios_loadcd_boot  retv=%d\n", retv);

	return retv;
}


/**********************************************************/

