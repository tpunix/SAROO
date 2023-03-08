
#include "main.h"
#include "smpc.h"





/**********************************************************/
// KAITEI_DAISENSOU (Japan) 海底大战争日版

void KAITEI_DAISENSOU_patch(void)
{

		
	//*(u16*)(0x6074D60) = 0x9;
	*(u8*)(0x0600A2e5) = 0x30;
	*(u16*)(0x0600A316) = 0x9;
	
/*	*(u32*)(0x600A374) = 0x2FE6E600;
	*(u32*)(0x600A378) = 0x4F22E500;
	*(u32*)(0x600A37c) = 0xDE0AD30B;
	*(u32*)(0x600A380) = 0x430B64E2;
	*(u32*)(0x600A384) = 0xE501D30A;
	*(u32*)(0x600A388) = 0x430B64E2;
	*(u32*)(0x600A38c) = 0x950AD30B;
	*(u32*)(0x600A390) = 0x430B64E2;
	*(u32*)(0x600A394) = 0xD308D707;
	*(u32*)(0x600A398) = 0xD6099503;
	*(u32*)(0x600A39c) = 0x64E24F26;
	*(u32*)(0x600A3a0) = 0x432B6EF6;
	*(u32*)(0x600A3a4) = 0x00AA0009;
	*(u32*)(0x600A3a8) = 0x06098028;
	*(u32*)(0x600A3ac) = 0x060724B4;	
	*(u32*)(0x600A3b0) = 0x06072C2C;
	*(u32*)(0x600A3b4) = 0x00054F00;
	*(u32*)(0x600A3b8) = 0x060727A4;
	*(u32*)(0x600A3bc) = 0x06072D3E;
	*(u32*)(0x600A3c0) = 0x002AA000;	
	*/
	
}


/**********************************************************/
// Dungeons & Dragons Collection (Japan) (Disc 2) 
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
// Magical Night Dreams - Cotton 1 (Japan)
void cotton1_patch(void)
{
	*(u32*)(0x06018DA8) = 0x34403440;
}

/**********************************************************/
// Magical Night Dreams - Cotton 2 (Japan)
void cotton2_patch(void)
{
	*(u32*)(0x060041BC) = 0x34403440;
}


/**********************************************************/
// Final Fight Revenge (Japan)
// 0x06011A6C:  mov.l #0x23301FF0, r1
// need change to 0x34403440

void FINAL_FIGHT_REVENGE_patch(void)
{
	*(u32*)(0x06011A6C) = 0x34403440;
}





/**********************************************************/
// Pocket Fighter (Japan)
void POCKET_FIGHTER_patch(void)
{
	*(u32*)(0x06014080) = 0x34403440;
}



/**********************************************************/
// SF_ZERO3
void SF_ZERO3_patch(void)
{
	*(u32*)(0x0602B8AC) = 0x34403440;
}

















/**********************************************************/
// Metal_Slug
// 0x06079E18:  mov.l #0x23301ff0, r2
// need change to 0x34403440

void Metal_Slug_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x06079E18) = 0x34403440;

}

/**********************************************************/
// KOF96
// 0x0605821A:  mov.l #0x23301ff0, r2
// need change to 0x34403440

void kof96_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x06058248) = 0x34403440;

}


/**********************************************************/
// KOF97
// 0x06066f7e:  mov.l #0x23301ff0, r2
// need change to 0x34403440

void kof97_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x06066FC0) = 0x34403440;
}



/**********************************************************/
// Samurai Spirits - Amakusa Kourin
// 1MB RAM cart only
// 0x06067e32:  mov.l #0x23301ff0, r2
// need change to 0x34403440


void smrsp4_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x6067E60) = 0x34403440;
}


/**********************************************************/
// Samurai Spirits - Zankuro Musouken
// 0x0600d1d0: 23301ff0
// 0x0605ad90: 23301ff0

void smrsp3_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)0x0600d1d0 = 0x34403440;
	*(u32*)0x0605ad90 = 0x34403440;
}




/**********************************************************/
// Real Bout Garou Densetsu (Japan) (2M)
// 0x0602F6FC: 23301ff0
// 0x0607C36C: 23301ff0

void REAL_BOUT_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)0x0602F6FC = 0x34403440;
	*(u32*)0x0607C36C = 0x34403440;
}

/**********************************************************/
// Real Bout Garou Densetsu Special (Japan) (Rev A)  v2
// 0x060913C4: 23301ff0

