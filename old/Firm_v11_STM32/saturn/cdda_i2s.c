
#include "../main.h"
#include <stdlib.h>
#include "../fatfs/ff.h"
#include "cdc.h"

/******************************************************************************/


#define ABUF_NUM 2
static u8 awp, arp, aflags[ABUF_NUM];
static u8 audio_buf[ABUF_NUM][2352];


/******************************************************************************/


static void (*spi2_dma_notify)(void) = NULL;

void spi2_set_notify(void *handle)
{
	spi2_dma_notify = (void(*)(void))handle;
}


void spi2_init(void)
{
	awp = 0;
	arp = 0;
	aflags[0] = 0;
	aflags[1] = 0;

	SPI2->CR1  = 0;
	SPI2->CR2  = 0;
	SPI2->I2SCFGR = 0x0a10;
	SPI2->I2SPR   = 0x0119;

	DMA1_Channel5->CCR = 0;
	NVIC_SetPriority(DMA1_Channel5_IRQn, 4);
	NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}


void spi2_dma_start(void *buf, int len)
{
	DMA1_Channel5->CCR = 0;
	DMA1_Channel5->CNDTR = len/2;
	DMA1_Channel5->CPAR = (u32)&(SPI2->DR);
	DMA1_Channel5->CMAR = (u32)buf;
	DMA1_Channel5->CCR = 0x059b;  // MSIZE:16 PSIZE:16 MINC TE TCIE EN

	if(SPI2->CR2==0){
		SPI2->CR2  = 0x0002;
		SPI2->I2SCFGR |= 0x0400;
	}
}


void DMA1_Channel5_IRQHandler(void)
{
	DMA1_Channel5->CCR = 0;
	DMA1->IFCR = 0x000f0000;
	//printk("DMA1S5: CCR=%08x\n", DMA1_Channel5->CCR);

	aflags[arp] = 0;
	arp = (arp+1)%ABUF_NUM;
	if(aflags[arp]==0){
		SPI2->I2SCFGR &= 0xfbff;
		SPI2->CR2 = 0;
	}else{
		spi2_dma_start(audio_buf[arp], 2352);
		if(spi2_dma_notify)
			spi2_dma_notify();
	}
}


void spi2_trans(u32 data)
{
	while((SPI2->SR&0x0002)==0);
	SPI2->DR = data;
}


/******************************************************************************/


int fill_audio_buffer(u8 *data)
{
	if(aflags[awp])
		return -1;
	memcpy32(audio_buf[awp], data, 2352);

	int key = disable_irq();

	aflags[awp] = 1;
	awp = (awp+1)%ABUF_NUM;
	spi2_dma_start(audio_buf[arp], 2352);

	restore_irq(key);

	return 0;
}


/******************************************************************************/


// test
void play_i2s(void)
{
	char *audio_file = "/srmp7.bin";

	FIL fp;
	int retv, size;
	u32 rv;

	retv = f_open(&fp, audio_file, FA_READ);
	if(retv){
		printk("Failed open audio file!\n");
		return;
	}
	size = f_size(&fp);
	printk("Open %s ...\n", audio_file);
	printk("    Size %08x\n", size);

	int count = 0;
	int sec = 0;
	//size = 2352*1024;
	while(size>0){
		rv = 0;
		f_read(&fp, (void*)0x61a80000, 2352*8, &rv);
		
		int i;
		for(i=0; i<rv; i+=2352){
			while(fill_audio_buffer((u8*)0x61a80000+i)<0){
				os_dly_wait(1);
			}
		}

		size -= rv;
		count += rv;
		if(count>176400){
			printk("%3d ...\n", sec);
			sec += 1;
			count -= 176400;
		}
	}

	f_close(&fp);
}


/******************************************************************************/



