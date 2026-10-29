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
#include "vdp2.h"

/******************************************************************************/

// _BGR
static const unsigned int vga_pal[16] = {
	0x00000000, 0x00aa0000, 0x0000aa00, 0x00aaaa00,
	0x000000aa, 0x00aa00aa, 0x000055aa, 0x00aaaaaa,
	0x00555555, 0x00ff5555, 0x0055ff55, 0x00ffff55,
	0x005555ff, 0x00ff55ff, 0x0055ffff, 0x00ffffff,
};


#include "font_latin.h"
static const u8 *font_cjk = (u8*)0x02020000;

static int pos_x, pos_y;
static int text_color;

static int llen;
int fbw = 320;
int fbh = 240;
u8 *fbptr = (u8*)VDP2_VRAM;

#include "slogo.h"

void logo_trans(void)
{
#if 1
	int i, j;

	if(*(u16*)(VDP2_VRAM+0x0848)!=0x000d || *(u16*)(VDP2_VRAM+0x084c)!=0x000d)
		return;

	for(i=0; i<16; i++){
		MZCTL = (i<<12) | (i<<8) | 0x0001;
		for(j=0; j<300000; j++){};
	}

	memcpy((void*)VDP2_VRAM+0x4000, logo_dat+32, size_logo_dat-32);
	memcpy((void*)VDP2_CRAM, logo_dat, 32);
	memset((void*)VDP2_VRAM+0x6000, 0, 64*64*2);

	j = 9;
	i = 11;
	int cid = 0x0200;
	u16 *pt = (u16*)(VDP2_VRAM+0x6000+(j*64+i)*2);
	for(j=0; j<6; j++){
		for(i=0; i<20; i++){
			pt[i] = cid;
			cid += 1;
		}
		pt += 64;
	}

	for(i=14; i>=0; i--){
		MZCTL = (i<<12) | (i<<8) | 0x0001;
		for(j=0; j<300000; j++){};
	}
	MZCTL = 0;

	for(j=0; j<5000000; j++){};

	CCCTL = 0x0001;
	for(i=0; i<32; i++){
		CCRNA = i;
		for(j=0; j<300000; j++){};
	}
	CCCTL = 0x0000;

#endif
}

void vdp_init(void)
{
	int i;
	volatile unsigned int *vdp2_cram = (volatile unsigned int*)VDP2_CRAM;

	logo_trans();

	// Disable VDP1
	VDP1_REG_PTMR = 0;
	*((volatile u16 *)(VDP1_RAM)) = 0x8000;
	VDP1_REG_FBCR = 0x0000;
 	VDP1_REG_TVMR = 0x0000;
 	VDP1_REG_EWDR = 0x0000;
 	VDP1_REG_EWLR = (0 << 9) | 0;

	TVMD = 0x0000;

	RAMCTL = 0x2300;
	CYCA0L = 0x0e44;
	CYCA0U = 0xeeee;
	CYCA1L = 0x1e55;
	CYCA1U = 0xeeee;

	// Map Offset: NBG0 at 0(0x00000), NBG1 at 1(0x20000)
	MPOFN = 0x0010;
	// ColorRAM: NBG0 at 0, NBG1 at 1
	CRAOFA = 0x0010;
	// Character Control: NBG0/1: 512x256, 256 colors, bitmap
	CHCTLA = 0x1212;
	// Priority: 7
	PRINA = 0x0707;

	// Screen Scroll Value Registers: No scroll
	SCXIN0 = 0;
	SCXDN0 = 0;
	SCYIN0 = 0;
	SCYDN0 = 0;

	// BackScreen mode
	LCTA = 0x7fffe>>1;
	BKTA = 0x7fffe>>1;

	/* Clear VRAM */
	memset((void*)VDP2_VRAM, 0, 0x80000);

	/* Set CRAM */
	for (i = 0; i < 256; i++)
		vdp2_cram[i] = vga_pal[i];

	// Enable NBG0
	BGON = 0x0001;

	TVMD = 0x8010;
}


