
#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"


/******************************************************************************/


static int max_depth;
static int *qstack = (int*)0x2400a000;
static int qtop;

#define qs_push(lp, rp)  ( qstack[qtop++] = ((lp)<<16) | (rp))

void qsort4(void *idata, int num, int (*cmp_func)(const void*, const void*))
{
	int *data = (int*)idata;
	int ilp, irp, lp, rp, flag;

	max_depth = 0;
	qtop = 0;
	qs_push(0, num-1);

	while(qtop){
		qtop -= 1;
		ilp = qstack[qtop]>>16;
		irp = qstack[qtop]&0xffff;

		lp = ilp;
		rp = irp;
		flag = -1;
		while(rp > lp){
			if(cmp_func(data+lp, data+rp)>0){
				int temp = data[rp];
				data[rp] = data[lp];
				data[lp] = temp;
				flag = -flag;
			}
			if(flag<0){
				rp -= 1;
			}else{
				lp += 1;
			}
		}
		if(ilp<lp-1) qs_push(ilp, lp-1);
		if(lp+1<irp) qs_push(lp+1, irp);
		if(max_depth<qtop){
			max_depth = qtop;
		}
	}
	//printk("qsort4: max_depth=%d\n", max_depth);
}


/******************************************************************************/


char *get_token(char **str_in)
{
	char *str, *start, match;

	if(str_in==NULL || *str_in==NULL)
		return NULL;
	str = *str_in;

	while(*str==' ' || *str=='\t') str++;

	if(*str=='"'){
		match = '"';
		start = str+1;
		str += 1;
	}else if(*str){
		match = ' ';
		start = str;
	}else{
		return NULL;
	}

	while(*str && *str!=match) str++;
	if(*str==match){
		*str = 0;
		*str_in = str+1;
	}else{
		*str_in = NULL;
	}

	return start;
}


char *get_line(u8 *buf, int *pos, int size)
{
	char *line = (char*)buf+*pos;
	
	if(*pos>=size)
		return NULL;

	while(*pos<size && (buf[*pos]!='\r' && buf[*pos]!='\n')) *pos = (*pos)+1;
	while(*pos<size && (buf[*pos]=='\r' || buf[*pos]=='\n')){
		buf[*pos] = 0;
		*pos = (*pos)+1;
	}

	return line;
}


/******************************************************************************/

static int sscfg_p = 0;

void ss_config_init(void)
{
	sscfg_p = 0;
	*(u32*)(SYSINFO_ADDR+0x0100) = 0;
}

void ss_config_put(u32 val)
{
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = val;
	sscfg_p += 4;
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = 0;
}

/******************************************************************************/


int config_exmem(char *lbuf)
{
	if(lbuf[1]!='M')
		return -1;

	if(lbuf[0]=='1'){
		ss_config_put(0x30000001);
	}else if(lbuf[0]=='4'){
		ss_config_put(0x30000004);
	}else{
		return -1;
	}

	printk("    exmem_%s\n", lbuf);
	return 0;
}


int config_wrmem(char *lbuf)
{
	char *p;

	int addr = strtoul(lbuf, &p, 16);

	p = strchr(lbuf, '=');
	if(p==NULL)
		return -1;
	p += 1;
	while(*p==' ') p += 1;

	int width = strlen(p);
	int val = strtoul(p, NULL, 16);
	addr = ((width/2)<<28) | (addr&0x0fffffff);
	ss_config_put(addr);
	ss_config_put(val);
	printk("    M_%08x=%x\n", addr, val);

	return 0;
}

static char catbuf[32];
int category_num = 0;

int config_category(char *lbuf)
{
	char *category = (char*)(SYSINFO_ADDR+0x0e80); // 610a3e80

	if(category_num<12){
		memcpy(category+category_num*32, catbuf, 31);
		category_num += 1;
		*(u8*)(SYSINFO_ADDR+0x0c) = category_num;
	}
	return 0;
}

char *get_category(int id)
{
	return (char*)(SYSINFO_ADDR + 0x0e80 + id*32); // 610a3e80
}


/******************************************************************************/


#define ARG_NON  0
#define ARG_HEX  1
#define ARG_DEC  2
#define ARG_STR  3

#define GA       0x0100


typedef struct {
	char *name;
	int type;
	void *action;
	void *action_ex;
}CFGARG;


