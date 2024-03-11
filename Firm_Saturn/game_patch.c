
#include "main.h"
#include "smpc.h"

int CHEAT_ADDRES;

/**********************************************************/

#define MASK_EXMEM 0x3000

void ssctrl_set(int mask, int val)
{
	int ssctrl = SS_CTRL;

	ssctrl &= ~mask;
	ssctrl |= val;

	SS_CTRL = ssctrl;
}


/**********************************************************/


void RUN_CHEAT(void)
{
	__asm__("mov    r3,  r4");
	__asm__("shlr16 r3");
	__asm__("ldc     r3, sr");
	__asm__("exts.w  r4, r4");
	__asm__("or      r4, r2");
	__asm__("mov.l   r2, @r1");	
	__asm__("mov.l   r2, @r5");
	
	if(CHEAT_ADDRES){
	void (*go)(void) = (void(*)(void))CHEAT_ADDRES;
	go();	
	
	}
	__asm volatile("jmp @%0"::"r"(0x600091a)); 
	__asm__( "lds.l   @r15+, pr");
}


// 需要插入中断的补丁程序入口
void CHEAT_patch(void)
{
	__asm__ ( "stc      sr, r0" );
	__asm__ ( "mov	    r0,r3" );
	__asm__ ( "or       #0xf0, r0" );
	__asm__ ( "ldc      r0, sr"  );
	*(u32*)(0x0600090c) = 0xd401442b;
	*(u16*)(0x06000910) = 0x9;
	*(u32*)(0x06000914) = (u32)RUN_CHEAT;
	__asm__( "ldc      r3, sr"  );
	
}


/**********************************************************/
// KAITEI_DAISENSOU (Japan) 海底大战争日版 第一关后半部分会有音乐丢失情况
void KAITEI_DAISENSOU_handle1(void)
{
	cdc_read_sector(2462+150, 0x54f00, (void*)0x22400000);
}


void KAITEI_DAISENSOU_handle2(void)
{
	memcpy((u8*)0x2AA000, (u8*)0x22400000, 0x54f00);
}


void KAITEI_DAISENSOU_patch(void)
{	
	*(u32*)(0x6007324) = (u32)KAITEI_DAISENSOU_handle1;
	*(u32*)(0x60044E0) = (u32)KAITEI_DAISENSOU_handle2;
}


/**********************************************************/
// Dungeons & Dragons Collection (Japan) (Disc 2) 龙与地下城2
void DND2_patch(void)
{
	*(u32*)(0x06015228) = 0x34403440;
}


/**********************************************************/
// NOEL3 (Japan)
void NOEL3_patch(void)
{
	*(u32*)(0x0603525C) = 0x34403440;
}


/**********************************************************/
// FRIENDS (Japan)
void FRIENDS_patch(void)
{
	*(u32*)(0x06042928) = 0x34403440;
	*(u32*)(0x060429B4) = 0x34403440;
}


/**********************************************************/
// Astra Superstars (Japan)
void ASTRA_SUPERSTARS_patch(void)
{
	*(u32*)(0x060289E4) = 0x34403440;
}


/**********************************************************/
// Magical Night Dreams - Cotton 1 (Japan)棉花小魔女1
void cotton1_patch(void)
{
	*(u32*)(0x06018DA8) = 0x34403440;
}


/**********************************************************/
// Magical Night Dreams - Cotton 2 (Japan)棉花小魔女2
void cotton2_patch(void)
{
	*(u32*)(0x060041BC) = 0x34403440;
}


/**********************************************************/
// Pocket Fighter (Japan) 口袋战士 
void POCKET_FIGHTER_patch(void)
{
	*(u32*)(0x06014080) = 0x34403440;
}


/**********************************************************/
// Street Fighter Zero 3 (Japan) 街霸ZERO3
void SF_ZERO3_patch(void)
{
	*(u32*)(0x0602B8AC) = 0x34403440;
}


