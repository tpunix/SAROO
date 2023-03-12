
#include "main.h"
#include "ff.h"
#include "cdc.h"

/******************************************************************************/

osSemaphoreId_t sem_wait_irq;
osSemaphoreId_t sem_wait_disc;

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
		printk("track %2d: fad_0=%08x fad_start=%08x fad_end=%08x offset=%08x %d\n",
					i, cdb.tracks[i].fad_0, cdb.tracks[i].fad_start, cdb.tracks[i].fad_end,
					cdb.tracks[i].file_offset, cdb.tracks[i].sector_size);
	}

	last = &cdb.tracks[cdb.track_num-1];
	cdb.TOC[99]  = bswap32((cdb.tracks[0].ctrl_addr<<24) | 0x010000);
	cdb.TOC[100] = bswap32((last->ctrl_addr<<24) | (cdb.track_num<<16));
	cdb.TOC[101] = bswap32((last->ctrl_addr<<24) | (last->fad_end+1));
	
	cdb.status = STAT_PAUSE;
	set_status(cdb.status);
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
	index = track_index&0xff;

	if(index==0x01){
		return cdb.tracks[t].fad_start;
	} else if(index==0x63){
		return cdb.tracks[t].fad_end;
	}

	return cdb.tracks[t].fad_start;
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


