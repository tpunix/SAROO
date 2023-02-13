
/*
 *  stm32_sdio.c
 *    low level driver for STM32F10x SDIO
 *    only SD card suport. no MMC support.
 *
 *  writen by tpu, 2014-10-28
 */

#include "string.h"

#include "main.h"


/******************************************************************************/

#define SDCLK_LOW  198
#define SDCLK_HIGH 4

#define RESP_NONE(cmd)  ((cmd&0x00c0)==0x0000)
#define RESP_SHORT(cmd) ((cmd&0x00c0)==0x0040)
#define RESP_LONG(cmd)  ((cmd&0x00c0)==0x00c0)
#define RESP_NOCRC(cmd) ((cmd&0x0400)!=0x0000)

#define HAS_DATA_R(cmd) ((cmd&0x0100)==0x0100)
#define HAS_DATA_W(cmd) ((cmd&0x0200)==0x0200)
#define HAS_DATA(cmd)   ((cmd&0x0300)!=0x0000)
#define NO_DATA(cmd)    ((cmd&0x0300)==0x0000)

#define CMD_RESP_NONE   0x0000
#define CMD_RESP_SHORT  0x0040
#define CMD_RESP_LONG   0x00c0
#define CMD_RESP_NOCRC  0x0400
#define CMD_HAS_DATA_R  0x0100
#define CMD_HAS_DATA_W  0x0200

#define CMD_00  ( 0 | CMD_RESP_NONE)
#define CMD_02  ( 2 | CMD_RESP_LONG  | CMD_RESP_NOCRC)
#define CMD_03  ( 3 | CMD_RESP_SHORT)
#define CMD_06  ( 6 | CMD_RESP_SHORT)
#define CMD_07  ( 7 | CMD_RESP_SHORT)
#define CMD_08  ( 8 | CMD_RESP_SHORT)
#define CMD_09  ( 9 | CMD_RESP_LONG  | CMD_RESP_NOCRC)
#define CMD_12  (12 | CMD_RESP_SHORT)
#define CMD_13  (13 | CMD_RESP_SHORT)
#define CMD_16  (16 | CMD_RESP_SHORT)
#define CMD_17  (17 | CMD_RESP_SHORT | CMD_HAS_DATA_R)
#define CMD_18  (18 | CMD_RESP_SHORT | CMD_HAS_DATA_R)
#define CMD_23  (23 | CMD_RESP_SHORT)
#define CMD_24  (24 | CMD_RESP_SHORT | CMD_HAS_DATA_W)
#define CMD_25  (25 | CMD_RESP_SHORT | CMD_HAS_DATA_W)
#define CMD_41  (41 | CMD_RESP_SHORT | CMD_RESP_NOCRC)
#define CMD_55  (55 | CMD_RESP_SHORT)

#define CMD_ERROR  (SDIO_STA_CTIMEOUT | SDIO_STA_CCRCFAIL)
#define DATA_ERROR (SDIO_STA_STBITERR | SDIO_STA_RXOVERR | SDIO_STA_TXUNDERR | SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL)

/******************************************************************************/

static u32 cmd_resp[4];
static u8  sd_cid[16];
static u8  sd_csd[16];
static int is_sdhc;
static int sd_rca;

static int bsize;
static int csize;
static int cblock;

static OS_SEM cmd_sem;

/******************************************************************************/

static void sd_clk_set(int div)
{
	u32 val;

	val = SDIO->CLKCR;
	val &= 0xffffff00;
	val |= div;
	SDIO->CLKCR = val;
}

static void sd_clk_enable(int en)
{
	if(en)
		SDIO->CLKCR |=  0x0100;
	else
		SDIO->CLKCR &= ~0x0100;
}

static void sd_bus_width(int width)
{
	u32 val;

	val = SDIO->CLKCR;
	val &= 0xffffe7ff;
	if(width==4)
		val |= 0x00000800;
	SDIO->CLKCR = val;
}

static void sd_power_on(int on)
{
	if(on)
		SDIO->POWER = 0x00000003;
	else
		SDIO->POWER = 0x00000000;
}

static int sd_card_insert(void)
{
	return (GPIOA->IDR&0x8000)? 0: 1;
}

/******************************************************************************/

void SDIO_IRQHandler(void)
{
	SDIO->MASK = 0;
	isr_sem_send(&cmd_sem);
}

