
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
	//*(u32*)(0x06015228) = 0x34403440;
	*(u16*)(0x060151A6) = 0x09;
	*(u16*)(0x060151AE) = 0x09;
}


/**********************************************************/
// NOEL3 (Japan)

void NOEL3_patch(void)
{
	//*(u32*)(0x0603525C) = 0x34403440;
	*(u16*)(0x06035104) = 0x09;
}


/**********************************************************/
// FRIENDS (Japan)

void FRIENDS_patch(void)
{
	//*(u32*)(0x06042928) = 0x34403440;
	//*(u32*)(0x060429B4) = 0x34403440;
	*(u16*)(0x060428E8) = 0x09;
	*(u16*)(0x0604298A) = 0x09;
}


/**********************************************************/
// Astra Superstars (Japan)

void ASTRA_SUPERSTARS_patch(void)
{
	//*(u32*)(0x060289E4) = 0x34403440;
	*(u16*)(0x06028974) = 0x09;
}


/**********************************************************/
// Magical Night Dreams - Cotton 1 (Japan)棉花小魔女1

void cotton1_patch(void)
{
	//*(u32*)(0x06018DA8) = 0x34403440;
	*(u16*)(0x06018D80) = 0x09;
}


/**********************************************************/
// Magical Night Dreams - Cotton 2 (Japan)棉花小魔女2

void cotton2_patch(void)
{
	//*(u32*)(0x060041BC) = 0x34403440;
	*(u16*)(0x06004194) = 0x09;
}


/**********************************************************/
// Pocket Fighter (Japan) 口袋战士 
void POCKET_FIGHTER_patch(void)
{
	//*(u32*)(0x06014080) = 0x34403440;
	*(u32*)(0x06013F48) = 0xd44ae001;
	*(u32*)(0x06013F4c) = 0x2400e05c;
	*(u32*)(0x06013F50) = 0x000b8041;
	//*(u8*)(0x06037C2A) = 0x11;//修正开头音频被错误的播放？？
	*(u16*)(0x0602DD68) = 0x09;
}


/**********************************************************/
// Street Fighter Zero 3 (Japan) 街霸ZERO3

void SF_ZERO3_patch(void)
{
	//*(u32*)(0x0602B8AC) = 0x34403440;
	*(u16*)(0x0602B7FA) = 0x09;
}


/**********************************************************/
// Metal_Slug  合金弹头

void Metal_Slug_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	//*(u32*)(0x06079E18) = 0x34403440;
	*(u16*)(0x06079DEC) = 0x09;
	//*(u16*)(0x06079DF2) = 0x09;
}


/*********************************************************/
// Metal_Slug A 合金弹头1.005版

void Metal_Slug_A_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	//*(u32*)(0x06079FBC) = 0x34403440;
	*(u16*)(0x06079F90) = 0x09;
	//*(u16*)(0x06079F96) = 0x09;
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
	*(u16*)(0x60020c2) = 0x0009;
	*(u16*)(0x600217E) = 0x0009;
	*(u16*)(0x6002180) = 0x0009;

	read_file("/SAROO/ISO/KOF95.BIN", 0, 0x200000, (void*)0x22400000);
	card_init();

	void (*go)(int);
	go = (void*)0x200000;
	go(0x6002048);	
}


void kof95_patch(void)
{
	need_bup = 0;

	if(*(u32*)(0x6002e28)==0x0607CD80){
		if(*(u32*)(0x607CDC4) == 0x6002048)
			*(u32*)(0x607CDC4) = (u32)kof95_handle;
		else if(*(u32*)(0x607CDC8) == 0x6002048)
			*(u32*)(0x607CDC8) = (u32)kof95_handle;
	}
}

/**********************************************************/
// KOF96
// 1MB RAM cart only

void kof96_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	//*(u32*)(0x06058248) = 0x34403440;
	*(u16*)(0x0605821C) = 0x09;
}


/**********************************************************/
// KOF97

void kof97_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u16*)(0x06066F80) = 0x0009;//34403440
}


/**********************************************************/
// Samurai Spirits - Amakusa Kourin 侍魂4
// 1MB RAM cart only

void smrsp4_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	//*(u32*)(0x6067E60) = 0x34403440;
	*(u16*)(0x06067E34) = 0x0009;
}


/**********************************************************/
// Samurai Spirits - Zankuro Musouken   侍魂3
// 0x0600d1d0: 23301ff0
// 0x0605ad90: 23301ff0

void smrsp3_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u16*)(0x0600D198) = 0x0009;
	*(u16*)(0x0605AD36) = 0x0009;
	//*(u32*)0x0600d1d0 = 0x34403440;
	//*(u32*)0x0605ad90 = 0x34403440;
}


