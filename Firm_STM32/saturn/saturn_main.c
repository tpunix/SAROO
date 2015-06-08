
#include "../main.h"
#include "../fatfs/ff.h"

#include "cdc.h"

/******************************************************************************/

OS_SEM sem_wait_cmd;
OS_SEM sem_wait_delay;
OS_SEM sem_wait_disc;

void hw_delay_us(int us);


/******************************************************************************/

// Track 信息
int track_num;
TRACK_INFO tracks[MAX_TRACKS];


#include "Langrisser3.h"

u32 bswap32(u32 d)
{
	return ((d&0xff)<<24) | ((d&0xff00)<<8) | ((d>>8)&0xff00) | ((d>>24)&0xff);
}

int load_l3(void)
{
	int i, retv;
	TRACK_INFO *last;
	int *track_list;
	char *iso_name;

#if 0
	iso_name   = l3_name;
	track_list = l3_track_fad;
	track_num  = l3_track_num;
#endif
#if 1
	iso_name   = sakura1_name;
	track_list = sakura1_track_fad;
	track_num  = sakura1_track_num;
#endif

	retv = f_open(&cdb.fp, iso_name, FA_READ);
	if(retv){
		printk("Open %s failed!\n", iso_name);
		return -1;
	}

	for(i=0; i<track_num; i++){
		tracks[i].fad_start = track_list[i]+150;
		tracks[i].sector_size = 2352;
		tracks[i].ctrl_addr = 0x01;
		tracks[i].file_offset = track_list[i]*2352;
		tracks[i].sector_size = 2352;
	}
	tracks[i].fad_start = 0xffffffff;
	tracks[i].file_offset = 0;

	for(i=0; i<track_num; i++){
		tracks[i].fad_end = tracks[i+1].fad_start-1;
	}

	last = &tracks[track_num-1];
	last->fad_end = (f_size(&cdb.fp)-last->file_offset)/2352;
	last->fad_end = last->fad_start+last->fad_end-1;

	tracks[0].ctrl_addr = 0x41;


	for(i=0; i<102; i++){
		cdb.TOC[i] = 0xffffffff;
	}

	printk("ISO file %s:\n", iso_name);
	for(i=0; i<track_num; i++){
		cdb.TOC[i] = bswap32((tracks[i].ctrl_addr<<24) | tracks[i].fad_start);
		printk("track %2d: fad_start=%08x fad_end=%08x file_offset=%08x sector_size=%d\n",
					i, tracks[i].fad_start, tracks[i].fad_end, tracks[i].file_offset, tracks[i].sector_size);
	}

	cdb.TOC[99]  = bswap32((tracks[0].ctrl_addr<<24) | 0x010000);
	cdb.TOC[100] = bswap32((last->ctrl_addr<<24) | (track_num<<16));
	cdb.TOC[101] = bswap32((last->ctrl_addr<<24) | (last->fad_end+1));

	return 1;
}


u32 track_to_fad(u16 track_index)
{
	int t, index;

	if(track_index==0xffff){
		return tracks[track_num-1].fad_end;
	}else if(track_index==0){
		return 0;
	}

	t     = (track_index>>8)-1;
	index = track_index&0xff;

	if(index==0x01){
		return tracks[t].fad_start;
	} else if(index==0x63){
		return tracks[t].fad_end;
	}

	return tracks[t].fad_start;
}

int fad_to_track(u32 fad)
{
	int i;

	for(i=0; i<track_num; i++){
		if(i>=tracks[i].fad_start && i<=tracks[i].fad_end)
			return i+1;
	}

	return 0xff;
}

TRACK_INFO *get_track_info(int track_index)
{
	int t;

	if(track_index==0xffff){
		t = track_num-1;
	}else if(track_index==0){
		return NULL;
	}else{
		t = (track_index>>8)-1;
	}

	if(t>=track_num)
		return NULL;
	
	return &tracks[t];
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

	if(cdb.in_cmd==0){
		set_report(cdb.status|STAT_PERI);
	}

	restore_irq(irqs);
}

u32 disk_run_count;

