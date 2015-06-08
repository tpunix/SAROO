


#include "main.h"


static int width;
static int dump_addr;

static int gets(char *buf, int len)
{
	int n, ch;

	printk("stm32> ");
	n = 0;
	while(1){
		ch = _getc();
		if(ch==0x0d)
			break;
		printk("%c", ch);
		buf[n] = ch;
		n++;
		if(n==len)
			break;
	}

	printk("\r\n");
	buf[n] = 0;
	return n;
}

static int str2hex(char *str, int *hex)
{
	int n, p, ch;

	n = 0;
	p = 0;
	hex[0] = 0;
	while(1){
		ch = *str++;
		if(ch==' '){
			if(p){
				n++;
				hex[n] = 0;
				p = 0;
			}
			continue;
		}else if((ch>='0' && ch<='9')){
			ch -= '0';
		}else if((ch>='a' && ch<='f')){
			ch = ch-'a'+10;
		}else if((ch>='A' && ch<='F')){
			ch = ch-'A'+10;
		}else{
			if(p)
				n++;
			return n;
		}
		hex[n] = (hex[n]<<4)|ch;
		p++;
	}
}


static void dump(int addr)
{
	int i;

	i = 0;
	while(i<256){
		if((i%16)==0){
			printk("\n%08x : ", addr);
		}
		if(width==1){
			printk("%02x ", *(unsigned  char*)(addr));
		}else if(width==2){
			printk("%04x ", *(unsigned short*)(addr));
		}else{
			printk("%08x ", *(unsigned   int*)(addr));
		}
		i += width;
		addr += width;
	}
	printk("\n");

	dump_addr = addr;
}


void simple_shell(void)
{
	char str[128];
	int argc, arg[4], ch;

	width = 4;
	dump_addr = 0x08000000;

	while(1){
		str[2] = 0;
		gets(str, 128);
		argc = str2hex(str+2, arg);

		if(str[1]=='b'){
			width = 1;
		}else if(str[1]=='w'){
			width = 2;
		}else if(str[1]=='d'){
			width = 4;
		}

		if(str[0]=='d'){
			if(argc)
				dump(arg[0]);
			else
				dump(dump_addr);
		}else if(str[0]=='r'){
			printk("%08x: ", arg[0]);
			if(width==1){
				ch = *(unsigned char*)arg[0];
				printk("%02x\n", ch);
			}else if(width==2){
				ch = *(unsigned short*)arg[0];
				printk("%04x\n", ch);
			}else if(width==4){
				ch = *(unsigned int*)arg[0];
				printk("%08x\n", ch);
			}
		}else if(str[0]=='w'){
			printk("%08x= ", arg[0]);
			if(width==1){
				printk("%02x\n", arg[1]);
				*(unsigned char*)arg[0] = arg[1];
			}else if(width==2){
				printk("%04x\n", arg[1]);
				*(unsigned short*)arg[0] = arg[1];
			}else if(width==4){
				printk("%08x\n", arg[1]);
				*(unsigned int*)arg[0] = arg[1];
			}
		}else if(str[0]=='g'){
			void (*go)(void) = (void(*)(void))arg[0];
			go();
		}else if(str[0]=='q'){
			break;
		}
	}
}

