
#include "main.h"
#include "smpc.h"



/******************************************************************************/

#define BUP_WORK_ADDR 0x06000354
#define BUP_MEM_ADDR  (SAVINFO_ADDR)

#define MEMS_HEADER   (SAVINFO_ADDR+0x10000)
#define MEMS_TMP      (SAVINFO_ADDR+0x12000)
#define MEMS_BUFFER   (SAVINFO_ADDR+0x13000)

#define START_BLOCK   0x8000

#define	BUP_NON                 (1)
#define	BUP_UNFORMAT            (2)
#define	BUP_WRITE_PROTECT       (3)
#define	BUP_NOT_ENOUGH_MEMORY   (4)
#define	BUP_NOT_FOUND           (5)
#define	BUP_FOUND               (6)
#define	BUP_NO_MATCH            (7)
#define	BUP_BROKEN              (8)


typedef struct {
	u16 unit_id;
	u16 partition;
}BUPCFG;

typedef struct {
	u32 total_size;
	u32 total_block;
	u32 block_size;
	u32 free_size;
	u32 free_block;
	u32 data_num;
}BUPSTAT;

typedef struct {
	char file_name[12];
	char comment[11];
	char language;
	u32  date;
	u32  data_size;
	u16  block_size;
}BUPDIR;

typedef struct {
	u8 year;
	u8 month;
	u8 day;
	u8 time;
	u8 min;
	u8 week;
}BUPDATE;


static int bup_dev;

// 64k buffer at 220c4000, 64 blocks
static int mblk_start;
static int mblk_end;
static int mblk_bmp[2];
// header at 220c1000, 8192 bytes
// save_hdr at 220c3000, 1024 bytes
static int msav_blk;


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

#define BUPMEM  ((volatile BUPHEADER*)(BUP_MEM_ADDR))


typedef struct {
	u32 magic0;
	u32 magic1;
	u32 total_size;
	u16 free_block;
	u16 first_save;
	u8  bitmap[1024-16];
}MEMSHEADER;

#define MEMS  ((volatile MEMSHEADER*)(MEMS_HEADER))


/******************************************************************************/

#define MEMS_HDR 1
#define MEMS_SAV 2
#define MEMS_BUF 4
#define MEMS_ALL 7
#define BUPSAVE  8

static void bup_update(int flag)
{
	int cmd = 0;
	int arg = 0;

	if(flag==BUPSAVE){
		cmd = SSCMD_SSAVE;
	}else{
		arg = flag;
		cmd = SSCMD_SMEMS;
	}

	SS_ARG = arg;
	SS_CMD = cmd;
	while(SS_CMD);
}


static void mems_load(int blk)
{
	//printk("mems_load %04x\n", blk);
	SS_ARG = blk;
	SS_CMD = SSCMD_LMEMS;
	while(SS_CMD);
}


static void set_bitmap(u8 *bmp, int index, int val)
{
	int byte = index/8;
	int mask = 1<<(index&7);

	if(val)
		bmp[byte] |= mask;
	else
		bmp[byte] &= ~mask;
}


static int get_free_block(int pos)
{
	int i, byte, mask, bsize;
	u8 *bmp;
	u16 *free_block;

	if(bup_dev==0){
		bmp = (u8*)BUPMEM->bitmap;
		free_block = (u16*)&BUPMEM->free_block;
		bsize = 512;
	}else{
		bmp = (u8*)MEMS->bitmap;
		free_block = (u16*)&MEMS->free_block;
		bsize = 8064;
	}

	for(i=pos; i<bsize; i++){
		byte = i/8;
		mask = 1<<(i&7);
		if( (bmp[byte]&mask) == 0 ){
			bmp[byte] |= mask;
			*free_block -= 1;
			return i;
		}
	}

	return 0;
}


static u8 *get_block_addr(int id)
{
	int is_hdr = id&0x8000;
	id &= 0x7fff;

	if(bup_dev==0){
		return (u8*)(BUP_MEM_ADDR + id*128);
	}

	if(is_hdr){
		if(id!=msav_blk){
			mems_load(0x8000|id);
			msav_blk = id;
		}
		return (u8*)(MEMS_HEADER+0x2000);
	}

	if(id<mblk_start || id>mblk_end){
		if(mblk_bmp[0] || mblk_bmp[1]){
			bup_update(MEMS_BUF);
		}
		mems_load(id);
		mblk_start = id;
		mblk_end = id+63;
		mblk_bmp[0] = 0;
		mblk_bmp[1] = 0;
	}
	return (u8*)(MEMS_BUFFER+(id-mblk_start)*1024);
}


