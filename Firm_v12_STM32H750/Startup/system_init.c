
#include <stm32h7xx.h>


/******************************************************************************/


extern uint32_t __Vectors;

uint32_t SystemCoreClock = 400000000;

void SystemInit (void)
{
	/* enable CP10/CP11 (FPU) */
	SCB->CPACR = 0x00f00000;

	/* Disable all interrupts */
	RCC->CIER = 0x00000000;

	/* Switch to VOS1(Must enable USBREG first) */
	PWR->CR3 |= 0x03000000;
	PWR->D3CR = 0xc000;
	while((PWR->D3CR&0x2000)==0);

	/* Enable HSI48 HSE HSI */
	RCC->CR = 0x1003;
	RCC->CR |= 0x00010000;
	while((RCC->CR&0x00020000)==0);

	/* PLL config. OSC:16M DIVM=1 */
	RCC->PLLCFGR = 0x00070ccc;
	RCC->PLLCKSELR = 0x00101012;

	/* Set PLL1 VCO to 800Mhz (16x50) */
	/* P=400M Q=200M R=133M */
	RCC->PLL1DIVR = ((6-1)<<24) | ((4-1)<<16) | ((2-1)<<9) | (50-1);
	RCC->CR |= (1<<24);
	while((RCC->CR&0x02000000)==0);

	/* Set PLL2 VCO to 361.267578Mhz (16x22.5792) */
	/* P=90.316894M */
	RCC->PLL2FRACR = 0x9448;
	RCC->PLL2DIVR = ((4-1)<<9) | (22-1);
	RCC->PLLCFGR = 0x000f0cdc;
	RCC->CR |= (1<<26);
	while((RCC->CR&0x08000000)==0);

	/* CpuCLK=400M BusCLK=200M AXI=200M AHB=200M APB=100M */
	RCC->D1CFGR = 0x0048;
	RCC->D2CFGR = 0x0440;
	RCC->D3CFGR = 0x0040;

	/* per_ck=hsi_ker_ck SDMMC=PLL1_Q QSPI=AHB FMC=AHB */
	RCC->D1CCIPR = 0x00000000;
	/* SPI123=PLL2_P SPI45=APB */
	RCC->D2CCIP1R = 0x00001000;
	/* USB=HSI48 I2C=APB RNG=HSI48 USART=APB */
	RCC->D2CCIP2R = 0x00300000;
	/* All default */
	RCC->D3CCIPR = 0x00000000;

	/* RTC:1Mhz sys_ck=PLL1_P */
	RCC->CFGR = 0x2003;

	/* 4 bits for pre-emption priority, 0 bits for subpriority */
	NVIC_SetPriorityGrouping(3);

	SCB->VTOR = (uint32_t)&__Vectors;
}


/******************************************************************************/