/**********************************************************/
// Real Bout Garou Densetsu (Japan) (2M) RB饿狼传说
// 0x0602F6FC: 23301ff0
// 0x0607C36C: 23301ff0

void REAL_BOUT_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u16*)(0x0602F5EC) = 0x0009;
	*(u16*)(0x0607C340) = 0x0009;
	//*(u32*)0x0602F6FC = 0x34403440;
	//*(u32*)0x0607C36C = 0x34403440;
}


/**********************************************************/
// Real Bout Garou Densetsu Special (Japan) (Rev A)   RB饿狼传说SP V2
// 0x060913C4: 23301ff0

void REAL_BOUT_SP_v2_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	//*(u32*)0x060913C4 = 0x34403440;
	*(u16*)(0x06091398) = 0x0009;
}


/**********************************************************/
// Real Bout Garou Densetsu Special (Japan)  v1 RB饿狼传说SP V1
// 0x060913C4: 23301ff0
// 0x06091230: 23301ff0

void REAL_BOUT_SP_v1_patch(void)
{
	ssctrl_set(MASK_EXMEM, CS0_RAM1M);
	*(u16*)(0x06091204) = 0x0009;
	//*(u32*)0x06091230 = 0x34403440;
}


/**********************************************************/
// SRMP7
// 0x06011204:  mov.l #0x23301ff0, r3
// need change to 0x34403440

void srmp7_patch(void)
{
	//*(u32*)(0x0601121C) = 0x34403440;
	*(u16*)(0x06011208) = 0x0009;
}


/**********************************************************/
// Gouketsuji Ichizoku 3 - Groove on Fight (Japan)豪血寺一族3

void GROOVE_ON_FIGHT_handle(void)
{
	//*(u32*)0x06099ABC=0x34403440;
	//*(u32*)0x060083B0=0x34403440;
	*(u16*)(0x6008338) = 0x0009;
	*(u16*)(0x6099A84) = 0x0009;
	*(u32*)0x00280008=0x06004000;
	__asm volatile("jmp @%0"::"r"(0x06004000)); 
	__asm volatile( "nop" :: );	
}

void GROOVE_ON_FIGHT_patch(void)
{
	//*(u32*)(0x200AA4) = 0x34403440;
	//*(u32*)(0x202EF0) = 0x34403440;
	*(u16*)(0x200A44) = 0x0009;
	*(u16*)(0x202EB8) = 0x0009;
	*(u32*)(0x200460) = (u32)GROOVE_ON_FIGHT_handle;
}


/**********************************************************/
// Vampire Savior (Japan) 恶魔战士3 救世主

void VAMPIRE_SAVIOR_handle(void)
{
	//*(u32*)0x06014A64=0x34403440;
	*(u16*)(0x6014A52) = 0x0009;
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
	*(u16*)(0x0600B0AC) = 0x0009;
	*(u16*)(0x0600B1D4) = 0x0009;
	//*(u32*)0x0600B0FC = 0x34403440;
	//*(u32*)0x0600B2D8 = 0x34403440;
}

void MARVEL_SUPER_patch(void)
{
	*(u32*)(0x060D13F8) = (u32)MARVEL_SUPER_handle;
}


/**********************************************************/
// X-Men vs. Street Fighter (Japan) (3M)

void xmvsf_handle2(void)
{
	//*(u32*)(0x06014158) = 0x34403440;
	*(u16*)(0x0601409C) = 0x0009;
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
	*(u16*)(0x0601C164) = 0x9;
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
	*(u16*)(0x0605002E) = 0x9;
	*(u16*)(0x06056FEC) = 0x9;
	*(u16*)(0x06057028) = 0x9;
	*(u16*)(0x060570BA) = 0x9;
	//*(u32*)0x060500C8 = 0x34403440;
	//*(u32*)0x06057000 = 0x34403440;
	//*(u32*)0x0605707C = 0x34403440;
	//*(u32*)0x060570E0 = 0x34403440;
}


/**********************************************************/
// Final Fight Revenge (Japan)  快打旋风复仇

void FINAL_FIGHT_REVENGE_patch(void)
{
	//*(u32*)(0x06011A6C) = 0x34403440;
	*(u16*)(0x0601199C) = 0x9;
}


/**********************************************************/
// Amagi_Shien_Japan_patch
// 游戏会检测PAUSE状态从而导致死循环 

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
// Fighting Vipers (Japan) (Rev C) patch

// JAP 1m 2m
void Fighting_Vipers_JAP_patch(void)
{
	*(u16*)(0x06022A60) = 0x9;
}

