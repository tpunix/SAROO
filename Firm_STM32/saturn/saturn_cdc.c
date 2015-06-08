
#include "../main.h"
#include "../fatfs/ff.h"
#include "cdc.h"


/******************************************************************************/

CDBLOCK cdb;

/******************************************************************************/
// 数据传输

BLOCK *alloc_block(void)
{
	int i, irqs;
	BLOCK *blk;

	blk = NULL;
	irqs = disable_irq();

	for(i=0; i<MAX_BLOCKS; i++){
		if(cdb.block[i].size==-1){
			cdb.block_free -= 1;

			cdb.block[i].size = cdb.sector_size;
			blk = &cdb.block[i];
			break;
		}
	}
			if(cdb.block_free==0){
				HIRQ = HIRQ_BFUL;
			}
	restore_irq(irqs);
	return blk;
}

void free_block(BLOCK *block)
{
	int irqs;

	irqs = disable_irq();
	block->size = -1;
	cdb.block_free += 1;
	restore_irq(irqs);

	if(cdb.play_wait==1 && cdb.block_free>16){
		cdb.play_wait = 0;
		if(in_isr())
			isr_sem_send(&sem_wait_disc);
		else
			os_sem_send(&sem_wait_disc);
	}
}


BLOCK *find_block(PARTITION *part, int index)
{
	BLOCK *p;
	int i, irqs;

	if(part->numblocks==0)
		return NULL;

	irqs = disable_irq();

	p = part->head;
	i = 0;

	while(p){
		if(i==index){
			restore_irq(irqs);
			return p;
		}
		i += 1;
		p = p->next;
	}

	restore_irq(irqs);
	return NULL;
}

void remove_block(PARTITION *part, int index)
{
	BLOCK *p, *prev;
	int i, irqs;

	irqs = disable_irq();

	prev = NULL;
	p = part->head;
	i = 0;

	while(p){
		if(i==index){
			if(prev==NULL){
				part->head = p->next;
			}else{
				prev->next = p->next;
			}
			if(part->tail==p){
				part->tail = prev;
			}
			part->size -= p->size;
			part->numblocks -= 1;
			free_block(p);
			break;
		}

		i += 1;
		prev = p;
		p = p->next;
	}

	restore_irq(irqs);
}

static int tcnt, min_num;

void trans_start(void)
{
	tcnt = 0;
	min_num = 40960;
	cdb.cdwnum = 0;

	if(cdb.trans_type==TRANS_TOC){
		BLK_ADDR = (u32)cdb.TOC&0x00ffffff;
		BLK_SIZE = 102*4;
	}
	if(cdb.trans_type==TRANS_FINFO_ALL){
		BLK_ADDR = (u32)cdb.FINFO&0x00ffffff;
		BLK_SIZE = cdb.cdir_pfnum*12;
	}
	if(cdb.trans_type==TRANS_FINFO_ONE){
		BLK_ADDR = (u32)cdb.FINFO&0x00ffffff;
		BLK_SIZE = 12;
	}
	if(cdb.trans_type==TRANS_SUBQ){
		BLK_ADDR = (u32)cdb.SUBH&0x00ffffff;
		BLK_SIZE = 10;
	}
	if(cdb.trans_type==TRANS_SUBRW){
		BLK_ADDR = (u32)cdb.SUBH&0x00ffffff;
		BLK_SIZE = 24;
	}

	if(cdb.trans_type==TRANS_DATA || cdb.trans_type==TRANS_DATA_DEL){
		PARTITION *pt = &cdb.part[cdb.trans_part_index];
		BLOCK *bt = find_block(pt, cdb.trans_block_start);
		u8 *dp;
		//printk("  trans pt:%d blk:%d\n", cdb.trans_part_index, cdb.trans_block_start);

		cdb.trans_bk = cdb.trans_block_start;

		if(cdb.sector_size==2352){
			dp = bt->data;
		}else if(cdb.sector_size==2340){
			dp = bt->data+12;
		}else if(cdb.sector_size==2336){
			dp = bt->data+16;
		}else{
			dp = bt->data+24;
		}

		BLK_ADDR = (u32)dp&0x00ffffff;
		BLK_SIZE = cdb.sector_size;

		cdb.trans_block = bt;
		cdb.trans_bk += 1;
	}

	ST_CTRL |= STIRQ_DAT;
	FIFO_CTRL = 0x0001;
	FIFO_CTRL = 0x0000;
	
	FIFO_RCNT = 0;
}


// 一个block的数据已经写入FIFO中
// 准备写入下一个block的数据，或者删除已经写完的block
void trans_handle(void)
{
	tcnt += 1;
	cdb.cdwnum += BLK_SIZE;

	if(cdb.trans_type==TRANS_DATA || cdb.trans_type==TRANS_DATA_DEL){
		if(cdb.trans_bk==cdb.trans_block_end){
			//printk("  trans end!\n");
			ST_CTRL &= ~STIRQ_DAT;
		}else{
			BLOCK *bt = cdb.trans_block->next;
			u8 *dp;
			int stat;

			if(cdb.sector_size==2352){
				dp = bt->data;
			}else if(cdb.sector_size==2340){
				dp = bt->data+12;
			}else if(cdb.sector_size==2336){
				dp = bt->data+16;
			}else{
				dp = bt->data+24;
			}

			BLK_ADDR = ((u32)dp)&0x00ffffff;
			BLK_SIZE = cdb.sector_size;

			cdb.trans_block = bt;
			cdb.trans_bk += 1;

			FIFO_CTRL = 0x0001;
			FIFO_CTRL = 0x0000;

			stat = FIFO_STAT;
			stat &= 0x0fff;
			if(stat<min_num)
				min_num = stat;
		}
	}else{
		ST_CTRL &= ~STIRQ_DAT;
	}
}


/******************************************************************************/

void set_report(u8 status)
{
	int irqs;

	irqs = disable_irq();

	SSCR1 = (status<<8) | ((cdb.options&0x0f)<<4) | (cdb.repcnt&0x0f);
	SSCR2 = (cdb.ctrladdr<<8) | cdb.track;
	SSCR3 = (cdb.index<<8) | (cdb.fad>>16)&0xff;
	SSCR4 = cdb.fad&0xffff;

	restore_irq(irqs);
}


