
#include "main.h"
#include "rtx_os.h"
#include "ff.h"

/******************************************************************************/

void set_mpu_entry(int index, u32 addr, u32 attr)
{
	MPU->RNR = index;
	MPU->RBAR = addr;
	MPU->RASR = attr;
}

void mpu_config(void)
{
	//set_mpu_entry(0, 0x00000000, 0x17000001|(31<<1));  // BackGround
	//set_mpu_entry(1, 0x00000000, 0x03020001|(28<<1));  // Code
	//set_mpu_entry(2, 0x20000000, 0x030b0001|(28<<1));  // SRAM
	//set_mpu_entry(3, 0x40000000, 0x13000001|(28<<1));  // InternalDevice
	set_mpu_entry(4, 0x60000000, 0x13000001|(28<<1));  // ExternalDevice

	MPU->CTRL = 0x0005;
}


void device_init(void)
{
	RCC->AHB3ENR  = 0x00011001;  /* SDMMC1 FMC MDMA*/
	RCC->AHB1ENR  = 0x08000003;  /* OTG2_FS DMA2 DMA1 */
	RCC->AHB2ENR  = 0xe0000000;  /* SRAM3 SRAM2 SRAM1 */
	RCC->AHB4ENR  = 0x000007ff;  /* GPIO */

	RCC->APB1LENR = 0x0008c011;  /* USART4 SPI3 SPI2 TIM6 TIM2 */
	RCC->APB1HENR = 0x00000000;  /* */
	RCC->APB2ENR  = 0x00001001;  /* SPI1 TIM1 */
	RCC->APB3ENR  = 0x00000000;  /* */
	RCC->APB4ENR  = 0x00010002;  /* RTC SYSCFG */
	
	RCC->AHB1LPENR &= ~0x14000000;

	GPIOA->MODER  = 0x2aa00000;
	GPIOA->OTYPER = 0x00000018;
	GPIOA->OSPEEDR= 0x16805400;
	GPIOA->PUPDR  = 0x40000040;
	GPIOA->AFR[0] = 0x55500000;
	GPIOA->AFR[1] = 0x000aaa00;
	GPIOA->ODR    = 0x00000008;

	GPIOB->MODER  = 0x8a0a9a80;
	GPIOB->OTYPER = 0x00000000;
	GPIOB->OSPEEDR= 0x8a008a80;
	GPIOB->PUPDR  = 0x00400008;
	GPIOB->AFR[0] = 0xc0766000;
	GPIOB->AFR[1] = 0x55550088;
	GPIOB->ODR    = 0x00000040;

	GPIOC->MODER  = 0x06aa2400;
	GPIOC->OTYPER = 0x00000000;
	GPIOC->OSPEEDR= 0x02aa2000;
	GPIOC->PUPDR  = 0x00000100;
	GPIOC->AFR[0] = 0x05000000;
	GPIOC->AFR[1] = 0x000ccccc;

	GPIOD->MODER  = 0xaaaaaaaa;
	GPIOD->OTYPER = 0x00000000;
	GPIOD->OSPEEDR= 0xaaaaaaaa;
	GPIOD->PUPDR  = 0x00000000;
	GPIOD->AFR[0] = 0xcccccccc;
	GPIOD->AFR[1] = 0xcccccccc;

	GPIOE->MODER  = 0xaaaaaaaa;
	GPIOE->OTYPER = 0x00000000;
	GPIOE->OSPEEDR= 0xaaaaaaaa;
	GPIOE->PUPDR  = 0x00000000;
	GPIOE->AFR[0] = 0xcccccccc;
	GPIOE->AFR[1] = 0xcccccccc;


	uart4_init();

	// FMC的地址复用模式, 只在MType为PSRAM/FLASH时可用.
	// 各种模式的优先级: ExtMode > MuxMode > Mode1/2
	// PSRAM模式下, 单次写入会扩展为带BL信号的4次写入. 所以FMC只能选择FLASH模式.
	// FMC_CS1: 0xc0000000
	FMC_Bank1_R->BTCR[0]  = 0x8000b05b;
	FMC_Bank1_R->BTCR[1]  = 0x00020612;
}