// USA PAL Korea
void Fighting_Vipers_USA_patch(void)
{
	if(*(u16*)(0x060229AC)==0x400b)
		*(u16*)(0x060229AC) = 0x9;        // 欧版
	else if(*(u16*)(0x06022990)==0x400b)
		*(u16*)(0x06022990) = 0x9;        // 美版
	else if(*(u16*)(0x06022980)==0x400b)
		*(u16*)(0x06022980) = 0x9;        // 韩版
}

/**********************************************************/
// Gotha

void Gotha_patch(void)
{
	*(u16*)(0x0604E3CC) = 0x9;
	*(u8*) (0x0604E3D2) = 0x2;
}


/**********************************************************/
// Terra_Cresta_3D (神鹰一号3D)  关闭光驱

void Terra_Cresta_3D_patch(void)
{
	*(u8*) (0x0603AB64) = 0x9;
}


/**********************************************************/
// 虎胆龙威
// 游戏程序员手滑，将0x25E7FFFF写成0x25E7FFF了，从VRAM到CS0了。

void Die_Hard_Trilogy_patch(void)
{
	*(u8*)(0x025E7FFF) = 0xff;
}


/**********************************************************/
// <Habitat II> and <Pad Nifty>  modem 补丁

void  HABITAT2_handle(void)
{
	if(*(u8*)(0x0602A15d)==0x11)
		*(u16*)(0x0602A15E) = 0x9;// Pad Nifty (Japan)
	else if(*(u8*)(0x0602B52D)==0x11)
		*(u16*)(0x0602B52E) = 0x9;// Habitat II (Japan)
	
	__asm volatile("jmp @%0"::"r"(0x6020000)); 
	__asm volatile ("nop" :: );		
}


void HABITAT2_patch(void)
{
	*(u32*)(0x060105B4) = (u32)HABITAT2_handle;
}


/**********************************************************/
//Dragon's Dream (Japan)  modem 补丁 

void Dragons_Dream_patch(void)
{
	*(u8*)(0x06040691) = 1;
}


/**********************************************************/
// Hebereke's Popoitto (Japan) (Europe) ???
// 游戏会检测PAUSE状态从而导致死循环

void Hebereke_Popoitto_patch(void)
{
	if(*(u8*)(0x0601D379)==0x01){
		*(u8*)(0x0601D379) = 0x56;        // (Japan)
		*(u8*)(0x0601D2F5) = 0x56;
	}else  if(*(u8*)(0x0601D399)==0x01){
		*(u8*)(0x0601D399) = 0x78;        // (Europe)
	}
}


/**********************************************************/
//Hissatsu Pachinko Collection (Japan) (Rev A) ???
// 游戏会检测PAUSE状态从而导致死循环

void Hissatsu_Pachinko_Collection_patch(void)
{
	if(*(u8*)(0x0601085D)==0x01)
		*(u8*)(0x0601085D) = 0x56;        // (Rev A)
	else if(*(u8*)(0x0601088D)==0x01)
		*(u8*)(0x0601088D) = 0x78;        //
}


/**********************************************************/
//Heart of Darkness (Japan) (Proto) 补丁 

void Heart_of_Darkness_patch(void)
{
	if(*(u32*)(0x060108c0)==0x04000000){
		*(u16*)(0x060108C0) = 0x0240;
		*(u16*)(0x060106C4) = 0x0009;
	}
}


/**********************************************************/
// VANDAL_HEARTS_CN

void VANDAL_HEARTS_CN_patch(void)
{
	int addres=0;

	if(REG16(0x06002F70)==0x4C2B)         // B2
		addres=0x48;
	else if(REG16(0x06002F5e)==0x4C2B)    // B1
		addres=0x51;

	if(addres){
		REG8(0x06002F5e) =0xaf;
		REG8(0x06002F5f) =addres;
		REG32(0x06002E04)=0xE00961C3;
		REG32(0x06002E08)=0x71547170;
		REG32(0x06002E0C)=0x4C2B2101;
	}
}


/**********************************************************/
//WHIZZ (Japan) (Europe) 补丁 
// 游戏自身问题，没初始化VDP1

void WHIZZ_patch(void)
{
	*(u8*)(0x06003193) = 0x8;
}


/**********************************************************/
// Kidou Senshi Z Gundam - Zenpen Zeta no Kodou (Japan)
// 游戏自身问题，没初始化VDP1

void Gundam_Zenpen_Zeta_no_Kodou_patch(void)
{
	*(u8*)(0x060030A1) = 0x8;
}


/**********************************************************/
// RMENU2_patch  菜单补丁

void RMENU2_patch(void)
{
	*(u32*)(0x06014774) = (u32)bios_cd_cmd;
}


/**********************************************************/
// Fenrir_patch  菜单补丁

