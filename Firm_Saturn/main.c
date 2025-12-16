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

u32 mcu_ver;
u32 debug_flag;
int bios_type;

/**********************************************************/


u32 BE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
}


u32 LE16(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[1]<<8) | b[0];
}


u32 LE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[3]<<24) | (b[2]<<16) | (b[1]<<8) | b[0];
}

void LE32W(void *ptr, u32 val)
{
	u8 *b = (u8*)ptr;
	b[0] = (val>> 0)&0xff;
	b[1] = (val>> 8)&0xff;
	b[2] = (val>>16)&0xff;
	b[3] = (val>>24)&0xff;
}


/**********************************************************/


static int pad_state;

int pad_read(void)
{
	u32 bits=0, bits1=0;
	u8 *oreg;
	int p;

#ifdef PADMODE_DIRECT
	PDR1 = 0x60; bits |= (PDR1 & 0x8) << 0;
	PDR1 = 0x40; bits |= (PDR1 & 0xf) << 8;
	PDR1 = 0x20; bits |= (PDR1 & 0xf) << 12;
	PDR1 = 0x00; bits |= (PDR1 & 0xf) << 4;
    bits ^= 0xfFF8;
#else
	if(pad_state==0){
		if((TVSTAT&0x0008)==0)
			return -1;
		while(SF&0x01);
		SF = 0x01;
		IREG0 = 0x00;
		IREG1 = 0x08;
		IREG2 = 0xf0;
		COMREG = 0x10;
		pad_state = 1;
		return -1;
	}else if(pad_state==1){
		if(SF&1){
			return -1;
		}

		oreg = (u8*)0x20100021;
		p = 0;

		// read PAD1
		if(oreg[p]==0xf1){
			bits = (oreg[p+4]<<8) | (oreg[p+6]);
			bits ^= 0xFFFF;
			p += (oreg[p+2]&0x0f)*2;
		}else{
			p += 2;
		}
		// read PAD2
		if(oreg[p]==0xf1){
			bits1 = (oreg[p+4]<<8) | (oreg[p+6]);
			bits1 ^= 0xFFFF;
			bits |= bits1;
		}

		IREG0 = 0x40;
		pad_state = 2;
	}else if(pad_state==2){
		if((TVSTAT&0x0008)==0){
			pad_state = 0;
		}
		return -1;
	}
#endif

	//printk("pad_read: %04x\n", bits);
	return bits;
}


void pad_init(void)
{
	pad_state = 0;

#ifdef PADMODE_DIRECT
    PDR1 = 0;
    DDR1 = 0x60;
    IOSEL = IOSEL1;
#else
	DDR1 = 0;
	DDR2 = 0;
	EXLE = 0;
    IOSEL = 0;

	IREG0 = 0x00;
	IREG1 = 0x08;
	IREG2 = 0xf0;
#endif
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
	u32 stm32_base = TMPBUFF_ADDR;
	int len;

	len = strlen(str);
	LE32W((void*)(stm32_base+0), len);

	memcpy((void*)stm32_base+0x10, str, len+1);

	SS_CMD = 0x0001;
	while(SS_CMD);
}


/**********************************************************/

#if 0
const int HZ = 1000000;

void reset_timer(void)
{
	SS_TIMER = 0;
}

u32 get_timer(void)
{
	return SS_TIMER;
}

#else

const int HZ = 208496; /* for NTSC: 320 lines mode */
static int timer_base;

void reset_timer(void)
{
	timer_base = 0;
	TCR = 0x02;
	FRCH = 0;
	FRCL = 0;
}

u32 get_timer(void)
{
	int s1, s2, th, tl;
	while(1){
		s1 = FTCSR&0x02;
		th = FRCH;
		tl = FRCL;
		s2 = FTCSR&0x02;
		if(s1==s2)
			break;
	}

	th = (th<<8) | tl;
	if(s2&0x02){
		timer_base += 65536;
		FTCSR = 0;
	}

	return timer_base+th;
}

#endif


void usleep(u32 us)
{
	u32 now, end;

	if(us==0)
		us = 1;
	end = (us*HZ)/1000000;

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
	end = (ms*HZ)/1000;

	reset_timer();
	while(1){
		now = get_timer();
		if(now>=end)
			break;
	}
}

/**********************************************************/

