
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bup.h"
#include "bup_format.h"


/******************************************************************************/

static u8 *bup_buf;
static u8 *bup_mem;

#define	BUP_NON                 (1)
#define	BUP_UNFORMAT            (2)
#define	BUP_WRITE_PROTECT       (3)
#define	BUP_NOT_ENOUGH_MEMORY   (4)
#define	BUP_NOT_FOUND           (5)
#define	BUP_FOUND               (6)
#define	BUP_NO_MATCH            (7)
#define	BUP_BROKEN              (8)


typedef struct {
	char file_name[12];
	char comment[11];
	char language;
	u32  date;
	u32  data_size;
	u16  block_size;
}BUPDIR;


/******************************************************************************/

typedef struct {
	u32 magic0;       // 00
	u32 magic1;
	u32 total_size;
	u16 block_size;
	u16 free_block;
	u8  unuse_10[16]; // 10
	u8  gameid[16];   // 20
	u8  unuse_30[14]; // 30
	u16 first_save;   // 3e
	u8  bitmap[64];   // 40
}BUPHEADER;

#define BUPMEM  ((BUPHEADER*)(bup_mem))

static u16 block_size;
static u16 free_block;

/******************************************************************************/


static int get_free_block(int pos)
{
	int i, byte, mask;

	for(i=pos; i<512; i++){
		byte = i/8;
		mask = 1<<(i&7);
		if( (BUPMEM->bitmap[byte]&mask) == 0 ){
			BUPMEM->bitmap[byte] |= mask;
			free_block -= 1;
			return i;
		}
	}

	return 0;
}


static int get_next_block(u8 *bmp, int pos)
{
	int byte, mask;

	while(pos<512){
		byte = pos/8;
		mask = 1<<(pos&7);
		if(bmp[byte]&mask){
			return pos;
		}
		pos += 1;
	}

	return 0;
}


static u8 *get_block_addr(int id)
{
	return (u8*)(bup_mem + block_size*id);
}


static int access_data(int block, u8 *data, int type)
{
	u8 *bp, *bmp;
	int dsize, asize, bsize;

	bsize = block_size;

	bp = get_block_addr(block);
	bmp = bp+0x40;
	dsize = get_be32(bp+0x0c);
	block = 0;

	while(dsize>0){
		block = get_next_block(bmp, block);
		bp = get_block_addr(block);

		asize = (dsize>bsize)? bsize : dsize;
		//printf("dsize=%04x block=%04x asize=%04x\n", dsize, block, asize);

		if(type==0){
			// read
			memcpy(data, bp, asize);
		}else{
			// write
			memcpy(bp, data, asize);
		}
		data += asize;
		dsize -= asize;
		block += 1;
	}

	return 0;
}


/******************************************************************************/


static int sr_bup_format(u8 *gameid)
{
	memset(bup_mem, 0, 0x10000);

	// SaroSave
	put_be32(&BUPMEM->magic0, 0x5361726f);
	put_be32(&BUPMEM->magic1, 0x53617665);
	put_be32(&BUPMEM->total_size, 0x10000);
	put_be16(&BUPMEM->block_size, 0x80);
	put_be16(&BUPMEM->free_block, 512-1);
	memcpy(&BUPMEM->gameid, gameid, 16);
	put_be16(&BUPMEM->first_save, 0x0000);

	memset(&BUPMEM->bitmap, 0, 0x40);
	BUPMEM->bitmap[0] = 0x01;

	return 0;
}


static int sr_bup_select(int slot_id)
{
	char sname[20];

	if(slot_id<0 || *(u32*)(bup_buf+slot_id*16)==0)
		return -1;

	memcpy(sname, bup_buf+slot_id*16, 16);
	sname[16] = 0;
	printf("[%s] :\n", sname);
	bup_mem = bup_buf+slot_id*0x10000;

	// Check "SaroSave"
	if(get_be32(&BUPMEM->magic0)!=0x5361726f || get_be32(&BUPMEM->magic1)!=0x53617665){
		printf("  empty bup memory, need format.\n");
		sr_bup_format(bup_buf+slot_id*16);
	}

	block_size = get_be16(&BUPMEM->block_size);
	free_block = get_be16(&BUPMEM->free_block);
	return 0;
}