/******************************************************************************/


void do_hardfault(u32 *sp, u32 *esp)
{
	char hbuf[128];
	int i;
	for(i=0; i<10000000; i++){
		__asm volatile("nop");
	}
	_puts("\r\n\r\nHardFault!\r\n");
	sprintk(hbuf, "HFSR=%08x CFSR=%08x BFAR=%08x MMFAR=%08x\n",
			SCB->HFSR, SCB->CFSR, SCB->BFAR, SCB->MMFAR);
	_puts(hbuf);

	sprintk(hbuf, "  R0 : %08x\n", esp[0]); _puts(hbuf);
	sprintk(hbuf, "  R1 : %08x\n", esp[1]); _puts(hbuf);
	sprintk(hbuf, "  R2 : %08x\n", esp[2]); _puts(hbuf);
	sprintk(hbuf, "  R3 : %08x\n", esp[3]); _puts(hbuf);
	sprintk(hbuf, "  R4 : %08x\n", sp[0]);  _puts(hbuf);
	sprintk(hbuf, "  R5 : %08x\n", sp[1]);  _puts(hbuf);
	sprintk(hbuf, "  R6 : %08x\n", sp[2]);  _puts(hbuf);
	sprintk(hbuf, "  R7 : %08x\n", sp[3]);  _puts(hbuf);
	sprintk(hbuf, "  R8 : %08x\n", sp[4]);  _puts(hbuf);
	sprintk(hbuf, "  R9 : %08x\n", sp[5]);  _puts(hbuf);
	sprintk(hbuf, "  R10: %08x\n", sp[6]);  _puts(hbuf);
	sprintk(hbuf, "  R11: %08x\n", sp[7]);  _puts(hbuf);
	sprintk(hbuf, "  R12: %08x\n", esp[4]); _puts(hbuf);
	sprintk(hbuf, "  SP : %08x\n", esp);    _puts(hbuf);
	sprintk(hbuf, "  LR : %08x\n", esp[5]); _puts(hbuf);
	sprintk(hbuf, "  PC : %08x\n", esp[6]); _puts(hbuf);
	sprintk(hbuf, "  PSR: %08x\n", esp[7]); _puts(hbuf);

	while(1);
}


/******************************************************************************/

void    *osRtxMemoryAlloc(void *mem, uint32_t size, uint32_t type);
uint32_t osRtxMemoryFree (void *mem, void *block);


void *malloc(uint32_t size)
{
	void *p = osRtxMemoryAlloc(osRtxInfo.mem.common, size, 0);
	//printk("malloc: %08x  %d\n", p, size);
	return p;
}


void free(void *p)
{
	//printk("free: %08x\n", p);
	osRtxMemoryFree(osRtxInfo.mem.common, p);
}


void memcpy32(void *dst, void *src, int len)
{
	u32 *dp = (u32*)dst;
	u32 *sp = (u32*)src;

	while(len>0){
		*dp++ = *sp++;
		len -= 4;
	}

}


/******************************************************************************/


int flash_erase(int addr)
{
	int retv;

	if(FLASH->CR1 & 1){
		FLASH->KEYR1 = 0x45670123;
		FLASH->KEYR1 = 0xcdef89ab;
	}

	addr = (addr>>17)&7;
	FLASH->CR1 = 0x0024 | (addr<<8);
	FLASH->CR1 = 0x00a4 | (addr<<8);

	while(FLASH->SR1&7);
	retv = FLASH->SR1 & 0x07fe0000;

	FLASH->CCR1 = FLASH->SR1;
	FLASH->CR1 = 0x0000;

	return retv;
}


int flash_write32(int addr, u8 *buf)
{
	int i, retv;

	if(FLASH->CR1 & 1){
		FLASH->KEYR1 = 0x45670123;
		FLASH->KEYR1 = 0xcdef89ab;
	}

	FLASH->CR1 = 0x0022;
	__DSB();

	for(i=0; i<32; i+=4){
		*(volatile u32*)(addr+i) = *(u32*)(buf+i);
	}
	__DSB();

	while(FLASH->SR1&7);
	retv = FLASH->SR1 & 0x07fe0000;

	FLASH->CCR1 = FLASH->SR1;
	FLASH->CR1 = 0x0000;

	return retv;
}