CFGARG arg_list [] = {
	{"lang_id",      GA|ARG_DEC, &lang_id,},
	{"debug",        GA|ARG_HEX, &debug_flags,},
	{"log_mask",     GA|ARG_HEX, &log_mask,},
	{"auto_update",  GA|ARG_DEC, &auto_update,},
	{"category",     GA|ARG_STR, &catbuf, config_category},
	{"sector_delay",    ARG_DEC, &sector_delay,},
	{"play_delay",      ARG_DEC, &play_delay,},
	{"pend_delay",      ARG_DEC, &pend_delay,},
	{"exmem_",          ARG_NON, config_exmem,},
	{"M_",              ARG_NON, config_wrmem,},
	{"multi_disc",      ARG_STR, &mdisc_str,},
	{"sort_mode",    GA|ARG_DEC, &sort_mode,},

	{NULL},
};


int parse_config(char *fname, char *gameid)
{
	FIL fp;
	u8 *fbuf = (u8*)0x24002000;
	char *p = NULL;
	int retv;

	ss_config_init();

	retv = f_open(&fp, fname, FA_READ);
	if(retv){
		return -2;
	}

	u32 nread;
	retv = f_read(&fp, fbuf, f_size(&fp), &nread);
	f_close(&fp);
	if(retv){
		return -3;
	}

	printk("\nLoad config [%s] for [%s]\n", fname, (gameid==NULL)?"Global":gameid);

	int cpos = 0;
	int g_sec = 0;
	int in_sec = 0;
	char *lbuf;
	while((lbuf=get_line(fbuf, &cpos, nread))!=NULL){
_next_section:
		//查找section: [xxxxxx]
		if(in_sec==0){
			if(lbuf[0]=='['){
				p = strrchr(lbuf, ']');
				if(p==NULL){
					return -4;
				}
				*p = 0;
				if(g_sec==0){
					// 第一个section一定是global
					if(strcmp(lbuf+1, "global")){
						return -5;
					}
					g_sec = 1;
					in_sec = 1;
					printk("Global config:\n");
				}else if(gameid && strcmp(lbuf+1, gameid)==0){
					// 找到了与game_id匹配的section
					in_sec = 1;
					printk("Game config:\n");
				}
			}
			continue;
		}else{
			if(lbuf[0]=='['){
				if(g_sec==1){
					g_sec = 2;
					in_sec = 0;
					if(gameid==NULL)
						break;
					goto _next_section;
				}else{
					break;
				}
			}
			CFGARG *arg = arg_list;
			while(arg->name){
				int nlen = strlen(arg->name);
				int type = arg->type;
				int global = (type&GA);
				type &= 0xff;
				if(global && (gameid || g_sec!=1) ){
					arg += 1;
					continue;
				}
				if(strncmp(lbuf, arg->name, nlen)==0){
					if(type==ARG_NON){
						int (*action)(char*) = arg->action;
						retv = action(lbuf+nlen);
					}else if(type==ARG_STR){
						p = strchr(lbuf+nlen, '"');
						if(p){
							char *st = p+1;
							p = strchr(st, '"');
							if(p){
								*p = 0;
								printk("    %s = \"%s\"\n", arg->name, st);
								strcpy((char*)(arg->action), st);
								retv = 0;
							}else{
								retv = -1;
							}
						}else{
							retv = -1;
						}
					}else{
						int base = (type==ARG_DEC)? 10 : 16;
						p = strchr(lbuf+nlen, '=');
						if(p){
							int value = strtoul(p+1, NULL, base);
							if(base==10){
								printk("    %s = %d\n", arg->name, value);
							}else{
								printk("    %s = %08x\n", arg->name, value);
							}
							*(int*)(arg->action) = value;
							retv = 0;
						}else{
							retv = -1;
						}
					}
					if(retv){
						printk("Invalid config line: {%s}\n", lbuf);
						led_event(LEDEV_SCFG_ERROR);
					}else{
						if(arg->action_ex){
							void (*action_ex)(void) = arg->action_ex;
							action_ex();
						}
					}
				}
				arg += 1;
			}

		}
	}
	printk("\n");

	return 0;
}

/******************************************************************************/

#define SSAVE_FILE "/SAROO/SS_SAVE.BIN"
#define SSAVE_ADDR (SAVINFO_ADDR+0x00000)

static FIL ss_fp;
static int ss_index;


#define SMEMS_FILE "/SAROO/SS_MEMS.BIN"
#define SMEMS_HDR  (SAVINFO_ADDR+0x10000)
#define SMEMS_BUF  (SAVINFO_ADDR+0x13000)

static FIL sm_fp;
static int sm_index0;
static int sm_index1;


