
#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"


/******************************************************************************/


FIL track_fp[100];


/******************************************************************************/


static char *get_token(char **str_in)
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


int parse_cue(char *fname, int check)
{
	FIL fp;
	u8 *fbuf = (u8*)0x24002000;
	char *dir_name  = (char*)0x2400a000;
	char *full_path = (char*)0x2400a200;
	int i, retv;

	FIL *tk_fp = NULL;
	TRACK_INFO *tk;
	int fad_offset = 150;
	int tno = -1;
	int last_tk = -1;

	retv = f_open(&fp, fname, FA_READ);
	if(retv){
		return -2;
	}

	u32 nread;
	retv = f_read(&fp, fbuf, f_size(&fp), &nread);
	if(retv){
		return -3;
	}

	strcpy(dir_name, fname);
	char *p = strrchr(dir_name, '/');
	*p = 0;


	int cpos = 0;
	char *lbuf;
	while((lbuf=get_line(fbuf, &cpos, nread))!=NULL){
		//printk("nread=%d cpos=%d lbuf: {%s}\n", nread, cpos, lbuf);
		char *token = get_token(&lbuf);
		//printk("token: {%s}\n", token);
		if(strcmp(token, "FILE")==0){
			char *tfile = get_token(&lbuf);
			char *ftype = get_token(&lbuf);
			if(tfile==NULL)
				return -4;
			if(strcmp(ftype, "BINARY"))
				return -5;

			sprintk(full_path, "%s/%s", dir_name, tfile);
			if(check){
				FILINFO fi;
				retv = f_stat(full_path, &fi);
				if(retv){
					return -6;
				}
			}else{
				if(tno!=-1){
					// close last track
					tk = &cdb.tracks[tno];
					int fad_size = (f_size(tk_fp)-tk->file_offset)/tk->sector_size;
					tk->fad_end = tk->fad_start + fad_size - 1;
					fad_offset = tk->fad_end+1;
				}
				last_tk = tno+1;
				tk = &cdb.tracks[tno+1];
				tk_fp = &track_fp[tno+1];
				retv = f_open(tk_fp, full_path, FA_READ);
				if(retv){
					return -6;
				}
			}
		}else if(strcmp(token, "TRACK")==0){
			char *tnum = get_token(&lbuf);
			int tid = strtoul(tnum, NULL, 10);
			tno += 1;
			if(tid!=tno+1){
				return -7;
			}

			char *mstr = get_token(&lbuf);
			int mode, size;
			if(strncmp(mstr, "MODE1", 5)==0){
				mode = 1;
				size = strtoul(mstr+6, NULL, 10);
			}else if(strncmp(mstr, "MODE2", 5)==0){
				mode = 2;
				size = strtoul(mstr+6, NULL, 10);
			}else if(strncmp(mstr, "AUDIO", 5)==0){
				mode = 3;
				size = 2352;
			}else{
				return -8;
			}

			if(check==0){
				tk = &cdb.tracks[tno];
				tk->fp = tk_fp;
				tk->sector_size = size;
				tk->mode = mode;
			}
		}else if(strcmp(token, "INDEX")==0){
			char *inum = get_token(&lbuf);
			char *tstr = get_token(&lbuf);
			if(inum==NULL || tstr==NULL){
				return -9;
			}
			if(check==0){
				int idx = strtoul(inum, NULL, 10);
				int m = strtoul(tstr+0, &tstr, 10);
				int s = strtoul(tstr+1, &tstr, 10);
				int f = strtoul(tstr+1, &tstr, 10);
				int fad = MSF_TO_FAD(m, s, f);

				if(last_tk < tno){
					// close last track
					tk = &cdb.tracks[last_tk];
					tk->fad_end = fad_offset+fad-1;
					last_tk = tno;
				}

				tk = &cdb.tracks[tno];
				if(idx==0){
					tk->fad_0 = fad_offset + fad;
				}
				if(idx==1){
					tk->fad_start = fad_offset + fad;
					if(tk->fad_0==0)
						tk->fad_0 = tk->fad_start;
					tk->file_offset = fad*tk->sector_size;
					tk->ctrl_addr = (tk->mode==3)? 0x01 : 0x41;
				}
			}
		}else if(strcmp(token, "CATALOG")==0){
		}else{
			return -10;
		}
	}

	if(check)
		return 0;

	// close last track
	tk = &cdb.tracks[tno];
	int fad_size = (f_size(tk_fp)-tk->file_offset)/tk->sector_size;
	tk->fad_end = tk->fad_start + fad_size - 1;
	fad_offset = tk->fad_end+1;

	cdb.track_num = tno+1;

	init_toc();

	return 0;
}


/******************************************************************************/

int total_disc;
static int *disc_path = (int *)0x61800004;
static char *path_str = (char*)0x61800000;
static int pbptr;

static int scan_cue(char *dirname, char *fname, int level)
{
	if(level>1)
		return 1;

	char *p = strrchr(fname, '.');
	if(p==NULL || strcmp(p, ".cue")){
		return 0;
	}

	int retv = parse_cue(fname, 1);
	if(retv){
		printk("parse_cue: %d {%s}\n", retv, fname);
		return 0;
	}

	disc_path[total_disc] = pbptr;
	strcpy(path_str+pbptr, fname);
	pbptr += strlen(fname)+1;

	total_disc += 1;
	*(int*)(0x61800000) = total_disc;
	return 0;
}


void list_disc(void)
{
	int i;

	total_disc = 0;
	pbptr = 0x1000;
	memset(disc_path, 0x00, 0x20000);

	scan_dir("/SAROO/ISO", 0, scan_cue);

	printk("Total discs: %d\n", total_disc);
	for(i=0; i<total_disc; i++){
		printk(" %2d:  %s\n", i, path_str+disc_path[i]);
	}
	printk("\n");
}


int unload_disc(void)
{
	int i;

	cdb.status = STAT_NODISC;
	set_status(cdb.status);

	if(cdb.track_num==0)
		return 0;

	for(i=0; i<cdb.track_num; i++){
		TRACK_INFO *tk = &cdb.tracks[i];
		if(tk->fp){
			f_close(tk->fp);
		}
		memset(tk, 0, sizeof(TRACK_INFO));
	}

	return 0;
}


int load_disc(int index)
{
	int retv;

	unload_disc();

	if(disc_path[index]==0){
		printk("Invalid disc index %d\n", index);
		return -1;
	}

	printk("Load disc: {%s}\n", path_str+disc_path[index]);
	retv = parse_cue(path_str+disc_path[index], 0);
	if(retv){
		printk("  retv=%d\n", retv);
	}
	return retv;
}
