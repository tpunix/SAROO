

#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdifb.h"


typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;


int data_offset;
int cpk_height;
int cpk_width;
int cpk_bpp;
int cpk_audio_ch;
int cpk_audio_res;
int cpk_audio_comp;
int cpk_audio_freq;

/******************************************************************************/

u16 get_be16(u8 *buf)
{
	return (buf[0]<<8)|buf[1];
}

u32 get_be32(u8 *buf)
{
	return (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
}


/******************************************************************************/

int codebook_v4a[3][256*4];
int codebook_v1a[3][256*4];


int *codebook_v4;
int *codebook_v1;


/******************************************************************************/

// r = y+2*v;
// g = y-u/2-v;
// b = y+2*u

u8 clip_c(int c)
{
	if(c&~0xff)
		return (~c)>>31;
	return c;
}

int yuv2rgb(int y, int u, int v)
{
	u = (signed char)u;
	v = (signed char)v;


	int r = y+v*2;
	int g = y-(u>>1)-v;
	int b = y+u*2;

	r = clip_c(r);
	g = clip_c(g);
	b = clip_c(b);

	return (r<<16)|(g<<8)|b;
}


void update_codebook(u8 *buf, int len, int id)
{
	int flag=0, bits=0;
	int *ck = (id&2)? codebook_v1: codebook_v4;

	int c = 0;
	int p = 0;
	while(p<len){
		if((len-p)<4)
			break;

		int update = 1;
		if(id&1){
			if(bits==0){
				flag = get_be32(buf+p);
				bits = 32;
				p += 4;
			}
			update = flag&0x80000000;
			bits -= 1;
			flag <<= 1;
		}

		if(update){
			u8 *cbuf = buf+p;
			if(id&4){
				ck[0] = (cbuf[0]<<16) | (cbuf[0]<<8) | cbuf[0];
				ck[1] = (cbuf[1]<<16) | (cbuf[1]<<8) | cbuf[1];
				ck[2] = (cbuf[2]<<16) | (cbuf[2]<<8) | cbuf[2];
				ck[3] = (cbuf[3]<<16) | (cbuf[3]<<8) | cbuf[3];
				p += 4;
			}else{
				int u = cbuf[4];
				int v = cbuf[5];
				ck[0] = yuv2rgb(cbuf[0], u, v);
				ck[1] = yuv2rgb(cbuf[1], u, v);
				ck[2] = yuv2rgb(cbuf[2], u, v);
				ck[3] = yuv2rgb(cbuf[3], u, v);
				p += 6;
			}
		}
		ck += 4;
		c += 1;
		if(c>255)
			break;
	}
}



void draw_sublock(int x, int y, int c0, int c1, int c2, int c3)
{
	gdifb_pixel(x+0, y+0, c0);
	gdifb_pixel(x+1, y+0, c1);
	gdifb_pixel(x+0, y+1, c2);
	gdifb_pixel(x+1, y+1, c3);
}


void draw_block(int x, int y, u8 *buf, int bpp)
{
	int *ck;

	if(bpp==4){
		ck = codebook_v4+buf[0]*4;
		draw_sublock(x+0, y+0, ck[0], ck[1], ck[2], ck[3]);

		ck = codebook_v4+buf[1]*4;
		draw_sublock(x+2, y+0, ck[0], ck[1], ck[2], ck[3]);

		ck = codebook_v4+buf[2]*4;
		draw_sublock(x+0, y+2, ck[0], ck[1], ck[2], ck[3]);

		ck = codebook_v4+buf[3]*4;
		draw_sublock(x+2, y+2, ck[0], ck[1], ck[2], ck[3]);
	}else{
		ck = codebook_v1+buf[0]*4;

		draw_sublock(x+0, y+0, ck[0], ck[0], ck[0], ck[0]);

		draw_sublock(x+2, y+0, ck[1], ck[1], ck[1], ck[1]);

		draw_sublock(x+0, y+2, ck[2], ck[2], ck[2], ck[2]);

		draw_sublock(x+2, y+2, ck[3], ck[3], ck[3], ck[3]);
	}
}


void draw_screen(int x1, int y1, int x2, int y2, u8 *buf, int len, int id)
{
	int x, y, p=0;
	int flag=0, bits=0;

	for(y=y1; y<y2; y+=4){
		for(x=x1; x<x2; x+=4){
			int draw = 1;
			if(id&1){
				if(bits==0){
					flag = get_be32(buf+p);
					bits = 32;
					p += 4;
				}
				draw = flag&0x80000000;
				bits -= 1;
				flag <<= 1;
			}
			if(draw){
				int bpp = 1;
				if((id&2)==0){
					if(bits==0){
						flag = get_be32(buf+p);
						bits = 32;
						p += 4;
					}
					if(flag&0x80000000)
						bpp = 4;
					bits -= 1;
					flag <<= 1;
				}
				draw_block(x, y, buf+p, bpp);
				p += bpp;
			}
		}
	}
}


/******************************************************************************/

int last_y;

int parse_strip(u8 *buf, int len, int offset)
{
	//int id = get_be16(buf+0);
	int y1 = get_be16(buf+4);
	int x1 = get_be16(buf+6);
	int y2 = get_be16(buf+8);
	int x2 = get_be16(buf+10);

	if(y1==0 && last_y!=0){
		y1 = last_y;
		y2 += last_y;
	}
	last_y = y2;

	//printf("    Strip %04x: len=%04x  (%d,%d)-(%d,%d)\n", id, len, x1, y1, x2, y2);

	int sp = 12;
	while(sp<len){
		int chunk_id = get_be16(buf+sp+0);
		int chunk_sz = get_be16(buf+sp+2);
		//printf("      Chunk %04x: len=%04x offset=%08x\n", chunk_id, chunk_sz, offset+sp);

		if(chunk_id<0x3000){
			update_codebook(buf+sp+4, chunk_sz-4, chunk_id>>8);
		}else{
			draw_screen(x1, y1, x2, y2, buf+sp+4, chunk_sz-4, chunk_id>>8);
		}

		sp += chunk_sz;
	}

	return 0;
}


int parse_frame(u8 *buf, int len, int offset, u32 info1, u32 info2)
{
	u32 flen = get_be32(buf);
	int flag = flen>>24;
	flen &= 0x00ffffff;

#if 0
	int width = get_be16(buf+4);
	int height = get_be16(buf+6);
	int strips = get_be16(buf+8);
	printf("  Frame:  flag=%d len=%04x %dx%d  strips=%d\n", flag, flen, width, height, strips);
#endif

	last_y = 0;
	int sn = 0;

	int fp = 12;
	while(fp<flen){
		int sz = get_be16(buf+fp+2);

		if(flag==0 && sn>0){
			// 关于codebook的使用:
			//   Strip0始终使用codebook_vx0
			//   Strip1始终使用codebook_vx1
			//   非关键桢, 更新对应的codebook
			//   关键帧, 因为不需要参考前一桢的信息, 故Strip1的codebook需要从Strip0复制一份.
			memcpy(codebook_v4a[sn], codebook_v4a[sn-1], 256*4*4);
			memcpy(codebook_v1a[sn], codebook_v1a[sn-1], 256*4*4);
		}

		codebook_v4 = codebook_v4a[sn];
		codebook_v1 = codebook_v1a[sn];

		parse_strip(buf+fp, sz, offset+fp);
		sn += 1;
		fp += sz;
	}
	
	gdifb_flush();
	//gdifb_waitkey();

	return 0;
}



/******************************************************************************/


#define WAVE_BUFNUM 8

HWAVEOUT hWaveOut;

WAVEHDR wave_hdr[WAVE_BUFNUM];
u8 wave_buf[WAVE_BUFNUM*0x10000];
int wave_wp;


void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WOM_DONE) {
		LPWAVEHDR hdr = (LPWAVEHDR)dwParam1;
		hdr->dwBufferLength = 0;
	}
}