void conio_init(void)
{

	vdp_init();

	llen = 512;
	pos_x = 0;
	pos_y = 0;
	text_color = 0xf0;
	printk_putc = conio_putc;


	void fbtest();
	fbtest();
}


void nbg1_on(void)
{
	CCRNA = 0x0700; // 24:8
	CCCTL = 0x0002;

	BGON = 0x0003;
}


void nbg1_set_cram(int index, int r, int g, int b)
{
	*(u32*)(VDP2_CRAM + 0x0400 + index*4) = (b<<16) | (g<<8) | (r);
}


void nbg1_put_pixel(int x, int y, int color)
{
	*(u8*)(VDP2_VRAM + 0x20000 + y*512 + x) = color;
}


void vdp2_win0(int scr, int outside, int sx, int sy, int ex, int ey)
{
	if(scr==-1){
		WCTLA = 0;
		return;
	}

	WPSX0 = sx<<1;
	WPSY0 = sy;
	WPEX0 = ex<<1;
	WPEY0 = ey;

	if(scr==0){
		WCTLA = 0x0002 | (outside);
	}else{
		WCTLA = 0x0200 | (outside<<8);
	}
}


/******************************************************************************/


void fbtest(void)
{
	int i;

	put_rect(0, 0, fbw-1, fbh-1, 15);

	for(i=0; i<16; i++){
		int x = 10+i*16;
		int y = 32;
		put_box(x, y, x+10, y+10, i);
	}
}


/******************************************************************************/


static u8 *find_ucs(u8 *fdat, int ucs)
{
	int total = *(u16*)(fdat);
	u8 *fi;

	int low, high, mp;

	low = 0;
	high = total-1;
	while(low<=high){
		mp = (low+high)/2;
		fi = fdat+2+mp*5;
		int code = (fi[0]<<8) | fi[1];
		if(ucs==code){
			int offset = (fi[2]<<16) | (fi[3]<<8) | fi[4];
			return fdat+offset;
		}else if(ucs>code){
			low = mp+1;
		}else{
			high = mp-1;
		}
	}

	return NULL;
}


static u8 *find_font(int ucs)
{
	u8 *fdat;

	fdat = find_ucs(font_latin, ucs);
	if(fdat==NULL){
		fdat = find_ucs((u8*)font_cjk, ucs);
		if(fdat==NULL){
			fdat = find_ucs((u8*)font_cjk, 0x25a1);
		}
	}

	return fdat;
}


int fb_draw_font_buf(u8 *bmp, int x, int color, u8 *font_data, int lb, int rb)
{
	int r, c, bx;
	u8 fg = (color >> 4) & 0x0F;
	u8 bg = (color >> 0) & 0x0F;

	int ft_adv = font_data[0];
	int ft_bw = font_data[1]>>4;
	int ft_bh = font_data[1]&0x0f;
	int ft_bx = font_data[2]>>4;
	int ft_by = font_data[2]&0x0f;
	int ft_lsize = (ft_bw>8)? 2: 1;
	//printk("%02x: w=%d h=%d x=%d y=%d, adv=%d\n", v, ft_bw, ft_bh, ft_bx, ft_by, ft_adv);
	
	if(x+ft_adv<lb)
		return ft_adv;

	bx = x + ft_bx;
	bmp += ft_by*llen + bx;

	for (r=0; r<ft_bh; r++) {
		int b, mask;
		if(ft_lsize==1){
			b = font_data[3+r];
			mask = 0x80;
		}else{
			b = (font_data[3+r*2+0]<<8) | (font_data[3+r*2+1]);
			mask = 0x8000;
		}
		for (c=0; c<ft_bw; c++) {
			if((bx+c)>=lb && (bx+c)<=rb){
				bmp[c] = (b&mask) ? fg : bg;
			}
			mask >>= 1;
		}
		bmp += llen;
	}

	return ft_adv;
}


int fb_draw_font(int x, int y, int color, u8 *font_data, int lb, int rb)
{
	u8 *bmp = fbptr + y*llen;
	return fb_draw_font_buf(bmp, x, color, font_data, lb, rb);
}


