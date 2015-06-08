/*
 * Sega Saturn cartridge flash tool
 * by Anders Montonen, 2012
 *
 * Original software by ExCyber
 * Graphics routines by Charles MacDonald
 *
 * Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
 */

#include "vdp2.h"
#include "smpc.h"

#include "vdp.h"
#include "conio.h"


#define bios_loadcd_init    INDIRECT_CALL(0x0600029c, int,  int)

#define bios_loadcd_read    INDIRECT_CALL(0x060002cc, int,  void)

#define bios_loadcd_boot    INDIRECT_CALL(0x06000288, int,  int)




#if 0

u32 set_sp(u32 addr)
{
	asm("
		mov       r15, r0
		mov       r4, r15
	");
}

int load_game(void)
{
	int retv, save_sp;

	cdc_cdb_init(0);
	cdc_change_dir(23, 0xffffff);
	cdc_abort_file();

	retv = bios_loadcd_init(0);
	printk("bios_loadcd_init: %d\n", retv);

	save_sp = set_sp(0x06002000);

	while(1){
		retv = bios_loadcd_boot(0);
		printk("bios_loadcd_boot: %d\n", retv);
		if(retv<0)
			break;
	}

	set_sp(save_sp);
	return retv;
}

#else

static inline void	set_sr2( unsigned int	sr ){
	asm volatile ( "ldc\t%0,sr":: "r" (sr) );
}

static inline unsigned int	get_sr( void ){
	unsigned int	sr;

	asm volatile ( "stc\tsr,%0": "=r" (sr) );
	return	sr;
}

static inline void	set_imask( unsigned int	imask ){
	unsigned int	sr = get_sr();

	sr &= ~0x000000f0;
	sr |= ( imask << 4 );
	set_sr( sr );
}

static inline void	set_vbr2( void	*vbr ){
	asm volatile ( "ldc\t%0,vbr":: "r" (vbr) );
}

static inline void	set_gbr2( void	*gbr ){
	asm volatile ( "ldc\t%0,gbr":: "r" (gbr) );
}


int load_game(void)
{
	int retv;
	u8 *ip_addr = (u8*)0x06002000;
	u32 main_addr;
	u32 main_size;

	retv = cdc_read_sector(ip_addr, 150, 16);
	if(retv<0){
		printk("load_game: read ip.bin failed!\n");
		return retv;
	}

	retv = cdc_read_sector((u8*)0x0600a000, 150+20, 1);
	if(retv<0){
		printk("load_game: read root dir failed!\n");
		return retv;
	}

	memcpy((u8*)0x06000c00, (u8*)0x06002000, 0x0100);
	memcpy((u8*)0x060002a0, (u8*)0x060020e0, 0x0020);

	*(u32*)(0x060002b0) = 0x06010000;
	*(u32*)(0x06000290) = 3;
	
	*(u32*)(0x06002270) = 0x06000284;

	*(u32*)(0x06000278) = 0;
	*(u32*)(0x0600027c) = 0;

	main_addr = *(u32*)(0x060020f0);
	main_size = *(u32*)(0x0600a052);
	main_offs = *(u32*)(0x0600a04a);
	printk("main_offset=%08x main_size=%08x main_addr=%08x\n", main_offs, main_size, main_addr);

	// Setup the vector table area, etc.(all bioses have it at 0x00000600-0x00000810)
	for(i=0; i<0x210; i+=4){
		*(u32*)(0x06000000+i) = *(u32*)(0x00000600+i);
	}

	// Setup the bios function pointers, etc.(all bioses have it at 0x00000820-0x00001100)
	for(i=0; i<0x8e0; i+=4){
		*(u32*)(0x06000220+i) = *(u32*)(0x00000820+i);
	}

	// I'm not sure this is really needed
	for(i=0; i<0x700; i+=4){
		*(u32*)(0x06001100+i) = *(u32*)(0x00001100+i);
	}

	// Fix some spots in 0x06000210-0x0600032C area
	*(u32*)0x06000234 = 0x000002ac;
	*(u32*)0x06000238 = 0x000002bc;
	*(u32*)0x0600023c = 0x00000350;
	*(u32*)0x06000240 = 0x32524459;
	*(u32*)0x0600024c = 0x00000000;
	*(u32*)0x06000268 = *(u32*)0x00001344;
	*(u32*)0x0600026c = *(u32*)0x00001348;
	*(u32*)0x0600029c = *(u32*)0x00001354;
	*(u32*)0x060002c4 = *(u32*)0x00001104;
	*(u32*)0x060002c8 = *(u32*)0x00001108;
	*(u32*)0x060002cc = *(u32*)0x0000110c;
	*(u32*)0x060002d0 = *(u32*)0x00001110;
	*(u32*)0x060002d4 = *(u32*)0x00001114;
	*(u32*)0x060002d8 = *(u32*)0x00001118;
	*(u32*)0x060002dc = *(u32*)0x0000111c;
	*(u32*)0x06000328 = 0x000004c8;
	*(u32*)0x0600032c = 0x00001800;

	// Fix SCU interrupts
	for(i=0; i<0x80; i+=4){
		*(u32*)(0x06000a00+i) = 0x0600083c;
	}


	set_sr2(0);
	set_vbr2(0x06000000);

	__asm__  volatile ("mov   #0, r0");
	__asm__  volatile ("mov   #0, r1");
	__asm__  volatile ("mov   #0, r2");
	__asm__  volatile ("mov   #0, r3");
	__asm__  volatile ("mov   #0, r4");
	__asm__  volatile ("mov   #0, r5");
	__asm__  volatile ("mov   #0, r6");
	__asm__  volatile ("mov   #0, r7");
	__asm__  volatile ("mov   #0, r8");
	__asm__  volatile ("mov   #0, r9");
	__asm__  volatile ("mov   #0, r10");
	__asm__  volatile ("mov   #0, r11");
	__asm__  volatile ("mov   #0, r12");
	__asm__  volatile ("mov   #0, r13");
	__asm__  volatile ("mov   #0, r14");
	__asm__  volatile ("lds   r14, pr");

	__asm__  volatile ("ldc	r14, gbr");
	__asm__  volatile ("lds	r14, mach");
	__asm__  volatile ("lds r14, macl");


	setStackptr(0x6002000, 0x6002e00);

	return 0;
}

#endif

