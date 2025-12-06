
#include "main.h"
#include "ff.h"
#include "cdc.h"

/******************************************************************************/

osSemaphoreId_t sem_wait_irq;
osSemaphoreId_t sem_wait_disc;
osSemaphoreId_t sem_wait_pause;
osMutexId_t mutex_fs;

int lang_id = 0;
int debug_flags = 0;
int sector_delay = 0;
int sector_delay_force = -1;
int play_delay = 0;
int play_delay_force = -1;
int pend_delay = 0;
int pend_delay_force = -1;
int auto_update = 0;
char mdisc_str[16];
int next_disc = -1;

int log_mask = LOG_MASK_DEFAULT;

void hw_delay(int us);


/******************************************************************************/


u32 bswap32(u32 d)
{
	return ((d&0xff)<<24) | ((d&0xff00)<<8) | ((d>>8)&0xff00) | ((d>>24)&0xff);
}


// Track 信息
void init_toc(void)
{
	TRACK_INFO *last;
	int i;

	if(cdb.track_num==0)
		return;

	for(i=0; i<102; i++){
		cdb.TOC[i] = 0xffffffff;
	}

	for(i=0; i<cdb.track_num; i++){
		cdb.TOC[i] = bswap32((cdb.tracks[i].ctrl_addr<<24) | cdb.tracks[i].fad_start);
		SSLOG(_INFO, "track %2d: fad_0=%08x fad_start=%08x fad_end=%08x offset=%08x %d\n",
					i, cdb.tracks[i].fad_0, cdb.tracks[i].fad_start, cdb.tracks[i].fad_end,
					cdb.tracks[i].file_offset, cdb.tracks[i].sector_size);
	}

	last = &cdb.tracks[cdb.track_num-1];
	cdb.TOC[99]  = bswap32((cdb.tracks[0].ctrl_addr<<24) | 0x010000);
	cdb.TOC[100] = bswap32((last->ctrl_addr<<24) | (cdb.track_num<<16));
	cdb.TOC[101] = bswap32((last->ctrl_addr<<24) | (last->fad_end+1));
	
	cdb.status = STAT_PAUSE;
	set_status(cdb.status);

	sector_delay = 0;
}


u32 track_to_fad(u16 track_index)
{
	int t, index;

	if(track_index==0xffff){
		return cdb.tracks[cdb.track_num-1].fad_end;
	}else if(track_index==0){
		return 0;
	}

	t     = (track_index>>8)-1;
	if(t<0)
		t = 0;
	index = track_index&0xff;
	TRACK_INFO *tk = &cdb.tracks[t];

	if(index<=0x01){
		return tk->fad_start;
	} else if(index > tk->max_index){
		return tk->fad_end;
	} else{
		return tk->index[index-2];
	}
}

int fad_to_track(u32 fad)
{
	int i;

	for(i=0; i<cdb.track_num; i++){
		if(fad>=cdb.tracks[i].fad_0 && fad<=cdb.tracks[i].fad_end)
			return i+1;
	}

	return 0xff;
}

TRACK_INFO *get_track_info(int track_index)
{
	int t;

	if(track_index==0xffff){
		t = cdb.track_num-1;
	}else if(track_index==0){
		return NULL;
	}else{
		t = (track_index>>8)-1;
	}

	if(t>=cdb.track_num)
		return NULL;
	
	return &cdb.tracks[t];
}


/******************************************************************************/

// 内部扇区缓存, 32K大小, 大概16个2k的扇区. 位于SDRAM中
// FATFS层以512字节为单位读取, 每次读64个单位.
static u8 *sector_buffer = (u8*)0x24002000;

// 内部缓存中保存的FAD扇区范围
static u32 buf_fad_start = 0;
static u32 buf_fad_end = 0;
// 由于存在2352这样的扇区格式, FAD扇区不是2048字节对齐的.
// FATFS层以512为单位读取数据. FAD扇区相对于sector_buffer有一个偏移.
static u32 buf_fad_offset = 0;
// 内部缓存中的扇区大小
static u32 buf_fad_size = 0;


