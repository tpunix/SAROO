
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bup.h"
#include "bup_format.h"


/******************************************************************************/

static u8 bup_mem[32768];
static int ss_bup_type;

typedef struct {
	u32 total_size;
	u32 total_block;
	u32 block_size;
	u8  block_map[64];

	// 描述某个存档的信息
	int dsize;      // 数据大小
	int bsize;      // 一共有多少块
	u16 blist[512];
	int nbp;        // blist中下一块的索引
	int dp;         // 块中的数据指针
}BUPINFO;


static BUPINFO bup_info;
#define GET_BUP_INFO()  (&bup_info)


/******************************************************************************/


static u8 *get_block_addr(BUPINFO *binfo, int id)
{
	return (u8*)(bup_mem + binfo->block_size*id);
}


static void load_blist(BUPINFO *binfo, int block)
{
	int nbp, dp, i;
	u8 *bp;

	binfo->blist[0] = block;
	binfo->bsize = 1;
	bp = get_block_addr(binfo, block);

	binfo->dsize = get_be32(bp+0x1e);
	if(binfo->dsize<=0x1e){
		// 数据可以放在起始块内
		binfo->nbp = 1;
		binfo->dp = 0x22;
		return;
	}else{
		nbp = 1;
		dp = 0x22;
		i = 1;
		while(1){
			block = get_be16(bp+dp);
			if(block==0)
				break;

			binfo->blist[i] = block;
			i += 1;
			dp += 2;
			if(dp==binfo->block_size){
				// 当前块用完了，取得块列表中下一块的地址。
				bp = get_block_addr(binfo, binfo->blist[nbp]);
				nbp += 1;
				dp = 4;
			}
		}
		dp += 2;
		if(dp==binfo->block_size){
			nbp += 1;
			dp = 4;
		}

		binfo->bsize = i;
		binfo->nbp = nbp;
		binfo->dp = dp;
	}
}


static int read_data(BUPINFO *binfo, u8 *data)
{
	u8 *bp;
	int dp, nbp, dsize;

	dsize = binfo->dsize;
	nbp = binfo->nbp;

	bp = get_block_addr(binfo, binfo->blist[nbp-1]);
	dp = binfo->dp;
	//printf("read_data: %d %d\n", binfo->blist[nbp-1], dp);
	while(dsize>0){
		*(u16*)data = *(u16*)(bp+dp);
		dp += 2;
		data += 2;
		dsize -= 2;
		if(dp==binfo->block_size){
			// 当前块用完了，取得块列表中下一块的地址。
			bp = get_block_addr(binfo, binfo->blist[nbp]);
			//printf("next block: %d %08x dsize=%04x\n", binfo->blist[nbp], bp, dsize);
			nbp += 1;
			dp = 4;
		}
	}

	return 0;
}


static void scan_save(BUPINFO *binfo)
{
	int i;
	u8 *bp;

	memset(binfo->block_map, 0, 64);
	binfo->block_map[0] = 0x03;

	for(i=2; i<binfo->total_block; i++){
		bp = get_block_addr(binfo, i);
		if(get_be32(bp)==0x80000000){
			// 找到了save起始块
			load_blist(binfo, i);
		}
	}
}


/******************************************************************************/


int ss_bup_export(int slot_id, int index, int exp_type)
{
	BUPINFO *binfo = GET_BUP_INFO();
	int block, i;
	u8 *bp;

	i = 0;
	for(block=2; block<binfo->total_block; block++){
		bp = get_block_addr(binfo, block);
		if(get_be32(bp)==0x80000000){
			// 找到了save起始块
			if(i == index){
				memset(save_buf, 0, 32768);
				strcpy((char*)save_buf, "SSAVERAW");
				memcpy(save_buf+0x10, bp+4, 11);    // Filename
				memcpy(save_buf+0x1c, bp+0x1e, 4);  // Size
				memcpy(save_buf+0x20, bp+0x10, 10); // Comment
				save_buf[0x2b] = bp[0x0f];          // Language
				memcpy(save_buf+0x2c, bp+0x1a, 4);  // Date

				load_blist(binfo, block);
				read_data(binfo, save_buf+0x40);

				char fname[16];
				int fsize = get_be32(save_buf+0x1c);
				put_be32(save_buf+0x0c, crc32b(save_buf+0x10, fsize+0x30));
				sprintf(fname, "%s%s", save_buf+0x10, exp_type ? BUP_EXTENSION : ".SRO");

				// if Export format is .BUP, set new header
				if (exp_type==1){
					vmem_bup_header_t bup_header = {0};
					
					memcpy(bup_header.magic, VMEM_MAGIC_STRING, VMEM_MAGIC_STRING_LEN);
					memcpy(bup_header.dir.filename, bp+4, 11);
					memcpy(bup_header.dir.comment, bp+0x10, 10);
					memcpy(&bup_header.dir.date, bp+0x1a, 4);
					memcpy(&bup_header.dir.datasize, bp+0x1e, 4);
					memcpy(&bup_header.date, bp+0x1a, 4);
					bup_header.dir.language = bp[0x0f];

					memcpy(save_buf, &bup_header, BUP_HEADER_SIZE);
				}

				write_file(fname, save_buf, fsize+0x40);

				printf("Export Save_%d: %s\n", index, fname);
				return 0;
			}
			i += 1;
		}
	}

	return -1;
}


int ss_bup_import(int slot_id, int save_id, char *save_name)
{
	return -1;
}


int ss_bup_delete(int slot_id, int save_id)
{
	return -1;
}


int ss_bup_create(char *game_id)
{
	return -1;
}


int ss_bup_list(int slot_id)
{
	BUPINFO *binfo = GET_BUP_INFO();
	int i, snum;
	u8 *bp;

	snum = 0;
	for(i=2; i<binfo->total_block; i++){
		bp = get_block_addr(binfo, i);
		if(get_be32(bp)==0x80000000){
			// 找到了save起始块
			printf("Save_%d: %s\n", snum, bp+4);
			snum += 1;
		}
	}

	return 0;
}


int ss_bup_init(u8 *bbuf)
{
	BUPINFO *binfo = GET_BUP_INFO();
	int i;

	if(bbuf[1]=='B' && bbuf[3]=='a' && bbuf[5]=='c' && bbuf[7]=='k'){
		ss_bup_type = 0;
		for(i=0; i<32768; i++){
			bup_mem[i] = bbuf[i*2+1];
		}
	}else if(strncmp((char*)bbuf, "BackUpRam Format", 16)==0){
		ss_bup_type = 1;
		memcpy(bup_mem, bbuf, 32768);
	}

	memset(binfo, 0, sizeof(BUPINFO));

	binfo->total_size = 0x8000;
	binfo->total_block = 512;
	binfo->block_size = 64;

	scan_save(binfo);

	return 0;
}


/******************************************************************************/


