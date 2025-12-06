
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gdifb.h"



typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct {
	u8 cmd;
	u8 inst;
	u8 parityQ[2];
	u8 data[16];
	u8 ParityP[4];
}SUBCODE;


u8 buf[24*0x100000];

u32 clut[16];
u8 raw_buf[300*(216+216)];

void convert_block(int pos)
{
	u8 *sub = buf+pos;
	u8 tbuf[96];
	int i, j, k;

	memset(tbuf, 0, 96);

//	for(i=0; i<8; i++){ // 带PQ通道
	for(i=2; i<8; i++){ // 无PQ通道
		for(j=0; j<12; j++){
			u8 d = sub[i*12+j];
			for(k=0; k<8; k++){
				u8 b = (d>>(7-k))&1;
				tbuf[j*8+k] |= b<<(7-i);
			}
		}
	}

	memcpy(sub, tbuf, 96);
}


u8 reorder[24] = {
	0x00, 0x42, 0x7d, 0xbf, 0x64, 0x32, 0x96, 0xaf,
	0x08, 0x21, 0x3a, 0x53, 0x6c, 0x85, 0x9e, 0xb7,
	0x10, 0x29, 0x19, 0x5b, 0x74, 0x8d, 0xa6, 0x4b
};

void reorder_block(int size)
{
	u8 tbuf[192*2];
	int p = 192;
	int i, j;

	memcpy(tbuf, buf, 192);
	size -= 192;
	while(size>0){
		memcpy(tbuf+192, buf+p, 192);
		for(i=0; i<192; i+=24){
			for(j=0; j<24; j++){
				buf[p+i+j] = tbuf[i+reorder[j]];
			}
		}
		p += 192;
		size -= 192;
		memcpy(tbuf, tbuf+192, 192);
	}
}


