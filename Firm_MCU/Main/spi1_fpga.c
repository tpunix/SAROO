
#include "main.h"
#include "ff.h"


/******************************************************************************/


#define FSPI_DELAY     100

void fspi_delay(void)
{
	int i;

	for(i=0; i<FSPI_DELAY; i++){
		NOTHING();
	}
}


void fspi_setcs(int val)
{
	int ctrl = *(volatile u16*)(0x60000016);
	if(val)
		ctrl |= 0x08;
	else
		ctrl &= 0x07;
	*(volatile u16*)(0x60000016) = ctrl;
}

void fspi_setclk(int val)
{
	int ctrl = *(volatile u16*)(0x60000016);
	if(val)
		ctrl |= 0x04;
	else
		ctrl &= 0x0b;
	*(volatile u16*)(0x60000016) = ctrl;
}

void fspi_setdi(int val)
{
	int ctrl = *(volatile u16*)(0x60000016);
	if(val)
		ctrl |= 0x02;
	else
		ctrl &= 0x0d;
	*(volatile u16*)(0x60000016) = ctrl;
}

int fspi_getdo(void)
{
	return (*(volatile u16*)(0x60000016))&1;
}

int fspi_init(void)
{
	fspi_setcs(1);
	fspi_setclk(1);

	return 0;
}

int fspi_trans(int byte)
{
	int i, data;

	data = 0;
	for(i=0; i<8; i++){
		fspi_setdi(byte&0x80);
		fspi_setclk(0);
		fspi_delay();

		byte <<= 1;
		data <<= 1;
		data |= fspi_getdo();

		fspi_setclk(1);
		fspi_delay();

	}

	return data;
}

/******************************************************************************/
/* SPI flash                                                                  */
/******************************************************************************/ 

int epcs_readid(void)
{
	int mid, pid;

	fspi_init();

	fspi_setcs(0);
	fspi_delay();

	fspi_trans(0x90);
	fspi_trans(0);
	fspi_trans(0);
	fspi_trans(0);

	mid = fspi_trans(0);
	pid = fspi_trans(0);

	fspi_setcs(1);
	fspi_delay();

	return (mid<<8)|pid;
}

int epcs_status(void)
{
	int status;

	fspi_setcs(0);
	fspi_delay();
	fspi_trans(0x05);
	status = fspi_trans(0);
	fspi_setcs(1);
	fspi_delay();

	return status;
}

int epcs_wen(int en)
{
	fspi_setcs(0);
	fspi_delay();
	if(en)
		fspi_trans(0x06);
	else
		fspi_trans(0x04);
	fspi_setcs(1);
	fspi_delay();

	return 0;
}

int epcs_wait()
{
	int status;

	while(1){
		status = epcs_status();
		if((status&1)==0)
			break;
		fspi_delay();
	}
	
	return 0;
}


int epcs_sector_erase(int addr)
{
	epcs_wen(1);

	fspi_setcs(0);
	fspi_delay();

	fspi_trans(0x20);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	fspi_setcs(1);
	fspi_delay();

	epcs_wait();
	
	return 0;
}

int epcs_page_write(int addr, u8 *buf)
{
	int i;

	epcs_wen(1);

	fspi_setcs(0);
	fspi_delay();

	fspi_trans(0x02);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	for(i=0; i<256; i++){
		fspi_trans(buf[i]);
	}

	fspi_setcs(1);
	fspi_delay();
	
	epcs_wait();

	return 0;
}


int epcs_read(int addr, int len, u8 *buf)
{
	int i;

	fspi_setcs(0);
	fspi_delay();

	fspi_trans(0x0b);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	fspi_trans(0);

	for(i=0; i<len; i++){
		buf[i] = fspi_trans(0);
	}

	fspi_setcs(1);
	fspi_delay();

	return len;
}

/******************************************************************************/

static int rbit_u8(int val)
{
	return __RBIT(val)>>24;
}


int fpga_update(int check)
{
	FIL fp;
	int i, addr, retv, fsize;
	u8 fbuf[256];

	// 检查是否有升级文件.
	retv = f_open(&fp, "/SAROO/update/SSMaster.rbf", FA_READ);
	if(retv){
		printk("No FPGA config file.\n");
		return -1;
	}
	fsize = f_size(&fp);

	if(check){
		printk("Found FPGA config file.\n");
		printk("    Size %08x\n", fsize);
		f_close(&fp);
		return 0;
	}


	printk("EPCS ID: %08x\n", epcs_readid());

	for(addr=0; addr<fsize; addr+=4096){
		epcs_sector_erase(addr);
	}

	for(addr=0; addr<fsize; addr+=256){
		u32 rv;
		f_read(&fp, fbuf, 256, &rv);
		for(i=0; i<256; i++){ // Altera need LSB first
			fbuf[i] = rbit_u8(fbuf[i]);
		}
		epcs_page_write(addr, fbuf);
	}
	f_close(&fp);

	printk("FPGA update OK!\n");
	f_unlink("/SAROO/update/SSMaster.rbf");
	return 0;
}


/******************************************************************************/


int fpga_config_done(void)
{
	return (GPIOB->IDR&0x0800)? 1 : 0;
}

int fpga_status(void)
{
	return (GPIOC->IDR&0x0010)? 1 : 0;
}

int fpga_init_done(void)
{
	return (GPIOC->IDR&0x0080)? 1 : 0;
}

int fpga_set_config(int val)
{
	if(val)
		GPIOA->BSRR = 0x0010;
	else
		GPIOA->BSRR = 0x0010<<16;
	
	return (GPIOA->IDR&0x0010)? 1 : 0;
}

void fpga_reset(int val)
{
	if(val)
		GPIOC->BSRR = 0x0020;
	else
		GPIOC->BSRR = 0x0020<<16;
}


void fpga_config(void)
{
	FIL fp;
	int i, retv, addr;
	u32 rv;
	u8 fbuf[256];

	fpga_reset(0);

	// 等待FPGA配置完成
	for(i=0; i<100; i++){
		if(fpga_config_done() && fpga_init_done())
			break;
		osDelay(1);
	}
	if(i==10){
		printk("FPGA config timeout!\n");
		led_event(LEDEV_FPGA_ERROR);
	}else{
		fpga_reset(1);
		printk("FPGA config done!\n");
	}

	printk("FPGA state:\n");
	printk("    config done: %d\n", fpga_config_done());
	printk("      init done: %d\n", fpga_init_done());
	printk("         status: %d\n", fpga_status());
	printk("\n");
	
	return;
}


/******************************************************************************/

