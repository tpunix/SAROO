
#include "main.h"
#include <stdlib.h>
#include "ff.h"
#include "cdc.h"

/******************************************************************************/

// 使用AHB SRAM1做音频buffer. 每个2352字节, 共32个.

#define ABUF_NUM  32
//#define ABUF_BASE 0x30002000
#define ABUF_BASE 0x30020000
//#define ABUF_BASE 0x61a90000


static int awp, arp, afull, aplay;
static u8 *audio_buf[ABUF_NUM];


/******************************************************************************/


static void (*spi2_dma_notify)(void) = NULL;

void spi2_set_notify(void *handle)
{
	spi2_dma_notify = handle;
}


void spi2_init(void)
{
	int i;
	for(i=0; i<ABUF_NUM; i++){
		audio_buf[i] = (u8*)(0x30002000+i*2352);
	}

	awp = 0;
	arp = 0;
	afull = 0;

	SPI2->CR1  = 0x00000000;
	SPI2->I2SCFGR = 0x02040015;
	SPI2->CR2  = 0;
	SPI2->CFG1 = 0x00000000;
	SPI2->CFG2 = 0x00000000;
	SPI2->IER  = 0x00000000;
	SPI2->CR1  = 0x00000001;
	SPI2->CR1  = 0x00000201;

	DMAMUX1_Channel1->CCR = 40;  // SPI2_TX
	NVIC_EnableIRQ(DMA1_Stream1_IRQn);
}

void spi2_dma_start(void *buf0, void *buf1, int len)
{
	__DMB();

	DMA1_Stream1->CR = 0;
	DMA1->LIFCR = 0x00000f40;
	DMA1_Stream1->NDTR = len/2;     // PSIZE count
	DMA1_Stream1->PAR = (u32)&SPI2->TXDR;
	DMA1_Stream1->M0AR = (u32)buf0;
	DMA1_Stream1->M1AR = (u32)buf1;
	DMA1_Stream1->CR = 0x00042d51;  // DBUF MSIZE:16 PSIZE:16 MINC CIRC M2P TCIE EN

	SPI2->CR1  = 0x00000000;
	SPI2->CFG1 = 0x00008000;
	SPI2->CR1  = 0x00000001;
	SPI2->CR1  = 0x00000201;
}

void DMA1_Stream1_IRQHandler(void)
{
	int next;
	DMA1->LIFCR = 0x00000800;
	//printk("DMA1S1: CR=%08x\n", DMA1_Stream1->CR);

	arp  = (arp+1)&(ABUF_NUM-1);
	afull = 0;
	if(arp==awp){
		SPI2->CR1 = 0;
		DMA1_Stream1->CR = 0;
		aplay = 0;
	}else{
		next = (arp+1)&(ABUF_NUM-1);
		if(DMA1_Stream1->CR & (1<<19)){
			DMA1_Stream1->M0AR = (u32)audio_buf[next];
		}else{
			DMA1_Stream1->M1AR = (u32)audio_buf[next];
		}
		if(spi2_dma_notify)
			spi2_dma_notify();
	}
}


void spi2_trans(u32 data)
{
	if(SPI2->SR&0x0020)
		SPI2->IFCR = 0x0020;
	while((SPI2->SR&SPI_SR_TXP)==0);
	*(u16*)&SPI2->TXDR = data;
}


/******************************************************************************/


static int abuf_size(void)
{
	if(afull)
		return ABUF_NUM;
	else if(awp>arp)
		return awp-arp;
	else
		return ABUF_NUM+awp-arp;
}

int fill_audio_buffer(u8 *data)
{
	int asize;

	if(afull)
		return -1;

	memcpy32(audio_buf[awp], data, 2352);

	int key = disable_irq();

	awp = (awp+1)&(ABUF_NUM-1);
	if(awp==arp)
		afull = 1;

	asize = abuf_size();
	if(aplay==0 && asize>=2){
		u8 *buf0 = audio_buf[arp];
		u8 *buf1 = audio_buf[(arp+1)&(ABUF_NUM-1)];
		spi2_dma_start(buf0, buf1, 2352);
		aplay = 1;
	}

	restore_irq(key);

	return asize;
}


/******************************************************************************/


// test
void play_i2s(char *audio_file)
{
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
		f_read(&fp, (void*)0x24002000, 2352*8, &rv);
		
		int i;
		for(i=0; i<rv; i+=2352){
			while(fill_audio_buffer((u8*)0x24002000+i)<0){
				osDelay(1);
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



