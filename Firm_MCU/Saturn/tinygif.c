

#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"


/******************************************************************************/


typedef struct {
	u16 prefix;
	u8  last;
	u8  current;
	u16 length;
}DICT;


typedef struct GIF_DECODER{
	// 指向文件头后面的数据块
	u8 *ibuf;
	int iptr;
	// 解码状态
	int frames;
	int gtimer;

	// 全局信息
	int gflag;
	int width;
	int height;

	// 调色板
	void (*set_palete)(struct GIF_DECODER *gd, int pal_num, u8 *pal, int global);
	// 像素输出
	void (*out_pixel)(struct GIF_DECODER *gd, int x, int y, int val);

	// 控制块信息
	int cflag;
	int delay;
	int trans_color;
	// 指向局部图像
	u8 *lbuf;
	int lpos;
	int lflag;
	int l_x;
	int l_y;
	int l_width;
	int l_height;

	// 块缓存
	u8 *bbuf;
	int buf_size;
	int buf_bits;
	int buf_pos;

	DICT *dict;
}GIF_DECODER;


static GIF_DECODER g_gd;
int gd_state = -1;


/******************************************************************************/


static int get_code(GIF_DECODER *gd, int code_len)
{
	int code;
	u8 *p = gd->bbuf;

	if(code_len > gd->buf_bits){
		p[1] = p[gd->buf_size+2-1];
		p[0] = p[gd->buf_size+2-2];
		gd->buf_pos = 16 - gd->buf_bits;

		gd->buf_size = gd->lbuf[gd->lpos];
		memcpy(p+2, gd->lbuf+gd->lpos+1, gd->buf_size);

		gd->lpos += gd->buf_size+1;
		gd->buf_bits += gd->buf_size*8;
	}

	int byte = gd->buf_pos>>3;
	int bit  = gd->buf_pos&7;
	code = (p[byte+2]<<16) | (p[byte+1]<<8) | p[byte+0];
	code >>= bit;
	code &= (1<<code_len)-1;

	gd->buf_pos  += code_len;
	gd->buf_bits -= code_len;
	return code;
}


static u8 *gif_decode_clip(GIF_DECODER *gd)
{
	int code_size, clear_code, eoi_code, code_len, code_level;
	int last_code, code, top, i;
	int x, y;
	u8 *p = gd->lbuf;

	gd->l_x = (p[2]<<8) | p[1];
	gd->l_y = (p[4]<<8) | p[3];
	gd->l_width  = (p[6]<<8) | p[5];
	gd->l_height = (p[8]<<8) | p[7];
	gd->lflag = p[9];
	//printk("lflag: %02x\n", gd->lflag);
	//printk("lres: (%d,%d) %dx%d\n", gd->l_x, gd->l_y, gd->l_width, gd->l_height);
	if(gd->lflag&0x80){
		// local palete
		int pal_num = 1<<((gd->lflag&7)+1);
		gd->set_palete(gd, pal_num, p+10, 0);
		p += pal_num*3;
	}
	p += 10;

	code_size = *p++;
	gd->lpos = p - gd->lbuf;
	clear_code = 1<<code_size;
	eoi_code = clear_code+1;

	// 初始化字典
	for(i=0; i<clear_code; i++){
		gd->dict[i].prefix = 0;
		gd->dict[i].last = i;
		gd->dict[i].current = i;
		gd->dict[i].length = 1;
	}


	gd->buf_bits = 0;
	code = clear_code;
	last_code = clear_code;

	x = 0;
	y = 0;

	while(1){
		if(code==clear_code){
			// clear
			top = eoi_code;
			code_len = code_size+1;
			code_level = clear_code<<1;
		}

		code = get_code(gd, code_len);
		//printf("C:%04x T:%04x\n", code, top);
		if(code==clear_code)
			continue;
		if(code==eoi_code)
			break;

		// 在字典中加一项。注意:第一次加入的是无效项(top=eoi)。
		gd->dict[top].prefix = last_code;
		gd->dict[top].length = gd->dict[last_code].length+1;
		gd->dict[top].last = gd->dict[last_code].last;
		if(code>=top){
			gd->dict[top].current = gd->dict[last_code].last;
		}else{
			gd->dict[top].current = gd->dict[code].last;
		}

		top += 1;
		if(top==code_level && code_len<12){
			code_len += 1;
			code_level <<= 1;
		}
		last_code = code;

		// output code
		int len = gd->dict[code].length;
		for(i=0; i<len; i++){
			gd->out_pixel(gd, x+len-1-i, y, gd->dict[code].current);
			code = gd->dict[code].prefix;
		}

		x += len;
		while(x > gd->l_width){
			x -= gd->l_width;
			y += 1;
		}

	}

	return p;
}