static u8 reorder[24] = {
	0x00, 0x42, 0x7d, 0xbf, 0x64, 0x32, 0x96, 0xaf,
	0x08, 0x21, 0x3a, 0x53, 0x6c, 0x85, 0x9e, 0xb7,
	0x10, 0x29, 0x19, 0x5b, 0x74, 0x8d, 0xa6, 0x4b
};

void subcode_convert(void)
{
	u8 *dst = cdb.subcode_buf;
	u8 *sub = sector_buffer;
	u8 *tbuf = sector_buffer+2048;
	int p, i, j, k;

	p = 0;
	while(p<1536){
		memset(tbuf, 0, 96);
	//	for(i=0; i<8; i++){ // 带PQ通道
		for(i=2; i<8; i++){ // 无PQ通道
			for(j=0; j<12; j++){
				u8 d = sub[i*12+j];
				for(k=0; k<8; k++){
					u8 b = (d>>(7-k))&1;
					tbuf[j*8+k] |= b<<(7-i);
				}
			}
		}
		tbuf += 96;
		sub += 96;
		p += 96;
	}

	sub = sector_buffer+2048;
	p = 0;
	while(p<(96*14)){
		for(j=0; j<24; j++){
			dst[j] = sub[reorder[j]];
		}
		p += 24;
		dst += 24;
		sub += 24;
	}
}


int get_sector(int fad, BLOCK *wblk)
{
	int retv, dp, nread;

	// 先查找track信息
	if(cdb.play_track==NULL || fad<cdb.play_track->fad_0 || fad>cdb.play_track->fad_end){
		cdb.track = fad_to_track(fad);
		SSLOG(_DTASK, "Change to track %d\n", cdb.track);
		if(cdb.track!=0xff){
			cdb.play_track = &cdb.tracks[cdb.track-1];
			cdb.subrw_wp = cdb.subrw_rp;
		}else{
			// play的FAD参数错误
			SSLOG(_DTASK, "play_track not found!\n");
			cdb.status = STAT_ERROR;
			cdb.play_type = 0;
			return -1;
		}
	}
	if(wblk==NULL)
		return 0;

	if(fad>=buf_fad_start && fad<buf_fad_end){
		// 在内部缓存中找到了要play的扇区
		//SSLOG(_DTASK, " fad_%08x found at buffer!\n", fad);
		dp = buf_fad_offset+(fad-buf_fad_start)*buf_fad_size;
		wblk->data = sector_buffer+dp;
	}else{
		// 内部缓存中没有要play的扇区. 从文件重新读取.
		//SSLOG(_DTASK, " fad_%08x not found. need read from file.\n", fad);
		
		// 先读subcode. 最多读1536字节((32768/2048)*96).
		if(cdb.has_subrw && cdb.iflag&0x02){
			f_lseek(&cdb.subcode_fp, (fad-150)*96);
			f_read(&cdb.subcode_fp, sector_buffer, 1536, (u32*)&nread);
			if(cdb.has_subrw==1){
				// sub格式需要先解码
				subcode_convert();
			}else{
				// cdg格式直接使用
				memcpy(cdb.subcode_buf, sector_buffer, 1536);
			}
		}

		cdb.ctrladdr = cdb.play_track->ctrl_addr;
		cdb.index = 1;

		buf_fad_start = fad;
		buf_fad_size = cdb.play_track->sector_size;

		dp = cdb.play_track->file_offset+(fad-cdb.play_track->fad_start)*cdb.play_track->sector_size;
		buf_fad_offset = dp&0x1ff;
		dp &= ~0x1ff;

		fs_lock();
		//printk("  seek at %08x\n", dp);
		f_lseek(cdb.play_track->fp, dp);
		retv = f_read(cdb.play_track->fp, sector_buffer, 0x8000, (u32*)&nread);
		fs_unlock();
		if(retv==0){
			int num_fad = (nread-buf_fad_offset)/buf_fad_size;
			buf_fad_end = buf_fad_start+num_fad;
		}else{
			SSLOG(_DTASK, "f_read error!\n");
			cdb.status = STAT_ERROR;
			cdb.play_type = 0;
			buf_fad_end = 0;
			return -2;
		}

		wblk->data = sector_buffer+buf_fad_offset;
	}

	wblk->size = buf_fad_size;
	wblk->fad = fad;
	if(buf_fad_size==2352 && wblk->data[0x0f]==0x02){
		wblk->fn = wblk->data[0x10];
		wblk->cn = wblk->data[0x11];
		wblk->sm = wblk->data[0x12];
		wblk->ci = wblk->data[0x13];
	}

	// 处理subcode
	if(cdb.has_subrw && cdb.iflag&0x02){
		u8 *sbuf = cdb.subcode_buf+(fad-buf_fad_start)*96;
		for(int i=0; i<4; i++){
			memcpy(cdb.subrw_buf+cdb.subrw_wp*24, sbuf, 24);
			int new_wp = (cdb.subrw_wp+1)%64;
			cdb.subrw_wp = new_wp;
			if(new_wp==cdb.subrw_rp){
				cdb.subrw_rp = (cdb.subrw_rp+1)%64;
			}
			sbuf += 24;
		}
	}

	return 0;
}


