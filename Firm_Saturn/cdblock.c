
#include "main.h"
#include "smpc.h"

/**********************************************************/

typedef struct {
	u16 cr1;
	u16 cr2;
	u16 cr3;
	u16 cr4;
}CDCMD;

typedef struct {
	int fad;
	int size;
	u8  unit;
	u8  gap;
	u8  fn;
	u8  attr;
}CdcFile;

#define CR1     REG16(0x25890018)
#define CR2     REG16(0x2589001c)
#define CR3     REG16(0x25890020)
#define CR4     REG16(0x25890024)
#define HIRQ    REG16(0x25890008)
#define HMSK    REG16(0x2589000c)

#define HIRQ_CMOK   0x0001
#define HIRQ_DRDY   0x0002
#define HIRQ_CSCT   0x0004
#define HIRQ_BFUL   0x0008
#define HIRQ_PEND   0x0010
#define HIRQ_DCHG   0x0020
#define HIRQ_ESEL   0x0040
#define HIRQ_EHST   0x0080
#define HIRQ_ECPY   0x0100
#define HIRQ_EFLS   0x0200
#define HIRQ_SCDQ   0x0400


#define STATUS_BUSY             0x00
#define STATUS_PAUSE            0x01
#define STATUS_STANDBY          0x02
#define STATUS_PLAY             0x03
#define STATUS_SEEK             0x04
#define STATUS_SCAN             0x05
#define STATUS_OPEN             0x06
#define STATUS_NODISC           0x07
#define STATUS_RETRY            0x08
#define STATUS_ERROR            0x09
#define STATUS_FATAL            0x0a
#define STATUS_PERIODIC         0x20
#define STATUS_TRANSFER         0x40
#define STATUS_WAIT             0x80
#define STATUS_REJECT           0xff



void clear_hirq(int mask)
{
	HIRQ = ~mask;
}

int wait_hirq(int mask)
{
	int i;

	for(i=0; i<0x240000; i++){
		if(HIRQ&mask){
			return 0;
		}
	}

	return -3;
}

int wait_peri(void)
{
	CDCMD resp;
	int status;

	while(1){
		int i;
		resp.cr1 = CR1;
		resp.cr2 = CR2;
		resp.cr3 = CR3;
		resp.cr4 = CR4;
		printk("STATUS=%04x\n", resp.cr1);
		status = resp.cr1>>8;
		if(status&STATUS_PERIODIC)
			break;
		for(i=0; i<100000; i++);
	}

	return 0;
}


int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *cmdname)
{

	//printk("\ncdc_cmd: %04x %04x %04x %04x  %s\n", cmd->cr1, cmd->cr2, cmd->cr3, cmd->cr4, cmdname);

	clear_hirq(wait|HIRQ_CMOK);

	CR1 = cmd->cr1;
	CR2 = cmd->cr2;
	CR3 = cmd->cr3;
	CR4 = cmd->cr4;

	if(wait_hirq(HIRQ_CMOK)){
		printk("CMOK TIMEOUT! HIRQ=%04x wait=%04x %s\n", HIRQ, wait, cmdname);
		return -3;
	}

	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;

	if((resp->cr1>>8)==0xff){
		printk("CMD REJECT!\n");
		return -5;
	}
	if((resp->cr1>>8)&0x80){
		printk("CMD WAIT!\n");
		return -6;
	}

	if(wait){
		if(wait_hirq(wait)){
			printk("HIRQ TIMEOUT! HIRQ=%04x wait=%04x\n", HIRQ, wait);
		}
	}
	//clear_hirq(wait|HIRQ_CMOK);

	//printk("   resp: %04x %04x %04x %04x\n", resp->cr1,resp->cr2,resp->cr3,resp->cr4);

	return 0;
}

/**********************************************************/


u32 *toc_buf = (u32*)0x06090000;

void show_toc(void)
{
	int i;

	for(i=0; i<102; i++){
		if(toc_buf[i]!=0xffffffff)
			printk("TOC %3d:  %08x\n", i+1, toc_buf[i]);
	}
}



int cdc_get_status(int *status)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_status");

	if(status)
		*status = resp.cr1>>8;
	return retv;
}

int cdc_get_hwinfo(int *status)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_hwinfo");

	if(status)
		*status = resp.cr1>>8;
	return retv;
}


int cdc_cdb_init(int standby)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x0401;
	cmd.cr2 = standby;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x040f;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_cdb_init");
}