static int get_next_block(u8 *bmp, int pos)
{
	int byte, mask;

	if(bup_dev==1){
		return *(u16*)bmp;
	}

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


static void mbuf_mark(int id)
{
	if(bup_dev==0)
		return;

	id -= mblk_start;
	if(id<32){
		mblk_bmp[0] |= 1<<id;
	}else{
		mblk_bmp[1] |= 1<<(id-32);
	}
}

#ifdef BUP_CHECK
static int chksum(u8 *data, int size)
{
	int sum = 0;

	while(size>0){
		sum += *(u16*)data;
		size -= 2;
		data += 2;
	}

	return sum;
}
#endif

static int access_data(int block, u8 *data, int type)
{
	u8 *bp, *bmp;
	int dsize, asize, bsize;

	bsize = (bup_dev==0) ? 128 : 1024;

	bp = get_block_addr(START_BLOCK|block);
	dsize = *(u32*)(bp+0x0c);

	if(bup_dev==1 && dsize<=960){
		if(type==0){ // read
			memcpy(data, bp+0x40, dsize);
		}else if(type==1){
			if(memcmp(data, bp+0x40, dsize)){
				return BUP_NO_MATCH;
			}
		}else{       // write
			memcpy(bp+0x40, data, dsize);
		}

		return 0;
	}

	bmp = bp+0x40;
	block = 0;
	while(dsize>0){
		block = get_next_block(bmp, block);
		bp = get_block_addr(block);

		asize = (dsize>bsize)? bsize : dsize;
		//printk("dsize=%04x block=%04x asize=%04x", dsize, block, asize);

		if(type==0){ // read
			memcpy(data, bp, asize);
#ifdef BUP_CHECK
			printk("  sum=%08x\n", chksum(data, asize));
#endif
		}else if(type==1){
			if(memcmp(data, bp, asize)){
				return BUP_NO_MATCH;
			}
		}else{       // write
#ifdef BUP_CHECK
			printk("  sum=%08x\n", chksum(data, asize));
#endif
			memcpy(bp, data, asize);
			mbuf_mark(block);
		}
		data += asize;
		dsize -= asize;
		block += 1;
		if(bup_dev==1)
			bmp += 2;
	}

	return 0;
}


static int find_save(char *file_name, int offset, int *last)
{
	int block, len, index;
	u8 *bp;

	len = strlen(file_name);
	if(len>11)
		len = 11;

	index = 0;

	if(bup_dev==0){
		block = BUPMEM->first_save;
		if(last) *last = 0;

		while(block){
			bp = get_block_addr(block);
			if(strncmp(file_name, (char*)bp, len)==0){
				if(index==offset){
					printk("  found at %04x\n", block);
					return block;
				}
				index += 1;
			}
			if(last)
				*last = block;
			block = *(u16*)(bp+0x3e);
		}
	}else{
		if(last) *last = -1;

		for(int i=0; i<448; i++){
			bp = (u8*)(MEMS_HEADER+1024+i*16);
			if(bp[0]==0 && last && *last==-1){
				*last = i;
			}
			if(bp[0] && strncmp(file_name, (char*)bp, len)==0){
				if(index==offset){
					block = *(u16*)(bp+0x0e);
					printk("  found at %04x\n", block);
					if(last)
						*last = i;
					return block;
				}
				index += 1;
			}
		}
	}

	printk("  not found!\n");
	if(last) printk("    last=%04x\n", *last);
	return -1;
}


/******************************************************************************/


// 根据BIOS的BUP代码，只有dev为2时才检测num。
// According to BIOS BUP code, only check num when dev is 2.
int sro_bup_sel_part(int dev, int num)
{
	printk("bup_sel_part(%d): %d\n", dev, num);

	if(dev>=2)
		return BUP_NON;

	return 0;
}


int sro_bup_format(int dev)
{
	printk("bup_format(%d)\n", dev);

	if(dev>1)
		return BUP_NON;

	if(dev==0){
		memset((u8*)BUP_MEM_ADDR, 0, 0x8000);
		// SaroSave
		BUPMEM->magic0 = 0x5361726f;
		BUPMEM->magic1 = 0x53617665;
		BUPMEM->total_size = 0x10000;
		BUPMEM->block_size = 0x80;
		memcpy(&BUPMEM->gameid, (u8*)0x06000c20, 16);
		memset(&BUPMEM->bitmap, 0, 0x40);
		BUPMEM->first_save = 0x0000;
		BUPMEM->free_block = 512-1;
		BUPMEM->bitmap[0] = 0x01;

		bup_update(BUPSAVE);
	}else{
		memset((u8*)MEMS_HEADER, 0, 0x2000);
		// SaroMems
		MEMS->magic0 = 0x5361726f;
		MEMS->magic1 = 0x4d656d73;
		MEMS->total_size = 0x00800000;
		MEMS->free_block = 1008*8-8;
		MEMS->bitmap[0] = 0xff;

		bup_update(MEMS_HDR);
	}

	return 0;
}


int sro_bup_stat(int dev, int dsize, BUPSTAT *stat)
{
	printk("bup_stat(%d): dsize=%d\n", dev, dsize);

	if(dev>1)
		return BUP_NON;

	if(dev==0){
		if(BUPMEM->magic0!=0x5361726f || BUPMEM->magic1!=0x53617665)
			return BUP_UNFORMAT;

		stat->total_size = BUPMEM->total_size;
		stat->block_size = BUPMEM->block_size;
		stat->free_block = BUPMEM->free_block;
		stat->total_block = stat->total_size / stat->block_size;
		stat->free_size = (stat->free_block-1) * stat->block_size;
		stat->data_num = stat->free_block/2;
	}else{
		if(MEMS->magic0!=0x5361726f || MEMS->magic1!=0x4d656d73)
			return BUP_UNFORMAT;

		stat->total_size = MEMS->total_size;
		stat->block_size = 1024;
		stat->free_block = MEMS->free_block;
		stat->total_block = 1008*8;
		stat->free_size = (stat->free_block-1)*1024;
		stat->data_num = stat->free_block/2;
	}

	return 0;
}


int sro_bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
	int block_need, block, hdr, last, i;
	u8 *bp;

	printk("bup_write(%d): %s data=%08x dsize=%08x\n", dev, dir->file_name, data, dir->data_size);
	bup_dev = dev;
	if(dev>1)
		return BUP_NON;

	// 计算所需的块数量。
// Calculate the number of blocks needed.
	int block_size = (dev==0) ? 128 : 1024;
	if(dev==1 && (dir->data_size < (1024-64))){
		block_need = 0;
	}else{
		block_need = (dir->data_size+block_size-1)/block_size;
	}

	// 1. 根据dir查找存档
// 1. Find save according to dir
	block = find_save(dir->file_name, 0, &last);

	// 2. 如果找到，且允许覆盖，则覆盖写入
// 2. If found and overwrite is allowed, overwrite
	if(block>0){
		if(mode)
			return BUP_FOUND;

		bp = get_block_addr(START_BLOCK|block);
		*(u32*)(bp+0x1c) = dir->date;
		access_data(block, data, 2);

		if(dev==0){
			bup_update(BUPSAVE);
		}else{
			bup_update((block_need)? MEMS_SAV|MEMS_BUF : MEMS_SAV);
		}
		printk("Over write.\n");
		return 0;
	}

	// 3. 未找到。新建存档。
// 3. Not found. Create new save.
	int free_block = (dev==0) ? BUPMEM->free_block: *(u16*)(MEMS_HEADER+0x0c);
	printk("block_need=%d\n", block_need+1);
	if((block_need+1) > free_block){
		return BUP_NOT_ENOUGH_MEMORY;
	}

	// 分配起始块
// Allocate starting block
	block = get_free_block(0);
	hdr = block;

	bp = get_block_addr(START_BLOCK|hdr);
	printk("start at %04x %08x\n", hdr, bp);

	// 写开始块
// Write starting block
	memset(bp, 0, block_size);
	memcpy(bp+0x00, dir->file_name, 11);
	*(u32*)(bp+0x0c) = dir->data_size;
	memcpy(bp+0x10, dir->comment, 10);
	bp[0x1b] = dir->language;
	*(u32*)(bp+0x1c) = dir->date;

	// 分配块
// Allocate blocks
	block = 0;
	for(i=0; i<block_need; i++){
		block = get_free_block(block);
		if(dev==0){
			set_bitmap(bp+0x40, block, 1);
		}else{
			*(u16*)(bp+0x40+i*2) = block;
		}
		block += 1;
	}

	// 写数据
// Write data
	access_data(hdr, data, 2);

	// 更新last指针
// Update last pointer
	if(dev==1){
		memcpy((u8*)MEMS_HEADER+0x400+last*16, dir->file_name, 11);
		*(u16*)(MEMS_HEADER+0x400+last*16+0x0e) = hdr;
	}else{
		printk("Link save: %04x -> %04x\n", last, hdr);
		bp = get_block_addr(last);
		*(u16*)(bp+0x3e) = hdr;
	}

	if(dev==0){
		bup_update(BUPSAVE);
	}else{
		bup_update((block_need)? MEMS_ALL: MEMS_HDR|MEMS_SAV);
	}

	printk("Write done.\n");
	return 0;
}