static int sd_cmd(u32 cmd, u32 arg)
{
	u32 mask, stat;
	int retv;

	if(HAS_DATA(cmd)){
		mask = SDIO_STA_DATAEND | DATA_ERROR | CMD_ERROR;
	}else if(RESP_NONE(cmd)){
		mask = SDIO_STA_CMDSENT | CMD_ERROR;
	}else{
		mask = SDIO_STA_CMDREND | CMD_ERROR;
	}
	SDIO->MASK = mask;
	SDIO->ARG = arg;
	SDIO->CMD = 0x0400|(cmd&0x00ff);

	retv = os_sem_wait(&cmd_sem, 0x8000);
	if(retv==OS_R_TMO){
		printk("sd_cmd: irq timeout!\n");
		stat = 0xffffffff;
		goto _exit;
	}

	stat = SDIO->STA;
	if(RESP_NOCRC(cmd)){
		stat &= 0xfffffffe;
	}
	if(stat&(CMD_ERROR|DATA_ERROR)){
		// CMD or DATA error
		printk("sd_cmd: CMD_%02d error %08x!\n", (cmd&0x3f), stat);
		goto _exit;
	}
	stat = 0;

	if(RESP_SHORT(cmd)){
		cmd_resp[0] = SDIO->RESP1;
	}else if(RESP_LONG(cmd)){
		cmd_resp[0] = SDIO->RESP4;
		cmd_resp[1] = SDIO->RESP3;
		cmd_resp[2] = SDIO->RESP2;
		cmd_resp[3] = SDIO->RESP1;
	}

	if(HAS_DATA(cmd)){
		while(DMA2_Channel4->CNDTR);
	}

_exit:
	DMA2_Channel4->CCR = 0;
	SDIO->DCTRL = 0;
	SDIO->MASK = 0;
	SDIO->CMD = 0;
	SDIO->ICR = 0x000007ff;
	return stat;
}

static int sd_acmd(u32 cmd, u32 arg)
{
	int retv;

	retv = sd_cmd(CMD_55, sd_rca<<16);
	if(retv)
		return retv;

	retv = sd_cmd(cmd, arg);
	return retv;
}

/******************************************************************************/

int sd_read_blocks(u32 block, int count, u8 *buf)
{
	int retv, dsize;

	dsize = count*512;

	SDIO->DTIMER = 0x000fffff;
	SDIO->DLEN   = dsize;
	SDIO->DCTRL  = 0x0000009b;

	DMA2_Channel4->CCR   = 0;
	DMA2_Channel4->CNDTR = dsize/4;
	DMA2_Channel4->CPAR  = (u32)&(SDIO->FIFO);
	DMA2_Channel4->CMAR  = (u32)buf;
	DMA2_Channel4->CCR   = 0x2a81;

	if(!is_sdhc)
		block *= 512;

	if(count==1){
		retv = sd_cmd(CMD_17, block);
	}else{
		retv = sd_cmd(CMD_18, block);
		if(retv==0){
			retv = sd_cmd(CMD_12, 0);
		}
	}

	return retv;
}

int sd_write_blocks(u32 block, int count, u8 *buf)
{
	int retv, dsize;

	dsize = count*512;

	SDIO->DTIMER = 0x000fffff;
	SDIO->DLEN   = dsize;
	SDIO->DCTRL  = 0x00000099;

	DMA2_Channel4->CCR   = 0;
	DMA2_Channel4->CNDTR = dsize/4;
	DMA2_Channel4->CPAR  = (u32)&(SDIO->FIFO);
	DMA2_Channel4->CMAR  = (u32)buf;
	DMA2_Channel4->CCR   = 0x2a91;

	if(!is_sdhc)
		block *= 512;

	if(count==1){
		retv = sd_cmd(CMD_24, block);
	}else{
		retv = sd_acmd(CMD_23, count);
		if(retv){
			return retv;
		}
		retv = sd_cmd(CMD_25, block);
		if(retv==0){
			retv = sd_cmd(CMD_12, 0);
		}
	}

	return retv;
}

/******************************************************************************/