int cdc_end_trans(int *cdwnum)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0600;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_end_trans");
	if(cdwnum){
		cdwnum[0] = ((resp.cr1&0xff)<<16) | resp.cr2;
	}
	clear_hirq(HIRQ_DRDY);

	return retv;
}

int cdc_get_toc(u8 *buf)
{
	CDCMD cmd, resp;
	int i, retv;

	cmd.cr1 = 0x0200;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_toc");

	for(i=0; i<102*4; i+=2){
		*(u16*)(buf+i) = REG16(0x25898000);
	}

	cdc_end_trans(NULL);

	return retv;
}


/**********************************************************/


int cdc_play_fad(int mode, int start_fad, int range)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x1080 | (start_fad>>16);
	cmd.cr2 = start_fad&0xffff;
	cmd.cr3 = (mode<<8) | 0x80 | (range>>16);
	cmd.cr4 = range&0xffff;

	return cdc_cmd(0, &cmd, &resp, "cdc_play_fad");
}


/**********************************************************/

int cdc_cddev_connect(int filter)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x3000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = filter<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_cddev_connect");
}

int cdc_get_cddev(int *selnum)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x3100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_cddev");

	*selnum = resp.cr3>>8;
	return retv;
}

/**********************************************************/


int cdc_set_filter_range(int selnum, int start_fad, int num_sector)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4000 | (start_fad>>16);
	cmd.cr2 = start_fad&0xffff;
	cmd.cr3 = (selnum<<8) | (num_sector>>16);
	cmd.cr4 = num_sector&0xffff;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_range");
}

int cdc_get_filter_range(int selnum, int *fad, int *num_sector)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_range");

	*fad = ((resp.cr1&0xff)<<16) | resp.cr2;
	*num_sector = ((resp.cr3&0xff)<<16) | resp.cr4;

	return retv;
}


int cdc_set_filter_mode(int selnum, int mode)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4400 | mode;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_mode");
}

int cdc_get_filter_mode(int selnum, int *mode)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4500;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_mode");

	*mode = resp.cr1&0xff;
	return retv;
}

int cdc_set_filter_connect(int selnum, int c_true, int c_false)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4603;
	cmd.cr2 = (c_true<<8) | c_false;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_connect");
}

int cdc_get_filter_connect(int selnum, int *c_true, int *c_false)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4700;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_connect");

	*c_true = resp.cr2>>8;
	*c_false = resp.cr2&0xff;
	return retv;
}


int cdc_reset_selector(int flag, int selnum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4800 | flag;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_reset_selector");
}

void show_selector(int id)
{
	int i;
	int mode, fad, range, c_true, c_false;

	for(i=0; i<24; i++){
		if(id==-1 || id==i){
			cdc_get_filter_mode(i, &mode);
			cdc_get_filter_range(i, &fad, &range);
			cdc_get_filter_connect(i, &c_true, &c_false);
			printk("filter %2d: mode_%02x fad_%08x range_%08x true_%02x false_%02x\n",
					i, mode, fad, range, c_true, c_false);
		}
	}

}

/**********************************************************/

int cdc_get_buffer_size(int *total, int *pnum, int *free)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_buffer_size");

	*free = resp.cr2;
	*pnum = resp.cr3>>8;
	*total = resp.cr4;
	return retv;
}

int cdc_get_numsector(int selnum, int *snum)
{
	CDCMD cmd, resp;
	int retv;

	*snum = 0;

	cmd.cr1 = 0x5100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_numsector");
	*snum = resp.cr4;
	return retv;
}


int cdc_calc_actsize(int bufno, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x5200;
	cmd.cr2 = spos;
	cmd.cr3 = bufno<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_calc_actsize");
}

int cdc_get_actsize(int *actsize)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5300;
	cmd.cr2 = 0;
	cmd.cr3 = 0;
	cmd.cr4 = 0;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_actsize");

	*actsize = ((resp.cr1&0xff)<<16) | resp.cr2;
	return retv;
}

int cdc_get_sector_info(int bufno, int secno, int *fad)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5400;
	cmd.cr2 = secno;
	cmd.cr3 = bufno<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_sector_info");

	*fad = ((resp.cr1&0xff)<<16) | resp.cr2;
	return retv;
}

/**********************************************************/

int cdc_set_size(int size)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6000 | size;
	cmd.cr2 = size<<8;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_size");
}