void Fenrir_patch(void)
{
	*(u16*)(0x0600DCCA) = 0x9;
	*(u8*) (0x0600DCCC) = 0x41;
	*(u32*)(0x0600DD18) = (u32)bios_cd_cmd;
}


/**********************************************************/


void MANX_TT_SUPER_BIKE_patch(void)
{
	if(*(u8*)(0x60219B1)==0x01){
		//日版和欧版相同
		*(u8*)(0x60219B3)= 0x03;
		*(u8*)(0x60219E7)= 0x03;
		*(u8*)(0x602037D)= 0x43;
		*(u8*)(0x60203FD)= 0x43;
		*(u8*)(0x60204D9)= 0x43;
		*(u8*)(0x6020619)= 0x13;
	}else if(*(u8*)(0x6021A11)==0x01){
		//美版
		*(u8*)(0x6021A13)= 0x03;
		*(u8*)(0x6021A47)= 0x03;
		*(u8*)(0x60203DD)= 0x43;
		*(u8*)(0x602045D)= 0x43;
		*(u8*)(0x6020539)= 0x43;
		*(u8*)(0x6020679)= 0x13;
	}
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
	{"MK-81088",           "KOF95",              kof95_patch},
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
	{"GS-9107",            "FIGHTERS_HISTORY",   FIGHTERS_HISTORY_patch},
	{"T-1248G",            "FINAL_FIGHT_REVENGE",FINAL_FIGHT_REVENGE_patch},
	{"T-1226G",            "XMEN_VS_SF",         xmvsf_patch},
	{"T-1215G",            "MARVEL_SUPER",       MARVEL_SUPER_patch},
	{"T-1238G   V1.000",   "MSH_VS_SF",          mshvssf_patch},

	{"T-1513G",            "Amagi_Shien",        Amagi_Shien_patch},
	{"T-13325G",           "Tamagotchi_Park",    Tamagotchi_Park_patch},
	{"T-1217G",            "CYBER_BOTS",         CYBER_BOTS_patch},
	{"GS-9126",            "FIGHTERS_MEGAMIX",   FIGHTERS_MEGAMIX_JAP_patch},
	{"MK-81073",           "FIGHTERS_MEGAMIX",   FIGHTERS_MEGAMIX_USA_patch},

	{"GS-9101",            "Fighting_Vipers",    Fighting_Vipers_JAP_patch},
	{"MK-81041",           "Fighting_Vipers",    Fighting_Vipers_USA_patch},
	{"GS-9009   V1.000",   "Gotha",              Gotha_patch},
	{"T-7102G",            "Terra_Cresta_3D",    Terra_Cresta_3D_patch},

	{"GS-9123",            "Die_Hard_Trilogy",   Die_Hard_Trilogy_patch},
	{"T-16103H",           "Die_Hard_Trilogy",   Die_Hard_Trilogy_patch},
	{"GS-7101",            "HABITAT2",           HABITAT2_patch},
	{"GS-7105",            "HABITAT2",           HABITAT2_patch},
	{"GS-7114",            "Dragons_Dream",      Dragons_Dream_patch},

	{"T-1502H",            "Hebereke_Popoitto",  Hebereke_Popoitto_patch},
	{"T-1503G",            "Hissatsu_Pachinko",  Hissatsu_Pachinko_Collection_patch},
	{"T-1504G",            "Hebereke_Popoitto",  Hebereke_Popoitto_patch},

	{"999999999 V1.000",   "Heart_of_Darkness",  Heart_of_Darkness_patch},
//	{"T-2001G",            "Houma_Hunter_Lime_Perfect_Collection",
//	                                             Houma_Hunter_Lime_Perfect_Collection_patch},
	{"T-1219G",            "VANDAL_HEARTS_CN",   VANDAL_HEARTS_CN_patch},
	{"T-9515H",            "WHIZZ",              WHIZZ_patch},
	{"T-36102G",           "WHIZZ",              WHIZZ_patch},
	{"T-13315G",           "Gundam_Zenpen_Zeta", Gundam_Zenpen_Zeta_no_Kodou_patch},

	{"T-000001  V0.2.0",   "RMENU2",             RMENU2_patch},
	{"CD0000001 V1.000",   "Fenrir",             Fenrir_patch},

	{"GS-9102   V1.001",   "ManX_TT_Super_Bike", MANX_TT_SUPER_BIKE_patch},
	{"MK-81210  V1.001",   "ManX_TT_Super_Bike", MANX_TT_SUPER_BIKE_patch},
	{"MK-81210  V1.000",   "ManX_TT_Super_Bike", MANX_TT_SUPER_BIKE_patch},

	{NULL,},
};


// Marvel Super Heroes (Europe)	T-7032H-50
// Pia Carrot e Youkoso!! 2 (Japan)	T-20114G
//
//

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


