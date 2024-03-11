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


static const unsigned short vga_pal[256] = {
    0x0000, 0x5400, 0x02A0, 0x56A0, 0x0015, 0x5415, 0x0155, 0x56B5, 0x294A, 0x7D4A, 0x2BEA, 0x7FEA, 0x295F, 0x7D5F, 0x2BFF, 0x7FFF,
    0x0000, 0x0842, 0x1084, 0x14A5, 0x1CE7, 0x2108, 0x294A, 0x318C, 0x39CE, 0x4210, 0x4A52, 0x5294, 0x5AD6, 0x6739, 0x739C, 0x7FFF,
    0x7C00, 0x7C08, 0x7C0F, 0x7C17, 0x7C1F, 0x5C1F, 0x3C1F, 0x201F, 0x001F, 0x011F, 0x01FF, 0x02FF, 0x03FF, 0x03F7, 0x03EF, 0x03E8,
    0x03E0, 0x23E0, 0x3FE0, 0x5FE0, 0x7FE0, 0x7EE0, 0x7DE0, 0x7D00, 0x7DEF, 0x7DF3, 0x7DF7, 0x7DFB, 0x7DFF, 0x6DFF, 0x5DFF, 0x4DFF,
    0x3DFF, 0x3E7F, 0x3EFF, 0x3F7F, 0x3FFF, 0x3FFB, 0x3FF7, 0x3FF3, 0x3FEF, 0x4FEF, 0x5FEF, 0x6FEF, 0x7FEF, 0x7F6F, 0x7EEF, 0x7E6F,
    0x7ED6, 0x7ED8, 0x7EDB, 0x7EDD, 0x7EDF, 0x76DF, 0x6EDF, 0x62DF, 0x5ADF, 0x5B1F, 0x5B7F, 0x5BBF, 0x5BFF, 0x5BFD, 0x5BFB, 0x5BF8,
    0x5BF6, 0x63F6, 0x6FF6, 0x77F6, 0x7FF6, 0x7FB6, 0x7F76, 0x7F16, 0x3800, 0x3803, 0x3807, 0x380A, 0x380E, 0x280E, 0x1C0E, 0x0C0E,
    0x000E, 0x006E, 0x00EE, 0x014E, 0x01CE, 0x01CA, 0x01C7, 0x01C3, 0x01C0, 0x0DC0, 0x1DC0, 0x29C0, 0x39C0, 0x3940, 0x38E0, 0x3860,
    0x38E7, 0x38E8, 0x38EA, 0x38EC, 0x38EE, 0x30EE, 0x28EE, 0x20EE, 0x1CEE, 0x1D0E, 0x1D4E, 0x1D8E, 0x1DCE, 0x1DCC, 0x1DCA, 0x1DC8,
    0x1DC7, 0x21C7, 0x29C7, 0x31C7, 0x39C7, 0x3987, 0x3947, 0x3907, 0x394A, 0x394B, 0x394C, 0x394D, 0x394E, 0x354E, 0x314E, 0x2D4E,
    0x294E, 0x296E, 0x298E, 0x29AE, 0x29CE, 0x29CD, 0x29CC, 0x29CB, 0x29CA, 0x2DCA, 0x31CA, 0x35CA, 0x39CA, 0x39AA, 0x398A, 0x396A,
    0x2000, 0x2002, 0x2004, 0x2006, 0x2008, 0x1808, 0x1008, 0x0808, 0x0008, 0x0048, 0x0088, 0x00C8, 0x0108, 0x0106, 0x0104, 0x0102,
    0x0100, 0x0900, 0x1100, 0x1900, 0x2100, 0x20C0, 0x2080, 0x2040, 0x2084, 0x2085, 0x2086, 0x2087, 0x2088, 0x1C88, 0x1888, 0x1488,
    0x1088, 0x10A8, 0x10C8, 0x10E8, 0x1108, 0x1107, 0x1106, 0x1105, 0x1104, 0x1504, 0x1904, 0x1D04, 0x2104, 0x20E4, 0x20C4, 0x20A4,
    0x20A5, 0x20A6, 0x20A6, 0x20A7, 0x20A8, 0x1CA8, 0x18A8, 0x18A8, 0x14A8, 0x14C8, 0x14C8, 0x14E8, 0x1508, 0x1507, 0x1506, 0x1506,
    0x1505, 0x1905, 0x1905, 0x1D05, 0x2105, 0x20E5, 0x20C5, 0x20C5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

#include "font_latin.h"
static const u8 *font_cjk = (u8*)0x02020000;

static int pos_x, pos_y;
static int text_color;

static int llen;
int fbw = 320;
int fbh = 224;
u8 *fbptr = (u8*)VDP2_VRAM;

void vdp_init(void)
{
    int ii;
    volatile unsigned short *vdp2_vram = (volatile unsigned short *)VDP2_VRAM;
    volatile unsigned short *vdp2_cram = (volatile unsigned short *)VDP2_CRAM;
    TVMD = 0x0000;

    RAMCTL = RAMCTL & (~0x3000);

    // Map Offset Register: Bitmap screen will be located at VRAM offset 0
    MPOFN = 0;

    // Character Control Register: 256 colors, enable NBG0 as a bitmap. |8 for 1024x256 bitmap
    CHCTLA = 0x0012|8;

    // Screen Scroll Value Registers: No scroll
    SCXIN0 = 0;
    SCXDN0 = 0;
    SCYIN0 = 0;

	// BackScreen mode
	BKTA = 0x7fffe>>1;

    // Screen Display Enable Register: Invalidate the transparency code for
    // NBG0 and display NBG0
    BGON = 0x0001;

    /* Clear VRAM */
    for (ii = 0; ii < 0x40000; ii++)
        vdp2_vram[ii] = 0x0000;

    /* Clear CRAM */
    for (ii = 0; ii < 0x0800; ii++)
        vdp2_cram[ii] = 0x0000;
}


void conio_init(void)
{
    int ii;
    volatile unsigned short *vdp2_cram = (volatile unsigned short*)VDP2_CRAM;

	vdp_init();

    for (ii = 0; ii < 256; ii++)
        vdp2_cram[ii] = vga_pal[ii];

    //vdp2_cram[0] = vdp2_cram[1];
    //vdp2_cram[1] = 0x10 << 10;

	llen = 1024;

	pos_x = 0;
	pos_y = 0;
	text_color = 0xf0;
	printk_putc = conio_putc;

	TVMD = 0x8000;

	void fbtest();
	fbtest();
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

	if(ucs==0x30fc)
		ucs = 0x2014; // gb2312没有'ー'这个符号，用A1AA(U2014)代替。

	fdat = find_ucs(font_latin, ucs);
	if(fdat==NULL){
		fdat = find_ucs((u8*)font_cjk, ucs);
		if(fdat==NULL){
			fdat = find_ucs((u8*)font_cjk, 0x25a1);
		}
	}

	return fdat;
}


int fb_draw_font(int x, int y, int color, u8 *font_data, int lb, int rb)
{
	int r, c, bx;
	u8 *bmp;
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
	bmp = fbptr + (y+ft_by)*llen + bx;

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

static u32 last_pdat = 0;
static u32 last_value;
static u32 curr_pdat = 0;
static int pdat_state = 0;
static int pdat_count = 0;
static int pdat_wait;

u32 conio_getc(void)
{
	u32 pdat = pad_read();

	switch(pdat_state){
	case 0:
		// 空闲检测
		if(pdat != curr_pdat){
			curr_pdat = pdat;
			pdat_state = 1;
			pdat_count = 0;
		}
		break;
	case 1:
		// 等待稳定
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
		pdat = last_pdat ^ curr_pdat;
		last_pdat = curr_pdat;
		if(curr_pdat){
			// 转入重复状态
			pdat_state = 3;
			pdat_count = 0;
			pdat_wait  = 500;
		}else{
			pdat_state = 0;
		}
		last_value = (pdat<<16) | curr_pdat;
		//printk("key0: %08x\n", last_value);
		return last_value;
	case 3:
		// 重复状态
		if(pdat != curr_pdat){
			pdat_state = 0;
		}else{
			pdat_count += 1;
			if(pdat_count==pdat_wait){
				pdat_wait  = 30;
				pdat_count = 0;
				//printk("keyr: %08x\n", last_value);
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

#define MENU_LB  16
#define MENU_RB  303

static int sel_oob;
static int sel_state;
static int sel_cnt;
static int sel_offset;
static u8 *sel_fdat[96];

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
		color = 0x0f;
	}
	put_box(MENU_LB, y, MENU_RB, y+16-1, (color&0x0f));
	menu_draw_string(MENU_LB, y, color, item, MENU_LB, MENU_RB);
}


static int draw_menu_item_select(int index, char *item)
{
	int x, y = 24+index*16;
	int color = 0x0f;
	int i, adv;

	put_box(MENU_LB, y, MENU_RB, y+16-1, color);

	x = MENU_LB-sel_offset;
	i = 0;
	while(sel_fdat[i]){
		adv = (sel_fdat[i])[0];
		if(x+adv>=MENU_LB){
			fb_draw_font(x, y, color, sel_fdat[i], MENU_LB, MENU_RB);
		}
		i += 1;
		x += adv;
		if(x>MENU_RB){
			return 1;
		}
	}

	return 0;
}


#define SEL_IDLE_TIME 1280

void menu_timer(MENU_DESC *menu)
{

	if(sel_state==0){
		if(sel_oob==0)
			return;
		sel_cnt += 1;
		if(sel_cnt==SEL_IDLE_TIME){
			sel_state = 1;
			sel_offset = 0;
		}
	}else if(sel_state==1){
		sel_offset += 1;
		int i = menu->current;
		while((TVSTAT&0x0008)==0);
		int oob = draw_menu_item_select(i, menu->items[i]);
		if(oob==0){
			sel_state = 2;
			sel_cnt = 0;
		}
	}else if(sel_state==2){
		sel_cnt += 1;
		if(sel_cnt==SEL_IDLE_TIME){
			sel_state = 3;
		}
	}else if(sel_state==3){
		sel_offset -= 1;
		int i = menu->current;
		while((TVSTAT&0x0008)==0);
		draw_menu_item_select(i, menu->items[i]);
		if(sel_offset==0){
			sel_state = 0;
			sel_cnt = 0;
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


void menu_update(MENU_DESC *menu)
{
	int i, select;

	for(i=0; i<menu->num; i++){
		select = (i==menu->current)? 1: 0;
		draw_menu_item(i, menu->items[i], select);
	}

	menu_status(menu, NULL);

	sel_init(menu);
}


void draw_menu_frame(MENU_DESC *menu)
{
	memset(fbptr, 0, fbh*llen);
	menu_draw_string(32, 4, text_color, menu->title, 0, fbw-1);

	put_rect(10, 19, 309, 205, 0x0f);
	put_rect(13, 22, 306, 202, 0x0f);

	menu_update(menu);
	if(menu->version){
		menu_status(menu, menu->version);
	}
}


void menu_status(MENU_DESC *menu, char *string)
{
	int mx = 16;
	int my = 224-16;

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
			menu->current -= 1;
			menu_update(menu);
		}
		return 1;
	}else if(BUTTON_DOWN(ctrl, PAD_DOWN)){
		if(menu->current<(menu->num-1)){
			menu->current += 1;
			menu_update(menu);
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