/**********************************************************/
// Metal_Slug  合金弹头
void Metal_Slug_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x06079E18) = 0x34403440;

}


/*********************************************************/
 //Metal_Slug A 合金弹头1.005版
void Metal_Slug_A_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x06079FBC) = 0x34403440;
}


/*********************************************************/
// ULTRAMAN
// 2MB ROM cart
void jump_06b0(void)
{
	void (*go)(void) = (void*)(0x04c8);
	go();
	*(u32*)(0x25fe00b8) = 0x13;
	go = (void*)0x1800;
	go();

	*(u32*)(0x25fe00b0) = 0x34403440;
	*(u32*)(0x25fe00b8) = 0x13;

	SF = 1;
	COMREG = 0x19;
	while(SF&1);
}


void jump_copy_code(int type) 
{	
	for(int i=0; i<0x200000; i+=4)
		(REG32(0x22000000+i)) = (REG32(0x22400000+i));
	void (*go)(void) = (void*)type;
	go();		
}


void card_init(void)
{	
	*(u32*)(0x06000358) = 0x7d600;
	*(u32*)(0x0600026c) = 0x186c;
	*(u32*)(0x06000320) = 0x6000c80;
	memcpy((u8*)0x200000, jump_copy_code, 0x60);
	memcpy((u8*)0x6000c80, jump_06b0, 0x60);	
}


void ULTRAMAN_patch(void)
{
	need_bup = 0;

	if(*(u32*)(0x6002fb4)==0x06004000){
		read_file("/SAROO/ISO/ULTRAMAN.BIN", 0, 0x200000, (void*)0x22400000);
		card_init();
		void (*go)(int);
		go = (void*)0x200000;
		go(0x6002222);	
	}
}


/*********************************************************/
// KOF95
// 2MB ROM cart
void kof95_handle(void)
{
	if(*(u32*)(0x6002234)== 0x14401FF0) *(u32*)(0x6002234) = 0x34403440;
	if(*(u32*)(0x600230c)== 0x14401FF0) *(u32*)(0x600230c) = 0x34403440;
	if(*(u32*)(0x6002300)== 0x1FF01FF0) *(u32*)(0x6002300) = 0x34403440;

	read_file("/SAROO/ISO/KOF95.BIN", 0, 0x200000, (void*)0x22400000);
	card_init();
	
	void (*go)(int);
	go = (void*)0x200000;
	go(0x6002048);	
}


void kof95_patch(void)
{
	need_bup = 0;

	if(*(u32*)(0x6002e28)==0x0607CD80)
		*(u32*)(0x607CDC4) = (u32)kof95_handle;
}


/**********************************************************/
// KOF96
// 1MB RAM cart only
void kof96_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x06058248) = 0x34403440;
}


/**********************************************************/
// KOF97
void kof97_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x06066FC0) = 0x34403440;
}


/**********************************************************/
// Samurai Spirits - Amakusa Kourin 侍魂4
// 1MB RAM cart only
void smrsp4_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x6067E60) = 0x34403440;
}


/**********************************************************/
// Samurai Spirits - Zankuro Musouken   侍魂3
// 0x0600d1d0: 23301ff0
// 0x0605ad90: 23301ff0
void smrsp3_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)0x0600d1d0 = 0x34403440;
	*(u32*)0x0605ad90 = 0x34403440;
}


/**********************************************************/
// Real Bout Garou Densetsu (Japan) (2M) RB饿狼传说
// 0x0602F6FC: 23301ff0
// 0x0607C36C: 23301ff0
void REAL_BOUT_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)0x0602F6FC = 0x34403440;
	*(u32*)0x0607C36C = 0x34403440;
}


/**********************************************************/
// Real Bout Garou Densetsu Special (Japan) (Rev A)   RB饿狼传说SP V2
// 0x060913C4: 23301ff0
void REAL_BOUT_SP_v2_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)0x060913C4 = 0x34403440;
}