// clk = 26874880
// bps = ((clk/32)/n)/(BRR+1)

void sci_init(void)
{
	SCR = 0x00;     /* set TE,RE to 0 */
	SMR = 0x00;     /* set SMR to 8n1 async mode */
//	BRR = 0x15;     /* set BRR to 9600 bps */
//	BRR = 0x01;     /* set BRR to 104980 bps */
	BRR = 0x00;     /* set BRR to 209960 bps */
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


void cpu_dmemcpy(void *dst, void *src, int size, int ch)
{
	int chcr = 0x5a11;

	if(((u32)src&0xfff8703f)==0x25800000){
		// CDBLOCK的DATA_PORT
		// CDBLOCK's DATA_PORT
		chcr &= 0xcfff;
	}

	while((CHCR0&0x03)==0x01);
	CHCR0 = 0;
	DMAOR = 1;

	SAR0 = (u32)src;
	DAR0 = (u32)dst;
	TCR0 = (size+3)>>2;
	CHCR0 = chcr;
}

#if 1
void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	volatile SCUDMA_REG *sdma = (SCUDMA_REG *)(SCU_DMA_BASE+ch*0x20);
	int mask  = 0x0030;
	int maskh = 0x0001;

	if(ch>0){ mask <<= 4; maskh <<= 1; }
	if(ch>1){ mask <<= 4; maskh   = 0; }
	mask |= maskh<<16;
	while(SDSTAT&mask); // 等待传输结束
	// Wait for transfer to complete

	sdma->src = (u32)src;
	sdma->dst = (u32)dst;
	sdma->count = size;
	sdma->addv = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	sdma->mode = 0x07;
	sdma->enable = 0x101;
}

#else

void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	while(SDSTAT&0x10030); // 等待传输结束
	// Wait for transfer to complete

	SD0R = (u32)src;
	SD0W = (u32)dst;
	SD0C = size;
	SD0AD = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	SD0MD = 0x07;
	SD0EN = 0x101;
}

#endif

/**********************************************************/

int read_file (char *name, int offset, int size, void *buf)
{
	LE32W((void*)(TMPBUFF_ADDR+0x00), offset);
	LE32W((void*)(TMPBUFF_ADDR+0x04), size);
	strcpy((void*)(TMPBUFF_ADDR+0x10), name);

	u32 bus_addr = (u32)buf & 0x0fffffff;
	if(bus_addr>=0x02000000 && bus_addr<0x03000000){
		// buf位于MCU和SS可以直接访问的空间内。
		// buf is in a space directly accessible by both MCU and SS.
		LE32W((void*)(TMPBUFF_ADDR+0x08), bus_addr);
	}else{
		// buf不可直接访问，必须中转。
		// buf is not directly accessible, must use intermediate transfer.
		LE32W((void*)(TMPBUFF_ADDR+0x08), TMPBUFF_ADDR+0x0100);
	}


	SS_ARG = 0;
	SS_CMD = SSCMD_FILERD;
	while(SS_CMD);

	int retv = (signed short)SS_ARG;
	if(retv<0)
		return retv;

	size = LE32((void*)(TMPBUFF_ADDR+0x04));
	if(bus_addr<0x02000000 || bus_addr>=0x03000000){
		memcpy(buf, (void*)(TMPBUFF_ADDR+0x0100), size);
	}

	return size;
}


int write_file(char *name, int offset, int size, void *buf)
{
	LE32W((void*)(TMPBUFF_ADDR), offset);
	LE32W((void*)(TMPBUFF_ADDR+0x04), size);
	strcpy((void*)(TMPBUFF_ADDR+0x10), name);

	u32 bus_addr = (u32)buf & 0x0fffffff;
	if(bus_addr>=0x02000000 && bus_addr<0x03000000){
		// buf位于MCU和SS可以直接访问的空间内。
		// buf is in a space directly accessible by both MCU and SS.
		LE32W((void*)(TMPBUFF_ADDR+0x08), bus_addr);
	}else{
		// buf不可直接访问，必须中转。
		// buf is not directly accessible, must use intermediate transfer.
		LE32W((void*)(TMPBUFF_ADDR+0x08), TMPBUFF_ADDR+0x0100);
		memcpy((void*)(TMPBUFF_ADDR+0x0100), buf, size);
	}


	SS_ARG = 0;
	SS_CMD = SSCMD_FILEWR;
	while(SS_CMD);

	int retv = (signed short)SS_ARG;
	if(retv<0)
		return retv;

	size = LE32((void*)(TMPBUFF_ADDR+0x04));
	return size;
}