int wave_init(int freq, int ch, int res)
{
	WAVEFORMATEX wfx;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = ch;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = res;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    MMRESULT result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, 0, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        printf("Failed to open wave out device\n");
        return 1;
    }

	memset(wave_hdr, 0, sizeof(wave_hdr));
	memset(wave_buf, 0, sizeof(wave_buf));

	for(int i=0; i<WAVE_BUFNUM; i++){
		wave_hdr[i].lpData = (char*)(wave_buf+i*0x10000);
		wave_hdr[i].dwUser = i;
	}

	wave_wp = 0;

	return 0;
}


int wave_fillbuf(u8 *buf, int len)
{
	WAVEHDR *hdr = &wave_hdr[wave_wp];

	//printf("wave_buf %d: len=%ld\n", wave_wp, hdr->dwBufferLength);

	if(hdr->dwBufferLength){
		while(hdr->dwBufferLength){
			Sleep(10);
		}
		waveOutUnprepareHeader(hWaveOut, hdr, sizeof(WAVEHDR));
	}

	if(cpk_audio_ch==1){
		for(int i=0; i<len; i+=2){
			hdr->lpData[i+0] = buf[i+1];
			hdr->lpData[i+1] = buf[i+0];
		}
	}else{
		int half = len/2;
		for(int i=0; i<half; i+=2){
			hdr->lpData[i*2+0] = buf[i+1];
			hdr->lpData[i*2+1] = buf[i+0];
			hdr->lpData[i*2+2] = buf[half+i+1];
			hdr->lpData[i*2+3] = buf[half+i+0];
		}
	}
	hdr->dwBufferLength = len;

	MMRESULT result = waveOutPrepareHeader(hWaveOut, hdr, sizeof(WAVEHDR));
	if(result != MMSYSERR_NOERROR){
		printf("Failed to prepare WAVEHDR!\n");
		return -1;
	}

	result = waveOutWrite(hWaveOut, hdr, sizeof(WAVEHDR));
	if(result != MMSYSERR_NOERROR){
		printf("Failed to waveOutWrite!\n");
		return -1;
	}

	wave_wp += 1;
	wave_wp %= WAVE_BUFNUM;
	return 0;
}


