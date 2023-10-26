
#include "main.h"
#include "smpc.h"



/******************************************************************************/

#define BUP_WORK_ADDR 0x06000354
#define BUP_MEM_ADDR  0x020b0000

#define GET_BUP_INFO()  (BUPINFO*) ( (*(u32*)BUP_WORK_ADDR) + 0x0100 )

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


/******************************************************************************/


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
	int i, byte, mask;

	for(i=pos; i<512; i++){
		byte = i/8;
		mask = 1<<(i&7);
		if( (BUPMEM->bitmap[byte]&mask) == 0 ){
			BUPMEM->bitmap[byte] |= mask;
			BUPMEM->free_block -= 1;
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
	return (u8*)(BUP_MEM_ADDR + BUPMEM->block_size*id);
}


static int access_data(int block, u8 *data, int type)
{
	u8 *bp, *bmp;
	int dsize, asize, bsize;

	bsize = BUPMEM->block_size;

	bp = get_block_addr(block);
	bmp = bp+0x40;
	dsize = *(u32*)(bp+0x0c);
	block = 0;

	while(dsize>0){
		block = get_next_block(bmp, block);
		bp = get_block_addr(block);

		asize = (dsize>bsize)? bsize : dsize;
		//printk("dsize=%04x block=%04x asize=%04x\n", dsize, block, asize);

		if(type==0){
			// read
			memcpy(data, bp, asize);
		}else if(type==1){
			if(memcmp(data, bp, asize)){
				return BUP_NO_MATCH;
			}
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


static int find_save(char *file_name, int offset, int *last)
{
	int block, len, index;
	u8 *bp;

	len = strlen(file_name);
	if(len>11)
		len = 11;
	block = BUPMEM->first_save;
	index = 0;
	if(last) *last = 0;

	while(block){
		bp = get_block_addr(block);
		if(strncmp(file_name, (char*)bp, len)==0){
			if(index<offset){
				index += 1;
				goto _next;
			}
			printk("  found at %d\n", block);
			return block;
		}
_next:
		if(last) *last = block;
		block = *(u16*)(bp+0x3e);
	}

	printk("  not found!\n");
	if(last) printk("    last=%04x\n", *last);
	return -1;
}


/******************************************************************************/


int bup_sel_part(int dev, int num)
{
	printk("bup_sel_part(%d): %d\n", dev, num);
	if(dev || num)
		return BUP_NON;

	return 0;
}


int bup_format(int dev)
{
	printk("bup_format(%d)\n", dev);
	if(dev>0)
		return BUP_NON;

	memset((u8*)BUP_MEM_ADDR, 0, 0x8000);

	// SaroSave
	BUPMEM->magic0 = 0x5361726f;
	BUPMEM->magic1 = 0x53617665;
	BUPMEM->total_size = 0x10000;
	BUPMEM->block_size = 0x80;
	memcpy(&BUPMEM->gameid, (u8*)0x06000c20, 16);
	memset(&BUPMEM->bitmap, 0, 0x40);
	BUPMEM->first_save = 0x0000;

	if(BUPMEM->block_size == 0x0040){
		// 32K块
		BUPMEM->free_block = 512-2;
		BUPMEM->bitmap[0] = 0x03;
	}else{
		BUPMEM->free_block = 512-1;
		BUPMEM->bitmap[0] = 0x01;
	}

	SS_CMD = SSCMD_SSAVE;
	while(SS_CMD);

	return 0;
}


int bup_stat(int dev, int dsize, BUPSTAT *stat)
{
	printk("bup_stat(%d): dsize=%d\n", dev, dsize);

	if(dev>0)
		return BUP_NON;
	if(BUPMEM->magic0!=0x5361726f || BUPMEM->magic1!=0x53617665)
		return BUP_UNFORMAT;

	stat->total_size = BUPMEM->total_size;
	stat->block_size = BUPMEM->block_size;
	stat->free_block = BUPMEM->free_block;
	stat->total_block = stat->total_size / stat->block_size;

	int hsize = (stat->block_size==64)? 2 : 1;
	stat->free_size = (stat->free_block-hsize) * stat->block_size;
	stat->data_num = stat->free_block/(hsize+1);

	return 0;
}


int bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
	int block_need, block, hdr, last, i;
	u8 *bp;

	printk("bup_write(%d): %s data=%08x dsize=%08x\n", dev, dir->file_name, data, dir->data_size);
	if(dev>0)
		return BUP_NON;

	// 1. 根据dir查找存档
	block = find_save(dir->file_name, 0, &last);

	// 2. 如果找到，且允许覆盖，则覆盖写入
	if(block>0){
		if(mode)
			return BUP_FOUND;

		bp = get_block_addr(block);
		*(u32*)(bp+0x1c) = dir->date;
		access_data(block, data, 2);
		SS_CMD = SSCMD_SSAVE;
		while(SS_CMD);
		printk("Over write.\n");
		return 0;
	}

	// 3. 未找到。新建存档。
	int hsize = (BUPMEM->block_size==64)? 2 : 1;

	// 计算所需的块数量。起始块+块列表块+数据块
	block_need = (dir->data_size+BUPMEM->block_size-1)/(BUPMEM->block_size);
	printk("block_need=%d\n", block_need+hsize);
	if((block_need+hsize) > BUPMEM->free_block){
		return BUP_NOT_ENOUGH_MEMORY;
	}

	// 分配起始块
	if(hsize==2){
		// 要分配两个连续的块
		int map_block = 0;
		while(1){
			block = get_free_block(map_block);
			map_block = get_free_block(block+1);
			if(map_block==block+1)
				break;
			set_bitmap((u8*)BUPMEM->bitmap, block, 0);
			set_bitmap((u8*)BUPMEM->bitmap, map_block, 0);
		}
	}else{
		block = get_free_block(0);
	}
	BUPMEM->free_block -= hsize;
	hdr = block;

	bp = get_block_addr(hdr);
	printk("start at %d %08x\n", hdr, bp);

	// 写开始块
	memset(bp, 0, 64*2);
	memcpy(bp+0x00, dir->file_name, 11);
	*(u32*)(bp+0x0c) = dir->data_size;
	memcpy(bp+0x10, dir->comment, 10);
	bp[0x1b] = dir->language;
	*(u32*)(bp+0x1c) = dir->date;

	// 分配块
	block = 0;
	for(i=0; i<block_need; i++){
		block = get_free_block(block);
		set_bitmap(bp+0x40, block, 1);
		block += 1;
	}

	// 写数据
	access_data(hdr, data, 2);

	bp = get_block_addr(last);
	*(u16*)(bp+0x3e) = hdr;

	SS_CMD = SSCMD_SSAVE;
	while(SS_CMD);

	printk("Write done.\n");
	return 0;
}


int bup_read(int dev, char *file_name, u8 *data)
{
	int block;

	printk("bup_read(%d): %s  data=%08x\n", dev, file_name, data);
	if(dev>0)
		return BUP_NON;

	block = find_save(file_name, 0, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	access_data(block, data, 0);

	return 0;
}


int bup_delete(int dev, char *file_name)
{
	int block, i, last;
	u8 *bp;

	printk("bup_delete(%d): %s\n", dev, file_name);
	if(dev>0)
		return BUP_NON;

	block = find_save(file_name, 0, &last);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	int hsize = (BUPMEM->block_size==64)? 2 : 1;

	set_bitmap((u8*)BUPMEM->bitmap, block, 0);
	BUPMEM->free_block += 1;
	if(hsize==2){
		set_bitmap((u8*)BUPMEM->bitmap, block+1, 0);
		BUPMEM->free_block += 1;
	}

	bp = get_block_addr(block);
	for(i=0; i<64; i++){
		BUPMEM->bitmap[i] &= ~bp[0x40+i];
	}
	u8 *last_bp = get_block_addr(last);
	*(u16*)(last_bp+0x3e) = *(u16*)(bp+0x3e);

	int bsize = (*(u32*)(bp+0x0c) + BUPMEM->block_size-1)/(BUPMEM->block_size);
	BUPMEM->free_block += bsize;

	SS_CMD = SSCMD_SSAVE;
	while(SS_CMD);

	return 0;
}


int bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
	int block, fnum;
	u8 *bp;

	printk("bup_dir(%d): %s  %d\n", dev, file_name, tbsize);
	if(dev>0)
		return BUP_NON;

	fnum = 0;
	while(1){
		block = find_save(file_name, fnum, NULL);
		if(block<0){
			break;
		}

		if(fnum<tbsize){
			bp = get_block_addr(block);
			memcpy(dir->file_name, (char*)(bp+0), 12);
			dir->data_size = *(u32*)(bp+0x0c);
			memcpy(dir->comment, bp+0x10, 11);
			dir->language = bp[0x1b];
			dir->date = *(u32*)(bp+0x1c);
			dir->block_size = (dir->data_size + BUPMEM->block_size-1)/(BUPMEM->block_size);

			dir += 1;
		}
		fnum += 1;
	}

	if(fnum>tbsize){
		return -fnum;
	}
	return fnum;
}


int bup_verify(int dev, char *file_name, u8 *data)
{
	int block;

	printk("bup_verify(%d): %s\n", dev, file_name);
	if(dev>0)
		return BUP_NON;

	block = find_save(file_name, 0, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	return access_data(block, data, 1);
}


static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void bup_get_date(u32 date, BUPDATE *bdt)
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


u32 bup_set_date(BUPDATE *bdt)
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


void bup_init(u8 *lib_addr, u8 *work_addr, BUPCFG *cfg)
{
	if(debug_flag&1)
		sci_init();

	printk("bup_init: lib=%08x work=%08x\n", lib_addr, work_addr);

	//memset(lib_addr, 0, 16384);
	//memset(work_addr, 0, 8192);

	*(u32*)(BUP_WORK_ADDR) = (u32)work_addr;

	*(u32*)(work_addr+0x00) = (u32)lib_addr;
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

	cfg[0].unit_id = 1;
	cfg[0].partition = 1;
	cfg[1].unit_id = 0;
	cfg[1].partition = 0;
	cfg[2].unit_id = 0;
	cfg[2].partition = 0;

	// Check "SaroSave"
	if(BUPMEM->magic0!=0x5361726f || BUPMEM->magic1!=0x53617665){
		printk("  empty bup memory, need format.\n");
		bup_format(0);
	}

}


/******************************************************************************/




