
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bup.h"


/******************************************************************************/


u8 *load_file(char *name, int *size)
{
	FILE *fp;
	u8 *buf;

	fp = fopen(name, "rb");
	if(fp==NULL){
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = (u8*)malloc((*size)*2);
	fread(buf, *size, 1, fp);
	fclose(fp);

	return buf;
}


int write_file(char *file, void *buf, int size)
{
	FILE *fp;
	int written;

	fp = fopen(file, "wb");
	if(fp==NULL)
		return -1;
	written = fwrite(buf, 1, size, fp);
	fclose(fp);

	return written;
}


u32 get_be32(void *p)
{
	u8 *d = (u8*)p;
	return (d[0]<<24) | (d[1]<<16) | (d[2]<<8) | d[3];

}


void put_be32(void *p, u32 v)
{
	u8 *d = (u8*)p;

	d[0] = v>>24;
	d[1] = v>>16;
	d[2] = v>>8;
	d[3] = v;
}


u32 get_be16(void *p)
{
	u8 *d = (u8*)p;
	return (d[0]<<8) | d[1];

}


void put_be16(void *p, u32 v)
{
	u8 *d = (u8*)p;

	d[0] = v>>8;
	d[1] = v;
}


void set_bitmap(u8 *bmp, int index, int val)
{
	int byte = index/8;
	int mask = 1<<(index&7);

	if(val)
		bmp[byte] |= mask;
	else
		bmp[byte] &= ~mask;
}


/******************************************************************************/

u8 save_buf[0x100000];
static SAVEINFO saveinfo;

SAVEINFO *load_saveraw(char *save_name)
{
	SAVEINFO *sinfo = &saveinfo;
	u8 *fbuf;
	int fsize;

	fbuf = load_file(save_name, &fsize);
	if(fbuf==NULL){
		printf("Faield to load file %s!\n", save_name);
		return NULL;
	}
	if(strcmp((char*)fbuf, "SSAVERAW")){
		printf("%s: Not a SSAVERAW file!\n", save_name);
		return NULL;
	}

	memset(sinfo, 0, sizeof(SAVEINFO));
	memcpy(sinfo->file_name, fbuf+0x10, 11);
	memcpy(sinfo->comment, fbuf+0x20, 10);
	memcpy(&sinfo->data_size, fbuf+0x1c, 4);
	memcpy(&sinfo->date, fbuf+0x2c, 4);
	sinfo->language = fbuf[0x2b];

	memcpy(save_buf, fbuf+0x40, fsize-0x40);
	sinfo->dbuf = save_buf;
	return sinfo;
}

/******************************************************************************/

// 0: SAROO save file
// 1: Saturn save file
// 2: SAROO extend save

static int bup_type;
static u8 *bup_buf;
static int bup_size;
static char *bup_name = NULL;


void bup_flush(void)
{
	write_file(bup_name, bup_buf, bup_size);
}


int bup_create(char *game_id)
{
	int retv;

	if(bup_type==0){
		retv =  sr_bup_create(game_id);
	}else if(bup_type==1){
		retv =  ss_bup_create(game_id);
	}else if(bup_type==2){
		retv =  sr_mems_create(game_id);
	}
	if(retv>0){
		bup_size += retv;
		bup_flush();
		return 0;
	}

	return retv;
}


int bup_import(int slot_id, int save_id, char *save_name)
{
	int retv;

	if(bup_type==0){
		retv = sr_bup_import(slot_id, save_id, save_name);
	}else if(bup_type==1){
		retv = ss_bup_import(slot_id, save_id, save_name);
	}else if(bup_type==2){
		retv = sr_mems_import(slot_id, save_id, save_name);
	}
	if(retv==0){
		bup_flush();
	}

	return retv;
}


int bup_delete(int slot_id, int save_id)
{
	int retv;

	if(bup_type==0){
		retv = sr_bup_delete(slot_id, save_id);
	}else if(bup_type==1){
		retv = ss_bup_delete(slot_id, save_id);
	}else if(bup_type==2){
		retv = sr_mems_delete(slot_id, save_id);
	}
	if(retv==0){
		bup_flush();
	}

	return retv;
}


int bup_export(int slot_id, int save_id)
{
	if(bup_type==0){
		return sr_bup_export(slot_id, save_id);
	}else if(bup_type==1){
		return ss_bup_export(slot_id, save_id);
	}else if(bup_type==2){
		return sr_mems_export(slot_id, save_id);
	}

	return -1;
}


int bup_list(int slot_id)
{
	if(bup_type==0){
		return sr_bup_list(slot_id);
	}else if(bup_type==1){
		return ss_bup_list(slot_id);
	}else if(bup_type==2){
		return sr_mems_list(slot_id);
	}

	return -1;
}


int bup_load(char *bup_name)
{
	bup_buf = load_file(bup_name, &bup_size);
	if(bup_buf==NULL)
		return -1;

	if(strcmp((char*)bup_buf, "Saroo Save File")==0){
		bup_type = 0;
		return sr_bup_init(bup_buf);
	}
	if(strncmp((char*)bup_buf, "BackUpRam Format", 16)==0){
		bup_type = 1;
		return ss_bup_init(bup_buf);
	}
	if(bup_buf[1]=='B' && bup_buf[3]=='a' && bup_buf[5]=='c' && bup_buf[7]=='k'){
		bup_type = 1;
		return ss_bup_init(bup_buf);
	}
	if(strncmp((char*)bup_buf, "SaroMems", 8)==0){
		bup_type = 2;
		return sr_mems_init(bup_buf);
	}

	return -2;
}


/******************************************************************************/


int main(int argc, char *argv[])
{
	int slot_id = -1;
	int save_id = -1;
	int create = 0;
	int import = 0;
	int delete = 0;
	char *save_name = NULL;
	char *game_id = NULL;

	int retv;
	int p = 1;


	if(argc==1){
		printf("Usage: sstool  save_file [-c \"gameid\"][-t n] [-s n] [-i [file]]\n");
		printf("    -t n        Select save slot(for SAROO save).\n");
		printf("    -c \"gameid\" New save slot(for SAROO save).\n");
		printf("    -s n        Select and Export(without -i/-d) save.\n");
		printf("    -i [file]   Import save.\n");
		printf("    -d          Delete save.\n");
		printf("                List save.\n");
		printf("\n");
		return 0;
	}


	while(p<argc){
		if(argv[p][0]=='-'){
			switch(argv[p][1]){
			case 't':
				if(p+1==argc)
					goto _invalid_params;
				slot_id = atoi(argv[p+1]);
				p += 1;
				break;
			case 's':
				if(p+1==argc)
					goto _invalid_params;
				save_id = atoi(argv[p+1]);
				p += 1;
				break;
			case 'c':
				if(p+1==argc)
					goto _invalid_params;
				create = 1;
				game_id = argv[p+1];
				p += 1;
				break;
			case 'i':
				import = 1;
				if(p+1<argc){
					save_name = argv[p+1];
					p += 1;
				}
				break;
			case 'd':
				delete = 1;
				break;
			default:
_invalid_params:
				printf("Invalid params: %s\n", argv[p]);
				return -1;
			}
		}else if(bup_name==NULL){
			bup_name = argv[p];
		}
		p += 1;
	}

	if(bup_name==NULL){
		printf("Need a Save file!\n");
		return -1;
	}
	if(create){
		if(game_id==NULL){
			printf("Need a Game ID!\n");
			return -1;
		}
	}


	retv = bup_load(bup_name);
	if(retv<0){
		printf("Faield to load save file!\n");
		return retv;
	}

	if(create){
		retv = bup_create(game_id);
	}else if(import){
		retv = bup_import(slot_id, save_id, save_name);
	}else if(delete){
		retv = bup_delete(slot_id, save_id);
	}else if(save_id>=0){
		retv = bup_export(slot_id, save_id);
	}else{
		retv = bup_list(slot_id);
	}

	return retv;
}