void show_pt(void)
{
	PARTITION *pt;
	FILTER *ft;

	if(cdb.cddev_filter==0xff){
		return;
	}
	ft = &cdb.filter[cdb.cddev_filter];
	pt = &cdb.part[ft->c_true];

	printk("           : Part_%d: nblks=%d\n", ft->c_true, pt->numblocks);


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

u32 disk_run_count;

void disk_task(void *arg)
{
	// 内部扇区缓存, 32K大小, 大概16个2k的扇区. 位于SDRAM中
	// FATFS层以512字节为单位读取, 每次读64个单位.
	u8 *sector_buffer = (u8*)0x24002000;

	// 内部缓存中保存的FAD扇区范围
	u32 buf_fad_start;
	u32 buf_fad_end;
	// 由于存在2352这样的扇区格式, FAD扇区不是2048字节对齐的.
	// FATFS层以512为单位读取数据. FAD扇区相对于sector_buffer有一个偏移.
	u32 buf_fad_offset;
	// 内部缓存中的扇区大小
	u32 buf_fad_size;


	int retv, dp, nread;
	BLOCK wblk;
	TRACK_INFO *play_track;

	cdb.status = STAT_NODISC;
	
	list_disc(0);

	play_track = NULL;
	buf_fad_start = 0;
	buf_fad_end = 0;
	buf_fad_offset = 0;
	buf_fad_size = 0;

	disk_run_count = 0;

	while(1){
_restart_wait:
		retv = osSemaphoreAcquire(sem_wait_disc, 10);
		disk_run_count += 1;
		if(retv==osErrorTimeout){
			if(cdb.pause_request){
				goto _restart_nowait;
			}
			if(cdb.play_wait && cdb.block_free){
				cdb.play_wait = 0;
				goto _restart_nowait;
			}
			HIRQ = HIRQ_SCDQ;
			set_peri_report();
#if 0
			if((disk_run_count&0x1f)==0){
				printk("\nPERI Report: Status=%04x HIRQ=%04x count=%08x\n", RESP1, HIRQ, disk_run_count);
				printk("           : play_wait=%d free_block=%d\n", cdb.play_wait, cdb.block_free);
				show_pt();
			}
#endif
			goto _restart_wait;
		}

_restart_nowait:
		if(cdb.pause_request){
			printk("play_task: Recv PAUSE request!\n");
			cdb.status = STAT_PAUSE;
			cdb.play_type = 0;
			buf_fad_start = 0;
			buf_fad_end = 0;
			cdb.pause_request = 0;
			goto _restart_wait;
		}
		if(cdb.play_fad_start==0 || cdb.play_type==0){
			printk("play_task: play_type=%d! play_fad_start=%08x!\n", cdb.play_type, cdb.play_fad_start);
			cdb.status = STAT_PAUSE;
			goto _restart_wait;
		}

		if(cdb.play_type!=PLAYTYPE_FILE && play_track->mode==3){
		}else{
			printk("\nplay_task! fad_start=%08x fad_end=%08x fad=%08x type=%d\n",
					cdb.play_fad_start, cdb.play_fad_end, cdb.fad, cdb.play_type);
		}

		if(cdb.fad==0){
			cdb.fad = cdb.play_fad_start;
		}
		cdb.status = STAT_PLAY;

		while(cdb.fad<cdb.play_fad_end){
			if(cdb.pause_request){
				printk("play_task: Recv PAUSE request!\n");
				cdb.status = STAT_PAUSE;
				cdb.play_type = 0;
				cdb.pause_request = 0;
				goto _restart_wait;
			}

			int fad = cdb.fad;
			// 先查找track信息
			if(play_track==NULL || fad<play_track->fad_start || fad>play_track->fad_end){
				cdb.track = fad_to_track(fad);
				if(cdb.track!=0xff){
					play_track = &cdb.tracks[cdb.track-1];
				}else{
					// play的FAD参数错误
					printk("play_track not found!\n");
					cdb.status = STAT_ERROR;
					cdb.play_type = 0;
					break;
				}
			}

			if(fad>=buf_fad_start && fad<buf_fad_end){
				// 在内部缓存中找到了要play的扇区
				//printk(" fad_%08x found at buffer!\n", fad);
				dp = buf_fad_offset+(fad-buf_fad_start)*buf_fad_size;
				wblk.data = sector_buffer+dp;
			}else{
				// 内部缓存中没有要play的扇区. 从文件重新读取.
				//printk(" fad_%08x not found. need read from file.\n", fad);

				cdb.ctrladdr = play_track->ctrl_addr;
				cdb.index = 1;

				buf_fad_start = fad;
				buf_fad_size = play_track->sector_size;

				dp = play_track->file_offset+(fad-play_track->fad_start)*play_track->sector_size;
				buf_fad_offset = dp&0x1ff;
				dp &= ~0x1ff;

				//printk("  seek at %08x\n", dp);
				retv = f_lseek(play_track->fp, dp);
				retv = f_read(play_track->fp, sector_buffer, 0x8000, (u32*)&nread);
				if(retv==0){
					int num_fad = (nread-buf_fad_offset)/buf_fad_size;
					buf_fad_end = buf_fad_start+num_fad;
				}else{
					printk("f_read error!\n");
					cdb.status = STAT_ERROR;
					cdb.play_type = 0;
					buf_fad_end = 0;
					break;
				}

				wblk.data = sector_buffer+buf_fad_offset;
			}
			HIRQ = HIRQ_SCDQ;
			set_peri_report();


			wblk.size = buf_fad_size;
			wblk.fad = fad;
			if(buf_fad_size==2352 && wblk.data[0x0f]==0x02){
				wblk.fn = wblk.data[0x10];
				wblk.cn = wblk.data[0x11];
				wblk.sm = wblk.data[0x12];
				wblk.ci = wblk.data[0x13];
			}

			if(cdb.play_type!=PLAYTYPE_FILE && play_track->mode==3){
				if(fill_audio_buffer(wblk.data)<0){
					cdb.play_wait = 1;
					goto _restart_wait;
				}
			}else
			{
				//printk("filter sector %08x...\n", fad);
				retv = filter_sector(play_track, &wblk);
				HIRQ = HIRQ_CSCT;
				if(retv==0){
					// 送到某个过滤器成功
				}else if(retv==2){
					printk("buffer full! wait ...\n");
					// block缓存已满, 需要等待某个事件再继续
					cdb.status = STAT_PAUSE;
					cdb.play_wait = 1;
					goto _restart_wait;
				}else{
					// 无人接收这个扇区
				}
				//printk("filter return %d\n", retv);
			}

			cdb.fad++;
		}

		printk("\nplay end! fad=%08x end=%08x repcnt=%d maxrep=%d play_type=%d\n",
				cdb.fad, cdb.play_fad_end, cdb.repcnt, cdb.max_repeat, cdb.play_type);

		if(cdb.fad>=cdb.play_fad_end){
			// 本次play结束
			play_track = NULL;
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
					cdb.status = STAT_PAUSE;
					if(cdb.play_type==PLAYTYPE_FILE){
						HIRQ = HIRQ_EFLS;
					}
					HIRQ = HIRQ_PEND;
					cdb.play_type = 0;
				}else{
					if(cdb.repcnt<14)
						cdb.repcnt += 1;
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


void cdc_delay(int ticks)
{
	osDelay(ticks);
}


/******************************************************************************/


void ss_cmd_handle(void)
{
	int retv;
	u32 cmd = SS_CMD;

	printk("scmd_task: %04x\n", SS_CMD);

	switch(cmd){
	case 0x0001:
		// 输出信息
		printk("%s", (char*)0x61820010);
		SS_CMD = 0;
		break;
	case 0x0002:
		// 列出镜像信息
		retv = list_disc(0);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case 0x0003:
		// 装载镜像
		retv = load_disc(SS_ARG);
		SS_ARG = retv;
		SS_CMD = 0;
		break;
	case 0x0004:
	{
		// 检查是否有升级固件
		int fpga = (fpga_update(1)==0)? 1: 0;
		int firm = (flash_update(1)==0)? 1: 0;
		SS_ARG = (fpga<<1) | firm;
		SS_CMD = 0;
		break;
	}
	case 0x0005:
	{
		// 固件升级
		int fpga = (fpga_update(0)>=-1)? 0: 1;
		int firm = (flash_update(0)>=-1)? 0: 1;
		SS_ARG = (fpga<<1) | firm;
		SS_CMD = 0;
		break;
	}
	default:
		printk("[SS] unkonw cmd: %04x\n", cmd);
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

void saturn_config(void)
{
	FIL fp;
	int retv;
	u32 rv;

	// 检查是否有bootrom. 如果有,就加载到FPGA中
	retv = f_open(&fp, "/ramimage.bin", FA_READ);
	if(retv){
		printk("NO bootrom file found!\n");
		return;
	}
	printk("Found Saturn bootrom file.\n");
	printk("    Size %08x\n", f_size(&fp));

	rv = 0;
	retv = f_read(&fp, (void*)0x61000000, f_size(&fp), &rv);
	printk("    f_read: retv=%d rv=%08x\n", retv, rv);
	f_close(&fp);

	// I2S
	spi2_init();
	spi2_set_notify(disk_task_notify);

	ss_hw_init();
	ss_sw_init();

	sem_wait_irq  = osSemaphoreNew(1, 0, NULL);
	sem_wait_disc = osSemaphoreNew(1, 0, NULL);

	osThreadAttr_t attr;
	memset(&attr, 0, sizeof(attr));

	attr.priority = osPriorityNormal+7;
	osThreadNew(sirq_task, NULL, &attr);

	attr.priority = osPriorityHigh;
	osThreadNew(disk_task, NULL, NULL);

}

/******************************************************************************/