int sro_bup_read(int dev, char *file_name, u8 *data)
{
	int block;
	char nbuf[12];

	memcpy(nbuf, file_name, 11);
	nbuf[11] = 0;
	file_name = nbuf;

	printk("bup_read(%d): %s  data=%08x\n", dev, file_name, data);
	bup_dev = dev;
	if(dev>1)
		return BUP_NON;

	block = find_save(file_name, 0, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	access_data(block, data, 0);

	return 0;
}


int sro_bup_delete(int dev, char *file_name)
{
	int block, last, has_data;
	u8 *bp;
	char nbuf[12];

	memcpy(nbuf, file_name, 11);
	nbuf[11] = 0;
	file_name = nbuf;

	printk("bup_delete(%d): %s\n", dev, file_name);
	bup_dev = dev;
	if(dev>1)
		return BUP_NON;

	block = find_save(file_name, 0, &last);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	bp = get_block_addr(START_BLOCK|block);

	has_data = 1;
	// 释放开始块
// Release starting block
	if(dev==0){
		set_bitmap((u8*)BUPMEM->bitmap, block, 0);
		BUPMEM->free_block += 1;
	}else{
		set_bitmap((u8*)MEMS->bitmap, block, 0);
		MEMS->free_block += 1;
		int dsize = *(u32*)(bp+0x0c);
		if(dsize<=960)
			has_data = 0;
	}

// 释放数据块
// Release data blocks
	if(has_data){
		u8 *bmp = bp+0x40;
		block = 0;
		while(1){
			block = get_next_block(bmp, block);
			if(block==0)
				break;
			if(dev==0){
				set_bitmap((u8*)BUPMEM->bitmap, block, 0);
				BUPMEM->free_block += 1;
			}else{
				set_bitmap((u8*)MEMS->bitmap, block, 0);
				MEMS->free_block += 1;
			}
			if(dev==1){
				bmp += 2;
			}else{
				block += 1;
			}
		}
	}

	// 更新last指针
// Update last pointer
	if(dev==1){
		memset((u8*)MEMS_HEADER+1024+last*16, 0, 16);
	}else{
		u8 *last_bp = get_block_addr(last);
		*(u16*)(last_bp+0x3e) = *(u16*)(bp+0x3e);
	}

	if(dev==0){
		bup_update(BUPSAVE);
	}else{
		bup_update(MEMS_HDR);
	}

	return 0;
}


int sro_bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
	int block, fnum;
	char nbuf[12];
	u8 *bp;

	memcpy(nbuf, file_name, 11);
	nbuf[11] = 0;
	printk("bup_dir(%d): %s  %d\n", dev, nbuf, tbsize);

	bup_dev = dev;
	if(dev>1)
		return 0;

	int block_size = (dev==0) ? 128 : 1024;

	fnum = 0;
	while(1){
		block = find_save(file_name, fnum, NULL);
		if(block<0){
			break;
		}

		if(fnum<tbsize){
			bp = get_block_addr(START_BLOCK|block);
			memcpy(dir->file_name, (char*)(bp+0), 12);
			dir->data_size = *(u32*)(bp+0x0c);
			memcpy(dir->comment, bp+0x10, 11);
			dir->language = bp[0x1b];
			dir->date = *(u32*)(bp+0x1c);
			dir->block_size = (dir->data_size + block_size-1)/block_size;

			dir += 1;
		}
		fnum += 1;
	}

	if(fnum>tbsize){
		return -fnum;
	}
	return fnum;
}