/**********************************************************/
// Real Bout Garou Densetsu Special (Japan)  v1 RB饿狼传说SP V1
// 0x060913C4: 23301ff0
// 0x06091230: 23301ff0
void REAL_BOUT_SP_v1_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)0x06091230 = 0x34403440;
}


/**********************************************************/
// SRMP7
// 0x06011204:  mov.l #0x23301ff0, r3
// need change to 0x34403440
void srmp7_patch(void)
{
	*(u32*)(0x0601121C) = 0x34403440;
}


/**********************************************************/
// Gouketsuji Ichizoku 3 - Groove on Fight (Japan)豪血寺一族3
void GROOVE_ON_FIGHT_handle(void)
{
	*(u32*)0x06099ABC=0x34403440;
	*(u32*)0x060083B0=0x34403440;
	*(u32*)0x00280008=0x06004000;
	__asm volatile("jmp @%0"::"r"(0x06004000)); 
	__asm volatile( "nop" :: );	
}

void GROOVE_ON_FIGHT_patch(void)
{
	*(u32*)(0x200AA4) = 0x34403440;
	*(u32*)(0x202EF0) = 0x34403440;
	*(u32*)(0x200460) = (u32)GROOVE_ON_FIGHT_handle;
}


/**********************************************************/
// Vampire Savior (Japan) 恶魔战士3 救世主
void VAMPIRE_SAVIOR_handle(void)
{
	*(u32*)0x06014A64=0x34403440;
	__asm volatile("jmp @%0"::"r"(0x600D000)); 
	__asm volatile ( "nop" :: );
}

void VAMPIRE_SAVIOR_patch(void)
{
	 *(u8*)(0x60A02D4) = 0x43;
	 *(u8*)(0x60A442C) = 0x43;
	 *(u32*)(0x060A03B8) = (u32)VAMPIRE_SAVIOR_handle;
	 *(u32*)(0x060A447C) = (u32)VAMPIRE_SAVIOR_handle;
}


/**********************************************************/
// XMarvel Super Heroes (Japan)
void MARVEL_SUPER_handle(void)
{	
	void (*go)(void) = (void(*)(void))0x60D0FF4;
	go();	
	*(u32*)0x0600B0FC = 0x34403440;
	*(u32*)0x0600B2D8 = 0x34403440;		
}

void MARVEL_SUPER_patch(void)
{
	*(u32*)(0x060D13F8) = (u32)MARVEL_SUPER_handle;
}


/**********************************************************/
// X-Men vs. Street Fighter (Japan) (3M)
void xmvsf_handle2(void)
{
	*(u32*)(0x06014158) = 0x34403440;
	__asm volatile("jmp @%0"::"r"(0x6014000)); 
	__asm volatile ( "nop" :: );		
}

void xmvsf_handle1(void)
{
	
	*(u16*)(0x60228E0) = 0xe000;
	*(u16*)(0x60228f4) = 0xe000;
	*(u8*)0x060801FE = 0x5c;
	*(u8*)0x06080200 = 0xFF;
	__asm volatile("jmp @%0"::"r"(0x6022000)); 
	__asm volatile ("nop" :: );	
}

void xmvsf_patch(void)
{
	*(u8*)0x060084DD= 0x10;
	*(u16*)(0x60084EC) = 0xB11C;
	*(u32*)(0x6008520) = (u32)xmvsf_handle1;
	*(u32*)(0x6008530) = (u32)xmvsf_handle2;
}


/**********************************************************/
// Marvel Super Heroes vs. Street Fighter (Japan)
void mshvssf_handle1(void)
{
	if(*(u16*)(0x600286e)== 0x2012)
		*(u16*)(0x600286e) = 0x0009;
	if(*(u16*)(0x600205A)== 0x2102)
		*(u16*)(0x600205A) = 0x0009;
	__asm volatile("jmp @%0"::"r"(0x6002000)); 
	__asm volatile ("nop" :: );	
}