/******************************************************************************/


void show_pt(int id)
{
	PARTITION *pt;
	BLOCK *bk;
	int i;

	pt = &cdb.part[id];
	SSLOG(_INFO, "Part_%d: nblks=%d\n", id, pt->numblocks);
	bk = pt->head;
	for(i=0; i<pt->numblocks; i++){
		SSLOG(_INFO, "  %2d: size=%4d fad=%08x data=%08x\n", i, bk->size, bk->fad, bk->data);
		bk = bk->next;
	}
}


void set_peri_report(void)
{
	int irqs;

	irqs = disable_irq();

	if((ST_STAT&0x1000)==0){
		set_report(cdb.status|STAT_PERI);
	}

	restore_irq(irqs);
}


void disk_task(void *arg)
{
	int retv;
	BLOCK wblk;

	cdb.status = STAT_NODISC;
	set_status(cdb.status);

	int wait_ticks = 10;
	int play_wait_count = 0;
	int lazy_play_state = 0;
	while(1){
_restart_wait:
		wait_ticks = (cdb.play_wait || cdb.block_free!=MAX_BLOCKS)? 1: 2;
		retv = osSemaphoreAcquire(sem_wait_disc, wait_ticks);
		if(retv==osErrorTimeout){
			if(cdb.pause_request){
				goto _restart_nowait;
			}
			if(cdb.play_wait){
				play_wait_count += 1;
				if(play_wait_count==10){
					play_wait_count = 0;
					if(cdb.block_free){
						cdb.play_wait = 0;
						goto _restart_nowait;
					}
				}
			}
			if(next_disc>=0){
				int sd_card_insert(void);
				int sdio_reset(void);
				if(cdb.status==STAT_OPEN){
					if(sd_card_insert()){
						sdio_reset();
						load_disc(next_disc);
						HIRQ = HIRQ_DCHG;
					}
				}else{
					if(sd_card_insert()==0){
						cdb.status = STAT_OPEN;
					}
				}
			}
			HIRQ = HIRQ_SCDQ;
			set_peri_report();

			gif_decode_timer();
			goto _restart_wait;
		}

_restart_nowait:
		if(cdb.pause_request){
			SSLOG(_DTASK, "play_task: Recv PAUSE request!\n");
			cdb.status = STAT_PAUSE;
			cdb.play_type = 0;
			buf_fad_start = 0;
			buf_fad_end = 0;
			cdb.pause_request = 0;
			set_pause_ok();
			goto _restart_wait;
		}
		
		get_sector(cdb.fad, NULL);

		if(cdb.play_fad_start==0 || cdb.play_type==0){
			SSLOG(_DTASK, "play_task: play_type=%d! play_fad_start=%08x!\n", cdb.play_type, cdb.play_fad_start);
			cdb.status = STAT_PAUSE;
			goto _restart_wait;
		}

		if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_track->mode==3){
		}else{
			SSLOG(_DTASK, "\nplay_task! fad_start=%08x(lba_%d) fad_end=%08x fad=%08x type=%d free=%d\n",
					cdb.play_fad_start, cdb.play_fad_start-150, cdb.play_fad_end, cdb.fad, cdb.play_type, cdb.block_free);
		}

		if(cdb.fad==0){
			cdb.fad = cdb.play_fad_start;
		}
		if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_fad_start == cdb.fad){
			if(play_delay){
				int calc_delay = play_delay;
#if 0
				int fadiff = (cdb.old_fad>cdb.fad)? (cdb.old_fad-cdb.fad) : (cdb.fad-cdb.old_fad);
				calc_delay = fadiff/5;
				SSLOG(_DTASK, "calc_delay=%d  fadiff=%d\n", calc_delay, fadiff);
				if(calc_delay>1000)
#endif
				{
					cdb.status = STAT_BUSY;
					hw_delay(10000);
					cdb.status = STAT_SEEK;
					hw_delay(calc_delay);
				}
				lazy_play_state = 1;
			}
		}

		int old_status = cdb.status;
		if(play_delay){
			// SEEK向PLAY状态转换，有一定的延迟。
			if(lazy_play_state==1){
				lazy_play_state = 2;
			}else if(lazy_play_state==2){
				cdb.status = STAT_PLAY;
				lazy_play_state = 0;
			}
		}else{
			cdb.status = STAT_PLAY;
		}
		if(old_status!=cdb.status){
			if(cdb.play_track->mode==3){
				cdb.options = 0x00;
			}
		}

		while(cdb.fad<cdb.play_fad_end){
			if(cdb.pause_request){
				SSLOG(_DTASK, "play_task: Recv PAUSE request!\n");
				cdb.status = STAT_PAUSE;
				cdb.play_type = 0;
				cdb.pause_request = 0;
				set_pause_ok();
				goto _restart_wait;
			}

			retv = get_sector(cdb.fad, &wblk);
			if(retv)
				break;
			led_event(LEDEV_CSCT);
			HIRQ = HIRQ_SCDQ;
			set_peri_report();

			if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_track->mode==3){
				int asize = fill_audio_buffer(wblk.data);
				if(asize<0){
					cdb.play_wait = 1;
					goto _restart_wait;
				}
				if(asize<=4){
					hw_delay(10000);
				}else{
					hw_delay(13500);
				}
			}else
			{
				//printk("filter sector %08x...\n", cdb.fad);
				if(cdb.play_type==PLAYTYPE_SECTOR && sector_delay){
					hw_delay(sector_delay);
				}
				retv = filter_sector(cdb.play_track, &wblk);
				HIRQ = HIRQ_CSCT;

				if(retv==0){
					// 送到某个过滤器成功
				}else if(retv==2){
					SSLOG(_DTASK, "buffer full! wait ...\n");
					// block缓存已满, 需要等待某个事件再继续
					HIRQ = HIRQ_BFUL;
					if(cdb.play_type==PLAYTYPE_FILE){
						HIRQ = HIRQ_EFLS;
					}
					cdb.status = STAT_PAUSE;
					cdb.play_wait = 1;
					goto _restart_wait;
				}else{
					// 无人接收这个扇区
				}
				//printk("filter return %d\n", retv);
			}

			cdb.old_fad = cdb.fad;
			cdb.fad++;
		}

		SSLOG(_DTASK, "\nplay end! fad=%08x end=%08x repcnt=%d maxrep=%d play_type=%d\n",
				cdb.fad, cdb.play_fad_end, cdb.repcnt, cdb.max_repeat, cdb.play_type);

		if(cdb.fad>=cdb.play_fad_end){
			// 本次play结束
			cdb.play_track = NULL;
			if(cdb.play_type==PLAYTYPE_DIR){
				if(handle_diread()==0){
					// 返回0表示本次dir_read完成
					// 返回1表示需要继续读取下一个扇区
					cdb.status = STAT_PAUSE;
					cdb.play_type = 0;
				}else{
					goto _restart_nowait;
				}
			}else{
				if(cdb.repcnt>=cdb.max_repeat){
					if(pend_delay){
						hw_delay(pend_delay);
						cdb.status = STAT_BUSY;
						hw_delay(pend_delay);
					}
					cdb.status = STAT_PAUSE;
					if(cdb.play_type==PLAYTYPE_FILE){
						HIRQ = HIRQ_EFLS;
					}
					HIRQ = HIRQ_PEND;
					cdb.play_type = 0;
					cdb.index = 0;
				}else{
					if(cdb.repcnt<14)
						cdb.repcnt += 1;
					cdb.old_fad = cdb.fad;
					cdb.fad = cdb.play_fad_start;
					cdb.track = fad_to_track(cdb.fad);
					goto _restart_nowait;
				}
			}
		}

		set_peri_report();
	}
}