int filter_sector(BLOCK *wblk)
{
	PARTITION *pt;
	FILTER *ft;
	BLOCK *blk;

	int retv, irqs;

	if(cdb.cddev_filter==0xff){
		return 1;
	}
	ft = &cdb.filter[cdb.cddev_filter];
	
	//printk("filter %08x ...\n", wblk->fad);

	while(1){
		retv = 1;
		
		if(wblk->data[0x0f]==2){
			if(ft->mode&0x01){
				if(wblk->fn!=ft->fid)
					retv = 0;
			}
			if(ft->mode&0x02){
				if(wblk->cn!=ft->chan)
					retv = 0;
			}
			if(ft->mode&0x04){
				if((wblk->sm&ft->smmask)!=ft->smval)
					retv = 0;
			}
			if(ft->mode&0x08){
				if((wblk->ci&ft->cimask)!=ft->cival)
					retv = 0;
			}
			if(ft->mode&0x10){
				retv ^= 1;
			}
		}

		if(ft->mode&0x40){
			if(wblk->fad<ft->fad || wblk->fad>=(ft->fad+ft->range))
				retv = 0;
		}

		if(retv==1){
			cdb.last_buffer = ft->c_true;
			break;
		}else{
			if(ft->c_false==0xff){
				printk("  c_false is FF!\n");
				return 1;
			}
			ft = &cdb.filter[ft->c_false];
		}
	}

	//printk("  ft->pt: %02x\n", ft->c_true);
	pt = &cdb.part[ft->c_true];

	blk = alloc_block();
	if(blk==NULL){
		return 2;
	}

	blk->size = cdb.sector_size;
	blk->fad  = wblk->fad;
	blk->fn   = wblk->fn;
	blk->cn   = wblk->cn;
	blk->sm   = wblk->sm;
	blk->ci   = wblk->ci;

	if(wblk->size==2048){
		memset(blk->data, 0, 24);
		memcpy32(blk->data+24, wblk->data, 2048);
	}else if(wblk->size==2352){
		if(wblk->data[15]==1){
			// MODE1
			memcpy32(blk->data, wblk->data, 16);
			memset(blk->data+16, 0, 8);
			memcpy32(blk->data+24, wblk->data+16, 2048);
		}else{
			// MODE2
			memcpy32(blk->data, wblk->data, 2352);
		}
	}
	//printk("  blk copy data.\n");

	irqs = disable_irq();

	if(pt->size==-1)
		pt->size = 0;

	// 疑问: partition中的size, 是按wblk的size算,还是按sector_size算?
	// 已经确认,是按设置的sector_size算.
	pt->size += blk->size;
	pt->numblocks += 1;

	if(pt->head==NULL){
		pt->head = blk;
		pt->tail = blk;
	}else{
		pt->tail->next = blk;
		pt->tail = blk;
	}
	blk->next = NULL;

	restore_irq(irqs);
	//printk("  filter done.\n");
	return 0;
}


/******************************************************************************/
// 普通命令

u16 old_cr1, old_cr2, old_cr3, old_cr4;

// 0x00 [SR]
int get_cd_status(void)
{
	set_report(cdb.status);

	if(old_cr1!=RESP1 || old_cr2!=RESP2 || old_cr3!=RESP3 || old_cr4!=RESP4){
		//printk("SS_CDC: CR1=%04x CR2=%04x CR3=%04x CR4=%04x  get_cd_status\n", RESP1, RESP2, RESP3, RESP4);
	}
	old_cr1 = RESP1;
	old_cr2 = RESP2;
	old_cr3 = RESP3;
	old_cr4 = RESP4;

	return 0;
}

// 0x01 [S-]
int get_hw_info(void)
{
	SSCR1 = (cdb.status<<8);
	SSCR2 = 0x0001;
	SSCR3 = 0x0000;
	SSCR4 = 0x0400;

	return 0;
}

// 0x02 [S-]
int get_toc_info(void)
{
	printk("get_toc_info\n");

	if(cdb.trans_type){
		set_report(STAT_WAIT);
		return 0;
	}

	SSCR1 = 0x4000|(cdb.status<<8);
	SSCR2 = 0x00cc;
	SSCR3 = 0x0000;
	SSCR4 = 0x0000;

	cdb.trans_type = TRANS_TOC;
	trans_start();

	return HIRQ_DRDY;
}

// 0x03 [S-]
int get_session_info(void)
{
	int sid, fad;

	sid = cdb.cr1&0xff;
	printk("get_session_info: sid=%d\n", sid);

	SSCR1 = (cdb.status<<8);
	SSCR2 = 0x0000;
	if(sid==0){
		fad = bswap32(cdb.TOC[101]);
		SSCR3 = 0x0100 | (fad>>16)&0xff;
		SSCR4 = fad&0xffff;
	}else if(sid==1){
		SSCR3 = 0x0100;
		SSCR4 = 0x0000;
	}else{
		SSCR3 = 0xffff;
		SSCR4 = 0xffff;
	}

	return 0;
}

