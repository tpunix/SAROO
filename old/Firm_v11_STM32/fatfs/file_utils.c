
#include <string.h>

#include "../main.h"
#include "ff.h"

/**************************************************************/

/**************************************************************/

void sps(int level)
{
	int i;

	level *= 4;

	for(i=0; i<level; i++){
		printk(" ");
	}
}


void mkdir_p(char *dname)
{
	char name[256];
	char *p, *cp;

	memcpy(name, dname, 256);

	cp = name;
	while(1){
		p = strchr(cp, '/');
		if(p==NULL)
			break;

		*p = 0;
		f_mkdir(name);
		*p = '/';
		cp = p+1;
	};
}


int scan_dir(char *dirname, int level, int (*func)(char *, char*, int))
{
	FRESULT retv;
	DIR dir;
	FILINFO info;
	char tmp[192];
	char *fn;

	memset(&dir, 0, sizeof(dir));
	memset(&info, 0, sizeof(info));


	retv = f_opendir(&dir, dirname);
	if(retv)
		return -1;

	while(1){
		retv = f_readdir(&dir, &info);
		if(retv!=FR_OK || info.fname[0]==0)
			break;
		if(info.fname[0]=='.')
			continue;

		fn = info.fname;
		sprintk(tmp, "%s/%s", dirname, fn);

		if(info.fattrib & AM_DIR){
			if(func==NULL){
				sps(level); printk("[%s]\n", fn);
			}
			scan_dir(tmp, level+1, func);
		}else{
			if(func==NULL){
				sps(level); printk("%9d  %s\n", info.fsize, fn);
			}else{
				if(func(dirname, tmp, level)){
					break;
				}
			}
		}
	}

	f_closedir(&dir);

	return 0;
}

/**************************************************************/

