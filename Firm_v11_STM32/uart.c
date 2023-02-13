
#include "main.h"

/******************************************************************************/

#define BUF_SIZE 4096

static u8  qbuf[BUF_SIZE];
static u32 rp, wp, new_rp;
static u32 lost_str;

void usart1_dma_send(void *data, int length)
{
	DMA1_Channel4->CNDTR = length;
	DMA1_Channel4->CPAR = (u32)&(USART1->DR);
	DMA1_Channel4->CMAR = (u32)data;
	DMA1_Channel4->CCR = 0x0091;
	
	USART1->SR &= ~USART_SR_TC;
	USART1->CR3 |= USART_CR3_DMAT;
	USART1->CR1 |= USART_CR1_TCIE;
}

void usart1_send_start(void)
{
	int key = disable_irq();

	if(!(USART1->CR1&USART_CR1_TCIE)){
		if(rp>wp){
			new_rp = 0;
			usart1_dma_send(qbuf+rp, BUF_SIZE-rp);
		}else if(rp<wp){
			new_rp = wp;
			usart1_dma_send(qbuf+rp, wp-rp);
		}
	}

	restore_irq(key);
}

void USART1_IRQHandler(void)
{
	DMA1->IFCR = 0x0000f000;
	DMA1_Channel4->CCR = 0x0000;
	while(DMA1_Channel4->CCR&1);
	USART1->CR1 &= ~USART_CR1_TCIE;
	USART1->SR &= ~USART_SR_TC;
	USART1->CR3 &= ~USART_CR3_DMAT;

	rp = new_rp;
	usart1_send_start();
}

/******************************************************************************/

int usart1_put_buf(void *data, int length)
{
	int free, tail;
	u8 *dptr = (u8*)data;
	int irqs;
	
	irqs = disable_irq();

	if(wp>=rp){
		free = rp+BUF_SIZE-wp-1;
	}else{
		free = rp-wp-1;
	}

	if(length>free){
		lost_str += 1;
		restore_irq(irqs);
		return -1;
	}

	if(wp>=rp){
		tail = BUF_SIZE-wp;
		if(length>tail){
			memcpy(qbuf+wp, dptr, tail);
			wp = length-tail;
			memcpy(qbuf, dptr+tail, wp);
		}else{
			memcpy(qbuf+wp, dptr, length);
			wp += length;
			if(wp==BUF_SIZE)
				wp = 0;
		}
	}else{
		memcpy(qbuf+wp, dptr, length);
		wp += length;
	}

	usart1_send_start();

	restore_irq(irqs);
	return 0;
}

void usart1_puts(char *str, int len)
{
	int retv;

	do{
		retv = usart1_put_buf(str, len);
		if(in_isr())
			break;
		if(retv){
			while(rp!=wp){
				os_dly_wait(10);
			};
		}
	}while(retv);
}

/******************************************************************************/

void usart1_init(void)
{
	rp = 0;
	wp = 0;
	lost_str = 0;

	USART1->CR1  = 0x0;    // Reset USART1
	USART1->CR1 |= 0x0C;   // 使能 UART1_接收 发送
	USART1->CR2  = 0;
	USART1->CR3  = 0;
	USART1->BRR  = 0x0;    // Reset
	USART1->BRR |= 0x0271; // 设置波特比率寄存器为115200
//	USART1->BRR |= 0x0048; // 设置波特比率寄存器为1000000
	USART1->CR1 |= 0x2000; // Enable UART1

	NVIC_SetPriority(USART1_IRQn, 8);
	NVIC_EnableIRQ(USART1_IRQn);
}

void _putc(u8 byte)
{
	USART1->DR = byte & 0x01FF;
	while(((USART1->SR & 0x80) >> 7)!=1); //等待TXE不为1 
}

void _puts(char *str)
{
	char *p = str;

	for(p=str; *p; p++) {
		if (*p == '\n')
			_putc('\r');
		_putc(*p);
	}
}

int _getc(void)
{
	while((USART1->SR&0x20)==0){ //等待RXNE不为1 
		os_dly_wait(10);
	}

	return USART1->DR;
}

/******************************************************************************/