static void disk_task_notify(void)
{
	if(cdb.play_wait){
		cdb.play_wait = 0;
		disk_task_wakeup();
	}
}


void disk_task_wakeup(void)
{
	osSemaphoreRelease(sem_wait_disc);
}


void set_pause_ok(void)
{
	osSemaphoreRelease(sem_wait_pause);
}

void wait_pause_ok(void)
{
	osSemaphoreAcquire(sem_wait_pause, 2);
}

void cdc_delay(int ticks)
{
	osDelay(ticks);
}

void cdc_dump(int status)
{
	printk("CDB:\n");
	printk("  status     : %02x\n", cdb.status);
	printk("  block_free : %d\n",   cdb.block_free);
	printk("  fad        : %08x\n", cdb.fad);
	
	if(status){
		cdb.status = status;
	}
}

/******************************************************************************/


void ss_cmd_handle(void)
{
	int retv;
	u32 cmd = SS_CMD;

	//SSLOG(_INFO, "scmd_task: %04x\n", SS_CMD);

	switch(cmd){
	case SSCMD_PRINTF:
		// 输出信息
		printk("%s", (char*)(TMPBUFF_ADDR+0x10));
		SS_CMD = 0;
		break;
	case SSCMD_LISTBIN:
		// 列出bin信息
		retv = list_bins(1);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_LISTDISC:
		// 列出镜像信息
		retv = list_disc(SS_ARG, 0);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_LOADDISC:
		// 装载镜像
		retv = load_disc(SS_ARG);
		SS_ARG = retv;
		SS_CMD = 0;

		gif_decode_exit();
		break;
	case SSCMD_CHECK:
	{
		// 检查是否有升级固件
		int fpga = (fpga_update(1)==0)? 1: 0;
		int firm = (flash_update(1)==0)? 1: 0;
		SS_ARG = (fpga<<1) | firm;
		SS_CMD = 0;
		break;
	}
	case SSCMD_UPDATE:
	{
		// 固件升级
		int fpga = (fpga_update(0)>=-1)? 0: 1;
		int firm = (flash_update(0)>=-1)? 0: 1;
		SS_ARG = (fpga<<1) | firm;
		SS_CMD = 0;
		break;
	}
	case SSCMD_FILERD:
	{
		FIL fp;
		int offset = *(u32*)(TMPBUFF_ADDR+0x00);
		int size   = *(u32*)(TMPBUFF_ADDR+0x04);
		int baddr  = *(u32*)(TMPBUFF_ADDR+0x08);
		char *name = (char*)(TMPBUFF_ADDR+0x10);
		int retv = f_open(&fp, name, FA_READ);
		if(retv==FR_OK){
			u32 rsize = 0;
			f_lseek(&fp, offset);
			if(size==0)
				size = f_size(&fp);
			u8 *rbuf = (u8*)((baddr&0x00ffffff)|0x61000000);
			retv = f_read(&fp, rbuf, size, &rsize);
			*(u32*)(TMPBUFF_ADDR+0x04) = rsize;
			f_close(&fp);
			SSLOG(_INFO, "\nSSCMD_FILERD: retv=%d rsize=%08x  %s\n", retv, rsize, name);
		}
		SS_ARG = -retv;
		SS_CMD = 0;
		break;
	}
	case SSCMD_FILEWR:
	{
		FIL fp;
		int offset = *(u32*)(TMPBUFF_ADDR);
		int size = *(u32*)(TMPBUFF_ADDR+0x04);
		int baddr  = *(u32*)(TMPBUFF_ADDR+0x08);
		char *name = (char*)(TMPBUFF_ADDR+0x10);
		int flags = (offset==-1)? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS;
		int retv = f_open(&fp, name, flags|FA_WRITE);
		if(retv==FR_OK){
			u32 wsize = 0;
			u8 *wbuf = (u8*)((baddr&0x00ffffff)|0x61000000);
			if(offset<0)
				offset = 0;
			f_lseek(&fp, offset);
			retv = f_write(&fp, wbuf, size, &wsize);
			*(u32*)(TMPBUFF_ADDR+0x04) = wsize;
			f_close(&fp);
			SSLOG(_INFO, "\nSSCMD_FILEWR: retv=%d wsize=%08x  %s\n", retv, wsize, name);
		}
		SS_ARG = -retv;
		SS_CMD = 0;
		break;
	}
	case SSCMD_SSAVE:
		// 保存当前SAVE
		retv = flush_savefile();
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_LSAVE:
		// 加载指定SAVE
		retv = load_savefile((char*)(TMPBUFF_ADDR+0x10));
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_SMEMS:
		// 保存当前SMEMS
		retv = flush_smems(SS_ARG);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_LMEMS:
		// 加载指定SMEMS块
		retv = load_smems(SS_ARG);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case SSCMD_STARTUP: {
		// 土星端启动!

		// 复位buffer
		buf_fad_start = 0;
		buf_fad_end = 0;
		buf_fad_size = 0;
		// 镜像列表
		list_disc(0, 0);

		// 封面数据
		cover_init();

		// 背景音乐
		int audio_repeat = 0;
		retv = load_pcm("/SAROO/bgsound.pcm");
		if(retv){
			retv = load_pcm("/SAROO/bgsound_r.pcm");
			if(retv==0){
				audio_repeat = 1;
			}
		}
		SS_ARG = retv;
		//SS_CMD = 0;
		if(retv==0){
			cdb.play_fad_start = track_to_fad(0x0100);
			cdb.play_fad_end   = track_to_fad(0x0163);
			cdb.track = 0x01;
			cdb.index = 0x00;
			cdb.fad = cdb.play_fad_start;
			cdb.max_repeat = (audio_repeat)? 0x0f : 0x00;
			cdb.repcnt = 0;
			cdb.options = 0x00;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_SECTOR;
			disk_task_wakeup();
		}

		TIM6->CR1 = 0x0005;
		gif_decode_init();
		break;
	}
	case SSCMD_SELECT: {
		int index = SS_ARG;
		printk("\nSelect %04x\n", index);
		
		if(index&0x8000){
			gif_decode_exit();
		}

		if(index&0x4000){
			gif_decode_init();
		}else{
			load_cover(index);
		}
		break;
	}
	default:
		SSLOG(_INFO, "[SS] unkonw cmd: %04x\n", cmd);
		break;
	}

}