int sro_bup_verify(int dev, char *file_name, u8 *data)
{
	int block;
	char nbuf[12];

	memcpy(nbuf, file_name, 11);
	nbuf[11] = 0;
	file_name = nbuf;

	printk("bup_verify(%d): %s\n", dev, file_name);
	bup_dev = dev;
	if(dev>1)
		return BUP_NON;

	block = find_save(file_name, 0, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	return access_data(block, data, 1);
}


static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void sro_bup_get_date(u32 date, BUPDATE *bdt)
{
	int y, d, h, m;
	printk("bup_get_date: %08x\n", date);

	d = date/(24*60); date -= d*24*60;
	h = date/60; date -= h*60;

	y = d/(365*4+1); d -= y*(365*4+1);
	m = d/365; d -= m*365;
	if(m==3){
		if(d==0){
			m = 2;
			d = 365;
		}else{
			d -= 1;
		}
	}
	y = y*4+m;

	mdays[1] = (m==2)? 29: 28;
	m = 0;
	while(d>mdays[m]){
		d -= mdays[m];
		m += 1;
	}
	m += 1;
	d += 1;

	bdt->year = y;
	bdt->month = m;
	bdt->day = d;
	bdt->time = h;
	bdt->min = date;

	printk("  : %2d-%2d-%2d %2d:%2d\n",
		bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);
}


u32 sro_bup_set_date(BUPDATE *bdt)
{
	u32 date;
	int y, m ,rem;

	printk("bup_set_date: %2d-%2d-%2d %2d:%2d\n",
		bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);

	y = bdt->year;
	date = (y/4)*0x5b5;
	rem = y%4;
	if(rem){
		date += rem*0x16d+1;
	}

	mdays[1] = (rem==0)? 29: 28;
	m = bdt->month;
	while(m>1){
		date += mdays[m-2];
		m -= 1;
	}
	date += bdt->day;
	date *= 1440;

	date += (bdt->time)*0x3c;
	date += bdt->min;

	printk("  date=%08x\n", date);
	return date;
}


/******************************************************************************/


int use_sys_bup = 0;


int (*sys_bup_sel_part)(int dev, int num);
int (*sys_bup_format)(int dev);
int (*sys_bup_stat)(int dev, int dsize, BUPSTAT *stat);
int (*sys_bup_write)(int dev, BUPDIR *dir, u8 *data, int mode);
int (*sys_bup_read)(int dev, char *file_name, u8 *data);
int (*sys_bup_delete)(int dev, char *file_name);
int (*sys_bup_dir)(int dev, char *file_name, int tbsize, BUPDIR *dir);
int (*sys_bup_verify)(int dev, char *file_name, u8 *data);
void (*sys_bup_get_date)(u32 date, BUPDATE *bdt);
u32 (*sys_bup_set_date)(BUPDATE *bdt);


int bup_sel_part(int dev, int num)
{
	if(use_sys_bup && dev==0){
		return sys_bup_sel_part(dev, num);
	}else{
		return sro_bup_sel_part(dev, num);
	}
}


int bup_format(int dev)
{
	if(use_sys_bup && dev==0){
		return sys_bup_format(dev);
	}else{
		return sro_bup_format(dev);
	}
}


int bup_stat(int dev, int dsize, BUPSTAT *stat)
{
	if(use_sys_bup && dev==0){
		return sys_bup_stat(dev, dsize, stat);
	}else{
		return sro_bup_stat(dev, dsize, stat);
	}
}


int bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
	if(use_sys_bup && dev==0){
		return sys_bup_write(dev, dir, data, mode);
	}else{
		return sro_bup_write(dev, dir, data, mode);
	}
}