void mshvssf_patch(void)
{
	*(u8*)0x060F000B = 0x46;
	*(u8*)0x060F001D = 0x41;
	*(u32*)(0x60F0124) = (u32)mshvssf_handle1;
}


/**********************************************************/
// WAKUWAKU7                火热火热7
void WAKU7_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x0601C19C) = 0x34403440;
	*(u16*)(0x0601C16C) = 0x9;
	*(u8*) (0x0601244d) = 0xa;

	*(u32*)(0x06011E64+0x00) = 0xD207D108;
	*(u32*)(0x06011E64+0x04) = 0x60436322;
	*(u32*)(0x06011E64+0x08) = 0x44087370;
	*(u32*)(0x06011E64+0x0c) = 0x40084000;
	*(u32*)(0x06011E64+0x10) = 0x4408523C;
	*(u32*)(0x06011E64+0x14) = 0x340C6312;
	*(u32*)(0x06011E64+0x18) = 0x324C5521;
	*(u32*)(0x06011E64+0x1c) = 0x432B6422;
	*(u32*)(0x06011E64+0x20) = 0x0603439C;
	*(u32*)(0x06011E64+0x24) = 0x02000F04;
}


/**********************************************************/
// FIGHTERS_HISTORY   斗士的历史
void FIGHTERS_HISTORY_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)0x060500C8 = 0x34403440;
	*(u32*)0x06057000 = 0x34403440;
	*(u32*)0x0605707C = 0x34403440;
	*(u32*)0x060570E0 = 0x34403440;
}


/**********************************************************/
// Final Fight Revenge (Japan)  快打旋风复仇

void FINAL_FIGHT_REVENGE_patch(void)
{
	*(u32*)(0x06011A6C) = 0x34403440;
}


/**********************************************************/
// Amagi_Shien_Japan_patch

void Amagi_Shien_patch(void)
{
	*(u16*)(0x06012258) = 0x9;
}


/**********************************************************/
// Sega Saturn de Hakken!! Tamagotchi Park (Japan)

void Tamagotchi_Park_patch(void)
{
	*(u16*)(0x06030EDE) = 0xE400;
}


/**********************************************************/
// CYBER_BOTS

void  CYBER_BOTS_handle1(void)
{
	*(u16*)(0x0601695A) = 0x9;
	__asm volatile("jmp @%0"::"r"(0x600D000));
	__asm volatile ("nop" :: );
}


void CYBER_BOTS_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u32*)(0x060DD058) = (u32)CYBER_BOTS_handle1;
}


/**********************************************************/
// FIGHTERS MEGAMIX_JAP_patch

//jap 1m 2m
void FIGHTERS_MEGAMIX_JAP_patch(void)
{
	*(u16*)(0x060203F4) = 0x9;
}

//USA PAL
void FIGHTERS_MEGAMIX_USA_patch(void)
{
	if(*(u16*)(0x06020408)==0x400b)
		*(u16*)(0x06020408) = 0x9;
	else if(*(u16*)(0x0602040C)==0x400b)
		*(u16*)(0x0602040C) = 0x9;
}


/**********************************************************/


int skip_patch = 0;
int need_bup = 1;

typedef struct _game_db {
	char *id;
	char *name;
	void (*patch_func)(void);
}GAME_DB;