int cdc_get_data(int bufnum, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6100;
	cmd.cr2 = spos;
	cmd.cr3 = bufnum<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_data");
}

int cdc_get_del_data(int bufnum, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6300;
	cmd.cr2 = spos;
	cmd.cr3 = bufnum<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_del_data");
}

void cdc_trans_data(u8 *buf, int length)
{
	int i;

	for(i=0; i<length; i+=4){
		*(u32*)(buf+i) = REG(0x25818000);
	}

}

/**********************************************************/


/**********************************************************/


int cdc_auth_status(int *status)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0xe100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	int retv = cdc_cmd(0, &cmd, &resp, "cdc_auth_status");

	*status = resp.cr2;
	return retv;
}



void cdc_wait_auth(void)
{
	int i, status;

	while(1){
		for(i=0; i<100000; i++);
		cdc_get_status(&status);
		if(status!=0xff)
			break;
		printk("STATUS: %02x  HIRQ: %04x\n", status, HIRQ);
	}
}

int cdc_auth_device(void)
{
	CDCMD cmd, resp;
	int i, status;

	printk("\n\nCD_AUTH ...\n");

	cmd.cr1 = 0xe000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_auth_device");

	while(1){
		for(i=0; i<100000; i++);
		cdc_get_status(&status);
		if(status!=0xff)
			break;
		printk("STATUS: %02x  HIRQ: %04x\n", status, HIRQ);
	}

	int retv = cdc_auth_status(&status);
	printk("AUTH: %02x\n\n", status);

	return retv;
}

/**********************************************************/

int cdc_change_dir(int selnum, int fid)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_change_dir");
}

int cdc_read_dir(int selnum, int fid)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_read_dir");
}

int cdc_get_file_scope(int *fid, int *fnum, int *drend)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x7200;
	cmd.cr2 = 0;
	cmd.cr3 = 0;
	cmd.cr4 = 0;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_file_scope");

	*fnum = resp.cr2;
	*drend = resp.cr3>>8;
	*fid = ((resp.cr3&0xff)<<16) | resp.cr4;
	return retv;
}

int cdc_get_file_info(int fid, CdcFile *buf)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x7300;
	cmd.cr2 = 0x0000;
	cmd.cr3 = ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	retv = cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_file_info");

	if(retv==0){
		//int len = ((resp.cr3&0xff)<<16) | resp.cr4;
		int len = (resp.cr2)*2;
		cdc_trans_data((u8*)buf, len);
		cdc_end_trans(NULL);
	}

	return retv;
}

int cdc_read_file(int selnum, int fid, int offset)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7400 | ((offset>>16)&0xff);
	cmd.cr2 = offset&0xffff;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_read_file");
}


int cdc_abort_file(void)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7500;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	return cdc_cmd(0, &cmd, &resp, "cdc_abort_file");
}

void cdc_init(void);

int wait_status(int wait)
{
	int status;

	while(1){
		cdc_get_status(&status);
		printk("STATUS: %02x  HIRQ=%04x\n", status, HIRQ);
		if(status&0x80){
			return -1;
		}
		status &= 0x0f;
		if(status==wait)
			return 0;
	}

	return -1;
}

void cdc_file_test(void)
{
	int i;
	int fid, fnum, drend;
	int total, npart, free;
	CdcFile finfo[256];
	u8 *sbuf = (u8*)0x06080000;

	cdc_init();

	cdc_get_toc((u8*)toc_buf);
	show_toc();

	cdc_change_dir(0, 0xffffff);

	fnum = 0;
	cdc_get_file_scope(&fid, &fnum, &drend);
	printk("file scope: fid=%d fnum=%d drend=%d\n", fid, fnum, drend);
	cdc_get_file_info(0xffffff, finfo);
	for(i=0; i<fnum; i++){
		printk("file %4d: fad=%08x size=%08x %02x %02x %02x %02x\n", i,
				finfo[i].fad, finfo[i].size,
				finfo[i].unit, finfo[i].gap, finfo[i].fn, finfo[i].attr);
	}

	cdc_read_file(1, 6, 0);
	show_selector(1);

	do{
		cdc_get_numsector(1, &free);
		printk("cdc_get_numsector 1: %d HIRQ=%04x\n", free, HIRQ);
	}while(free<200);

	cdc_get_buffer_size(&total, &npart, &free);
	printk("cdc_get_buffer_size: free=%d\n", free);

	cdc_get_del_data(1, 0, 128);
	cdc_get_numsector(1, &free);
	printk("cdc_get_numsector 1: %d\n", free);
	cdc_trans_data(sbuf, 2048*100);
	cdc_end_trans(NULL);

	while(1){
		cdc_get_numsector(1, &free);
		if(free==200)
			break;
	}


	cdc_get_numsector(1, &free);
	printk("cdc_get_numsector 1: %d HIRQ=%04x\n", free, HIRQ);
	cdc_get_buffer_size(&total, &npart, &free);



}