// 0x04 [SR]
int init_cdblock(void)
{
	int i;

	printk("init_cdblock\n");

	// Stop Play task
	if(cdb.play_type!=0){
		cdb.pause_request = 1;
		cdb.play_wait = 0;
		isr_sem_send(&sem_wait_disc);
	}

	// disable dma irq
	ST_CTRL &= ~STIRQ_DAT;
	// abort dma
	FIFO_CTRL = 0x0002;
	while((FIFO_STAT&0xe000));
	FIFO_CTRL = 0x0000;
	// reset fifo
	FIFO_CTRL = 0x0004;
	FIFO_CTRL = 0x0000;

	cdb.trans_type = 0;
	cdb.cdwnum = 0;
	cdb.fad = 150;
	cdb.cdir_lba = 0;

	// free all block
	cdb.block_free = MAX_BLOCKS;
	for(i=0; i<MAX_BLOCKS; i++){
		cdb.block[i].size = -1;
	}

	// reset all filter
	for(i=0; i<MAX_SELECTORS; i++){
		cdb.part[i].size = -1;
		cdb.part[i].numblocks = 0;
		cdb.part[i].head = NULL;
		cdb.part[i].tail = NULL;

		cdb.filter[i].c_false = 0xff;
		cdb.filter[i].c_true = i;
		cdb.filter[i].mode   = 0;
		cdb.filter[i].fad    = 0;
		cdb.filter[i].range  = 0;
		cdb.filter[i].chan   = 0;
		cdb.filter[i].smmask = 0;
		cdb.filter[i].cimask = 0;
		cdb.filter[i].smval  = 0;
		cdb.filter[i].cival  = 0;
	}
	
	cdb.saturn_auth = 0;
	cdb.mpeg_auth = 0;
	
	HIRQ_CLR = HIRQ_PEND | HIRQ_DRDY | HIRQ_BFUL;

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x06 [S-]
int end_trans(void)
{
	int fifo_remain, i;

	// disable dma irq
	ST_CTRL &= ~STIRQ_DAT;
	ST_STAT = STIRQ_DAT;
	// abort dma
	FIFO_CTRL = 0x0002;
	for(i=0; i<100; i++){};
	//while((FIFO_STAT&0xe000));
	FIFO_CTRL = 0x0000;

	//printk("    cdwnum=%08x FIFO_STAT=%08x BLK_SIZE=%08x FIFO_RCNT=%d min_num=%d\n", cdb.cdwnum, FIFO_STAT, BLK_SIZE, FIFO_RCNT, min_num);
	fifo_remain = (FIFO_STAT&0x0fff)*2; // FIFO中还有多少字节未读
	cdb.cdwnum -= fifo_remain;

	// reset fifo
	FIFO_CTRL = 0x0004;
	FIFO_CTRL = 0x0000;

	if(cdb.trans_type==TRANS_DATA_DEL){
		PARTITION *pp = &cdb.part[cdb.trans_part_index];
		int spos = cdb.trans_block_start;
		int snum = cdb.trans_block_end-spos;

		while(snum){
			remove_block(pp, spos);
			snum -= 1;
		}
	}

	//printk("end_trans: cdwnum=%08x\n", cdb.cdwnum);
	if(cdb.cdwnum){
		SSCR1 = (cdb.status<<8) | (cdb.cdwnum>>17)&0xff;
		SSCR2 = (cdb.cdwnum>>1)&0xffff;
	}else{
		SSCR1 = (cdb.status<<8) | 0xff;
		SSCR2 = 0xffff;
	}
	SSCR3 = 0;
	SSCR4 = 0;

	cdb.cdwnum = 0;
	cdb.trans_type = 0;

	return HIRQ_EHST;
}

/******************************************************************************/
// CD drive 相关命令

// cdb.fad: 当前播放的位置

// 0x10 [SR]
int play_cd(void)
{
	int start_pos, end_pos, mode;

	start_pos = ((cdb.cr1&0xff)<<16) | cdb.cr2;
	end_pos   = ((cdb.cr3&0xff)<<16) | cdb.cr4;
	mode = cdb.cr3>>8;

	printk("play_cd: start=%08x end=%08x mode=%02x\n", start_pos, end_pos, mode);
	
	HIRQ_CLR = HIRQ_PEND;

	if(start_pos==0xffffff){
		// PTYPE_NOCHG
	}else if(start_pos&0x800000){
		// PTYPE_FAD
		cdb.play_fad_start = start_pos&0x0fffff;
		cdb.track = fad_to_track(cdb.play_fad_start);
		cdb.index = 1;
	}else{
		// PTYPE_TNO or PTYPE_DFL
		if(start_pos==0)
			start_pos = 0x0100;
		cdb.play_fad_start = track_to_fad(start_pos);
		cdb.track = (start_pos>>8)&0xff;
		cdb.index = start_pos&0xff;
	}

	if(end_pos==0xffffff){
		// PTYPE_NOCHG
	}else if(end_pos&0x800000){
		// PTYPE_FAD
		cdb.play_fad_end = cdb.play_fad_start+end_pos&0x0fffff;
	}else if(end_pos){
		// PTYPE_TNO
		if(end_pos&0xff)
			cdb.play_fad_end = track_to_fad(end_pos);
		else
			cdb.play_fad_end = track_to_fad(end_pos|0x63);
	}else{
		// PTYPE_DFL
		cdb.play_fad_end = track_to_fad(0xffff);
	}

	if((mode&0x80)==0)
		cdb.fad = cdb.play_fad_start;
	if((mode&0x7f)!=0x7f)
		cdb.max_repeat = mode&0x0f;

	cdb.repcnt = 0;
	cdb.pause_request = 0;
	cdb.play_type = PLAYTYPE_SECTOR;
	isr_sem_send(&sem_wait_disc);

	set_report(STAT_BUSY);
	return 0;
}

// 0x11 [SR]
int seek_cd(void)
{
	int pos;
	TRACK_INFO *ti;

	pos = ((cdb.cr1&0xff)<<16) | cdb.cr2;
	printk("\nseek_cd: pos=%08x\n", pos);

	if(cdb.play_type!=0){
		cdb.pause_request = 1;
		cdb.play_wait = 0;
		printk("       : Send PAUSE request!\n");
	}
	cdb.status = STAT_PAUSE;

	if(pos==0xffffff){
		// act as PAUSE cmd
		printk("       : PAUSE cmd\n");
	}else if(pos&0x800000){
		// PTYPE_FAD
		cdb.play_fad_start = pos&0x0fffff;
		cdb.track = fad_to_track(cdb.play_fad_start);
		cdb.index = 1;
		cdb.options = 0x00;
		cdb.repcnt = 0;
	}else{
		// PTYPE_TNO
		if(pos){

			cdb.play_fad_start = track_to_fad(pos);
			cdb.fad = cdb.play_fad_start;
			cdb.track = (pos>>8)&0xff;
			cdb.index = pos&0xff;
			cdb.options = 0x00;
			cdb.repcnt = 0;
			ti = get_track_info(pos);
			cdb.ctrladdr = ti->ctrl_addr;
		}else{
			cdb.status = STAT_STANDBY;
			cdb.fad = 0xffffffff;
			cdb.track = 0xff;
			cdb.index = 0xff;
			cdb.ctrladdr = 0xff;
			cdb.repcnt = 0xff;
			cdb.options = 0xff;
		}
	}

	printk("       : fad=%08x track=%d index=%d ctladr=%02x opt=%02x\n",
			cdb.fad, cdb.track, cdb.index, cdb.ctrladdr, cdb.options);

	set_report(cdb.status);
	return 0;
}

// 0x12 [SR]
// Scan not support

/******************************************************************************/
// Subcode 相关命令

int TOBCD(int val)
{
	return ((val/10)<<4) | (val%10);
}

void fad_to_msf(int fad, int *m, int *s, int *f)
{
	*m = fad/4500;
	fad %= 4500;
	*s = fad/75;
	*f = fad%75;
}

// 0x20 [S-]
int get_subcode(void)
{
	int type;
	int rel_fad, rm, rs, rf, m, s, f;

	if(cdb.trans_type){
		set_report(STAT_WAIT);
		return 0;
	}

	type = cdb.cr1&0xff;
	printk("get_subcode: type=%d\n", type);

	if(type==0){
		// Get Q Channel
		SSCR1 = 0x4000 | (cdb.status<<8);
		SSCR2 = 5;
		SSCR3 = 0;
		SSCR4 = 0;

		rel_fad = cdb.fad-tracks[cdb.track-1].fad_start;
		fad_to_msf(rel_fad, &rm, &rs, &rf);
		fad_to_msf(cdb.fad,  &m,  &s,  &f);

		cdb.SUBH[0] = cdb.ctrladdr;
		cdb.SUBH[1] = TOBCD(cdb.track);
		cdb.SUBH[2] = TOBCD(cdb.index);
		cdb.SUBH[3] = TOBCD(rm);
		cdb.SUBH[4] = TOBCD(rs);
		cdb.SUBH[5] = TOBCD(rf);
		cdb.SUBH[6] = 0;
		cdb.SUBH[7] = TOBCD(m);
		cdb.SUBH[8] = TOBCD(s);
		cdb.SUBH[9] = TOBCD(f);

		cdb.trans_type = TRANS_SUBQ;
		trans_start();
	}else if(type==1){
		// Get RW Channel
		SSCR1 = 0x4000 | (cdb.status<<8);
		SSCR2 = 12;
		SSCR3 = 0;
		SSCR4 = 0;

		cdb.trans_type = TRANS_SUBRW;
		trans_start();
	}

	return HIRQ_DRDY;
}


/******************************************************************************/
// CD-ROM device 相关命令

// 0x30 [SR]
int set_cdev_connect(void)
{
	printk("set_cdev_connect: %02x\n", cdb.cr3>>8);
	cdb.cddev_filter = cdb.cr3>>8;

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x31 [S-]
int get_cdev_connect(void)
{
	SSCR1 = (cdb.status<<8);
	SSCR2 = 0;
	SSCR3 = cdb.cddev_filter<<8;
	SSCR4 = 0;

	return 0;
}

// 0x32 [S-]
int get_last_buffer(void)
{
	SSCR1 = (cdb.status<<8);
	SSCR2 = 0;
	SSCR3 = cdb.last_buffer<<8;
	SSCR4 = 0;

	return 0;
}

/******************************************************************************/
// 过滤器相关命令

// 0x40 [SR]
int set_filter_range(void)
{
	int fid;

	fid = cdb.cr3>>8;
	cdb.filter[fid].fad   = ((cdb.cr1&0xff)<<16) | cdb.cr2;
	cdb.filter[fid].range = ((cdb.cr3&0xff)<<16) | cdb.cr4;
	printk("set_filter_range: fid=%d fad=%08x range=%08x\n", fid, cdb.filter[fid].fad, cdb.filter[fid].range);

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x41 [S-]
int get_filter_range(void)
{
	int fid;
	int fad, range;

	fid = cdb.cr3>>8;

	fad   = cdb.filter[fid].fad;
	range = cdb.filter[fid].range;

	SSCR1 = (cdb.status<<8) | (fad>>16)&0xff;
	SSCR2 = fad&0xffff;
	SSCR3 = (range>>16)&0xff;
	SSCR4 = range&0xffff;

	return 0;
}

// 0x42 [SR]
int set_filter_subheader(void)
{
	int fid;

	fid = cdb.cr3>>8;
	printk("set_filter_subh: fid=%d\n", fid);

	cdb.filter[fid].chan   = cdb.cr1&0xff;
	cdb.filter[fid].smmask = cdb.cr2>>8;
	cdb.filter[fid].cimask = cdb.cr2&0xff;
	cdb.filter[fid].fid    = cdb.cr3&0xff;
	cdb.filter[fid].smval  = cdb.cr4>>8;
	cdb.filter[fid].cival  = cdb.cr4&0xff;

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x43 [S-]
int get_filter_subheader(void)
{
	int fid;

	fid = cdb.cr3>>8;

	SSCR1 = (cdb.status) | cdb.filter[fid].chan;
	SSCR2 = (cdb.filter[fid].smmask<<8) | cdb.filter[fid].cimask;
	SSCR3 =  cdb.filter[fid].fid;
	SSCR4 = (cdb.filter[fid].smval<<8) | cdb.filter[fid].cival;

	return 0;
}

// 0x44 [SR]
int set_filter_mode(void)
{
	int fid;

	fid = cdb.cr3>>8;
	cdb.filter[fid].mode = cdb.cr1&0xff;
	printk("set_filter_mode: fid=%d  mode=%02x\n", fid, cdb.cr1&0xff);

	if(cdb.filter[fid].mode&0x80){
		cdb.filter[fid].mode   = 0;
		cdb.filter[fid].fad    = 0;
		cdb.filter[fid].range  = 0;
		cdb.filter[fid].chan   = 0;
		cdb.filter[fid].smmask = 0;
		cdb.filter[fid].cimask = 0;
		cdb.filter[fid].smval  = 0;
		cdb.filter[fid].cival  = 0;
	}

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x45 [S-]
int get_filter_mode(void)
{
	int fid;

	fid = cdb.cr3>>8;

	SSCR1 = (cdb.status<<8) | cdb.filter[fid].mode;
	SSCR2 = 0;
	SSCR3 = 0;
	SSCR4 = 0;

	return 0;
}

// 0x46 [SR]
int set_filter_connection(void)
{
	int fid;

	fid = cdb.cr3>>8;
	printk("set_filter_connection: fid=%d  mode=%d true=%02x false=%02x\n", fid, cdb.cr1&0xff, cdb.cr2>>8, cdb.cr2&0xff);

	if(cdb.cr1&0x01){
		cdb.filter[fid].c_true = cdb.cr2>>8;
	}
	if(cdb.cr1&0x02){
		cdb.filter[fid].c_false = cdb.cr2&0xff;
	}

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x47 [S-]
int get_filter_connection(void)
{
	int fid;

	fid = cdb.cr3>>8;

	SSCR1 = (cdb.status<<8);
	SSCR2 = (cdb.filter[fid].c_true<<8) | cdb.filter[fid].c_false;
	SSCR3 = 0;
	SSCR4 = 0;

	return 0;
}

void free_partition(PARTITION *pt)
{
	BLOCK *bp = pt->head;
	
	while(bp){
		free_block(bp);
		bp = bp->next;
	}
	
	pt->head = NULL;
	pt->tail = NULL;
	pt->numblocks = 0;
	pt->size = -1;

}

// 0x48 [SR]
int reset_filter(void)
{
	int mode, fid;
	int i, j;

	mode = cdb.cr1&0xff;
	fid = cdb.cr3>>8;
	printk("reset_filter: mode=%02x fid=%d\n", mode, fid);

	if(mode==0){
		if(fid<MAX_SELECTORS){
			free_partition(&cdb.part[fid]);
		}
	}else{
		for(i=0; i<MAX_SELECTORS; i++){
			if(mode&0x80){
				cdb.filter[i].c_false = 0xff;
			}
			if(mode&0x40){
				cdb.filter[i].c_true = i;
			}
			if(mode&0x10){
				cdb.filter[i].mode   = 0;
				cdb.filter[i].fad    = 0;
				cdb.filter[i].range  = 0;
				cdb.filter[i].chan   = 0;
				cdb.filter[i].smmask = 0;
				cdb.filter[i].cimask = 0;
				cdb.filter[i].smval  = 0;
				cdb.filter[i].cival  = 0;
			}
			if(mode&0x04){
				free_partition(&cdb.part[i]);
			}
		}

		if(mode&0x04){
			cdb.block_free = MAX_BLOCKS;
			for(j=0; j<MAX_BLOCKS; j++){
				cdb.block[j].size = -1;;
			}
		}
	}

	set_report(cdb.status);
	return HIRQ_ESEL;
}

/******************************************************************************/
// buffer info 相关命令


u8 old_block_free;

// 0x50 [S-]
int get_buffer_size(void)
{
	SSCR1 = (cdb.status<<8);
	SSCR2 = cdb.block_free;
	SSCR3 = MAX_SELECTORS<<8;
	SSCR4 = MAX_BLOCKS;

	if(old_block_free!=cdb.block_free){
		//printk("SS_CDC: CR1=%04x CR2=%04x CR3=%04x CR4=%04x  get_buffer_size\n", RESP1, RESP2, RESP3, RESP4);
	}
	old_block_free = cdb.block_free;

	return 0;
}

int old_numblocks;

// 0x51 [S-]
int get_sector_num(void)
{
	int pt;

	pt = cdb.cr3>>8;

	SSCR1 = (cdb.status<<8);
	SSCR2 = 0;
	SSCR3 = 0;
	SSCR4 = cdb.part[pt].numblocks;

	if(old_numblocks != cdb.part[pt].numblocks){
		//printk("SS_CDC: CR1=%04x CR2=%04x CR3=%04x CR4=%04x  get_sector_num\n", RESP1, RESP2, RESP3, RESP4);
	}
	old_numblocks = cdb.part[pt].numblocks;

	return 0;
}

// 0x52 [SR]
int cal_actual_size(void)
{
	int pt, spos, snum, size;
	BLOCK *bp;

	spos = cdb.cr2;
	pt   = cdb.cr3>>8;
	snum = cdb.cr4;

	size = 0;
	bp = find_block(&cdb.part[pt], spos);
	while(bp && snum){
		size += bp->size;
		bp = bp->next;
		snum -= 1;
	}
	cdb.actsize = size;

	set_report(cdb.status);
	return HIRQ_ESEL;
}

int old_actsize;

// 0x53 [S-]
int get_actual_size(void)
{
	SSCR1 = (cdb.status<<8) | ((cdb.actsize/2)>>16)&0xff;
	SSCR2 = (cdb.actsize/2)&0xffff;
	SSCR3 = 0;
	SSCR4 = 0;

	if(old_actsize!=cdb.actsize){
		//printk("SS_CDC: CR1=%04x CR2=%04x CR3=%04x CR4=%04x  get_actual_size\n", RESP1, RESP2, RESP3, RESP4);
	}
	old_actsize = cdb.actsize;


	return 0;
}

// 0x54 [S-]
int get_sector_info(void)
{
	int pt, st;
	PARTITION *pp;
	BLOCK *bp;

	pt = cdb.cr3>>8;
	st = cdb.cr2&0xff;

	pp = &cdb.part[pt];
	bp = find_block(pp, st);

	SSCR1 = (cdb.status<<8) | (bp->fad>>16)&0xff;
	SSCR2 = bp->fad&0xff;
	SSCR3 = (bp->fn<<8) | (bp->cn);
	SSCR4 = (bp->sm<<8) | (bp->ci);

	return 0;
}

// 0x55 [SR]
// 0x56 [SR]


/******************************************************************************/
// buffer I/O相关命令

u32 sector_len_table[4] = {2048, 2336, 2340, 2352};

// 0x60 [SR]
int set_sector_length(void)
{
	int id;

	id = SSCR1&0xff;
	cdb.sector_size = sector_len_table[id];

	set_report(cdb.status);
	return HIRQ_ESEL;
}

// 0x61 [SR]
int get_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	if(cdb.trans_type){
		set_report(STAT_WAIT);
		return 0;
	}

	pt   = cdb.cr3>>8;
	spos = cdb.cr2;
	snum = cdb.cr4;

	pp = &cdb.part[pt];

	if(spos==0xffff){
		spos = pp->numblocks-1;
	}
	if(snum==0xffff){
		snum = pp->numblocks-spos;
	}
	//printk("get_sector_data: part=%d spos=%d snum=%d\n", pt, spos, snum);

	cdb.trans_type = TRANS_DATA;
	cdb.trans_part_index = pt;
	cdb.trans_block_start = spos;
	cdb.trans_block_end = spos+snum;

	trans_start();

	set_report(0x4000 | cdb.status);
	return HIRQ_DRDY|HIRQ_EHST;
}


// 0x62 [SR]
int del_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	pt   = cdb.cr3>>8;
	spos = cdb.cr2;
	snum = cdb.cr4;

	pp = &cdb.part[pt];

	if(spos==0xffff){
		spos = pp->numblocks-1;
	}
	if(snum==0xffff){
		snum = pp->numblocks-spos;
	}
	//printk("del_sector_data: part=%d spos=%d snum=%d\n", pt, spos, snum);

	while(snum){
		remove_block(pp, spos);
		snum -= 1;
	}

	set_report(cdb.status);
	return HIRQ_EHST;
}

// 0x63 [SR]
int get_del_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	if(cdb.trans_type){
		set_report(STAT_WAIT);
		return 0;
	}

	pt   = cdb.cr3>>8;
	spos = cdb.cr2;
	snum = cdb.cr4;

	pp = &cdb.part[pt];

	if(spos==0xffff){
		spos = pp->numblocks-1;
	}
	if(snum==0xffff){
		snum = pp->numblocks-spos;
	}
	//printk("get_del_sector_data: part=%d spos=%d snum=%d\n", pt, spos, snum);

	cdb.trans_type = TRANS_DATA_DEL;
	cdb.trans_part_index = pt;
	cdb.trans_block_start = spos;
	cdb.trans_block_end = spos+snum;

	trans_start();

	set_report(0x4000 | cdb.status);
	return HIRQ_DRDY|HIRQ_EHST;
}

// 0x67 [S-]
int get_copy_error(void)
{
	SSCR1 = (cdb.status<<8);
	SSCR2 = 0;
	SSCR3 = 0;
	SSCR4 = 0;

	return 0;
}

/******************************************************************************/

static u32 isonum_711(u8 *p)
{
	return p[0];
}

static u32 isonum_733(u8 *p)
{
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

void fill_fileinfo(void)
{
	int ptid = cdb.filter[cdb.dir_filter].c_true;
	PARTITION *pt = &cdb.part[ptid];
	BLOCK *blk = pt->head;
	u8 *buf = blk->data+24;
	int rsize, p, start, end, pfn, fid;

	rsize = cdb.cdir_size-1;
	p = 0;

	start = cdb.cdir_pfid;
	if(start<2){
		end = 256;
		pfn = 0;
		cdb.cdir_pfid = 2;
	}else{
		end = start+254;
		pfn = 2;
		cdb.cdir_pfid = start;
	}
	cdb.cdir_drend = 0;

	fid = 0;
	while(fid<end){
		if(fid>=start){
			cdb.fi[pfn].lba  = isonum_733(buf+p+2);
			cdb.fi[pfn].size = isonum_733(buf+p+10);
			cdb.fi[pfn].attr = isonum_711(buf+p+25);
			cdb.fi[pfn].unit = isonum_711(buf+p+26);
			cdb.fi[pfn].gap  = isonum_711(buf+p+27);
			cdb.fi[pfn].fid  = 0;
			if(fid>1 && cdb.fi[pfn].attr&0x02)
				cdb.cdir_drend = 1;
			pfn += 1;
		}

		fid += 1;
		p += buf[p];
		if(p>=2048 || buf[p]==0){
			if(rsize==0)
				break;
			rsize -= 2048;
			p = 0;
			blk = blk->next;
			buf = blk->data+24;
		}
	}

	cdb.cdir_pfnum = pfn-2;
	//printk("  cdir_pfid=%d fnum=%d\n", cdb.cdir_pfid, cdb.cdir_pfnum);
}


int handle_diread(void)
{
	int ptid = cdb.filter[cdb.dir_filter].c_true;
	PARTITION *pt = &cdb.part[ptid];
	BLOCK *blk = pt->head;
	u8 *buf = blk->data+24;

	//printk("handle_diread %d\n", cdb.rdir_state);
	if(cdb.rdir_state==1){
		// 主描述符已读
		cdb.root_lba  = isonum_733(buf+0x9c+2);
		cdb.root_size = isonum_733(buf+0x9c+10)/2048;
		free_partition(pt);
		//printk("  root_lba=%08x size=%08x\n", cdb.root_lba, cdb.root_size);

		cdb.cdir_lba = cdb.root_lba;
		cdb.cdir_size = cdb.root_size;

		// 准备读根目录
		cdb.rdir_state = 2;
		cdb.pause_request = 0;
		cdb.play_type = PLAYTYPE_DIR;
		cdb.play_fad_start = cdb.cdir_lba+150;
		cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
		cdb.fad = cdb.play_fad_start;
		return 1;
	}else if(cdb.rdir_state==2){
		// 目录数据已读。开始填充文件信息
		//printk("  cdir_lba=%08x size=%08x\n", cdb.cdir_lba, cdb.cdir_size);
		fill_fileinfo();
		free_partition(pt);
	}

	HIRQ = HIRQ_EFLS;
	return 0;
}


// 0x70 [SR]
int change_dir(void)
{
	int selnum, fid;
	FILE_INFO *fi;

	selnum = cdb.cr3>>8;
	fid = ((cdb.cr3&0xff)<<16) | (cdb.cr4);
	printk("change_dir: sel=%d fid=%08x\n", selnum, fid);

	cdb.dir_filter = selnum;

	if(fid==0xffffff){
		// 切换到根目录
		cdb.cdir_pfid = 0;
		if(cdb.root_lba==0){
			// 需要先读主描述符扇区,以便取得根目录的LBA地址
			cdb.rdir_state = 1;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = 16+150;
			cdb.play_fad_end = cdb.play_fad_start+1;
			cdb.fad = cdb.play_fad_start;
			isr_sem_send(&sem_wait_disc);
		}else{
			// 已经有根目录的LBA地址
			cdb.cdir_lba = cdb.root_lba;
			cdb.cdir_size = cdb.root_size;

			cdb.rdir_state = 2;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = cdb.cdir_lba+150;
			cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
			cdb.fad = cdb.play_fad_start;
			isr_sem_send(&sem_wait_disc);
		}
	}else{
		// 切换到fid所指的目录
		if(cdb.cdir_lba){
			if(fid<2){
				fi = &cdb.fi[fid];
			}else{
				if(fid<cdb.cdir_pfid || fid>=(cdb.cdir_pfid+cdb.cdir_pfnum)){
					set_report(STAT_REJECT);
					return 0;
				}else{
					fi = &cdb.fi[fid-cdb.cdir_pfid+2];
				}
			}

			cdb.cdir_lba = fi->lba;
			cdb.cdir_size = fi->size/2048;

			cdb.cdir_pfid = 0;
			cdb.rdir_state = 2;
			cdb.cddev_filter = selnum;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_DIR;
			cdb.play_fad_start = cdb.cdir_lba+150;
			cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
			cdb.fad = cdb.play_fad_start;
			isr_sem_send(&sem_wait_disc);
		}else{
			set_report(STAT_REJECT);
			return 0;
		}
	}

	set_report(cdb.status);
	return 0;
}

// 0x71 [SR]
int read_dir(void)
{
	int selnum, fid;

	selnum = cdb.cr3>>8;
	fid = ((cdb.cr3&0xff)<<16) | (cdb.cr4);
	printk("read_dir: sel=%d fid=%08x\n", selnum, fid);

	if(cdb.cdir_lba){
		cdb.cdir_pfid = fid;
		cdb.rdir_state = 2;
		cdb.cddev_filter = selnum;
		cdb.pause_request = 0;
		cdb.play_type = PLAYTYPE_DIR;
		cdb.play_fad_start = cdb.cdir_lba+150;
		cdb.play_fad_end = cdb.play_fad_start+cdb.cdir_size;
		cdb.fad = cdb.play_fad_start;
		isr_sem_send(&sem_wait_disc);
	}else{
		set_report(STAT_REJECT);
		return 0;
	}

	set_report(cdb.status);
	return 0;
}

// 0x72 [S-]
int get_file_scope(void)
{
	printk("get_file_scope: pfid=%d  pfnum=%d\n", cdb.cdir_pfid, cdb.cdir_pfnum);
	SSCR1 = (cdb.status<<8);
	SSCR2 = cdb.cdir_pfnum;
	SSCR3 = cdb.cdir_drend<<8;
	SSCR4 = cdb.cdir_pfid;

	return HIRQ_EFLS;
}

// 0x73 [S-]
int get_file_info(void)
{
	int i;

	if(cdb.trans_type){
		set_report(STAT_WAIT);
		return 0;
	}

	i = ((cdb.cr3&0xff)<<16) | cdb.cr4;
	printk("get_file_info: fid=%08x\n", i);

	if(i==0xffffff){
		// 不返回当前目录和根目录信息
		for(i=0; i<cdb.cdir_pfnum; i++){
			*(u32*)(cdb.FINFO+i*12+0) = bswap32(cdb.fi[i+2].lba+150); 
			*(u32*)(cdb.FINFO+i*12+4) = bswap32(cdb.fi[i+2].size); 
			*(u8 *)(cdb.FINFO+i*12+8) = cdb.fi[i+2].gap;
			*(u8 *)(cdb.FINFO+i*12+9) = cdb.fi[i+2].unit;
			*(u8 *)(cdb.FINFO+i*12+10)= cdb.fi[i+2].fid;
			*(u8 *)(cdb.FINFO+i*12+11)= cdb.fi[i+2].attr;
		}
		cdb.trans_type = TRANS_FINFO_ALL;
		trans_start();

		SSCR1 = 0x4000 | (cdb.status<<8);
		SSCR2 = (cdb.cdir_pfnum*12)/2;
		SSCR3 = 0x0000;
		SSCR4 = 0x0000;
	}else{
		if(i<cdb.cdir_pfid || i>=(cdb.cdir_pfid+cdb.cdir_pfnum)){
			// fid不在当前目录页的范围内
			set_report(STAT_REJECT);
			return 0;
		}
		i = i-cdb.cdir_pfid+2;
		*(u32*)(cdb.FINFO+0) = bswap32(cdb.fi[i].lba+150); 
		*(u32*)(cdb.FINFO+4) = bswap32(cdb.fi[i].size); 
		*(u8 *)(cdb.FINFO+8) = cdb.fi[i].gap;
		*(u8 *)(cdb.FINFO+9) = cdb.fi[i].unit;
		*(u8 *)(cdb.FINFO+10)= cdb.fi[i].fid;
		*(u8 *)(cdb.FINFO+11)= cdb.fi[i].attr;

		cdb.trans_type = TRANS_FINFO_ONE;
		trans_start();

		SSCR1 = 0x4000 | (cdb.status<<8);
		SSCR2 = 0x0006;
		SSCR3 = 0x0000;
		SSCR4 = 0x0000;
	}

	return HIRQ_DRDY;
}


// 0x74 [SR]
int read_file(void)
{
	int selnum, fid, offset, lba_size;
	FILTER *fsl;
	FILE_INFO *fi;
	PARTITION *pp;

	selnum = cdb.cr3>>8;
	offset = ((cdb.cr1&0xff)<<16) | cdb.cr2;
	fid = ((cdb.cr3&0xff)<<16) | cdb.cr4;
	printk("read_file: sel=%d fid=%08x\n", selnum, fid);

	if(fid<cdb.cdir_pfid || fid>=(cdb.cdir_pfid+cdb.cdir_pfnum)){
		set_report(STAT_REJECT);
		return 0;
	}

	fid = fid-cdb.cdir_pfid+2;
	fi = &cdb.fi[fid];
	lba_size = (fi->size+2047)/2048;

	if(offset>=lba_size){
		set_report(STAT_REJECT);
		return 0;
	}

	fsl = &cdb.filter[selnum];
	fsl->mode = 0x41;
	fsl->fad = fi->lba+offset+150;
	fsl->range = lba_size;

	pp  = &cdb.part[fsl->c_true];
	free_partition(pp);

	cdb.cddev_filter = selnum;
	cdb.repcnt = 0;
	cdb.pause_request = 0;
	cdb.play_type = PLAYTYPE_FILE;
	cdb.play_fad_start = fsl->fad;
	cdb.play_fad_end = fsl->fad+lba_size-offset;
	cdb.fad = fsl->fad;
	cdb.options = 0x08;
	isr_sem_send(&sem_wait_disc);

	set_report(0x4000 | cdb.status);
	return HIRQ_EFLS;
}

// 0x75 [SR]
int abort_file(void)
{
	printk("abort_file\n");
	if(cdb.play_type==PLAYTYPE_FILE){
		cdb.pause_request = 1;
		cdb.play_wait = 0;
		isr_sem_send(&sem_wait_disc);
	}

	return HIRQ_EFLS;
}

/******************************************************************************/

// 0xe0 [S-]
int auth_device(void)
{
	int mpeg_auth, hirq;

	printk("auth_device\n");

	mpeg_auth = cdb.cr2&0xff;
	if(mpeg_auth){
		hirq = HIRQ_MPED;
		cdb.mpeg_auth = 2;
	}else{
		hirq = HIRQ_EFLS | HIRQ_CSCT;
		cdb.saturn_auth = 4;
	}

	SSCR1 = (STAT_BUSY<<8) | 0xff;
	SSCR2 = 0xffff;
	SSCR3 = 0xffff;
	SSCR4 = 0xffff;

	return hirq;
}

// 0xe1 [S-]
int get_auth_status(void)
{
	printk("get_auth_status\n");
	if(cdb.cr2&0xff){
		SSCR2 = cdb.mpeg_auth;
	}else{
		SSCR2 = cdb.saturn_auth;
	}
	SSCR1 = (cdb.status<<8);
	SSCR3 = 0;
	SSCR4 = 0;

	return 0;
}


/******************************************************************************/

char *cmd_str(int cmd)
{
	switch(cmd){
	// common
	case 0x00: return "get_cd_status";
	case 0x01: return "get_hw_info";
	case 0x02: return "get_toc_info";
	case 0x03: return "get_session_info";
	case 0x04: return "init_cdblock";
	case 0x06: return "end_trans";

	// cd drive
	case 0x10: return "play_cd";
	case 0x11: return "seek_cd";

	// subcode
	case 0x20: return "get_subcode";

	// cd device
	case 0x30: return "set_cdev_connect";
	case 0x31: return "get_cdev_connect";
	case 0x32: return "get_last_buffer";

	// filter
	case 0x40: return "set_filter_range";
	case 0x41: return "get_filter_range";
	case 0x42: return "set_filter_subheader";
	case 0x43: return "get_filter_subheader";
	case 0x44: return "set_filter_mode";
	case 0x45: return "get_filter_mode";
	case 0x46: return "set_filter_connection";
	case 0x47: return "get_filter_connection";
	case 0x48: return "reset_filter";

	// buffer info
	case 0x50: return "get_buffer_size";
	case 0x51: return "get_sector_num";
	case 0x52: return "cal_actual_size";
	case 0x53: return "get_actual_size";
	case 0x54: return "get_sector_info";

	// buffer io
	case 0x60: return "set_sector_length";
	case 0x61: return "get_sector_data";
	case 0x62: return "del_sector_data";
	case 0x63: return "get_del_sector_data";
	case 0x67: return "get_copy_error";

	// file io
	case 0x70: return "change_dir";
	case 0x71: return "read_dir";
	case 0x72: return "get_file_scope";
	case 0x73: return "get_file_info";
	case 0x74: return "read_file";
	case 0x75: return "abort_file";

	case 0xe0: return "auth_device";
	case 0xe1: return "get_auth_status";

	default  : return "unkonw_cmd";
	}
}

void cdc_cmd_process(void)
{
	u32 cmd, hirq;

	if((SS_CTRL&0x8000)==0)
		return;

	cmd = (cdb.cr1>>8)&0xff;
	//if(cmd!=0)
	//	printk("\nSS CDC: CR1=%04x CR2=%04x CR3=%04x CR4=%04x %s\n",
	//			cdb.cr1, cdb.cr2, cdb.cr3, cdb.cr4, cmd_str(cmd));

	cmd = (cdb.cr1>>8)&0xff;
	switch(cmd){
	// common
	case 0x00: hirq = get_cd_status();
		break;
	case 0x01: hirq = get_hw_info();
		break;
	case 0x02: hirq = get_toc_info();
		break;
	case 0x03: hirq = get_session_info();
		break;
	case 0x04: hirq = init_cdblock();
		break;
	case 0x06: hirq = end_trans();
		break;

	// cd drive
	case 0x10: hirq = play_cd();
		break;
	case 0x11: hirq = seek_cd();
		break;

	// subcode
	case 0x20: hirq = get_subcode();
		break;

	// cd device
	case 0x30: hirq = set_cdev_connect();
		break;
	case 0x31: hirq = get_cdev_connect();
		break;
	case 0x32: hirq = get_last_buffer();
		break;

	// filter
	case 0x40: hirq = set_filter_range();
		break;
	case 0x41: hirq = get_filter_range();
		break;
	case 0x42: hirq = set_filter_subheader();
		break;
	case 0x43: hirq = get_filter_subheader();
		break;
	case 0x44: hirq = set_filter_mode();
		break;
	case 0x45: hirq = get_filter_mode();
		break;
	case 0x46: hirq = set_filter_connection();
		break;
	case 0x47: hirq = get_filter_connection();
		break;
	case 0x48: hirq = reset_filter();
		break;

	// buffer info
	case 0x50: hirq = get_buffer_size();
		break;
	case 0x51: hirq = get_sector_num();
		break;
	case 0x52: hirq = cal_actual_size();
		break;
	case 0x53: hirq = get_actual_size();
		break;
	case 0x54: hirq = get_sector_info();
		break;

	// buffer io
	case 0x60: hirq = set_sector_length();
		break;
	case 0x61: hirq = get_sector_data();
		break;
	case 0x62: hirq = del_sector_data();
		break;
	case 0x63: hirq = get_del_sector_data();
		break;
	case 0x67: hirq = get_copy_error();
		break;

	// file io
	case 0x70: hirq = change_dir();
		break;
	case 0x71: hirq = read_dir();
		break;
	case 0x72: hirq = get_file_scope();
		break;
	case 0x73: hirq = get_file_info();
		break;
	case 0x74: hirq = read_file();
		break;
	case 0x75: hirq = abort_file();
		break;

	case 0xe0: hirq = auth_device();
		break;
	case 0xe1: hirq = get_auth_status();
		break;

	default:
		printk("SS CDC: unknow cmd! %08x\n", cmd);
		hirq = 0;
		break;
	}

	//if(cmd!=0)
	//	printk("      : CR1=%04x CR2=%04x CR3=%04x CR4=%04x\n", RESP1, RESP2, RESP3, RESP4);

	HIRQ = hirq | HIRQ_CMOK;
}


/******************************************************************************/

