
#include "main.h"
#include "smpc.h"


/**********************************************************/
// KOF97
// 0x06066f7e:  mov.l #0x23301ff0, r2
// need change to 0x34403440

static void kof97_handle(REGS *reg)
{
	reg->r2 = 0x34403440;
}

void kof97_patch(void)
{
	break_in_game(0x06066f7e, kof97_handle);
}


/**********************************************************/
// SRMP7
// 0x06011204:  mov.l #0x23301ff0, r3
// need change to 0x34403440

static void srmp7_handle(REGS *reg)
{
	reg->r3 = 0x34403440;
}

void srmp7_patch(void)
{
	break_in_game(0x06011204, srmp7_handle);
}


/**********************************************************/
// Samurai Spirits - Amakusa Kourin
// 1MB RAM cart only
// 0x06067e32:  mov.l #0x23301ff0, r2
// need change to 0x34403440

static void smrsp4_handle(REGS *reg)
{
	reg->r2 = 0x34403440;
}


void smrsp4_patch(void)
{
	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	break_in_game(0x06067e32, smrsp4_handle);
}


/**********************************************************/
// Samurai Spirits - Zankuro Musouken
// 0x0605ad32:  mov.l #0x23301ff0, r1
// need change to 0x34403440

static void smrsp5_handle(REGS *reg)
{
	if(reg->r2==0x23301ff0){
		reg->r2 = 0x34403440;
		break_in_game_next(0x0605ad32, smrsp5_handle);
	}else{
		reg->r1 = 0x34403440;
	}
}


void smrsp5_patch(void)
{
//	SS_CTRL = (SAROO_EN | CS0_RAM1M);
	break_in_game(0x0600d190, smrsp5_handle);
}


/**********************************************************/

int skip_patch = 0;

typedef struct _game_db {
	char *id;
	char *name;
	void (*patch_func)(void);
}GAME_DB;


GAME_DB game_dbs[] = {
	{"T-3121G",   "KOF97",    kof97_patch},
	{"T-16509G",  "SRMP7",    srmp7_patch},
	{"T-16510G",  "SRMP7SP",  srmp7_patch},
	{"T-3116G",   "SamuraiSp4",  smrsp4_patch},
	{"T-3104G",   "SamuraiSp5",  smrsp5_patch},
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