/**********************************************************/


static u16 mksum(u32 addr)
{
	return (addr>>16) + addr;
}


int mem_test(int size)
{
	u32 addr0 = 0x22400000;
	u32 addr1 = 0x22600000;
	u16 rd, sum;
	int i;
	
	for(i=0; i<size; i++){
		*(u16*)(addr0) = mksum(addr0);
		addr0 += 2;
		*(u16*)(addr1) = mksum(addr1);
		addr1 += 2;
	}

	for(i=0; i<size; i++){
		addr0 -= 2;
		sum = mksum(addr0);
		rd  = *(u16*)(addr0);
		if(sum!=rd){
			printk("Mismatch0 at %08x! read=%08x calc=%08x\n", addr0, rd, sum);
			return -1;
		}
		*(u16*)(addr0) = ~sum;

		addr1 -= 2;
		sum = mksum(addr1);
		rd  = *(u16*)(addr1);
		if(sum!=rd){
			printk("Mismatch1 at %08x! read=%08x calc=%08x\n", addr1, rd, sum);
			return -2;
		}
		*(u16*)(addr1) = ~sum;
	}

	for(i=0; i<size; i++){
		sum = ~mksum(addr0);
		rd  = *(u16*)(addr0);
		if(sum!=rd){
			printk("Mismatch2 at %08x! read=%08x calc=%08x\n", addr0, rd, sum);
			return -3;
		}
		addr0 += 2;

		sum = ~mksum(addr1);
		rd  = *(u16*)(addr1);
		if(sum!=rd){
			printk("Mismatch3 at %08x! read=%08x calc=%08x\n", addr1, rd, sum);
			return -4;
		}
		addr1 += 2;
	}

	return 0;
}


/**********************************************************/

static int last_w, last_h, last_f;
static int cover_top = 1;

void gif_timer(void)
{
	if((TVSTAT&0x0008)==0)
		return;

	int gif_flag = *(u8*)0x22400000;
	if(gif_flag){
		u8 *dst = (u8*)(VDP2_VRAM+0x20000);
		u8 *src = (u8*)(0x22401000);
		int src_llen = (gif_flag>=3)? 128 : 512;

		int img_w = LE16((u8*)0x22400004);
		int img_h = LE16((u8*)0x22400006);
		int img_x = (short)LE16((u8*)0x22400008);
		int img_y = (short)LE16((u8*)0x2240000a);
		int adj_x = (short)LE16((u8*)0x2240000c);
		int adj_y = (short)LE16((u8*)0x2240000e);
		//printk("img: (%d,%d) %dx%d\n", img_x, img_y, img_w, img_h);

		//if( (last_f!=gif_flag) ){
		//	printk("gflag: %d\n", gif_flag);
		//}
		if( (last_f!=gif_flag) || (gif_flag>=3 && (last_w>img_w || last_h>img_h)) ){
			memset(dst, 0, 240*512);
		}
		last_f = gif_flag;
		last_w = img_w;
		last_h = img_h;

		src += img_y*src_llen+img_x;

		img_x += adj_x;
		if(img_x<0){
			src -= img_x;
			img_w += img_x;
			img_x = 0;
		}
		if(img_x+img_w>fbw){
			img_w = fbw-img_x;
		}

		img_y += adj_y;
		if(img_y<0){
			src -= img_y*512;
			img_h += img_y;
			img_y = 0;
		}
		if(img_y+img_h>fbh){
			img_h = fbh-img_y;
		}

#if 1
		if(cover_top){
			if(gif_flag==3){
				PRINA = 0x0706;
				CCCTL = 0x0000;
				BGON  = 0x0203;
				vdp2_win0(1, 1, img_x, img_y, img_x+img_w-1, img_y+img_h-1);
			}else{
				vdp2_win0(-1, 0, 0, 0, 0, 0);
				PRINA = 0x0707;
				CCCTL = 0x0002;
				BGON  = 0x0003;
			}
		}
#endif

		dst += img_y*512+img_x;

		//int start = get_timer();
		for(int y=0; y<img_h; y++){
			scu_dmemcpy(dst, src, img_w, 2);
			dst += 512;
			src += src_llen;
		}
		//int end = get_timer();
		//printk("gt: %d ms\n", TICK2MS(end-start));
		*(u8*)0x22400000 = 0;
	}

	int pal_flag = *(u8*)0x22400002;
	if(pal_flag){
		int i;
		u8 *pal = (u8*)0x22400100;
		for(i=0; i<256; i++){
			nbg1_set_cram(i, pal[2], pal[1], pal[0]);
			pal += 4;
		}
		*(u8*)0x22400002 = 0;
	}
}