int sdcard_identify(void)
{
	int retv, hcs;
	char tmp[8];

	sd_cmd(CMD_00, 0);
	os_dly_wait(1);

	retv = sd_cmd(CMD_08, 0x000001aa);
	//printk("CMD_08: retv=%08x resp0=%08x\n", retv, cmd_resp[0]);
	if(retv)
		hcs = 0;
	else
		hcs = 1<<30;

	while(1){
		retv = sd_acmd(CMD_41, 0x80100000|hcs);
		if(retv)
			break;
		if(cmd_resp[0]&0x80000000)
			break;
	}
	if(retv){
		printk("ACMD41 error! %08x\n", retv);
		return -1;
	}

	is_sdhc = 0;
	if(cmd_resp[0]&0x40000000)
		is_sdhc = 1;
	printk("    V2.0: %s\n", (hcs?"Yes":"No"));
	printk("    SDHC: %s\n", (is_sdhc?"Yes":"No"));

	// Send CID
	retv = sd_cmd(CMD_02, 0x00000000);
	if(retv)
		return retv;
	memcpy(sd_cid, (u8*)cmd_resp, 16);

	tmp[0] = sd_cid[12];
	tmp[1] = sd_cid[11];
	tmp[2] = sd_cid[10];
	tmp[3] = sd_cid[9];
	tmp[4] = sd_cid[8];
	tmp[5] = 0;
	printk("    Name: %s\n", tmp);

	// Send RCA
	retv = sd_cmd(CMD_03, 0x00000000);
	if(retv)
		return retv;
	sd_rca = cmd_resp[0]>>16;

	// Send CSD
	retv = sd_cmd(CMD_09, (sd_rca<<16));
	if(retv){
		sd_rca  =0;
		return retv;
	}
	memcpy(sd_csd, (u8*)cmd_resp, 16);

	// Get Size form CSD
	bsize = 1<<(sd_csd[10]&0x0f);
	printk("    Blen: %d\n", bsize);

	if(is_sdhc){
		csize = (sd_csd[8]<<16) | (sd_csd[7]<<8) | sd_csd[6];
		csize &= 0x003fffff;
		csize = (csize+1)*512;
		if(bsize==512){
			cblock = csize*2;
		}else if(bsize==1024){
			cblock = csize;
		}else{
			cblock = csize/2;
		}
	}else{
		int m;
		m = (sd_csd[6]<<8) | sd_csd[5];
		m = (m>>7)&7;
		m = 1<<(m+2);
		csize = (sd_csd[9]<<16) | (sd_csd[8]<<8) | sd_csd[7];
		csize = (csize>>6)&0x00000fff;
		csize = (csize+1)*m;
		cblock = csize;
		if(bsize==512)  csize /= 2;
		if(bsize==2048) csize *= 2;
	}
	printk("    Blks: %d\n", cblock);
	printk("    Size: %d K\n", csize);

	// Select CARD
	retv = sd_cmd(CMD_07, sd_rca<<16);
	if(retv){
		sd_rca = 0;
		return retv;
	}
	//printk("CMD_07: resp=%08x\n", cmd_resp[0]);
	sd_clk_set(SDCLK_HIGH);

	// Set Block Len
	sd_cmd(CMD_16, 512);
	//printk("CMD_16: resp=%08x\n", cmd_resp[0]);

	// Switch to 4bit
	retv = sd_acmd(CMD_06, 2);
	if(retv)
		return retv;
	//printk("ACMD06: resp=%08x\n", cmd_resp[0]);
	sd_bus_width(4);

	retv = sd_acmd(CMD_13, 0);
	if(retv)
		return retv;
	os_dly_wait(1);
	//printk("ACMD13: resp=%08x\n", cmd_resp[0]);

	return 0;
}

/******************************************************************************/

int sdio_init(void)
{
	SDIO->MASK = 0x00000000;
	SDIO->ICR  = 0x00c007ff;
	SDIO->DCTRL= 0;
	SDIO->CMD  = 0;

	sd_rca = 0;
	os_sem_init(&cmd_sem, 0);
	NVIC_SetPriority(SDIO_IRQn, 6);
	NVIC_EnableIRQ(SDIO_IRQn);

	sd_clk_set(SDCLK_LOW);
	sd_bus_width(1);
	sd_power_on(1);
	sd_clk_enable(1);

	if(sd_card_insert()){
		printk("SDCARD insert!\n");
		sdcard_identify();
	}

	return 0;
}

/******************************************************************************/