static int utf8_to_ucs(char **ustr)
{
	u8 *str = (u8*)*ustr;
	int ucs = 0;

	if(*str==0){
		return 0;
	}else if(*str<0x80){
		*ustr = str+1;
		return *str;
	}else if(*str<0xe0){
		if(str[1]<0x80){
			ucs = '?';
			*ustr = str+1;
		}else{
			ucs = ((str[0]&0x1f)<<6) | (str[1]&0x3f);
			*ustr = str+2;
		}
		return ucs;
	}else{
		if(str[1]<0x80){
			ucs = '?';
			*ustr = str+1;
		}else if(str[2]<0x80){
			ucs = '?';
			*ustr = str+2;
		}else{
			ucs = ((str[0]&0x0f)<<12) | ((str[1]&0x3f)<<6) | (str[2]&0x3f);
			*ustr = str+3;
		}
		return ucs;
	}

}


void conio_putc(int ch)
{
	if(ch=='\r'){
		pos_x = 0;
	}else if(ch=='\n'){
		pos_y += 16;
		if(pos_y>=fbh){
			pos_x = 0;
			pos_y = 0;
		}
	}else{
		u8 *font_data = find_font(ch);
		int adv = fb_draw_font(pos_x, pos_y, text_color, font_data, 0, fbw-1);
		pos_x += adv;
		if(pos_x>=fbw){
			pos_x = 0;
			pos_y += 16;
		}
		if(pos_y>=fbh){
			pos_x = 0;
			pos_y = 0;
		}
	}
}


/******************************************************************************/

int abs(int v)
{
	return (v<0)? -v : v;
}

void put_pixel(int x, int y, int c)
{
	volatile unsigned char *bmp = (volatile unsigned char*)VDP2_VRAM;
	*(bmp + y*llen + x) = c;
}


void put_line (int x1, int y1, int x2, int y2, int c)
{
	int tmp;
	int dx = x2 - x1;
	int dy = y2 - y1;

	if (abs (dx) < abs (dy)) {
		if (y1 > y2) {
			tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
			dx = -dx; dy = -dy;
		}
		x1 <<= 16;
		/* dy is apriori >0 */
		dx = (dx << 16) / dy;
		while (y1 <= y2) {
			put_pixel (x1 >> 16, y1, c);
			x1 += dx;
			y1++;
		}
	} else {
		if (x1 > x2) {
			tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
			dx = -dx; dy = -dy;
		}
		y1 <<= 16;
		dy = dx ? (dy << 16) / dx : 0;
		while (x1 <= x2) {
			put_pixel (x1, y1 >> 16, c);
			y1 += dy;
			x1++;
		}
	}
}

void put_hline(int y, int x1, int x2, int c)
{
	volatile unsigned char *bmp = (volatile unsigned char*)VDP2_VRAM;
	int x;

	if(x2>x1){
		x2 = x2-x1+1;
	}else{
		x = x2;
		x2 = x1-x2+1;
		x1 = x;
	}

	bmp += y*llen+x1;
	memset((u8*)bmp, c, x2);
}

void put_vline(int x, int y1, int y2, int c)
{
	volatile unsigned char *bmp = (volatile unsigned char*)VDP2_VRAM;
	int y;

	if(y2>y1){
		y2 = y2-y1+1;
	}else{
		y = y2;
		y2 = y1-y2+1;
		y1 = y;
	}

	bmp += y1*llen+x;
	for(y=0; y<y2; y++){
		*bmp = c;
		bmp += llen;
	}
}

void put_rect (int x1, int y1, int x2, int y2, int c)
{
	put_hline (y1, x1, x2, c);
	put_hline (y2, x1, x2, c);
	put_vline (x1, y1, y2, c);
	put_vline (x2, y1, y2, c);
}

void put_box(int x1, int y1, int x2, int y2, int c)
{
	int y;

	for(y=y1; y<=y2; y++){
		put_hline(y, x1, x2, c);
	}
}

/******************************************************************************/


#define PAD_REPEAT_TIME  500
#define PAD_REPEAT_RATE  100

static u32 last_pdat = 0;
static u32 last_value;
static u32 curr_pdat = 0;
static int pdat_state = 0;
static int pdat_count = 0;
static int pdat_wait;