void parse_block(int pos)
{
	SUBCODE *sc = (SUBCODE*)(buf+pos);
	int i;

	if((sc->cmd&0x3f)!=0x09)
		return;


	u8 inst = (sc->inst)&0x3f;
	sc->inst &= 0x3f;
	if(inst==30 || inst==31){
		//printf("%04x: %02x %02x\n", pos, sc->cmd, sc->inst);
		int base = (inst-30)*8;
		for(i=0; i<8; i++){
			u16 c = sc->data[i*2+0] & 0x3f;
			c = (c<<6) | (sc->data[i*2+1] & 0x3f);
			u8 b = (c>>8)&0x0f;
			u8 g = (c>>4)&0x0f;
			u8 r = c&0x0f;
			clut[base+i] = (b<<20) | (b<<16) | (g<<12) | (g<<8) | (r<<4) | (r);
		}
	}else if(inst==1){
		//printf("%04x: %02x %02x -- %02x %02x %02x %02x\n", pos, sc->cmd, sc->inst,
		//		sc->data[0], sc->data[1], sc->data[2], sc->data[3]);
		int c = sc->data[0]&0x0f;
		if(sc->data[1]==0)
			memset(raw_buf, c, 300*216);
	}else if(inst==2){
		//printf("%04x: %02x %02x\n", pos, sc->cmd, sc->inst);
		int c = sc->data[0]&0x0f;
		int x, y;
		for(y=0; y<216; y++){
			for(x=0; x<300; x++){
				if(x<6 || x>=294 || y<12 || y>=204){
					raw_buf[y*300+x] = c;
				}
			}
		}
	}else if(inst==6 || inst==38){
		//printf("%04x: %02x %02x\n", pos, sc->cmd, sc->inst);
		int c0 = sc->data[0]&0x0f;
		int c1 = sc->data[1]&0x0f;
		int row = sc->data[2]&0x1f;
		int col = sc->data[3]&0x3f;

		row *= 12;
		col *= 6;
		for(i=0; i<12; i+=1){
			int d = sc->data[4+i]&0x3f;
			int j;
			for(j=0; j<6; j++){
				int x = col+j;
				int y = row+i;
				int c = ( d & (1<<(5-j)) )? c1 : c0;
				if(inst==38)
					raw_buf[y*300+x] ^= c;
				else
					raw_buf[y*300+x]  = c;
			}
		}
	}else if(inst==20 || inst==24){
		//printf("%04x: %02x %02x -- %02x %02x\n", pos, sc->cmd, sc->inst, sc->data[1], sc->data[2]);
		int c = sc->data[0]&0x0f;
		int hcmd = (sc->data[1]>>4)&3;
		int hoff = 6-(sc->data[1]&0x07);
		int vcmd = (sc->data[2]>>4)&3;
		int voff = 12-(sc->data[2]&0x0f);

		if(hoff<0) hoff = 0;
		if(voff<0) voff = 0;

		if(hcmd==1){
			// right
			u8 hbuf[6];
			for(i=0; i<216; i++){
				u8 *lbuf = (u8*)(raw_buf+300*i);
				if(inst==20)
					memset(hbuf, c, hoff);
				else
					memcpy(hbuf, lbuf+(300-hoff), hoff);
				memmove(lbuf+hoff, lbuf, (300-hoff));
				memcpy(lbuf, hbuf, hoff);
			}
		}else if(hcmd==2){
			// left
			u8 hbuf[6];
			for(i=0; i<216; i++){
				u8 *lbuf = (u8*)(raw_buf+300*i);
				if(inst==20)
					memset(hbuf, c, hoff);
				else
					memcpy(hbuf, lbuf, hoff);
				memmove(lbuf, lbuf+hoff, (300-hoff));
				memcpy(lbuf+(300-hoff), hbuf, hoff);
			}
		}

		if(vcmd==1){
			// down
			u8 vbuf[300];
			for(int k=0; k<voff; k++){
				if(inst==20)
					memset(vbuf, c, 300);
				else
					memcpy(vbuf, raw_buf+300*215, 300);
				memmove(raw_buf+300, raw_buf, 300*215);
				memcpy(raw_buf, vbuf, 300);
			}
		}else if(vcmd==2){
			// up
			u8 vbuf[300];
			for(int k=0; k<voff; k++){
				if(inst==20)
					memset(vbuf, c, 300);
				else
					memcpy(vbuf, raw_buf, 300);
				memmove(raw_buf, raw_buf+300, 300*215);
				memcpy(raw_buf+300*215, vbuf, 300);
			}
		}
	}else if(inst==28){
		printf("%04x: %02x %02x\n", pos, sc->cmd, sc->inst);
	}else{
		printf("%04x: %02x %02x\n", pos, sc->cmd, sc->inst);
	}
}


void update_screen(void)
{
	int x, y, c;

	for(y=0; y<216; y++){
		for(x=0; x<300; x++){
			c = raw_buf[y*300+x];
			gdifb_pixel(x, y, clut[c]);
		}
	}

	gdifb_flush();
}


int main(int argc, char *argv[])
{
	FILE *fp;
	int fsize;
	int i;
	float frame_time, delay;

	fp = fopen(argv[1], "rb");
	if(fp==NULL)
		return -1;

	fsize = fread(buf, 1, 24*0x100000, fp);
	fclose(fp);

	if(buf[0]==0xff){
		for(i=0; i<fsize; i+=96){
			convert_block(i);
		}
		reorder_block(fsize);
	}

	if(argc==3){
		delay = atoi(argv[2]);
		delay = 40/delay;
	}else{
		delay = 1;
	}

	gdifb_init(300, 216);

	int count = 0;
	frame_time = clock();
	for(i=0; i<fsize; i+=24){
		parse_block(i);
		update_screen();

		count += 1;
		if(count==12){
			count = 0;

			frame_time += delay;
			while(1){
				float now = clock();
				if(now>=frame_time)
					break;
				Sleep(1);
			}
		}
	}

	if(argc==3){
		fp = fopen(argv[2], "wb");
		if(fp==NULL)
			return -1;

		fwrite(buf, 1, fsize, fp);
		fclose(fp);
	}

	return 0;
}