int gif_decode_info(GIF_DECODER *gd, u8 *fbuf, int fsize)
{
	u8 *p = fbuf;

	if(strncmp((char*)p, "GIF8", 4)){
		return -1;
	}
	p += 6;

	gd->gflag = p[4];
	gd->width  = (p[1]<<8) | p[0];
	gd->height = (p[3]<<8) | p[2];
	printk("gif_decode_info: %02x\n", gd->gflag);
	printk("   res: %dx%d\n", gd->width, gd->height);

	if(gd->gflag&0x80){
		// global palete
		int pal_num = 1<<((gd->gflag&7)+1);
		gd->set_palete(gd, pal_num, p+7, 1);
		p += pal_num*3;
	}
	p += 7;
	gd->ibuf = p;
	gd->iptr = 0;

	return 0;
}


int gif_decode_frame(GIF_DECODER *gd)
{
	u8 *p = gd->ibuf + gd->iptr;

	while(1){
		if(p[0]==0x3b){
			return 1;
		}else if(p[0]==0x21){
			if(p[1]==0xf9){
				gd->cflag = p[3];
				gd->delay = (p[5]<<8) | p[4];
				gd->trans_color = p[6];
				//printk("\nCTRL: %02x  %d\n", gd->cflag, gd->delay);
			}
			p += 2;
			while(p[0]){ p += p[0]+1; }
			p += 1;
		}else if(p[0]==0x2c){
			gd->lbuf = p;
			p = gif_decode_clip(gd);

			while(p[0]){ p += p[0]+1; }
			p += 1;
			gd->iptr = p - gd->ibuf;
			break;
		}else{
			printk("Wrong data!\n");
			return 1;
		}
	}

	return 0;
}




/******************************************************************************/

static int *gif_pal = (int*)0x61400100;
static u8 *gifbuf = (u8*)0x61401000;

static void gifb_set_palete(GIF_DECODER *gd, int pal_num, u8 *pal, int global)
{
	int i;

	for(i=0; i<pal_num; i++){
		gif_pal[i] = (pal[0]<<16) | (pal[1]<<8) | pal[2];
		pal += 3;
	}

	*(u8*)0x61400002 = 1;
}


static void gifb_put_pixel(GIF_DECODER *gd, int x, int y, int val)
{
	if(val==gd->trans_color){
		return;
	}

	while(x >= gd->l_width){
		x -= gd->l_width;
		y += 1;
	}

	x += gd->l_x;
	y += gd->l_y;

	*(u8*)(gifbuf + y*512 + x) = val;
}


void gif_decode_init(void)
{
	FIL fp;
	int retv;
	u8 *fbuf = (u8*)0x24020000;
	GIF_DECODER *gd = &g_gd;

	*(u32*)(0x61400000) = 0;
	memset(gd, 0, sizeof(GIF_DECODER));

	retv = f_open(&fp, "/SAROO/mainmenu_bg.gif", FA_READ);
	if(retv!=FR_OK){
		gd_state = -1;
		return;
	}

	int fsize = f_size(&fp);
	u32 rsize = 0;
	f_read(&fp, fbuf, fsize, &rsize);
	f_close(&fp);

	gd->bbuf = (u8*)0x24010000;
	gd->dict = (DICT*)(gd->bbuf+0x1000);
	gd->out_pixel  = gifb_put_pixel;
	gd->set_palete = gifb_set_palete;

	gif_decode_info(gd, fbuf, fsize);
	
	*(u16*)(0x61400004) = gd->width;
	*(u16*)(0x61400006) = gd->height;

	gd_state = 0;
}


void gif_decode_timer(void)
{
	GIF_DECODER *gd = &g_gd;

	if(gd_state==0){
		// 解码一帧图像
		//int start = osKernelGetTickCount();
		int eof = gif_decode_frame(gd);
		//start = osKernelGetTickCount()-start;
		//printk("gif_decode_frame: %d\n", start);
		if(eof){
			if(gd->frames>1){
				gd->iptr = 0;
				gd->frames = 0;
			}else{
				gd_state = -2;
			}
		}else{
			gd->frames += 1;
			gd_state = 1;
			// 通知SS来取
			*(u8*)0x61400000 = 1;
		}
	}else if(gd_state==1){
		if(*(u8*)0x61400000==0){
			// SS显示完毕会清标志位
			gd_state = 2;
			if(gd->delay){
				gd->gtimer = osKernelGetTickCount() + gd->delay;
				gd_state = 2;
			}else{
				gd_state = 0;
			}
		}
	}else if(gd_state==2){
		if(osKernelGetTickCount() >= gd->gtimer){
			gd_state = 0;
		}
	}
}


void gif_decode_exit(void)
{
	gd_state = -1;
}


/******************************************************************************/