/**********************************************************/



#define INDIRECT_CALL(addr, return_type, ...) (**(return_type(**)(__VA_ARGS__)) addr)

#define bios_run_cd_player  INDIRECT_CALL(0x0600026C, void, void)

//!< mode 0 -> check, 1 -> do auth
#define bios_check_cd_auth  INDIRECT_CALL(0x06000270, int,  int mode)

#define bios_loadcd_init1   INDIRECT_CALL(0x060002dc, int,  int)  // 00002650

#define bios_loadcd_init    INDIRECT_CALL(0x0600029c, int,  int)  // 00001904

#define bios_loadcd_read    INDIRECT_CALL(0x060002cc, int,  void) // 00001912

#define bios_loadcd_boot    INDIRECT_CALL(0x06000288, int,  void)  // 000018A8


int old_mplayer_addr;

void my_myplayer(int addr)
{
	int retv;
	int (*go)(int) = (void*)old_mplayer_addr;

	sci_init();
	printk("\n-- read_main! addr=%08x --\n", addr);

	retv = go(addr);
	printk("\n-- read_main: retv=%08x --\n", retv);
}


int my_bios_loadcd_init(void)
{
	printk("\nmy_bios_loadcd_init!\n");

	*(u32*)(0x06000278) = 0;
	*(u32*)(0x0600027c) = 0;

	cdc_abort_file();
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);
	cdc_set_size(0);

	// This is needed for Yabause: Clear the diskChange flag.
	int status;
	cdc_get_hwinfo(&status);

	return 0;
}


int my_bios_loadcd_read(void)
{
	int status, tm;
	u8 *sbuf = (u8*)0x06002000;

	printk("\nmy_bios_loadcd_read!\n");

	cdc_reset_selector(0, 0);
	cdc_cddev_connect(0);
	cdc_play_fad(0, 150, 16);

	HIRQ = 0;
	tm = 10000000;
	while(tm){
		if(HIRQ&HIRQ_PEND)
			break;
		tm -= 1;
	}
	if(tm==0){
		printk("  PLAY timeout!\n");
		return -1;
	}

	cdc_get_data(0, 0, 16);
	cdc_trans_data(sbuf, 2048*16);
	cdc_end_trans(&status);

	*(u16*)(0x060003a0) = 1;

	return 0;
}

int read_1st(void)
{

	int retv;
	retv = *(u32*)(0x06000284);
	void (*go)(void) = (void(*)(void))retv;
	go();	
	patch_game((char*)0x06002020);

}


int my_bios_loadcd_boot(int r4, int r5)
{
	__asm volatile ( "sts.l  pr, @-r15" :: );
	__asm volatile ( "jmp  @%0":: "r" (r5) );
	__asm volatile ( "mov  r4, r0" :: );
	__asm volatile ( "nop" :: );
}


int bios_cd_cmd(void)
{
	int retv, ip_size;

	my_bios_loadcd_init();

	retv = my_bios_loadcd_read();
	if(retv)
		return retv;


	// emulate bios_loadcd_boot

	*(u32*)(0x06000290) = 3;
	ip_size = bios_loadcd_read();//1912读取ip文件
	*(u32*)(0x06002270) = read_1st;	
	*(u16*)(0x0600220c) = 9;

	retv = my_bios_loadcd_boot(ip_size, 0x18be);//跳到18be
	if((retv==-8)||(retv==-4)){
		*(u32*)(0x06000254) = 0x6002100;
		retv = my_bios_loadcd_boot(0, 0x18c6);
	}
	printk("bios_loadcd_boot  retv=%d\n", retv);

	return retv;
}


/**********************************************************/




#define       CDROM_syscdinit1(i) \
              ((**(void(**)(int))0x60002dc)(i))
#define       CDROM_syscdinit2() \
              ((**(void(**)(void))0x600029c)())