GAME_DB game_dbs[] = {
	{"T-1229G",            "VAMPIRE_SAVIOR",     VAMPIRE_SAVIOR_patch},
	{"T-14411G",           "GROOVE_ON_FIGHT",    GROOVE_ON_FIGHT_patch},
	{"T-22205G",           "NOEL3",              NOEL3_patch},
	{"T-20109G",           "FRIENDS",            FRIENDS_patch},
	{"T-1245G",            "DND2",               DND2_patch},
	{"T-1521G",            "ASTRA",              ASTRA_SUPERSTARS_patch},
	{"T-9904G",            "cotton2",            cotton2_patch},
	{"T-9906G",            "cotton1",            cotton1_patch},

	{"T-1230G",            "POCKET_FIGHTER",     POCKET_FIGHTER_patch},
	{"T-1246G",            "SF_ZERO3",           SF_ZERO3_patch},

	{"T-3111G   V1.002",   "Metal_Slug",         Metal_Slug_patch},
	{"T-3111G   V1.005",   "Metal_Slug_A",       Metal_Slug_A_patch},
	{"T-3101G",            "KOF95",              kof95_patch},
	{"T-3108G",            "KOF96",              kof96_patch},
	{"T-3121G",            "KOF97",              kof97_patch},
	{"T-3104G",            "SamuraiSp3",         smrsp3_patch},
	{"T-3116G",            "SamuraiSp4",         smrsp4_patch},
	{"T-3105G",            "REAL_BOUT",          REAL_BOUT_patch},
	{"T-3119G   V1.001",   "REAL_BOUT_SP_v1",    REAL_BOUT_SP_v1_patch},
	{"T-3119G   V1.002",   "REAL_BOUT_SP_v2",    REAL_BOUT_SP_v2_patch},
	{"T-10001G  V1.000",   "KAITEI_DAISENSOU",   KAITEI_DAISENSOU_patch},
	{"T-15006G  V1.004",   "KAITEI_DAISENSOU",   KAITEI_DAISENSOU_patch},

	{"T-13308G",           "ULTRAMAN",           ULTRAMAN_patch},
	{"T-16509G",           "SRMP7",              srmp7_patch},
	{"T-16510G",           "SRMP7SP",            srmp7_patch},

	{"T-1515G",            "WAKU7",              WAKU7_patch},
	{"GS-9107",            "FIGHTERS",           FIGHTERS_HISTORY_patch},
	{"T-1248G",            "FINAL_FIGHT_REVENGE",FINAL_FIGHT_REVENGE_patch},
	{"T-1226G",            "XMENVSSF",           xmvsf_patch},
	{"T-1215G",            "MARVEL_SUPER",       MARVEL_SUPER_patch},
	{"T-1238G   V1.000",   "MSH_VS_SF",          mshvssf_patch},

	{"T-1513G",            "asj",                Amagi_Shien_patch},
	{"T-13325G",           "jidang",             Tamagotchi_Park_patch},
	{"T-1217G",            "CYBER_BOTS",         CYBER_BOTS_patch},
	{"GS-9126",            "F_M_J",              FIGHTERS_MEGAMIX_JAP_patch},//包含3个日版地址相同
	{"MK-81073",           "F_M_U",              FIGHTERS_MEGAMIX_USA_patch},//包含美版 欧版

	{NULL,},
};


void patch_game(char *id)
{
	GAME_DB *gdb = &game_dbs[0];

	CHEAT_ADDRES = 0;
	need_bup = 1;

	if(skip_patch)
		return;

	while(gdb->id)
	{
		int slen = strlen(gdb->id);
		if(strncmp(gdb->id, id, slen)==0)
		{
			printk("Patch game %s ...\n", gdb->name);
			gdb->patch_func();
			break;
		}

		gdb += 1;
	}

	u32 *cfgword = (u32*)(SYSINFO_ADDR+0x0100);
	while(1){
		u32 key, val0, val1;

		key = LE32(cfgword);
		cfgword += 1;
		printk("Cfg key: %08x\n", key);
		if(key==0)
			break;

		val0 = key&0x0fffffff;
		key >>= 28;

		if(key==3){
			if(val0==1)
				ssctrl_set(MASK_EXMEM, CS0_RAM1M);
			else if(val0==4)
				ssctrl_set(MASK_EXMEM, CS0_RAM4M);
		}else if(key<5){
			val1 = LE32(cfgword);
			cfgword += 1;
			printk("Cfg val: %08x\n", val1);
			if(key==1)
				*(u8 *)(val0) = val1;
			else if(key==2)
				*(u16*)(val0) = val1;
			else
				*(u32*)(val0) = val1;
		}else{
		}
	}

}