int open_savefile(void)
{
	u32 rv;
	int retv;
	u8 *fbuf = (u8*)0x24002000;

	printk("Open SSAVE ...\n");
	ss_index = 0;

	retv = f_open(&ss_fp, SSAVE_FILE, FA_READ|FA_WRITE);
	if(retv==0){
		f_read(&ss_fp, fbuf, 0x40, &rv);
		if(strcmp((char*)fbuf, "Saroo Save File")==0){
			ss_index = 1;
			goto _open_mems;
		}
	}

	printk("SS_SAVE.BIN not found! Create now.\n");
	retv = f_open(&ss_fp, SSAVE_FILE, FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
	if(retv){
		printk("Create SS_SAVE.BIN failed! %d\n", retv);
		return retv;
	}

	memset(fbuf, 0, 0x10000);
	strcpy((char*)fbuf, "Saroo Save File");
	f_write(&ss_fp, fbuf, 0x10000, &rv);
	f_sync(&ss_fp);

	ss_index = 1;

_open_mems:

	retv = f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE);
	printk("Open SMEMS ... %d\n", retv);
	if(retv){
		printk("SS_MEMS.BIN not found! Create now.\n");
		retv = f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
		if(retv<0){
			printk("Create SS_MEMS.BIN failed! %d\n", retv);
			return retv;
		}
		f_lseek(&sm_fp, 0x800000);
		f_close(&sm_fp);
		f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE);
	}

	f_read(&sm_fp, (u8*)SMEMS_HDR, 8192, &rv);
	sm_index0 = 0;
	sm_index1 = 0;

	printk("Done.\n");
	return 0;
}


int load_savefile(char *gameid)
{
	u32 rv;
	int retv, i;
	u8 *fbuf = (u8*)0x24002000;

	if(ss_index==0)
		return -1;

	f_lseek(&ss_fp, 0);
	f_read(&ss_fp, fbuf, 0x10000, &rv);

	for(i=1; i<4096; i++){
		ss_index = i;
		if(*(u32*)(fbuf+i*16)==0)
			break;
		if(strncmp((char*)fbuf+i*16, gameid, 8)==0){
			// found it
			printk("Found savefile at %08x\n", i*0x10000);
			f_lseek(&ss_fp, i*0x10000);
			f_read(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);
			return 0;
		}
	}
	if(i==4096)
		return -1;

	// not found, create it.
	printk("Create savefile at %08x\n", i*0x10000);
	strcpy((char*)fbuf+i*16, gameid);
	// update index
	f_lseek(&ss_fp, i*16);
	f_write(&ss_fp, fbuf+i*16, 16, &rv);
	// write empty save
	memset((u8*)SSAVE_ADDR, 0, 0x10000);
	f_lseek(&ss_fp, i*0x10000);
	f_write(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);

	f_sync(&ss_fp);

	return 0;
}


int flush_savefile(void)
{
	u32 rv;

	if(ss_index==0)
		return -1;

	printk("Flush savefile at %08x\n", ss_index*0x10000);
	f_lseek(&ss_fp, ss_index*0x10000);
	f_write(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);
	f_sync(&ss_fp);

	return 0;
}


/******************************************************************************/

int load_smems(int id)
{
	u32 rv;
	int is_hdr;

	is_hdr = id&0x8000;
	id &= 0x7fff;
	printk("Load SMEMS %04x\n", id);
	if(id==0)
		return 0;

	f_lseek(&sm_fp, id*1024);
	if(is_hdr){
		f_read(&sm_fp, (u8*)SMEMS_HDR+8192, 1024, &rv);
		sm_index0 = id;
	}else{
		f_read(&sm_fp, (u8*)SMEMS_BUF, 64*1024, &rv);
		sm_index1 = id;
	}

	return 0;
}


int flush_smems(int flag)
{
	u32 rv;

	int d = sm_index0-sm_index1;
	if(d>=0 && d<64){
		memcpy32((u8*)SMEMS_BUF+d*1024, (u8*)SMEMS_HDR+8192, 1024);
	}
	printk("\nflush_smems: flag=%02x s0=%04x s1=%04x d=%d\n", flag, sm_index0, sm_index1, d);

	if(flag&1){
		printk("Flush SMEMS header.\n");
		f_lseek(&sm_fp, 0);
		f_write(&sm_fp, (u8*)SMEMS_HDR, 8192, &rv);
	}
	if(flag&2){
		printk("Flush SMEMS block %04x.\n", sm_index0);
		if(d<0 || d>=64 || (flag&4)==0){
			f_lseek(&sm_fp, sm_index0*1024);
			f_write(&sm_fp, (u8*)SMEMS_HDR+8192, 1024, &rv);
		}
	}
	if(flag&4){
		printk("Flush SMEMS buffer %04x.\n", sm_index1);
		f_lseek(&sm_fp, sm_index1*1024);
		f_write(&sm_fp, (u8*)SMEMS_BUF, 64*1024, &rv);
	}
	f_sync(&sm_fp);

	return 0;
}


/******************************************************************************/