/******************************************************************************/


int main(int argc, char *argv[])
{
	FILE *fp;
	u8 *fbuf;
	int fsize;
	int i;

	fp = fopen(argv[1], "rb");
	if(fp==NULL)
		return -1;

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fbuf = malloc(fsize);
	fread(fbuf, 1, fsize, fp);
	fclose(fp);

	if(strncmp((char*)fbuf, "FILM", 4)){
		printf("not a CPK file!\n");
		return -1;
	}

	data_offset = get_be32(fbuf+4);
	printf("Header: length=%04x version=%s\n", data_offset, (char*)(fbuf+8));

	int sp = 16;
	while(sp<data_offset){
		int chunk_len = get_be32(fbuf+sp+4);
		printf("Chunk: %s length %02x\n", (char*)fbuf+sp, chunk_len);
		if(strcmp((char*)fbuf+sp, "FDSC")==0){
			cpk_height = get_be32(fbuf+sp+12);
			cpk_width  = get_be32(fbuf+sp+16);
			cpk_bpp    = fbuf[sp+20];
			cpk_audio_ch   = fbuf[sp+21];
			cpk_audio_res  = fbuf[sp+22];
			cpk_audio_comp = fbuf[sp+23];
			cpk_audio_freq = get_be16(fbuf+sp+24);
			printf("  video info: %dx%d  %s\n", cpk_width, cpk_height, (char*)(fbuf+sp+8));
			printf("  audio info: %dkHz, %d bits,  %d channel, comp=%d\n",
					cpk_audio_freq, cpk_audio_res, cpk_audio_ch, cpk_audio_comp);

			if(cpk_audio_res==0)
				cpk_audio_res = 16;
			wave_init(cpk_audio_freq, cpk_audio_ch, cpk_audio_res);
			gdifb_init(cpk_width, cpk_height);

		}else if(strcmp((char*)fbuf+sp, "STAB")==0){
			int frame_rate = get_be32(fbuf+sp+8);
			int table_count = get_be32(fbuf+sp+12);
			printf("  frame_rate: %d\n", frame_rate);
			printf("  samples: %d\n", table_count);
#if 1
			u32 base_time = timeGetTime();
			int fn = 0;
			for(i=0; i<table_count; i++){
				u8 *tbl = fbuf+sp+16+16*i;
				u32 frame_offset = get_be32(tbl+0);
				int frame_length = get_be32(tbl+4);
				u32 info1 = get_be32(tbl+8);
				u32 info2 = get_be32(tbl+12);
				frame_offset += data_offset;
				if(info1!=0xffffffff){
					printf("\n%3d:  %08x %08x   %08x %08x\n", i, frame_offset, frame_length, info1, info2);

					u32 frame_time = (info1&0x7fffffff)*1000/frame_rate;

					int msec = frame_time%1000;
					u32 sec = frame_time/1000;
					u32 hour = sec/3600; sec %= 3600;
					u32 minute = sec/60; sec %= 60;
					printf("    %d:%02d:%02d.%03d\r", hour, minute, sec, msec);
					frame_time += base_time;
					while(1){
						u32 now = timeGetTime();
						if(now >= frame_time)
							break;
						Sleep(frame_time-now);
					}

					parse_frame(fbuf+frame_offset, frame_length, frame_offset, info1, info2);
					fn += 1;
				}else{
					printf("\n%3d:  %08x %08x   %08x %08x\n", i, frame_offset, frame_length, info1, info2);
					wave_fillbuf(fbuf+frame_offset, frame_length);
				}
			}
#else
			parse_frame(fbuf+0x0003bb78, 0x4cc4, 0x3bb78, 0x000000b0, 0x0002);
			parse_frame(fbuf+0x0004083c, 0x3498, 0x4083c, 0x000000b0, 0x0002);
#endif
		}

		sp += chunk_len;
	}

	gdifb_exit(0);
	return 0;
}