/******************************************************************************/


void sirq_task(void *arg)
{
	u32 stat;

	while(1){
		stat= ST_STAT;
		ST_STAT = stat;
		//printk("  stat=%04x CR1=%04x\n", stat, SSCR1);
		if((stat&0x0007)==0){
			ST_CTRL |= 7;
			osSemaphoreAcquire(sem_wait_irq, osWaitForever);
			continue;
		}

		if(stat&0x0004){
			// 数据传输相关
			trans_handle();
		}

		if(stat&0x0001){
			cdb.cr1 = SSCR1;
			cdb.cr2 = SSCR2;
			cdb.cr3 = SSCR3;
			cdb.cr4 = SSCR4;
			cdc_cmd_process();
		}

		if(stat&0x0002){
			ss_cmd_handle();
		}
		
	}

}

void EXTI1_IRQHandler(void)
{
	EXTI->PR1 = 0x0002;
	ST_CTRL &= ~7;
	//printk("\nST_IRQ!\n");

	osSemaphoreRelease(sem_wait_irq);
}


/******************************************************************************/

osSemaphoreId_t sem_tim6;

void TIM6_DAC_IRQHandler(void)
{
	TIM6->SR = 0;
	osSemaphoreRelease(sem_tim6);
}

void hw_delay(int us)
{
	us = (us+9)/10;
	if(us==0)
		us = 2;
	TIM6->CR1 = 0;
	TIM6->ARR = us;
	TIM6->CNT = 0;
	TIM6->CR1 = 0x000d;

	osSemaphoreAcquire(sem_tim6, osWaitForever);
}

