
#include "main.h"
#include "smpc.h"

/**********************************************************/

const int HZ = 1000000;

void reset_timer(void)
{
	REG16(0x25807008) = 0;
}

u32 get_timer(void)
{
	u32 high1, high2, low;
	u32 timer;

	high1 = REG16(0x25807008);
	low   = REG16(0x2580700a);
	high2 = REG16(0x25807008);
	if(high1!=high2){
		low = REG16(0x2580700a);
	}

	timer = (high2<<16) | low;
	return timer;
}

void usleep(u32 us)
{
	u32 now, end;

	if(us==0)
		us = 1;
	end = us*(HZ/1000000);

	reset_timer();
	while(1){
		now = get_timer();
		if(now>=end)
			break;
	}
}

void msleep(u32 ms)
{
	u32 now, end;

	if(ms==0)
		ms = 1;
	end = ms*(HZ/1000);

	reset_timer();
	while(1){
		now = get_timer();
		if(now>=end)
			break;
	}
}

/**********************************************************/


void sci_init(void)
{
	SCR = 0x00;     /* set TE,RE to 0 */
	SMR = 0x00;     /* set SMR to 8n1 async mode */
	BRR = 0x15;     /* set BRR to 9600 bps */
	SCR = 0xcd;     /* set TIE RIE MPIE TEIE CKE1 CKE0 */
	SCR = 0x31;     /* set TE RE to 1 */

	printk_putc = sci_putc;
}

void sci_putc(int ch)
{
	while((SSR&0x80)==0);
	TDR = ch;
	SSR &= 0x7f;
}

int sci_getc(int timeout)
{
	int ch;

	timeout *= 1500;
	while(timeout){
		if(SSR&0x40){
			ch = RDR;
			SSR &= 0xbf;
			return ch;
		}
		timeout -= 1;
	}

	return -1;
}

/**********************************************************/

int gets(char *buf, int len)
{
	int n, ch;

	printk("rboot> ");
	n = 0;
	while(1){
		while((ch=sci_getc(10))<0);
		if(ch==0x0d)
			break;
		printk("%c", ch);
		buf[n] = ch;
		n++;
		if(n==len)
			break;
	}

	printk("\n");
	buf[n] = 0;
	return n;
}

int parse_arg(char *str, char *argv[])
{
	int n;

	n = 0;
	while(1){
		while(*str==0x20) str++;
		if(*str==0)
			break;

		argv[n] = str;
		n++;

		while(*str!=' ' && *str!=0) str++;
		if(*str==0)
			break;
		*str = 0;
		str++;
	}

	return n;
}

/**********************************************************/

unsigned int dump_address;
int mem_width = 1;

void dump(int argc, char *argv[])
{
	int i, address, len;

	if(argc==1){
		address = dump_address;
		len = 256;
	}else{
		address = strtoul(argv[1], 0, 0, 0);
	}

	if(argc==3){
		len = strtoul(argv[2], 0, 0, 0);
	}else{
		len = 256;
	}

	if(argv[0][1]=='b')
		mem_width = 1;
	else if(argv[0][1]=='w')
		mem_width = 2;
	else if(argv[0][1]=='d')
		mem_width = 4;

	for(i=0; i<len; i++){
		if(i%16==0){
			printk("\n%08X:", address+i);
		}
		if(mem_width==1){
			printk(" %02x", *(volatile unsigned char*)(address+i));
		}else if(mem_width==2){
			printk(" %04x", *(volatile unsigned short*)(address+i));
			i += 1;
		}else if(mem_width==4){
			printk(" %08x", *(volatile unsigned int*)(address+i));
			i += 3;
		}
	}
	printk("\n\n");

	dump_address = address+len;
}

void mrd(int argc, char *argv[])
{
	int address;

	if(argc==1){
		address = dump_address;
	}else{
		address = strtoul(argv[1], 0, 0, 0);
	}

	if(argv[0][1]=='b')
		mem_width = 1;
	else if(argv[0][1]=='w')
		mem_width = 2;
	else if(argv[0][1]=='d')
		mem_width = 4;

	if(mem_width==1){
		printk("%08X = %02x\n", address, *(volatile unsigned char*)(address));
	}else if(mem_width==2){
		printk("%08X = %04x\n", address, *(volatile unsigned short*)(address));
	}else if(mem_width==4){
		printk("%08X = %08x\n", address, *(volatile unsigned int*)(address));
	}

	dump_address = address;
}