int flash_update(int check)
{
	FIL fp;
	int i, addr, retv, fsize;
	u8 *fbuf = (u8*)0x24002000;

	// 检查是否有升级文件.
	retv = f_open(&fp, "/SAROO/update/ssmaster.bin", FA_READ);
	if(retv){
		printk("No firm file.\n");
		return -1;
	}
	fsize = f_size(&fp);

	if(check){
		printk("Found firm file.\n");
		printk("    Size %08x\n", fsize);
		f_close(&fp);
		return 0;
	}


	u32 rv;
	f_read(&fp, fbuf, fsize, &rv);
	f_close(&fp);

	int key = disable_irq();

	int firm_addr = 0x08000000;
	_puts("erase ...\n");
	retv = flash_erase(firm_addr);
	if(retv){
		retv = -2;
		goto _exit;
	}

	_puts("write ...\n");
	for(i=0; i<fsize; i+=32){
		retv = flash_write32(firm_addr+i, fbuf+i);
		if(retv){
			retv = -3;
			goto _exit;
		}
	}

_exit:
	if(retv){
		_puts("    faile!\n");
	}
	*(volatile u16*)(0x60000012) = retv;
	*(volatile u16*)(0x60000010) = 0;

	restore_irq(key);

	if(retv==0){
		_puts("MCU update OK!\n");
		f_unlink("/SAROO/update/ssmaster.bin");
	}

	return retv;
}


int flash_write_config(u8 *cfgbuf)
{
	int retv;
	
	retv = flash_erase(0x080e0000);
	if(retv){
		printk("erase failed! %08x\n", retv);
		return retv;
	}
	
	retv = flash_write32(0x080e0000, cfgbuf);
	if(retv){
		printk("write failed! %08x\n", retv);
		return retv;
	}
	
	return 0;
}


/******************************************************************************/


FATFS sdfs;

void fs_mount(void *arg)
{
	FRESULT retv;

	retv = f_mount(&sdfs, "0:", 1);
	printk("Mount SDFS: %08x\n\n", retv);
	if(retv==0){
		//scan_dir("/", 0, NULL);
	}else{
		led_event(LEDEV_SD_ERROR);
	}

}


/******************************************************************************/

void fpga_config(void);


void main_task(void *arg)
{
	device_init();

	printk("\n\nSSMaster start! %08x\n\n", get_build_date());

	mpu_config();

	sdio_init();
	
	fs_mount(0);
	
	fpga_config();

	saturn_config();

	simple_shell();


}


/******************************************************************************/


static osSemaphoreId_t sem_led;
static int led_ev = 0;
static int loop = 0;

void led_event(int ev)
{
	led_ev = ev;
	if(ev==0)
		loop = 0;
	osSemaphoreRelease(sem_led);
}


void led_task(void *arg)
{
	int i;
	int freq;
	int times;
	int lock;

	sem_led = osSemaphoreNew(1, 0, NULL);

	while(1){
		if(led_ev && loop!=2){
			freq  = led_ev&0xff;
			times = (led_ev>>8)&0xff;
			loop  = (led_ev>>16)&0xff;
			led_ev = 0;
		}else if(loop==0){
			osSemaphoreAcquire(sem_led, 200);
			continue;
		}

		for(i=0; i<times; i++){
			GPIOC->BSRR = 0x2000;
			osDelay(freq);
			GPIOC->BSRR = 0x2000<<16;
			osDelay(freq);
		}
		if(times>1){
			osDelay(100);
		}
	}
}


/******************************************************************************/


int main(void)
{
	osKernelInitialize();

	osThreadAttr_t attr;

	memset(&attr, 0, sizeof(attr));
	attr.priority = osPriorityLow;
	osThreadNew(main_task, NULL, &attr);

	memset(&attr, 0, sizeof(attr));
	attr.priority = osPriorityHigh;
	osThreadNew(led_task, NULL, &attr);

	osKernelStart();

	return 0;
}