void tim6_init(void)
{
	sem_tim6 = osSemaphoreNew(1, 0, NULL);

	TIM6->CR1 = 0;
	TIM6->PSC = (2000-1); // 200M/2000 = 100k
	TIM6->SR = 0;
	TIM6->DIER = 1;

	NVIC_EnableIRQ(TIM6_DAC_IRQn);
}


void ss_hw_init(void)
{
	// Release FPGA_NRESET
	//GPIOC->BSRR = (1<<5);

	// EXTI1 -> PB1
	SYSCFG->EXTICR[0] &= 0xffffff0f;
	SYSCFG->EXTICR[0] |= 0x00000010;

	EXTI->RTSR1 |= 0x00000002;
	EXTI->FTSR1 &= 0xfffffffd;
	EXTI->IMR1 |= 0x00000002;
	EXTI->PR1   = 0x00000002;

	HIRQ = 0x0be1;
	SSCR1 = ( 0 <<8) | 'C';
	SSCR2 = ('D'<<8) | 'B';
	SSCR3 = ('L'<<8) | 'O';
	SSCR4 = ('C'<<8) | 'K';

	//NVIC_SetPriority(EXTI1_IRQn, 10);
	NVIC_EnableIRQ(EXTI1_IRQn);

	ST_CTRL = 0x0203;

	tim6_init();
}