void cdc_read_test(void);

void cdc_init1(void)
{
	cdc_abort_file();
	cdc_cdb_init(0);
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);
}

void cdc_init(void)
{
	int status;

	cdc_abort_file();
	cdc_cdb_init(0);
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);

	cdc_auth_device();

#if 0
	CDROM_syscdinit1(3);
	CDROM_syscdinit2();

	while(1){
		cdc_get_status(&status);
		if(status!=0xff)
			break;
	}
	printk("cdc_init done!\n");
	cdc_read_test();
#endif
}


int cdc_read_sector(u8 *buf, int fad, int num)
{
	int nsec, retv, status;

	cdc_set_size(0);
	cdc_reset_selector(0, 0);
	cdc_cddev_connect(0);

	cdc_play_fad(0, fad, num);
	while(num>0){
		retv = cdc_get_numsector(0, &nsec);
		if(retv<0)
			break;
		if(nsec==0)
			continue;

		cdc_get_del_data(0, 0, nsec);
		cdc_trans_data(buf, 2048*nsec);
		cdc_end_trans(&status);

		buf += 2048*nsec;
		num -= nsec;
	}

	return retv;
}

void cdc_read_test(void)
{
	int status;
	int cddev_filter;
	int nsec, actsize;
	int total, npart, free;
	int hirq, cr1, cr2, cr3, cr4;
	int old_hirq, old_cr1, old_cr2, old_cr3, old_cr4;

	u8 *sbuf = 0x06040000;
	char *logbuf = (char*)0x06080000;
	int log_p = 0;

	cdc_set_size(0);
	cdc_reset_selector(0, 0);
	cdc_cddev_connect(0);

	HIRQ = 0;
	cdc_play_fad(0, 0xd5, 0x40);

	printk("\n\nwait play end ...\n");
	reset_timer();
	while(1){
		hirq = HIRQ;
		cr1 = CR1;
		cr2 = CR2;
		cr3 = CR3;
		cr4 = CR4;
		if(old_hirq!=hirq || old_cr1!=cr1 || old_cr4!=cr4){
			cdc_get_buffer_size(&total, &npart, &free);
			printk("%08x: HIRQ=%04x CR1=%04x CR4=%04x free=%d\n", get_timer(), hirq, cr1, cr4, free);
			log_p += sprintf(logbuf+log_p, "%08x: HIRQ=%04x CR1=%04x CR4=%04x free=%d\n",
							get_timer(), hirq, cr1, cr4, free);
		}
		old_hirq = hirq;
		old_cr1 = cr1;
		old_cr2 = cr2;
		old_cr3 = cr3;
		old_cr4 = cr4;
		if(hirq&HIRQ_PEND)
			break;
		if(hirq)
			clear_hirq(hirq);

		//cdc_get_status(&status);
		//printk("STATUS: %02x  HIRQ=%04x\n", status, HIRQ);
	}
	//logbuf[log_p] = 0;
	printk("LOG: log_p=%08x\n", log_p);
	int i=0;
	while(i<log_p){
		printk("%s", logbuf+i);
		i += strlen(logbuf+i);
		i += 1;
	}

	cdc_get_numsector(0, &nsec);

	cdc_calc_actsize(0, 0, 2);
	cdc_get_actsize(&actsize);
	cdc_get_buffer_size(&total, &npart, &free);

	cdc_get_data(0, 0, 0xffff);
	cdc_trans_data(sbuf, 2048*0xc0);
	cdc_end_trans(&status);

	cdc_get_status(&status);
	cdc_get_buffer_size(&total, &npart, &free);
	mem_dump("Sector Data", sbuf, 256);

	memcmp(0x22100000, 0x06040000, 2048*0xc0);
}


/**********************************************************/



void cd_cmd(void)
{
	int total, npart, free;

	cdc_init();

	cdc_get_toc(toc_buf);
	show_toc();

	cdc_read_test();

	//cdc_read_sector(0x06040000, 0xd5, 0x195);

}

void cd_speed_test(void)
{
	int cmd_cnt, status, tm;

	cmd_cnt = 0;

	reset_timer();

	while(1){
		//cdc_get_status(&status);
		cdc_get_numsector(0, &tm);
		cmd_cnt += 1;
		tm = get_timer();
		if(tm>=HZ)
			break;
	}

	printk("cd_speed_test: cmd_cnt=%d tm=%d\n", cmd_cnt, tm);
}

