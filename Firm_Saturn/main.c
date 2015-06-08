/*
 * Sega Saturn cartridge flash tool
 * by Anders Montonen, 2012
 *
 * Original software by ExCyber
 * Graphics routines by Charles MacDonald
 *
 * Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
 */

#include "main.h"
#include "smpc.h"
#include "vdp2.h"


/**********************************************************/

#define PAD_START   (1<<11)
#define PAD_A       (1<<10)
#define PAD_C       (1<<9)
#define PAD_B       (1<<8)

void pad_init(void)
{
    PDR1 = 0;
    DDR1 = 0x60;
    IOSEL = IOSEL1;
}

u32 pad_read(void)
{
    u32 bits;

    PDR1 = 0x60;
    bits = (PDR1 & 0x8) << 9;
    PDR1 = 0x40;
    bits |= (PDR1 & 0xf) << 8;
    PDR1 = 0x20;
    bits |= (PDR1 & 0xf) << 4;
    PDR1 = 0x00;
    bits |= (PDR1 & 0xf);

    return bits ^ 0x1FFF;
}

/**********************************************************/

int smpc_cmd(int cmd)
{
	printk("SMPC_CMD(%02x) ...\n", cmd);
	// wait busy state
	while(SF&0x01);

	SF = 0x01;
	COMREG = cmd;
	while(SF&0x01);

	printk("    done. SS=%02x\n", SR);
	return SR;
}

/**********************************************************/

void stm32_puts(char *str)
{
	u32 stm32_base = 0x22800000;
	int i, len;

	//len = strlen(str);
	//len += 1;
	len = 32;
	*(u16*)(stm32_base+0x00) = len;

	for(i=0; i<len; i+=2)
		*(u16*)(stm32_base+0x10+i) = *(u16*)(str+i);

	REG16(0x2580700c) = 0x0001;
	//while(REG16(0x2580700c));
}



/**********************************************************/





/**********************************************************/





/**********************************************************/





/**********************************************************/







int _main(void)
{

    conio_init();
    pad_init();

	printk("    SRAOOO start!\n");

	//ASR0 = 0x7f007f00;
	//ASR1 = 0x7f007f00;

	sci_init();
	printk("\n\nSAROOO Serial Monitor Start!\n");

#if 1
	// CDBlock Off
	smpc_cmd(CDOFF);
	// SAROOO On
	REG16(0x25897004) = 0x8000;
	// boot game
	//install_ubr_isr();
	bios_cd_cmd();
#endif

	sci_shell();

	return 0;
}