void ss_sw_init(void)
{
	int i;

	memset(&cdb, 0, sizeof(cdb));

	cdb.status = STAT_PAUSE;
	cdb.fad = 150;
	cdb.ctrladdr = 0x41;
	cdb.track = 1;
	cdb.index = 1;

	cdb.sector_size = 2048;
	cdb.block_free = MAX_BLOCKS;

	for(i=0; i<MAX_BLOCKS; i++){
		cdb.block[i].size = -1;
		cdb.block[i].data = (u8*)(0x2400a000+i*2352);
	}

	u8 *info_buf = (u8*)0x2400a000+MAX_BLOCKS*2352; // 0x72d80

	cdb.FINFO = (u8*)(info_buf+0x00000);
	cdb.TOC  = (u32*)(info_buf+0x00c00);
	cdb.SUBH  = (u8*)(info_buf+0x00da0);
	cdb.tracks = (TRACK_INFO*)(info_buf+0x00e00); // 0x7db80
	memset(cdb.tracks, 0, 100*sizeof(TRACK_INFO));
	cdb.subcode_buf = (u8*)(info_buf+0x0e00+100*sizeof(TRACK_INFO));
	cdb.subrw_buf = cdb.subcode_buf+1536;

	for(i=0; i<MAX_SELECTORS; i++){
		cdb.filter[i].range = 0xffffffff;
		cdb.filter[i].c_false = 0xff;

		cdb.part[i].size = -1;
		cdb.part[i].numblocks = 0;
		cdb.part[i].head = NULL;
		cdb.part[i].tail = NULL;
	}

	cdb.last_buffer = 0xff;
	cdb.cddev_filter = 0xff;
}