/******************************************************************************/


int sr_bup_export(int slot_id, int save_id, int exp_type)
{
	int block, i;
	u8 *bp;

	if(sr_bup_select(slot_id)<0)
		return -1;

	i = 0;
	block = get_be16(&BUPMEM->first_save);
	while(block){
		bp = get_block_addr(block);
		if(i == save_id){
			memset(save_buf, 0, 32768);

			strcpy((char*)save_buf, "SSAVERAW");
			memcpy(save_buf+0x10, bp+0, 11);    // Filename
			memcpy(save_buf+0x1c, bp+0x0c, 4);  // Size
			memcpy(save_buf+0x20, bp+0x10, 10); // Comment
			save_buf[0x2b] = bp[0x1b];          // Language
			memcpy(save_buf+0x2c, bp+0x1c, 4);  // Date

			access_data(block, save_buf+0x40, 0);

			char fname[16];
			int fsize = get_be32(save_buf+0x1c);
			sprintf(fname, "%s%s", save_buf+0x10, exp_type ? BUP_EXTENSION : ".bin");

			// if Export format is .BUP, set new header
			if (exp_type==1){
				vmem_bup_header_t bup_header = {0};
				
				memcpy(bup_header.magic, VMEM_MAGIC_STRING, VMEM_MAGIC_STRING_LEN);
				memcpy(bup_header.dir.filename, bp, 11);
				memcpy(bup_header.dir.comment, bp+0x10, 10);
				memcpy(&bup_header.dir.date, bp+0x1c, 4);
				memcpy(&bup_header.dir.datasize, bp+0x0c, 4);
				memcpy(&bup_header.date, bp+0x1c, 4);
				bup_header.dir.language = bp[0x1b];

				memcpy(save_buf, &bup_header, BUP_HEADER_SIZE);
			}

			write_file(fname, save_buf, fsize+0x40);

			printf("Export Save_%d: %s\n", i, fname);
			return 0;
		}
		block = get_be16(bp+0x3e);
		i += 1;
	}

	printf("Save_%d not found!\n", save_id);
	return -1;
}


int sr_bup_import(int slot_id, int save_id, char *save_name)
{
	SAVEINFO *save;
	int block, block_need, i, last, hdr;
	u8 *bp;

	if(sr_bup_select(slot_id)<0)
		return -1;

	i = 0;
	last = 0;
	block = get_be16(&BUPMEM->first_save);
	while(block){
		bp = get_block_addr(block);
		if(i == save_id){
			break;
		}
		last = block;
		block = get_be16(bp+0x3e);
		i += 1;
	}
	if(block){
		// overwrite
		char sname[20];
		if(save_name==NULL){
			sprintf(sname, "%s.bin", bp);
			save_name = sname;
		}
		save = load_saveraw(save_name);
		if(save==NULL)
			return -1;

		*(u32*)(bp+0x1c) = save->date;
		access_data(block, save->dbuf, 2);
		printf("Import %s from %s.\n", bp, save_name);
		return 0;
	}
	if(save_id>=0){
		printf("Save_%d not found!\n", save_id);
		return -1;
	}

	// new save
	save = load_saveraw(save_name);
	if(save==NULL)
		return -1;

	// 计算所需的块数量。起始块+块列表块+数据块
	int dsize = get_be32(&save->data_size);
	block_need = (dsize+block_size-1)/(block_size);
	printf("block_need=%d\n", block_need+1);
	if((block_need+1) > free_block){
		return -1;
	}

	// 分配起始块
	block = get_free_block(0);
	hdr = block;

	bp = get_block_addr(hdr);
	printf("start at %d\n", hdr);

	// 写开始块
	memset(bp, 0, 64*2);
	memcpy(bp+0x00, save->file_name, 11);
	*(u32*)(bp+0x0c) = save->data_size;
	memcpy(bp+0x10, save->comment, 10);
	bp[0x1b] = save->language;
	*(u32*)(bp+0x1c) = save->date;

	// 分配块
	block = 0;
	for(i=0; i<block_need; i++){
		block = get_free_block(block);
		set_bitmap(bp+0x40, block, 1);
		block += 1;
	}

	// 写数据
	access_data(hdr, save->dbuf, 2);

	bp = get_block_addr(last);
	put_be16(bp+0x3e, hdr);
	put_be16(&BUPMEM->free_block, free_block);

	printf("Import %s.\n", save->file_name);
	return 0;
}


