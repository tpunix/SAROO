
#include "main.h"
#include "ff.h"


/******************************************************************************/


static void spi1_setcs(int cs)
{
	if(cs){
		GPIOA->BSRR = 0x0008;
	}else{
		GPIOA->BSRR = 0x0008<<16;
	}
}

static u32 spi1_trans(u32 data)
{
	*(volatile u8*)&(SPI1->TXDR) = (u8)data;
	while((SPI1->SR&SPI_SR_RXP)==0);
	return *(volatile u8*)&(SPI1->RXDR);
}

static void spi1_delay(void)
{
	int i;
	for(i=0; i<1000; i++){
	}
}

static void spi1_init(void)
{
	u32 cfg;

	// PA3: FPGA_nCS     (out)
	// PA4: FPGA_nCONFIG (out)
	// PA5: FPGA_DCLK    | SPI1_SCLK
	// PA6: FPGA_SO      | SPI1_MISO
	// PA7: FPGA_SI      | SPI1_MOSI
	cfg = GPIOA->MODER;
	cfg &= 0xffff003f;
	cfg |= 0x0000a940;
	GPIOA->MODER = cfg;


	SPI1->CR1  = 0x00001000;
	SPI1->CR2  = 0;
	SPI1->CFG1 = 0x20000007; // 1:50M 2:25M 3:12.5M 4:6.25M
	SPI1->CFG2 = 0x04c00000;
	SPI1->CR1  = 0x00001001;
	SPI1->CR1  = 0x00001201;

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
	for(i=0; i<10; i++){
		if(fpga_config_done())
			break;
		osDelay(1);
	}
	if(i==100){
		printk("FPGA config timeout!\n");
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

#if 0
	// 检查是否有升级文件. 如果有,就升级. 如果没有,就继续.
	retv = f_open(&fp, "/SSMaster.rbf", FA_READ);
	if(retv){
		printk("NO FPGA config file found!\n");
		return;
	}

	printk("Found FPGA config file.\n");
	printk("    Size %08x\n", f_size(&fp));

	// 发现升级文件
	spi1_init();

	// 启动配置
	fpga_set_config(0);
	osDelay(1);
	fpga_set_config(1);
	while(fpga_status()==1);
	osDelay(1);

	// 写配置数据
	for(addr=0; addr<f_size(&fp); addr+=256){
		rv = 0;
		f_read(&fp, fbuf, 256, &rv);
		for(i=0; i<rv; i++){
			spi1_trans(fbuf[i]);
		}
	}
	spi1_trans(0xff);


	// 等待FPGA配置完成
	for(i=0; i<10; i++){
		if(fpga_config_done())
			break;
		osDelay(10);
	}
	if(i==10){
		printk("FPGA config timeout!\n");
	}else{
		printk("FPGA config done!\n");
	}

	for(i=0; i<10; i++){
		if(fpga_init_done())
			break;
		osDelay(10);
	}
	if(i==10){
		printk("FPGA init timeout!\n");
	}else{
		printk("FPGA init done!\n");
	}

	// 关闭升级文件
	f_close(&fp);

	fpga_reset(1);

	printk("FPGA state:\n");
	printk("    config done: %d\n", fpga_config_done());
	printk("      init done: %d\n", fpga_init_done());
	printk("         status: %d\n", fpga_status());
	printk("\n");
#endif
}

/******************************************************************************/