void fs_lock(void)
{
	osMutexAcquire(mutex_fs, osWaitForever);
}


void fs_unlock(void)
{
	osMutexRelease(mutex_fs);
}


void saturn_config(void)
{
	FIL fp;
	int retv;
	u32 rv;

	memset((u8*)SYSINFO_ADDR, 0, 0x1000);
	parse_config("/SAROO/saroocfg.txt", NULL);

	if(auto_update){
		int fp, fl;

		fp = fpga_update(1);
		fl = flash_update(1);
		if(fp==-1 && fl==-1){
			// 没有升级文件
			led_event(LEDEV_NONE);
		}else{
			led_event(LEDEV_BUSY);
			if(fp==0){
				fpga_update(0);
			}
			if(fl==0){
				fl = flash_update(0);
				if(fl<-1){
					led_event(LEDEV_FILE_ERROR);
					return;
				}
			}
			led_event(LEDEV_OK);
			return;
		}
	}

	if(debug_flags&0xc0000000){
		led_event(LEDEV_BUSY);
		int cnt = 0;
		while(1){
			retv = mem_test(0x61000000, 0x00800000);
			if(retv){
				led_event(LEDEV_SDRAM_ERROR);
				//return;
			}
			if((debug_flags&0x40000000)==0)
				break;
			cnt += 1;
			printk("  times: %d\n", cnt);
		}
		led_event(LEDEV_NONE);
	}


	// 检查是否有bootrom. 如果有,就加载到FPGA中
	retv = f_open(&fp, "/SAROO/ssfirm.bin", FA_READ);
	if(retv){
		printk("NO bootrom file found!\n");
		led_event(LEDEV_NOFIRM);
		return;
	}
	int fsize = f_size(&fp);
	printk("Found Saturn bootrom file.\n");
	printk("    Size %08x\n", fsize);

	rv = 0;
	retv = f_read(&fp, (void*)FIRM_ADDR, fsize, &rv);
	printk("    f_read: retv=%d rv=%08x\n", retv, rv);
	f_close(&fp);


	// 打开存档文件
	open_savefile();


	// 放置系统信息
	*(u32*)(SYSINFO_ADDR+0x00) = get_build_date(); // 编译日期
	*(u32*)(SYSINFO_ADDR+0x04) = lang_id;          // 菜单语言
	*(u32*)(SYSINFO_ADDR+0x08) = debug_flags;      // 调试选项

	// I2S
	spi2_init();
	spi2_set_notify(disk_task_notify);

	ss_hw_init();
	ss_sw_init();

	sem_wait_irq   = osSemaphoreNew(1, 0, NULL);
	sem_wait_disc  = osSemaphoreNew(1, 0, NULL);
	sem_wait_pause = osSemaphoreNew(1, 0, NULL);
	mutex_fs = osMutexNew(NULL);

	osThreadAttr_t attr;
	memset(&attr, 0, sizeof(attr));

	attr.priority = osPriorityNormal+7;
	osThreadNew(sirq_task, NULL, &attr);

	attr.priority = osPriorityHigh;
	osThreadNew(disk_task, NULL, NULL);

}

/******************************************************************************/

