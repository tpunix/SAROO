
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/******************************************************************************/

typedef   signed int   s32;
typedef   signed short s16;
typedef   signed char  s8;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;


typedef struct {
	int unicode;

	/* Glyph bitmap infomation */
	int bw, bh;
	int bx, by;
	int advance;
	int bitmap[24];
	int bsize;
} GLYPH;


int clist[65536];
GLYPH *ctab[65536];
int total_chars;

int pixel_size;
int font_ascent;
int base_adj = 0;

u8 font_buf[0x100000];
int font_buf_size;

/******************************************************************************/


int ucs2utf8(u16 ucs, char *p)
{
	if(ucs<=0x7f){
		p[0] = ucs;
		return 1;
	}else if(ucs<=0x7ff){
		p[0] = 0xc0 | ((ucs>>6)&0x1f);
		p[1] = 0x80 | ((ucs>>0)&0x3f);
		return 2;
	}else if(ucs<=0xffff){
		p[0] = 0xe0 | ((ucs>>12)&0x0f);
		p[1] = 0x80 | ((ucs>>6)&0x3f);
		p[2] = 0x80 | ((ucs>>0)&0x3f);
		return 3;
	}

	return 0;
}


/******************************************************************************/


GLYPH *load_char(FILE *fp)
{
	int bmp;
	char lbuf[128];
	GLYPH *pg;

	pg = NULL;
	bmp = -1;
	while(1){
		if(fgets(lbuf, 128, fp)==NULL)
			break;

		if(strncmp(lbuf, "STARTCHAR", 9)==0){
			pg = (GLYPH*)malloc(sizeof(GLYPH));
			memset(pg, 0, sizeof(GLYPH));
		}else if(strncmp(lbuf, "ENCODING", 8)==0){
			pg->unicode = atoi(lbuf+9);
		}else if(strncmp(lbuf, "DWIDTH", 6)==0){
			pg->advance = atoi(lbuf+7);
		}else if(strncmp(lbuf, "BBX", 3)==0){
			sscanf(lbuf+4, "%d %d %d %d", &pg->bw, &pg->bh, &pg->bx, &pg->by);
			int ny = (font_ascent+base_adj-1) - (pg->bh - 1 + pg->by);
			if(ny<0)
				ny = 0;
			pg->by = ny;
			if(pg->bx<0){
				pg->advance -= pg->bx;
				pg->bx = 0;
			}
		}else if(strncmp(lbuf, "BITMAP", 6)==0){
			bmp = 0;
		}else if(strncmp(lbuf, "ENDCHAR", 7)==0){
			break;
		}else if(bmp>=0){
			sscanf(lbuf, "%x", &pg->bitmap[bmp]);
			bmp += 1;
		}

	}

	if(pg){
		pg->bsize = (pg->bw > 8)? 2*pg->bh : pg->bh;
	}

	return pg;
}


int load_bdf(char *bdf_name)
{
	FILE *fp;
	GLYPH *pg;
	char lbuf[128];

	total_chars = 0;
	memset(ctab, 0, sizeof(ctab));

	fp = fopen(bdf_name, "r");
	if(fp==NULL)
		return -1;

	fgets(lbuf, 128, fp);
	if(strncmp(lbuf, "STARTFONT", 9)){
		printf("STARTFONT not found!\n");
		return -1;
	}
	printf("\nLoad BDF: %s\n", bdf_name);

	pixel_size = 0;
	font_ascent = 0;
	while(1){
		if(fgets(lbuf, 128, fp)==NULL)
			break;
		if(strncmp(lbuf, "PIXEL_SIZE", 10)==0){
			pixel_size = atoi(lbuf+11);
			printf("  PIXEL_SIZE: %d\n", pixel_size);
		}else if(strncmp(lbuf, "FONT_ASCENT", 11)==0){
			font_ascent = atoi(lbuf+12);
			printf("  FONT_ASCENT: %d\n", font_ascent);
		}else if(strncmp(lbuf, "CHARS ", 6)==0){
			break;
		}
	}
	if(pixel_size==0 || pixel_size>14){
		printf("Wrong PIXEL_SIZE: %d\n", pixel_size);
		return -1;
	}


	while(1){
		pg = load_char(fp);
		if(pg==NULL)
			break;

		if(clist[pg->unicode]==0){
			free(pg);
			continue;
		}
		ctab[pg->unicode] = pg;
		total_chars += 1;
	}
	printf("  Chars load: %d\n", total_chars);

	fclose(fp);
	return 0;
}

/******************************************************************************/


void load_list(char *name)
{
	FILE *fp;
	char lbuf[128];
	int n;

	memset(clist, 1, sizeof(clist));

	if(name==NULL)
		return;

	fp = fopen(name, "r");
	if(fp==NULL)
		return;

	memset(clist, 0, sizeof(clist));

	n = 0;
	while(1){
		if(fgets(lbuf, 128, fp)==NULL)
			break;
		if(lbuf[0]=='#' || lbuf[0]=='\r' || lbuf[0]=='\n' || lbuf[0]==0){
			continue;
		}

		if(lbuf[4]=='-'){
			// 8123-8567
			int i, start, end;
			sscanf(lbuf, "%x-%x", &start, &end);
			for(i=start; i<=end; i++){
				clist[i] = 1;
				n += 1;
			}
		}else{
			// 8124=xx
			int ucs;
			lbuf[4] = 0;
			sscanf(lbuf, "%x", &ucs);
			clist[ucs] = 1;
			n += 1;
		}
	}

	printf("Load list form %s: %d\n", name, n);
}