void REAL_BOUT_SP_v2_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)0x060913C4 = 0x34403440;

}


/**********************************************************/
// Real Bout Garou Densetsu Special (Japan)  v1
// 0x06091230: 23301ff0

void REAL_BOUT_SP_v1_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)0x06091230 = 0x34403440;

}



/**********************************************************/
// SRMP7
// 0x06011204:  mov.l #0x23301ff0, r3
// need change to 0x34403440

void srmp7_patch(void)
{
//	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x0601121C) = 0x34403440;
}









/**********************************************************/
// WAKUWAKU7
void WAKU7_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	*(u32*)(0x0601C19C) = 0x34403440;
	*(u16*)(0x0601C16C) = 0x9;
	
}


/**********************************************************/
// FIGHTERS_HISTORY
// 0x06057026:  mov.l #0x2FF01FF0, r0
// need change to 0x34403440

static void FIGHTERS_HISTORY_handle(REGS *reg)
{
	reg->r0 = 0x34403440;
	*(u32*)0x060500C8 = 0x34403440;
	*(u32*)0x06057000 = 0x34403440;
	*(u32*)0x0605707C = 0x34403440;
	*(u32*)0x060570E0 = 0x34403440;
	set_break_pc(0, 0);
}





/**********************************************************/
// X-Men vs. Street Fighter (Japan) (3M)
// 0x0601409a:  mov.l #0x23301330, r0

static void xmvsf_handle(REGS *reg)
{
	reg->r0 = 0x34403440;
}

void xmvsf_patch(void)
{
	break_in_game(0x0601409a, xmvsf_handle);
}


/**********************************************************/

int skip_patch = 0;

typedef struct _game_db {
	char *id;
	char *name;
	void (*patch_func)(void);
}GAME_DB;


GAME_DB game_dbs[] = {
	
	{"T-22205G",  "NOEL3",   NOEL3_patch},
	{"T-20109G",  "FRIENDS", FRIENDS_patch},
	{"T-1245G",   "DND2",    DND2_patch},
	{"T-1521G",   "ASTRA",   ASTRA_SUPERSTARS_patch},
	{"T-9904G",   "cotton2", cotton2_patch},
	{"T-9906G",   "cotton1", cotton1_patch},
	
	
	{"T-1230G",   "POCKET_FIGHTER", POCKET_FIGHTER_patch},
	{"T-1246G",   "SF_ZERO3", SF_ZERO3_patch},
	
	
	{"T-3111G",   "Metal_Slug", Metal_Slug_patch},
	{"T-3108G",   "KOF96",    kof96_patch},
	{"T-3121G",   "KOF97",    kof97_patch},
	{"T-3104G",   "SamuraiSp3",  smrsp3_patch},
	{"T-3116G",   "SamuraiSp4",  smrsp4_patch},
	{"T-3105G",   "REAL_BOUT",  REAL_BOUT_patch},
	{"T-3119G   V1.001",   "REAL_BOUT_SP_v1",  REAL_BOUT_SP_v1_patch},
	{"T-3119G   V1.002",   "REAL_BOUT_SP_v2",  REAL_BOUT_SP_v2_patch},
	{"T-15006G  V1.004",   "KAITEI_DAISENSOU", KAITEI_DAISENSOU_patch},

	{"T-1515G",   "WAKU7",    WAKU7_patch},
		
	
	
	
	
	
	{"T-16509G",  "SRMP7",    srmp7_patch},
	{"T-16510G",  "SRMP7SP",  srmp7_patch},
	
	
	
	
// Pia Carrot e Youkoso!! 2 不在主程序文件	
	
//豪血寺3 恶魔战士3 不同文件都有23301330
	
//	{"T-1248G",   "FINAL_FIGHT_REVENGE",  FINAL_FIGHT_REVENGE_patch},

//	{"GS-9107",   "FIGHTERS", FIGHTERS_HISTORY_patch},
	
//	{"T-1226G",   "XMENVSSF",  xmvsf_patch},
	{NULL,},
};

void patch_game(char *id)
{
	GAME_DB *gdb = &game_dbs[0];

	if(skip_patch)
		return;

	while(gdb->id){
		int slen = strlen(gdb->id);
		if(strncmp(gdb->id, id, slen)==0){
			printk("Patch game %s ...\n", gdb->name);
			gdb->patch_func();
			break;
		}

		gdb += 1;
	}

}