static void change_cover_layer(void)
{
	if(cover_top==0){
		vdp2_win0(-1, 0, 0, 0, 0, 0);
		PRINA = 0x0707;
		CCCTL = 0x0002;
		BGON  = 0x0003;
	}else if(last_f==3){
		PRINA = 0x0706;
		CCCTL = 0x0000;
		BGON  = 0x0203;
		int x = 176;
		int y = (last_h>128)? 24 : 88;
		vdp2_win0(1, 1, x, y, x+last_w-1, y+last_h-1);
	}
}

/**********************************************************/

static int total_disc, total_page, page;
static int sel_mode; // 0:game  1:binary

MENU_DESC sel_menu;

static void fill_selmenu(void)
{
	int *disc_path = (int *)(IMGINFO_ADDR+0x04);
	char *path_str = (char*)(IMGINFO_ADDR);
	char tmp[128];
	int i;

	for(i=0; i<MENU_ITEMS; i++){
		int index = page*MENU_ITEMS+i;
		if(index>=total_disc)
			break;

		char *path = path_str + LE32(&disc_path[index]);
		snprintf(tmp, 128, "%2d: %s", index, path);
		tmp[127] = 0;
		if(sel_mode==0){
			// xxxx/xxxx.cue
			char *p = strchr(tmp, '/');
			if(p) *p = 0;
		}else{
			// xxxx@xxxx
			char *p = strchr(tmp, '@');
			if(p) *p = 0;
		}

		add_menu_item(&sel_menu, tmp);
	}

	int disp_page = (total_page)? page+1 : 0;
	if(sel_mode==0){
		sprintf(sel_menu.title, TT("选择游戏(%d/%d)"), disp_page, total_page);
	}else{
		sprintf(sel_menu.title, TT("选择文件(%d/%d)"), disp_page, total_page);
	}
}


static void select_notify(int index)
{
	if(sel_mode==0){
		SS_ARG = page*MENU_ITEMS + index;
		SS_CMD = SSCMD_SELECT;
	}
}


static void page_update(int up)
{
	MENU_DESC *menu = &sel_menu;

	menu->num = 0;
	fill_selmenu();

	menu->current = (up)? menu->num-1: 0;
	select_notify(menu->current);
	draw_menu_frame(menu);
}


int run_binary(int index, int run)
{
	int *disc_path = (int *)(IMGINFO_ADDR+0x04);
	char *path_str = (char*)(IMGINFO_ADDR);
	char bin_name[128];
	int load_addr = 0x06004000;

	sprintf(bin_name, "/SAROO/BIN/%s", path_str + LE32(&disc_path[index]));
	char *p = strchr(bin_name, '@');
	if(p){
		load_addr = strtoul(p+1, NULL, 16, NULL);
	}

	int retv = read_file(bin_name, 0, 0, (void*)load_addr);
	if(retv<0)
		return retv;

	int offset = 0;
	u32 *xbuf = (u32*)load_addr;
	if(xbuf[0]==0x53454741 && xbuf[1]==0x20534547)
		offset = 0x0f00;

	if(run){
		printk("Jump to %08x ...\n", load_addr);
		void (*go)(void) = (void(*)(void))(load_addr+offset);
		go();
	}else{
		printk("Load to %08x ...\n", load_addr);
	}

	return 0;
}