/******************************************************************************/


void print_glyph(GLYPH *pg)
{
	int x, y;
	char ubuf[4];

	*(u32*)(ubuf) = 0;
	ucs2utf8(pg->unicode, ubuf);

	printf("\nchar %04x [%s]\n", pg->unicode, ubuf);
	printf("  bw=%-2d bh=%-2d bx=%-2d by=%-2d  adv=%d\n", pg->bw, pg->bh, pg->bx, pg->by, pg->advance);

	for(y=0; y<pg->bh; y++){
		printf("    ");
		int mask = (pg->bw>8)? 0x8000 : 0x80;
		int bd = pg->bitmap[y];
		for(x=0; x<pg->bw; x++){
			if(bd&mask)
				printf("*");
			else
				printf(".");
			mask >>= 1;
		}
		printf("\n");
	}

}

void dump_glyph(void)
{
	int i;

	for(i=0; i<65536; i++){
		if(ctab[i]){
			print_glyph(ctab[i]);
		}
	}
}


void dump_list(void)
{
	int i;
	char ubuf[4];

	for(i=0; i<65536; i++){
		if(ctab[i]){
			*(u32*)(ubuf) = 0;
			ucs2utf8(i, ubuf);
			printf("%04x=%s\n", i, ubuf);
		}
	}

}


/******************************************************************************/


void write_be16(u8 *p, int val)
{
	p[0] = (val>>8)&0xff;
	p[1] = val&0xff;
}


void write_be24(u8 *p, int val)
{
	p[0] = (val>>16)&0xff;
	p[1] = (val>>8)&0xff;
	p[2] = val&0xff;
}


void build_font(void)
{
	int i, j, p, offset;

	write_be16(font_buf, total_chars);
	offset = total_chars*5+2;

	p = 2;
	for(i=0; i<65536; i++){
		if(ctab[i]){
			GLYPH *pg = ctab[i];
			write_be16(font_buf+p, i);
			write_be24(font_buf+p+2, offset);
			p += 5;
			offset += pg->bsize+3;
		}
	}

	for(i=0; i<65536; i++){
		if(ctab[i]){
			GLYPH *pg = ctab[i];
			font_buf[p+0] = pg->advance;
			font_buf[p+1] = (pg->bw<<4) | pg->bh;
			font_buf[p+2] = (pg->bx<<4) | (pg->by&0x0f);
			p += 3;
			for(j=0; j<pg->bh; j++){
				if(pg->bw > 8){
					write_be16(font_buf+p, pg->bitmap[j]);
					p += 2;
				}else{
					font_buf[p] = pg->bitmap[j];
					p += 1;
				}
			}
		}
	}

	font_buf_size = p;
}


/******************************************************************************/


int bin2c(u8 *buffer, int size, char *file_name, char *label_name)
{
	FILE *dest;
	int i, unit=1;

	if((dest = fopen(file_name, "w")) == NULL) {
		printf("Failed to open/create %s.\n", file_name);
		return 1;
	}

	fprintf(dest, "#ifndef __%s__\n", label_name);
	fprintf(dest, "#define __%s__\n\n", label_name);
	fprintf(dest, "#define %s_size  %d\n", label_name, size);
	fprintf(dest, "static unsigned char %s[] = {", label_name);

	for(i=0; i<size; i+=unit) {
		if((i % (16/unit)) == 0){
			fprintf(dest, "\n\t");
		}else{
			fprintf(dest, " ");
		}
		if(unit==1){
			fprintf(dest, "0x%02x,", buffer[i]);
		}else if(unit==2){
			fprintf(dest, "0x%04x,", buffer[i]);
		}else{
			fprintf(dest, "0x%08x,", buffer[i]);
		}
	}

	fprintf(dest, "\n};\n\n#endif\n");

	fclose(dest);

	return 0;
}


/******************************************************************************/


int main(int argc, char *argv[])
{
	int retv, i;
	int flags = 0;
	char *bdf_name = NULL;
	char *font_name = NULL;
	char *list_name = NULL;
	char *label_name = NULL;

	for(i=1; i<argc; i++){
		if(strcmp(argv[i], "-dg")==0){
			flags |= 1;
		}else if(strcmp(argv[i], "-dl")==0){
			flags |= 2;
		}else if(strcmp(argv[i], "-l")==0){
			list_name = argv[i+1];
			i += 1;
		}else if(strcmp(argv[i], "-adj")==0){
			base_adj = atoi(argv[i+1]);
			i += 1;
		}else if(strcmp(argv[i], "-c")==0){
			flags |= 4;
			font_name = argv[i+1];
			i += 1;
		}else if(strcmp(argv[i], "-h")==0){
			flags |= 8;
			font_name = argv[i+1];
			label_name = argv[i+2];
			i += 2;
		}else if(bdf_name==NULL){
			bdf_name = argv[i];
		}
	}

	load_list(list_name);

	retv = load_bdf(bdf_name);
	if(retv<0)
		return -1;

	if(flags&1){
		dump_glyph();
	}else if(flags&2){
		dump_list();
	}else if(flags&4){
		build_font();
		FILE *fp = fopen(font_name, "wb");
		fwrite(font_buf, 1, font_buf_size, fp);
		fclose(fp);
	}else if(flags&8){
		build_font();
		bin2c(font_buf, font_buf_size, font_name, label_name);
	}

	return 0;
}