u32 conio_getc(void)
{
	int pdat = pad_read();
	if(pdat==-1)
		return 0;

	switch(pdat_state){
	case 0:
		// 空闲检测
		// Idle detection
		if(pdat != curr_pdat){
			curr_pdat = pdat;
			pdat_state = 1;
			pdat_count = 0;
		}
		break;
	case 1:
		// 等待稳定
		// Waiting for stability
		if(pdat == curr_pdat){
			pdat_count += 1;
			if(pdat_count==2){
				pdat_state = 2;
			}
		}else{
			pdat_state = 0;
		}
		break;
	case 2:
		// 稳定状态
		// Stable state
		pdat = last_pdat ^ curr_pdat;
		last_pdat = curr_pdat;
		if(curr_pdat){
			// 转入重复状态
			// Enter repeat state
			pdat_state = 3;
			pdat_wait  = get_timer() + MS2TICK(PAD_REPEAT_TIME);
		}else{
			pdat_state = 0;
		}
		last_value = (pdat<<16) | curr_pdat;
		return last_value;
	case 3:
		// 重复状态
		// Repeat state
		if(pdat != curr_pdat){
			pdat_state = 0;
		}else{
			if(get_timer()>=pdat_wait){
				pdat_wait  = PAD_REPEAT_RATE;
				return last_value;
			}
		}
		break;
	default:
		break;
	}

	return 0;
}


/******************************************************************************/

#define MENU_LB  8
#define MENU_RB  311

static int sel_oob;
static int sel_state;
static int sel_cnt;
static int sel_offset;
static u8 *sel_fdat[96];
static u8 *sel_pbuf = (u8*)0x060f8000;

int menu_draw_string(int x, int y, int color, char *str, int lb, int rb)
{
	int ch, adv;

	while( (ch=utf8_to_ucs(&str)) ){
		u8 *font_data = find_font(ch);
		adv = fb_draw_font(x, y, color, font_data, lb, rb);
		x += adv;
		if(x>rb)
			return 1;
	}

	return 0;
}


static void draw_menu_item(int index, char *item, int select)
{
	int y = 24+index*16;
	int color = text_color;

	if(select){
		color = 0x8f;
	}
	put_box(MENU_LB, y, MENU_RB, y+16-1, (color&0x0f));
	menu_draw_string(MENU_LB, y, color, item, MENU_LB, MENU_RB);
}


static int draw_menu_item_select(int index, char *item)
{
	int x;
	int color = 0x8f;
	int i, adv;

	//int y = 24+index*16;
	//put_box(MENU_LB, y, MENU_RB, y+16-1, (color&0x0f));
	memset(sel_pbuf, (color&0x0f), llen*16);

	x = MENU_LB-sel_offset;
	i = 0;
	while(sel_fdat[i]){
		adv = (sel_fdat[i])[0];
		if(x+adv>=MENU_LB){
			fb_draw_font_buf(sel_pbuf, x, color, sel_fdat[i], MENU_LB, MENU_RB);
		}
		i += 1;
		x += adv;
		if(x>MENU_RB){
			return 1;
		}
	}

	return 0;
}


static void put_select_item(int index)
{
	int i, y = 24+index*16;
	u8 *dst = fbptr + y*llen + MENU_LB;
	u8 *src = sel_pbuf+MENU_LB;

	for(i=0; i<16; i++){
#if 0
		memcpy(dst, src, (MENU_RB-MENU_LB+1));
#else
		//cpu_dmemcpy(dst, src, (MENU_RB-MENU_LB+1), 0);
		scu_dmemcpy(dst, src, (MENU_RB-MENU_LB+1), 0);
#endif
		dst += llen;
		src += llen;
	}
}


#define SEL_IDLE_TIME 1000
static int scroll_oob;
static int stimer_end;