static int sel_handle(int ctrl)
{
	MENU_DESC *menu = &sel_menu;

	total_disc = LE32((u8*)(IMGINFO_ADDR));
	total_page = (total_disc+MENU_ITEMS-1)/MENU_ITEMS;

	if(BUTTON_DOWN(ctrl, PAD_UP)){
		if(menu->current>0){
			select_notify(menu->current-1);
			menu_update(menu, menu->current-1);
		}else if(page>0){
			page -= 1;
			page_update(1);
		}else if(total_page>1){
			page = total_page-1;
			page_update(1);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_DOWN)){
		if(menu->current<(menu->num-1)){
			select_notify(menu->current+1);
			menu_update(menu, menu->current+1);
		}else if((page+1)<total_page){
			page += 1;
			page_update(0);
		}else if(total_page>1){
			page = 0;
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_LT)){
		if(total_page>1){
			page -= 1;
			if(page<0){
				page = total_page-1;
			}
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_RT)){
		if(total_page>1){
			page += 1;
			if(page>=total_page){
				page = 0;
			}
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_A) && (total_page>0)){
		int index = page*MENU_ITEMS + menu->current;
		int retv;

		if(sel_mode==0){
			menu_status(menu, TT("游戏启动中......"));

			SS_ARG = index;
			SS_CMD = SSCMD_LOADDISC;
			while(SS_CMD);

			retv = SS_ARG;
			if(retv==0){
				retv = bios_cd_cmd(0);
			}
			if(retv){
				char buf[40];
				sprintf(buf, TT("游戏启动失败! %d"), retv);
				menu_status(menu, buf);
			}
		}else{
			menu_status(menu, TT("加载文件中......"));
			retv = run_binary(index, 1);
			if(retv){
				char buf[40];
				sprintf(buf, TT("文件加载失败! %d"), retv);
				menu_status(menu, buf);
			}
		}
	}else if(BUTTON_DOWN(ctrl, PAD_Z)){
		int index = page*MENU_ITEMS + menu->current;
		int retv;

		if(sel_mode==0){
			int index = page*MENU_ITEMS + menu->current;

			SS_ARG = index;
			SS_CMD = SSCMD_LOADDISC;
			while(SS_CMD);

			use_sys_load = 0;
			my_cdplayer();
		}else{
			menu_status(menu, TT("加载文件中......"));
			cdblock_on(0);
			retv = run_binary(index, 1);
			if(retv){
				char buf[40];
				sprintf(buf, TT("文件加载失败! %d"), retv);
				menu_status(menu, buf);
			}
		}
	}else if(BUTTON_DOWN(ctrl, PAD_LEFT)){
		cover_top ^= 1;
		change_cover_layer();
	}else if(BUTTON_DOWN(ctrl, PAD_C)){
		return MENU_EXIT;
	}

	return 0;
}


void select_game(void)
{
	total_disc = LE32((u8*)(IMGINFO_ADDR));
	total_page = (total_disc+MENU_ITEMS-1)/MENU_ITEMS;
	page = 0;

	memset(&sel_menu, 0, sizeof(sel_menu));
	sel_menu.handle = sel_handle;

	fill_selmenu();
	select_notify(sel_menu.current | SELECT_ENTER);

	menu_run(&sel_menu);

	select_notify(SELECT_EXIT);
}


/**********************************************************/


void select_bins(void)
{
	SS_CMD = SSCMD_LISTBINS;
	while(SS_CMD);

	select_game();

	SS_ARG = 0;
	SS_CMD = SSCMD_LISTDISC;
	while(SS_CMD);
}


/**********************************************************/


MENU_DESC category_menu;


static int category_handle(int ctrl)
{
	MENU_DESC *menu = &category_menu;

	if(menu_default(menu, ctrl))
		return 0;

	if(BUTTON_DOWN(ctrl, PAD_A)){
		int index = menu->current;

		SS_ARG = index;
		SS_CMD = SSCMD_LISTDISC;
		while(SS_CMD);

		select_game();
		return MENU_RESTART;
	}else if(BUTTON_DOWN(ctrl, PAD_C)){
		return MENU_EXIT;
	}

	return 0;
}


void select_category(void)
{
	int category_num = *(u8*)(SYSINFO_ADDR+0x0c);
	if(category_num==0)
		return select_game();

	memset(&category_menu, 0, sizeof(category_menu));
	category_menu.handle = category_handle;

	int i;
	for(i=0; i<category_num; i++){
		char *cat_name = (char*)(SYSINFO_ADDR+0x0e80+i*32);
		add_menu_item(&category_menu, cat_name);
	}

	strcpy(category_menu.title, TT("选择游戏分类") );
	menu_run(&category_menu);
}