__task void disk_task(void)
{
	// 内部扇区缓存, 32K大小, 大概16个2k的扇区. 位于SDRAM中
	// FATFS层以512字节为单位读取, 每次读64个单位.
	u8 *sector_buffer;
	// 内部缓存中保存的FAD扇区范围
	u32 buf_fad_start;
	u32 buf_fad_end;
	// 由于存在2352这样的扇区格式, FAD扇区不是2048字节对齐的.
	// FATFS层以512为单位读取数据. FAD扇区相对于sector_buffer有一个偏移.
	u32 buf_fad_offset;
	// 内部缓存中的扇区大小
	u32 buf_fad_size;


	int i, j, retv;
	int dp, nread, num_fad;
	BLOCK wblk;
	TRACK_INFO *play_track;

	if(load_l3()){
		cdb.status = STAT_PAUSE;
	}else{
		cdb.status = STAT_NODISC;
	}

	sector_buffer =(u8*)0x61a80000;
	play_track = NULL;
	buf_fad_start = 0;
	buf_fad_end = 0;
	buf_fad_offset = 0;
	buf_fad_size = 0;

	disk_run_count = 0;

	while(1){
_restart_wait:
		retv = os_sem_wait(&sem_wait_disc, 10);
		disk_run_count += 1;
		if(retv==OS_R_TMO){
			HIRQ = HIRQ_SCDQ;
			set_peri_report();
			if((disk_run_count&0x1f)==0){
				printk("\nPERI Report: Status=%04x HIRQ=%04x count=%08x\n", RESP1, HIRQ, disk_run_count);
				printk("           : play_wait=%d free_block=%d\n", cdb.play_wait, cdb.block_free);
				show_pt();
			}
			goto _restart_wait;
		}

_restart_nowait:
		if(cdb.pause_request){
			printk("play_task: Recv PAUSE request!\n");
			cdb.status = STAT_PAUSE;
			cdb.play_type = 0;
			cdb.pause_request = 0;
			goto _restart_wait;
		}
		if(cdb.play_fad_start==0 || cdb.play_type==0){
			printk("play_task: play_type==0!\n");
			cdb.status = STAT_PAUSE;
			goto _restart_wait;
		}

		printk("\nplay_task! fad_start=%08x fad_end=%08x fad=%08x type=%d\n",
					cdb.play_fad_start, cdb.play_fad_end, cdb.fad, cdb.play_type);

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

			i = cdb.fad;
			if(i>=buf_fad_start && i<buf_fad_end){
				// 在内部缓存中找到了要play的扇区
				//printk(" fad_%08x found at buffer!\n", i);
				dp = buf_fad_offset+(i-buf_fad_start)*buf_fad_size;
				wblk.data = sector_buffer+dp;
			}else{
				// 内部缓存中没有要play的扇区. 从文件重新读取.
				//printk(" fad_%08x not found. need read from file.\n", i);

				// 先查找track信息
				if(play_track==NULL || i<play_track->fad_start || i>play_track->fad_end){
					play_track = NULL;
					for(j=0; j<track_num; j++){
						//printk("track %2d: fad_start=%08x fad_end=%08x\n",
						//			j, tracks[j].fad_start, tracks[j].fad_end);
						if(i>=tracks[j].fad_start && i<=tracks[j].fad_end){
							play_track = &tracks[j];
							cdb.track = j+1;
						}
					}
				}
				if(play_track==NULL){
					// play的FAD参数错误
					printk("play_track not found!\n");
					cdb.status = STAT_ERROR;
					cdb.play_type = 0;
					break;
				}
				cdb.ctrladdr = play_track->ctrl_addr;
				cdb.index = 1;

				buf_fad_start = i;
				buf_fad_size = play_track->sector_size;

				dp = play_track->file_offset+(i-play_track->fad_start)*play_track->sector_size;
				buf_fad_offset = dp&0x1ff;
				dp &= ~0x1ff;

				//printk("  seek at %08x\n", dp);
				retv = f_lseek(&cdb.fp, dp);
				retv = f_read(&cdb.fp, sector_buffer, 0x8000, (u32*)&nread);

				num_fad = (nread-buf_fad_offset)/buf_fad_size;
				buf_fad_end = buf_fad_start+num_fad;

				wblk.data = sector_buffer+buf_fad_offset;
			}
			HIRQ = HIRQ_SCDQ;
			set_peri_report();


			wblk.size = buf_fad_size;
			wblk.fad = i;
			if(buf_fad_size==2352 && wblk.data[0x0f]==0x02){
				wblk.fn = wblk.data[0x10];
				wblk.cn = wblk.data[0x11];
				wblk.sm = wblk.data[0x12];
				wblk.ci = wblk.data[0x13];
			}


			//printk("filter sector %08x...\n", i);
			retv = filter_sector(&wblk);
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
			//hw_delay_us(4000);

			cdb.fad++;
		}

		printk("\nplay end! fad=%08x end=%08x repcnt=%d maxrep=%d play_type=%d\n",
				cdb.fad, cdb.play_fad_end, cdb.repcnt, cdb.max_repeat, cdb.play_type);

		if(cdb.fad>=cdb.play_fad_end){
			// 本次play结束
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

/******************************************************************************/

__task void scmd_task(void)
{
	u32 cmd;


	while(1){
		os_sem_wait(&sem_wait_cmd, 0xffff);

		cmd = SCMD;

		switch(cmd){
		case 0x0001:
			//printk("[SS] %s", (char*)0x61800010);
			printk("%s", (char*)0x61800010);
			break;

		default:
			printk("[SS] unkonw cmd: %04x\n", cmd);
			break;
		}

		SCMD = 0;
	}
}

/******************************************************************************/

// 用定时器, 让每个中断结束后, 延时一段时间后再设置CMOK.

void hw_delay_us(int us)
{
	TIM6->ARR = us;
	TIM6->CNT = 0;
	TIM6->CR1 |= 0x0001;
	os_sem_wait(&sem_wait_delay, 0xffff);
}

void TIM6_IRQHandler(void)
{
	TIM6->SR = 1;
	TIM6->SR = 0;
	isr_sem_send(&sem_wait_delay);
}

void cmok_init(void)
{
	TIM6->CR1 = 0x000c;
	TIM6->CR2 = 0x0000;
	TIM6->PSC = 72-1; /* 1MHz/1us*/
	TIM6->ARR = 6000;
	TIM6->CNT = 0;
	TIM6->SR = 1;
	TIM6->DIER = 1;

	NVIC_SetPriority(TIM6_IRQn, 10);
	NVIC_EnableIRQ(TIM6_IRQn);
}

void cmok_gen(void)
{
	TIM6->CR1 |= 0x0001;
}

/******************************************************************************/


void EXTI1_IRQHandler(void)
{
	u32 stat;

	while(ST_STAT&0x4000){
		stat= ST_STAT;

		if(stat&0x0004){
			// 数据传输相关
			trans_handle();
		}

		if(stat&0x0001){
			int irqs = disable_irq();
			cdb.cr1 = SSCR1;
			cdb.cr2 = SSCR2;
			cdb.cr3 = SSCR3;
			cdb.cr4 = SSCR4;
			cdb.in_cmd = 1;
			cdc_cmd_process();
			restore_irq(irqs);
		}
		if(stat&0x0008){
			cdb.in_cmd = 0;
		}

		if(stat&0x0002){
			isr_sem_send(&sem_wait_cmd);
		}

		ST_STAT = stat;
		EXTI->PR = 0x0002;
	}

}


/******************************************************************************/


void ss_hw_init(void)
{
	cmok_init();

	// PB1: high level interrupt
	//    : input pull low
	GPIOB->CRL &= 0xffffff0f;
	GPIOB->CRL |= 0x00000080;
	GPIOB->ODR &= 0xfffffffd;

	// EXTI1 -> PB1
	AFIO->EXTICR[0] &= 0xffffff0f;
	AFIO->EXTICR[0] |= 0x00000010;

	EXTI->IMR  |= 0x00000002;
	EXTI->RTSR |= 0x00000002;
	EXTI->FTSR &= 0xfffffffd;
	EXTI->PR    = 0x00000002;


	SSCR1 = ( 0 <<8) | 'C';
	SSCR2 = ('D'<<8) | 'B';
	SSCR3 = ('L'<<8) | 'O';
	SSCR4 = ('C'<<8) | 'K';

	NVIC_SetPriority(EXTI1_IRQn, 10);
	NVIC_EnableIRQ(EXTI1_IRQn);

	ST_CTRL = 0x000f;

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
		cdb.block[i].data = (u8*)(0x61a00000+i*2560);
	}

	cdb.FINFO = (u8*)0x61a7e000;
	cdb.TOC  = (u32*)0x61a7f000;
	cdb.SUBH  = (u8*)0x61a7f200;

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
	retv = f_open(&fp, "/romimage.bin", FA_READ);
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

	// 加载SAKURA文件,用于测试
	retv = f_open(&fp, "/SAROOO/SAKURA", FA_READ);
	if(retv==0){
		rv = 0;
		retv = f_read(&fp, (void*)0x61100000, f_size(&fp), &rv);
		printk("    f_read: retv=%d rv=%08x\n", retv, rv);
		f_close(&fp);
	}




	os_sem_init(&sem_wait_cmd, 0);
	os_sem_init(&sem_wait_delay, 0);
	os_sem_init(&sem_wait_disc,0);

	os_tsk_create(scmd_task, 10);

	ss_hw_init();
	ss_sw_init();

	os_tsk_create(disk_task, 5);

}

/******************************************************************************/

