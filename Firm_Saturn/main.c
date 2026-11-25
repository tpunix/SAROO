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

/* [MENU_WRAM] the resident (cart) image's own copies of these, at addresses
 * the build takes from the resident link (res_syms.ld) */
extern u32 res_debug_flag;
extern int res_bios_type;
extern int res_to_stm32;
extern void (*res_printk_putc)(int ch);
extern void res_sci_putc(int ch);
/* [MENU_WRAM] debug_flag, bios_type, BE32/LE16/LE32/LE32W: moved to res_util.c */


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
			p += (oreg[p+2]&0x0f)*4;
		}else if(oreg[p]==0x16){
			/* OEM Sega 6-player multitap: use socket 1 as PAD1. */
			if(oreg[p+2]==0x02){
				bits = (oreg[p+4]<<8) | oreg[p+6];
				bits ^= 0xFFFF;
			}
			p += 18;
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


/* [MENU_WRAM] smpc_cmd, stm32_puts, timers, usleep/msleep, sci_*, cpu/scu_dmemcpy, read_file, write_file: moved to res_util.c */

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

// [SEGA_ABUS] Debounced cover select: a cursor move only records the game;
// SSCMD_SELECT (which makes the STM32 load the cover into SDRAM) is sent once
// the cursor has rested SELECT_DELAY frames. Fast scrolling then causes no
// STM32 SDRAM traffic, which froze the menu at Sega cart timing.
#define SELECT_DELAY 15
static int sel_pending = -1;
static int sel_frames;
static int sel_vblank;

/*
 * [COVER_FIFO] Covers through the CD data FIFO (needs an mcuapp.bin with
 * SSCMD_COVER). The STM32 streams an 8-byte header ('C','V', width, height),
 * the 1 KB palette and the pixels into the FPGA's CD FIFO; the menu pops them
 * from the CD data port into high work RAM and draws them from there. The
 * picture never goes through the SDRAM, so the menu reads nothing from the
 * cart for it, and in the game list the menu doesn't read the cart at all.
 * This is the only way covers are loaded: if a cover doesn't come, the
 * empty frame is shown. If the STM32 doesn't answer at all (an mcuapp.bin
 * without SSCMD_COVER), the menu stops asking for this session: no covers.
 */
#define COVER_BUF   ((u8*)0x060C0000)	/* high work RAM: the SCU DMA can't read low work RAM */
#define CDC_DPORT   REG16(0x25818000)	/* CD data port: pops the FIFO */
#define COVER_FIRST_MS  3000	/* header: the STM32 may read the game's disc image first */
#define COVER_DATA_MS   1000	/* longest gap in the data */
static int cover_off;			/* the STM32 never answered: no covers this session */
static int in_list;

static int fifo_count(void)
{
	int s = SS_STAT;
	return (s&0x0800)? 2048 : (s&0x07ff);
}

/* [COVER_FIFO] diagnostics, shown on the status line (temporary) */
static int cd_stage, cd_cmd, cd_arg;
static u16 cd_hdr[8];

#define CDC_DPORT32 (*(volatile u32*)0x25818000)	/* 32-bit reads, as cdc_trans_data() does */

/* 1: cover in COVER_BUF, 0: no cover, -1: failed, -2: no answer at all */
static int cover_fetch(int index, int *pw, int *ph)
{
	int n, longs, k;
	u32 *p, v;

	cd_stage = 0; cd_cmd = 0; cd_arg = 0;
	for(k=0; k<8; k++) cd_hdr[k] = 0;

	/* nothing old left in the FIFO (no wait for SS_CMD==0: the STM32 never
	   clears it after SSCMD_STARTUP or SSCMD_SELECT; cover_send() resets the
	   FIFO itself before it sends) */
	while(fifo_count()>=2)
		v = CDC_DPORT32;
	while(fifo_count())
		n = CDC_DPORT;

	SS_ARG = index;
	SS_CMD = SSCMD_COVER;

	/* header: 8 words = 4 longs */
	cd_stage = 1;
	reset_timer();
	while(fifo_count()<8){
		if(get_timer()>MS2TICK(COVER_FIRST_MS)){
			cd_cmd = SS_CMD;
			return -2;
		}
	}
	cd_stage = 2;
	for(k=0; k<8; k+=2){
		v = CDC_DPORT32;
		cd_hdr[k]   = v>>16;
		cd_hdr[k+1] = v;
	}
	int w = cd_hdr[1];
	int h = cd_hdr[2];
	if(cd_hdr[0]!=0x4356 || w>512 || h>256){
		cd_cmd = SS_CMD;
		return -1;
	}

	cd_stage = 3;
	longs = (w && h)? (0x0400 + w*h + 3)/4 : 0;
	p = (u32*)COVER_BUF;
	reset_timer();
	while(longs){
		n = fifo_count()/2;
		if(n==0){
			if(get_timer()>MS2TICK(COVER_DATA_MS)){
				cd_cmd = SS_CMD;
				return -1;
			}
			continue;
		}
		reset_timer();
		if(n>longs)
			n = longs;
		longs -= n;
		while(n--)
			*p++ = CDC_DPORT32;
	}
	cd_stage = 4;
	reset_timer();
	while(SS_CMD && get_timer()<MS2TICK(COVER_DATA_MS));
	cd_cmd = SS_CMD;
	cd_arg = SS_ARG;

	*pw = w;
	*ph = h;
	return (w && h)? 1 : 0;
}
static void draw_image(int flag, u8 *src, int src_llen, int img_w, int img_h, int img_x, int img_y, int adj_x, int adj_y);

extern MENU_DESC sel_menu;

static void cover_update(int index)
{
	int w = 0, h = 0;
	int retv = (cover_off)? -3 : cover_fetch(index, &w, &h);
	if(retv==-2)
		cover_off = 1;

	/* [COVER_FIFO] diagnostics on the status line (temporary):
	   r = result (1 cover, 0 none, -1 bad data, -2 no answer, -3 off),
	   s = stage reached, n = most FIFO words seen while waiting,
	   m = first header word ('CV' = 4356), c = SS_CMD after, f = SS_STAT before */
	{
		char dbuf[64];
		snprintf(dbuf, 64, "%d%d %04x %04x %04x %02x %02x %02x %02x %02x a%d %02x",
			retv, cd_stage, cd_hdr[0], cd_hdr[1], cd_hdr[2], cd_hdr[3]&0xff, cd_hdr[4]&0xff, cd_hdr[5]&0xff,
			cd_hdr[6]&0xff, cd_hdr[7]&0xff, cd_arg, cd_cmd&0xff);
		menu_status(&sel_menu, dbuf);
	}
	if(retv>0){
		u8 *pal = COVER_BUF;
		for(int i=0; i<256; i++){
			nbg1_set_cram(i, pal[2], pal[1], pal[0]);
			pal += 4;
		}
		draw_image(3, COVER_BUF+0x0400, w, w, h, 0, 0, 176, (h>128)? 24 : 88);
	}else{
		memset(COVER_BUF, 0, 128*128);
		draw_image(4, COVER_BUF, 128, 128, 128, 0, 0, 176, 88);
	}
}

static void select_poll(void)
{
	int vb = (TVSTAT&0x0008)? 1 : 0;
	if(vb && !sel_vblank && sel_pending>=0 && sel_frames<SELECT_DELAY)
		sel_frames++;
	sel_vblank = vb;

	// no SS_CMD==0 check: the STM32 never clears SS_CMD after SSCMD_SELECT
	if(sel_pending>=0 && sel_frames>=SELECT_DELAY){
		int index = sel_pending;
		sel_pending = -1;
		cover_update(index);
	}
}

void gif_timer(void)
{
	select_poll();

	if((TVSTAT&0x0008)==0)
		return;

	/* [COVER_FIFO] in the game list the covers come through the FIFO: nothing
	   to poll in the cart */
	if(in_list)
		return;

	int gif_flag = *(u8*)0x22400000;
	if(gif_flag){
		int img_w = LE16((u8*)0x22400004);
		int img_h = LE16((u8*)0x22400006);
		int img_x = (short)LE16((u8*)0x22400008);
		int img_y = (short)LE16((u8*)0x2240000a);
		int adj_x = (short)LE16((u8*)0x2240000c);
		int adj_y = (short)LE16((u8*)0x2240000e);
		//printk("img: (%d,%d) %dx%d\n", img_x, img_y, img_w, img_h);

		draw_image(gif_flag, (u8*)(0x22401000), (gif_flag>=3)? 128 : 512,
			img_w, img_h, img_x, img_y, adj_x, adj_y);
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


/* [COVER_FIFO] the drawing part of gif_timer, unchanged, for both sources */
static void draw_image(int gif_flag, u8 *src, int src_llen, int img_w, int img_h, int img_x, int img_y, int adj_x, int adj_y)
{
	{
		u8 *dst = (u8*)(VDP2_VRAM+0x20000);

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
		if(index & (SELECT_ENTER|SELECT_EXIT)){
			// entering / leaving the list: send now, drop any pending move
			sel_pending = -1;
			if(index & SELECT_ENTER){
				// [COVER_FIFO] SSCMD_COVER also stops the background GIF
				in_list = 1;
				cover_update(page*MENU_ITEMS + (index & ~SELECT_ENTER));
			}else{
				// leaving the list: SSCMD_SELECT restarts the background GIF
				in_list = 0;
				SS_ARG = page*MENU_ITEMS + index;
				SS_CMD = SSCMD_SELECT;
			}
		}else{
			// cursor move: send later (select_poll, from gif_timer)
			sel_pending = page*MENU_ITEMS + index;
			sel_frames = 0;
		}
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
			use_sys_bup = 1;
			use_sys_load = 1;
			my_cdplayer();
		}else if(retv==2){
			type = 2;
		}else if(retv==3){
			// 是刻录游戏CD
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
			char buf[64];
			sprintf(buf, "%s%d", TT("不是游戏光盘!"), retv);
			menu_status(&main_menu, buf);
			cdblock_off();
			return 0;
		}

		// 停止正在播放的背景音乐。
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

	/* [MENU_WRAM] hand the debug settings to the resident copy, which runs the
	 * game start and the save hooks (plain data writes: no cart code runs) */
	res_debug_flag  = debug_flag;
	res_bios_type   = bios_type;
	res_to_stm32    = to_stm32;
	res_printk_putc = (printk_putc==sci_putc)? res_sci_putc : NULL;

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