int sr_bup_delete(int slot_id, int save_id)
{
	int block, i, last;
	u8 *bp;

	if(sr_bup_select(slot_id)<0)
		return -1;

	i = 0;
	last = 0;
	block = get_be16(&BUPMEM->first_save);
	while(block){
		bp = get_block_addr(block);
		if(i == save_id){
			break;
		}
		last = block;
		block = get_be16(bp+0x3e);
		i += 1;
	}
	if(block==0){
		printf("Save_%d not found!\n", save_id);
		return -1;
	}

	// 释放开始块
	set_bitmap((u8*)BUPMEM->bitmap, block, 0);
	free_block += 1;

	// 释放数据块
	u8 *bmp = bp+0x40;
	block = 0;
	while(1){
		block = get_next_block(bmp, block);
		if(block==0)
			break;
		set_bitmap((u8*)BUPMEM->bitmap, block, 0);
		free_block += 1;
		block += 1;
	}

	// 更新last指针
	u8 *last_bp = get_block_addr(last);
	*(u16*)(last_bp+0x3e) = *(u16*)(bp+0x3e);

	put_be16(&BUPMEM->free_block, free_block);

	printf("Delete Save_%d.\n", save_id);

	return 0;
}


int sr_bup_create(char *game_id)
{
	int i;

	printf("sr_bup_create: [%s]\n", game_id);

	for(i=1; i<4096; i++){
		if(*(u32*)(bup_buf+i*16)==0)
			break;
		if(strncmp((char*)bup_buf+i*16, game_id, 16)==0)
			return 0;
	}

	memcpy(bup_buf+i*16, game_id, 16);

	bup_mem = bup_buf+i*0x10000;
	sr_bup_format((u8*)game_id);

	return 0x10000;
}


int sr_bup_list(int slot_id)
{
	int block, i, dsize, bsize;
	u8 *bp;

	if(slot_id==-1){
		// List all slot
		for(i=1; i<4096; i++){
			char sname[20];
			if(*(u32*)(bup_buf+i*16)==0)
				break;
			memcpy(sname, bup_buf+i*16, 16);
			sname[16] = 0;
			printf("Slot_%-2d: [%s]\n", i, sname);
		}
	}else{
		// List saves in slot
		if(sr_bup_select(slot_id)<0)
			return -1;

		i = 0;
		block = get_be16(&BUPMEM->first_save);
		while(block){
			bp = get_block_addr(block);

			dsize = get_be32(bp+0x0c);
			bsize = (dsize+block_size-1)/(block_size);
			bsize += 1;

			printf("  Save_%-3d: %s  size=%5x  block=%d\n", i, bp, dsize, bsize);
			block = get_be16(bp+0x3e);
			i += 1;
		}
	}
	printf("\n");

	printf(" Total block: %4d   Free block: %d\n\n", 512, free_block);
	return 0;
}


int sr_bup_init(u8 *bbuf)
{
	bup_buf = bbuf;
	return 0;
}


/******************************************************************************/