int bup_read(int dev, char *file_name, u8 *data)
{
	if(use_sys_bup && dev==0){
		return sys_bup_read(dev, file_name, data);
	}else{
		return sro_bup_read(dev, file_name, data);
	}
}


int bup_delete(int dev, char *file_name)
{
	if(use_sys_bup && dev==0){
		return sys_bup_delete(dev, file_name);
	}else{
		return sro_bup_delete(dev, file_name);
	}
}


int bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
	if(use_sys_bup && dev==0){
		return sys_bup_dir(dev, file_name, tbsize, dir);
	}else{
		return sro_bup_dir(dev, file_name, tbsize, dir);
	}
}


int bup_verify(int dev, char *file_name, u8 *data)
{
	if(use_sys_bup && dev==0){
		return sys_bup_verify(dev, file_name, data);
	}else{
		return sro_bup_verify(dev, file_name, data);
	}
}


void bup_get_date(u32 date, BUPDATE *bdt)
{
	if(use_sys_bup){
		return sys_bup_get_date(date, bdt);
	}else{
		return sro_bup_get_date(date, bdt);
	}
}


u32 bup_set_date(BUPDATE *bdt)
{
	if(use_sys_bup){
		return sys_bup_set_date(bdt);
	}else{
		return sro_bup_set_date(bdt);
	}
}