void menu_timer(MENU_DESC *menu)
{

	if(sel_state==0){
		if(sel_oob==0)
			return;

		int now = get_timer();
		stimer_end = now + MS2TICK(SEL_IDLE_TIME);
		sel_state = 1;
	}else if(sel_state==1){
		int now = get_timer();
		if(now>=stimer_end){
			sel_state = 2;
			sel_offset = 0;
		}
	}else if(sel_state==2){
		sel_offset += 1;
		int i = menu->current;
		scroll_oob = draw_menu_item_select(i, menu->items[i]);
		sel_state = 3;
	}else if(sel_state==3){
		if((TVSTAT&0x0008)==0)
			return;
		put_select_item(menu->current);
		if(scroll_oob==0){
			sel_state = 4;
			int now = get_timer();
			stimer_end = now + MS2TICK(SEL_IDLE_TIME);
		}else{
			sel_state = 2;
		}
	}else if(sel_state==4){
		int now = get_timer();
		if(now>=stimer_end){
			sel_state = 5;
		}
	}else if(sel_state==5){
		sel_offset -= 1;
		int i = menu->current;
		draw_menu_item_select(i, menu->items[i]);
		sel_state = 6;
	}else if(sel_state==6){
		if((TVSTAT&0x0008)==0)
			return;
		put_select_item(menu->current);
		if(sel_offset==0){
			sel_state = 0;
		}else{
			sel_state = 5;
		}
	}

}


static void sel_init(MENU_DESC *menu)
{
	int ch, n, w;
	char *str = menu->items[menu->current];

	n = 0;
	w = 0;
	while( (ch=utf8_to_ucs(&str)) ){
		sel_fdat[n] = find_font(ch);
		w += (sel_fdat[n])[0];
		n += 1;
	}
	sel_fdat[n] = NULL;

	sel_oob = (w>(MENU_RB+1-MENU_LB))? 1: 0;
	sel_state = 0;
	sel_cnt = 0;
	sel_offset = 0;
}


void menu_update(MENU_DESC *menu, int new)
{
	int i, select;

	if(new>=0){
		int old = menu->current;
		menu->current = new;
		draw_menu_item(old, menu->items[old], 0);
		draw_menu_item(new, menu->items[new], 1);
	}else{
		for(i=0; i<menu->num; i++){
			select = (i==menu->current)? 1: 0;
			draw_menu_item(i, menu->items[i], select);
		}
	}

	menu_status(menu, NULL);

	sel_init(menu);
}


void draw_menu_frame(MENU_DESC *menu)
{
	memset(fbptr, 0, fbh*llen);
	menu_draw_string(16, 6, text_color, menu->title, 0, fbw-1);

	put_hline( 22, 0, fbw-1, 0x0f);
	put_hline(217, 0, fbw-1, 0x0f);

	menu_update(menu, -1);
	if(menu->version){
		menu_status(menu, menu->version);
	}
}


void menu_status(MENU_DESC *menu, char *string)
{
	int mx = 16;
	int my = 240-16-5;

	put_box(mx, my, mx+36*8-1, my+16-1, 0);
	if(string){
		menu_draw_string(mx, my, text_color, string, 0, fbw-1);
	}
}


void add_menu_item(MENU_DESC *menu, char *item)
{
	strncpy(menu->items[menu->num], item, 96);
	menu->items[menu->num][95] = 0;
	menu->num += 1;
}


int menu_default(MENU_DESC *menu, int ctrl)
{
	if(BUTTON_DOWN(ctrl, PAD_UP)){
		if(menu->current>0){
			menu_update(menu, menu->current-1);
		}
		return 1;
	}else if(BUTTON_DOWN(ctrl, PAD_DOWN)){
		if(menu->current<(menu->num-1)){
			menu_update(menu, menu->current+1);
		}
		return 1;
	}else{
		return 0;
	}
}


int menu_run(MENU_DESC *menu)
{
	u32 ctrl;
	int retv;

_restart:
	draw_menu_frame(menu);

#if 1
	while(1){
		ctrl = conio_getc();
		if(ctrl==0){
			menu_timer(menu);
			gif_timer();
			continue;
		}
		retv = menu->handle(ctrl);
		if(retv==MENU_EXIT)
			break;
		if(retv==MENU_RESTART){
			goto _restart;
		}
	}
#endif

	return 0;
}





/******************************************************************************/