/**********************************************************/


char *menu_str[] = {
	"选择游戏",
	"系统CD播放器",
	"运行光盘游戏",
	"运行二进制文件",
	"串口调试工具",
};
int menu_str_nr = sizeof(menu_str)/sizeof(char*);
char *update_str = "固件升级";
int update_index;

MENU_DESC main_menu;


int main_handle(int ctrl)
{
	int index = main_menu.current;

	if(menu_default(&main_menu, ctrl))
		return 0;

	if(BUTTON_DOWN(ctrl, PAD_LT)){
		lang_next();
		return MENU_EXIT;
	}

	if(!BUTTON_DOWN(ctrl, PAD_A) && !BUTTON_DOWN(ctrl, PAD_C))
		return 0;
	if(BUTTON_DOWN(ctrl, PAD_C) && index!=2)
		return 0;

	if(index==0){
		sel_mode = 0;
		select_category();
		return MENU_RESTART;
	}else if(index==1){
		cdblock_on(0);
		use_sys_bup = 1;
		use_sys_load = 1;
		my_cdplayer();
		return MENU_RESTART;
	}else if(index==2){
		int type = 4;
		menu_status(&main_menu, TT("检查光盘中......"));
		cdblock_on(1);
		//int retv = cdblock_check();
		int retv = jhl_auth_hack(100000);
		if(retv<0){
			menu_status(&main_menu, TT("未发现光盘!"));
			cdblock_off();
			return 0;
		}else if(retv==1){
			// 是Audio CD
			// Is Audio CD
			use_sys_bup = 1;
			use_sys_load = 1;
			my_cdplayer();
		}else if(retv==2){
			type = 2;
		}else if(retv==3){
			// 是刻录游戏CD
			// Is burned game CD
			retv = jhl_auth_hack(100000);
			if(retv!=2){
				char buf[64];
				sprintf(buf, "%s%d", TT("不是游戏光盘!"), retv);
				menu_status(&main_menu, buf);
				cdblock_off();
				return 0;
			}
			type = 2;
		}else if(retv!=4){
			// 是数据CD
			// Is data CD
			char buf[64];
			sprintf(buf, "%s%d", TT("不是游戏光盘!"), retv);
			menu_status(&main_menu, buf);
			cdblock_off();
			return 0;
		}

		// 停止正在播放的背景音乐。
		// Stop currently playing background music.
		SS_ARG = 0xffff;
		SS_CMD = SSCMD_LOADDISC;

		menu_status(&main_menu, TT("游戏启动中......"));
		if(BUTTON_DOWN(ctrl, PAD_C)){
			type |= 0x80;
		}
		retv = bios_cd_cmd(type);
		if(retv){
			char buf[40];
			sprintf(buf, TT("游戏启动失败! %d"), retv);
			menu_status(&main_menu, buf);
			cdblock_off();
		}
		return 0;
	}else if(index==3){
		sel_mode = 1;
		select_bins();
		return MENU_RESTART;
	}else if(index==4){
		menu_status(&main_menu, "Open serial console ...");
		sci_init();
		sci_shell();
		return MENU_EXIT;
	}else if(index==update_index){
		menu_status(&main_menu, TT("升级中,请勿断电..."));
		SS_ARG = 0;
		SS_CMD = SSCMD_UPDATE;
		while(SS_CMD);
		if(SS_ARG){
			menu_status(&main_menu, TT("升级失败!"));
		}else{
			menu_status(&main_menu, TT("升级完成,请重新开机!"));
		}
		while(1);
	}

	return 0;
}



int check_update(void)
{
	SS_ARG = 0;
	SS_CMD = SSCMD_CHECK;
	while(SS_CMD);

	printk("check_update: %02x\n", SS_ARG);
	return SS_ARG;
}


static char ver_buf[64];

void menu_init(void)
{
	int i;

	nbg1_on();

	memset(&main_menu, 0, sizeof(main_menu));
	strcpy(main_menu.title, TT("SAROO Boot Menu"));

	for(i=0; i<menu_str_nr; i++){
		add_menu_item(&main_menu, TT(menu_str[i]));
	}
	if(check_update()){
		add_menu_item(&main_menu, TT(update_str));
		update_index = i;
	}

	main_menu.handle = main_handle;

	sprintf(ver_buf, "MCU:%06x        SS:%06x        FPGA:%02x", mcu_ver&0xffffff, get_build_date()&0xffffff, SS_VER&0xff);
	main_menu.version = ver_buf;

	reset_timer();
	menu_run(&main_menu);
}