/******************************************************************************/


void bup_init(u8 *lib_addr, u8 *work_addr, void *cfg_ptr)
{
	BUPCFG *cfg = (BUPCFG*)cfg_ptr;

	if(debug_flag&1)
		sci_init();

	printk("bup_init: lib=%08x work=%08x\n", lib_addr, work_addr);

	if(use_sys_bup){
		printk("sys_bup_init ... %08x\n", sys_bup_init);
		sys_bup_init(lib_addr, work_addr, cfg_ptr);
		printk("done.\n");

		sys_bup_sel_part = (void*)*(u32*)(work_addr+0x04);
		sys_bup_format   = (void*)*(u32*)(work_addr+0x08);
		sys_bup_stat     = (void*)*(u32*)(work_addr+0x0c);
		sys_bup_write    = (void*)*(u32*)(work_addr+0x10);
		sys_bup_read     = (void*)*(u32*)(work_addr+0x14);
		sys_bup_delete   = (void*)*(u32*)(work_addr+0x18);
		sys_bup_dir      = (void*)*(u32*)(work_addr+0x1c);
		sys_bup_verify   = (void*)*(u32*)(work_addr+0x20);
		sys_bup_get_date = (void*)*(u32*)(work_addr+0x24);
		sys_bup_set_date = (void*)*(u32*)(work_addr+0x28);
	}else{
		*(u32*)(BUP_WORK_ADDR) = (u32)work_addr;
		*(u32*)(work_addr+0x00) = (u32)lib_addr;
		cfg[0].unit_id = 1;
		cfg[0].partition = 1;
	}

	*(u32*)(work_addr+0x04) = (u32)bup_sel_part;
	*(u32*)(work_addr+0x08) = (u32)bup_format;
	*(u32*)(work_addr+0x0c) = (u32)bup_stat;
	*(u32*)(work_addr+0x10) = (u32)bup_write;
	*(u32*)(work_addr+0x14) = (u32)bup_read;
	*(u32*)(work_addr+0x18) = (u32)bup_delete;
	*(u32*)(work_addr+0x1c) = (u32)bup_dir;
	*(u32*)(work_addr+0x20) = (u32)bup_verify;
	*(u32*)(work_addr+0x24) = (u32)bup_get_date;
	*(u32*)(work_addr+0x28) = (u32)bup_set_date;

	cfg[1].unit_id = 2;
	cfg[1].partition = 1;
	cfg[2].unit_id = 0;
	cfg[2].partition = 0;

	if(use_sys_bup==0){
		// Check "SaroSave"
		if(BUPMEM->magic0!=0x5361726f || BUPMEM->magic1!=0x53617665){
			printk("  empty bup memory, need format.\n");
			bup_format(0);
		}
	}

	// Check "SaroMems"
	if(MEMS->magic0!=0x5361726f || MEMS->magic1!=0x4d656d73){
		printk("  empty memory card, need format.\n");
		bup_format(1);
	}

}


/******************************************************************************/




