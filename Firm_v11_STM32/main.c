
#include "main.h"

#include "fatfs/ff.h"



/******************************************************************************/

void hex_dump(char *str, void *buf, int size)
{
	int i;
	u8 *ubuf = buf;

	if(str)
		printk("%s:", str);

	for(i=0; i<size; i++){
		if((i%16)==0){
			printk("\n%4x:", i);
		}
		printk(" %02x", ubuf[i]);
	}
	printk("\n\n");
}

/******************************************************************************/

uint32_t SystemCoreClock = 72000000;

void SystemInit(void)
{
	/* Enable HSE */
	RCC->CR = 0x83;
	RCC->CR |= 0x00010000;
	while((RCC->CR&0x00020000)==0);

	FLASH->ACR = 0x2;   // 2个访问等待状态
	FLASH->ACR |= 0x10; // 预取缓冲区使能

	/* Set PLL to 72Mhz */
	RCC->CFGR = 0x0;
	RCC->CFGR = 0x001D0000;
	RCC->CR |=  0x01000000;
	while((RCC->CR&0x01000000)==0);

	/* AHB=72Mhz APB1=36Mhz APB2=72Mhz ADC=12Mhz USB=48Mhz */
	RCC->CFGR |= 0x00008400;

	/* PLL as SYSCLK */
	RCC->CFGR |= 0x2;
	while((RCC->CFGR&0x0c)!=0x08);


	__set_BASEPRI(13);
	NVIC_SetPriorityGrouping(3);
	SCB->VTOR = FLASH_BASE;

}

/******************************************************************************/

// PA0
// PA1
// PA2 UART_TX
// PA3 UART_RX
// PA4 Output Bt
// PA5 Output LED
// PA6
// PA7

//
// 1,2,3: 推拉输出     10M 2M 50M
// 5,6,7: 开漏输出     10M 2M 50M
// 9,A,B: 推拉功能输出 10M 2M 50M
// D,E,F: 开漏功能输出 10M 2M 50M
//
// 0: 模拟输入
// 4: 浮空输入
// 8: 上下拉输入
//

void device_init(void)
{
	RCC->APB1ENR = 0x0000403f;
	RCC->APB2ENR = 0x0000587d;
	RCC->AHBENR  = 0x00000507;
	
	AFIO->MAPR = 0x02000000; // only use SW-DP

	GPIOA->CRL = 0x44484444;
	GPIOA->CRH = 0x844444a5;
	GPIOA->ODR = 0x00008010;

	GPIOB->CRL = 0xb4444444;
	GPIOB->CRH = 0xb4bb8844;
	GPIOB->ODR = 0x00000c00;

	GPIOC->CRL = 0x8b484444;
	GPIOC->CRH = 0x444bbbbb;
	GPIOC->ODR = 0x00000090;

	GPIOD->CRL = 0xb8bbbbbb;
	GPIOD->CRH = 0xbbbbbbbb;
	GPIOD->ODR = 0x00000040;

	GPIOE->CRL = 0xbbbbbbbb;
	GPIOE->CRH = 0xbbbbbbbb;
	GPIOE->ODR = 0x00000000;

	FSMC_Bank1->BTCR[0] = 0x0000905b; // BCR
	FSMC_Bank1->BTCR[1] = 0x00020310; // BTR

	usart1_init();
}

/******************************************************************************/

OS_TID main_id;


FATFS sdfs;
u8 fs_task_stack[0x1000];

int scan_dir(char *dirname, int level, void (*func)(char*));

__task void fs_mount(void)
{
	FRESULT retv;

	retv = f_mount(&sdfs, "0:", 1);
	printk("Mount SDFS: %08x\n\n", retv);

	//scan_dir("/", 0, NULL);

	fpga_config();

	saturn_config();

	//simple_shell();

	os_tsk_delete_self();
}


extern u32 disk_run_count;
u32 last_count;
u32 main_count;

__task void main_task(void)
{
	main_id = os_tsk_self();
	
	device_init();

	printk("\n\nSTM32 RTX start!\n\n");

	sdio_init();
	os_tsk_create_user(fs_mount, 10, fs_task_stack, 0x1000);

	main_count = 0;
	last_count = 0xffffffff;
	while(1){
		GPIOA->BRR  = 0x0100;
		os_dly_wait(50);
		GPIOA->BSRR = 0x0100;
		os_dly_wait(50);

		main_count += 1;
		if(disk_run_count==last_count){
			printk("\n\n==== main_task: disk_task stop! count=%08x ====\n\n", disk_run_count);
		}
		last_count = disk_run_count;
	}
}


int main(void)
{
	os_sys_init(main_task);
	while(1);
}

/******************************************************************************/

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