/**********************************************************/

// Sega Saturn 1.00J : BTR_1.00D1940921  0
// Victor Saturn 1.00: BTR_1.00DJ941018  1
//
// Sega Saturn 1.00a : BTR_1.000U941115  2  40417:f4
// Sega Saturn 1.01a : BTR_1.000U941115  3  40417:54
// Samsung Saturn    : BTR_1.000U941115  4  40417:84
//
// Sega Saturn 1.01J : BTR_1.0191941228  5
// V-Saturn    1.01J : BTR_1.019J950703  6
// HI-Saturn   1.01J : BTR_1.019H950130  7
// HI-Saturn   1.02J : BTR_1.020H950519  8
// HI-Saturn   1.03  : BTRF1.030H950720  9
//
// Sega Saturn 1.003 : BTRD1.0032941012  10
//


static char *bvstr[12] = {"0D1","0DJ","00U","00U","00U","191","19J","19H","20H","30H","032",};

void detect_bios_type(void)
{
	int i;
	char *bv = (char*)0x0807;

	for(i=0; i<11; i++){
		if(strncmp(bv, bvstr[i], 3)==0){
			bios_type = i;
			break;
		}
	}
	if(i==11){
		bios_type = BIOS_UNKNOW;
		return;
	}

	if(bios_type==BIOS_100A){
		if(*(u8*)(0x40417)==0x54){
			bios_type = BIOS_101A;
		}else if(*(u8*)(0x40417)==0x84){
			bios_type = BIOS_SAMS;
		}
	}
}


/**********************************************************/


int _main(void)
{
	// EFSDL/EFPAN for CDDA
	*(u8* )(0x25b00217) =  0xc0;
	*(u8* )(0x25b00237) =  0xc0;
	// Master Volume
	*(u16*)(0x25b00400) =  0x020f;

	detect_bios_type();

	SS_ARG = 0;
	SS_CMD = SSCMD_STARTUP;


	conio_init();
	pad_init();

	printk("    SRAOOO start!\n");

	mcu_ver = LE32((void*)(SYSINFO_ADDR+0x00));
	lang_id = LE32((void*)(SYSINFO_ADDR+0x04));
	debug_flag = LE32((void*)(SYSINFO_ADDR+0x08));

	// restore bios function
	*(u32*)(0x06000288) = 0x000018a8;  // bios_loadcd_boot
	*(u32*)(0x0600028c) = 0x00001874;  // bios_loadcd_readip
	*(u32*)(0x0600029c) = 0x00001904;  // bios_loadcd_init
	*(u32*)(0x060002dc) = *(u32*)(0x2000111c);

	*(u32*)(0x02000f04) =  (u32)cdc_read_sector;
	*(u32*)(0x02000f08) =  (u32)read_file;
	*(u32*)(0x02000f0c) =  (u32)write_file;
	*(u32*)(0x02000f24) =  (u32)conio_getc;
	*(u32*)(0x02000f30) =  (u32)bios_cd_cmd;
	*(u32*)(0x02000f34) =  (u32)page_update;
	*(u32*)(0x02000f38) =  (u32)sci_init;
	*(u32*)(0x02000f3c) =  (u32)printk;

	sys_bup_init = (void*)*(u32*)(0x20000958);

	if(debug_flag&0x0001){
		// sci_putc
		sci_init();
		printk("\n\nSAROOO Serial Monitor Start!\n");
		printk("   MCU ver: %08x\n", mcu_ver);
		printk("    SS ver: %08x\n", get_build_date());
		printk("  FPGA ver: %08x\n", SS_VER);
		printk("   lang_id: %d\n", lang_id);
		printk(" bios_type: %d\n", bios_type);
	}else if(debug_flag&0x0004){
		// conio_putc
	}else if(debug_flag&0x0008){
		// output to mcu
		to_stm32 = 1;
	}else{
		printk_putc = NULL;
	}

	cdblock_off();

	if((debug_flag&0x0003)==0x0003){
		sci_shell();
	}

	lang_init();
	while(1){
		menu_init();
	}

	return 0;
}