void mwr(int argc, char *argv[])
{
	int address, data;

	if(argc==3){
		address = strtoul(argv[1], 0, 0, 0);
		data = strtoul(argv[2], 0, 0, 0);
	}else{
		return;
	}

	if(argv[0][1]=='b')
		mem_width = 1;
	else if(argv[0][1]=='w')
		mem_width = 2;
	else if(argv[0][1]=='d')
		mem_width = 4;

	if(mem_width==1){
		*(volatile unsigned char*)(address) = (unsigned char)data;
		printk("%08X -> %02x\n", address, (unsigned char)data);
	}else if(mem_width==2){
		*(volatile unsigned short*)(address) = (unsigned short)data;
		printk("%08X -> %04x\n", address, (unsigned short)data);
	}else if(mem_width==4){
		*(volatile unsigned int*)(address) = (unsigned int)data;
		printk("%08X -> %08x\n", address, (unsigned int)data);
	}

	dump_address = address;
}

void command_go(int go_addr)
{
	void (*go)(void);

	go = (void*)go_addr;
	printk("Jump to %08x ...\n", go);
	msleep(1000);

	go();
}

void mem_dump(char *str, void *addr, int size)
{
	int i, j;

	if(str){
		printk("%s:\n", str);
	}

	for(i=0; i<size; i+=16){
		printk("%04x: ", i);
		for(j=0; j<16; j++){
			printk(" %02x", *(u8*)addr);
			addr += 1;
		}
		printk("\n");
	}
}

/**********************************************************/

#define INDIRECT_CALL(addr, return_type, ...) (**(return_type(**)(__VA_ARGS__)) addr)
#define bios_run_cd_player  INDIRECT_CALL(0x0600026C, void, void)


void sci_shell(void)
{
	int ch;
	char str[128];
	int argc;
	char *argv[16];

	mem_width = 2;
	dump_address = 0x00200000;

	while(1){
		ch = gets(str, 128);
		argc = parse_arg(str, argv);
		if(argv[0][0]=='d')
			dump(argc, argv);
		if(argv[0][0]=='r')
			mrd(argc, argv);
		if(argv[0][0]=='w')
			mwr(argc, argv);
		if(argv[0][0]=='g'){
			int go_addr = strtoul(argv[1], 0, 0, 0);
			command_go(go_addr);
		}

		if(argv[0][0]=='b'){
			cd_speed_test();
		}

		if(argv[0][0]=='t'){
			cd_cmd();
		}
		if(argv[0][0]=='f'){
			cdc_file_test();
		}
		if(argv[0][0]=='m'){
			/* 运行系统CD播放器 */
			bios_run_cd_player();
		}
		if(argv[0][0]=='n'){
			/* 启动游戏 */
			bios_cd_cmd();
		}

		if(argv[0][0]=='c'){
			/* 打开系统光驱 */
			smpc_cmd(CDOFF);
			printk("CDOFF ...\n");
			printk("CDOFF ...\n");
			smpc_cmd(CDON);
			printk("CDON ...\n");
			printk("CDON ...\n");
			REG16(0x25897004) = 0x0000;
		}
		if(argv[0][0]=='s'){
			/* 关闭系统光驱, 打开sarooo */
			smpc_cmd(CDOFF);
			printk("CDOFF ...\n");
			printk("CDOFF ...\n");
			REG16(0x25897004) = 0x8000;
		}

		if(argv[0][0]=='y'){
			/* 重新运行romimage */
			command_go(0x02000f00);
		}
		if(argv[0][0]=='z'){
			/* 下载ram版运行 */
			int xm_len, xm_addr;
			xm_addr = 0x002a0000;
			xm_len = tiny_xmodem_recv(xm_addr);
			command_go(xm_addr);
		}
	}
}

/**********************************************************/



