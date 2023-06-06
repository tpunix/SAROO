
#include "../main.h"
#include "../fatfs/ff.h"

/******************************************************************************/

static void spi1_setcs(int cs)
{
	if(cs){
		GPIOA->BSRR = 0x0008;
	}else{
		GPIOA->BRR  = 0x0008;
	}
}

static u32 spi1_trans(u32 data)
{
	SPI1->DR = data;
	while((SPI1->SR&SPI_SR_RXNE)==0);
	return SPI1->DR;
}

static void spi1_delay(void)
{
	int i;
	for(i=0; i<100; i++){
	}
}

static void spi1_init(void)
{
	u32 cfg;

	// PA3: FPGA_nCS
	// PA4: FPGA_nCONFIG
	// PA5: FPGA_DCLK    | SPI1_SCLK
	// PA6: FPGA_SO      | SPI1_MISO
	// PA7: FPGA_SI      | SPI1_MOSI
	cfg = GPIOA->CRL;
	cfg &= 0x00000fff;
	cfg |= 0x94951000;
	GPIOA->CRL = cfg;
	
	// PC5: FPGA_nRESET
	cfg = GPIOC->CRL;
	cfg &= 0xff0fffff;
	cfg |= 0x00200000;
	GPIOC->CRL = cfg;
	GPIOC->BRR = 0x0020;

	SPI1->CR2 = 0x0000;
	SPI1->CR1 = 0x0000;
	SPI1->CR1 = 0x03e4; // LSB first

}


/******************************************************************************/

/* SPI flash */


static int epcs_status(void)
{
	int s0, s1;

	spi1_setcs(0);
	spi1_delay();
	spi1_trans(0x05);
	s0 = spi1_trans(0);
	spi1_setcs(1);
	spi1_delay();

	spi1_setcs(0);
	spi1_delay();
	spi1_trans(0x35);
	s1 = spi1_trans(0);
	spi1_setcs(1);
	spi1_delay();

	return (s1<<8)|s0;
}

static int epcs_wen(int en)
{
	spi1_setcs(0);
	spi1_delay();
	if(en)
		spi1_trans(0x06);
	else
		spi1_trans(0x04);
	spi1_setcs(1);
	spi1_delay();

	return 0;
}

static int epcs_wait()
{
	int status;

	while(1){
		status = epcs_status();
		//printk("epcs_status: %04x\n", status);
		if((status&1)==0)
			break;
	}
	
	return 0;
}


static int epcs_sector_erase(int addr)
{
	epcs_wen(1);

	spi1_setcs(0);
	spi1_delay();

	spi1_trans(0x20);
	spi1_trans((addr>>16)&0xff);
	spi1_trans((addr>> 8)&0xff);
	spi1_trans((addr>> 0)&0xff);

	spi1_setcs(1);
	spi1_delay();

	epcs_wait();
	
	return 0;
}

static int epcs_page_write(int addr, u8 *buf)
{
	int i;

	epcs_wen(1);

	spi1_setcs(0);
	spi1_delay();

	spi1_trans(0x02);
	spi1_trans((addr>>16)&0xff);
	spi1_trans((addr>> 8)&0xff);
	spi1_trans((addr>> 0)&0xff);

	for(i=0; i<256; i++){
		spi1_trans(buf[i]);
	}

	spi1_setcs(1);
	spi1_delay();
	
	epcs_wait();

	return 0;
}

int epcs_reset(void)
{
	spi1_setcs(0);
	spi1_delay();
	spi1_trans(0xff);
	spi1_trans(0xff);
	spi1_trans(0xff);
	spi1_trans(0xff);
	spi1_setcs(1);
	spi1_delay();
	return 0;
}

int epcs_readid(void)
{
	int mid, pid;

	spi1_setcs(0);
	spi1_delay();

	spi1_trans(0x90);
	spi1_trans(0);
	spi1_trans(0);
	spi1_trans(0);

	mid = spi1_trans(0);
	pid = spi1_trans(0);

	spi1_setcs(1);
	spi1_delay();

	return (mid<<8)|pid;
}

int epcs_write(int addr, int len, u8 *buf)
{
	int sector, page;

	for(sector=0; sector<len; sector+=4096){
		epcs_sector_erase(addr+sector);
		for(page=0; page<4096; page+=256){
			epcs_page_write(addr+sector+page, buf+sector+page);
		}
	}
	
	return 0;
}

int epcs_read(int addr, int len, u8 *buf)
{
	int i;

	spi1_setcs(0);
	spi1_delay();

	spi1_trans(0x0b);
	spi1_trans((addr>>16)&0xff);
	spi1_trans((addr>> 8)&0xff);
	spi1_trans((addr>> 0)&0xff);

	spi1_trans(0);

	for(i=0; i<len; i++){
		buf[i] = spi1_trans(0);
	}

	spi1_setcs(1);
	spi1_delay();

	return len;
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
		GPIOA->BRR  = 0x0010;
	
	return (GPIOA->IDR&0x0010)? 1 : 0;
}

void fpga_reset(int val)
{
	if(val)
		GPIOC->BSRR = 0x0020;
	else
		GPIOC->BSRR = 0x0020<<16;
}

#if 0

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
		os_dly_wait(10);
	}
	if(i==10){
		printk("FPGA config timeout!\n");
	}else{
		printk("FPGA config done!\n");
	}

	printk("FPGA state:\n");
	printk("    config done: %d\n", fpga_config_done());
	printk("      init done: %d\n", fpga_init_done());
	printk("         status: %d\n", fpga_status());
	printk("\n");


	if(fpga_config_done()){
		fpga_reset(1);
		return;
	}
	return;
}

#else

void fpga_config(void)
{
	FIL fp;
	int i, retv, addr;
	u32 rv;
	u8 fbuf[256];

	while(fpga_status()==0);

	printk("FPGA state:\n");
	printk("    config done: %d\n", fpga_config_done());
	printk("      init done: %d\n", fpga_init_done());
	printk("         status: %d\n", fpga_status());
	printk("\n");

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
	os_dly_wait(1);
	fpga_set_config(1);
	while(fpga_status()==1);
	os_dly_wait(1);

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
		os_dly_wait(10);
	}
	if(i==10){
		printk("FPGA config timeout!\n");
	}else{
		printk("FPGA config done!\n");
	}

	for(i=0; i<10; i++){
		if(fpga_init_done())
			break;
		os_dly_wait(10);
	}
	if(i==10){
		printk("FPGA init timeout!\n");
	}else{
		printk("FPGA init done!\n");
	}

	// 关闭升级文件
	f_close(&fp);

	// FPGA_nRESET = 1;
	GPIOC->BSRR = 0x0020;

	printk("FPGA state:\n");
	printk("    config done: %d\n", fpga_config_done());
	printk("      init done: %d\n", fpga_init_done());
	printk("         status: %d\n", fpga_status());
	printk("\n");

}

#endif

/******************************************************************************/

