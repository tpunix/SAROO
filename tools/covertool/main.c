//
// SEGA SATURN SAROO COVER TOOL - by Bucanero (github.com/bucanero)
//

/*
    Copyright (c) 2025 Bucanero <github.com/bucanero>

    SAROO COVER TOOL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SAROO COVER TOOL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    apayloadlong with SAROO COVER TOOL.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define CT_VERSION "1.0.0"
#define MAX_COVERS 0x4000 // Maximum number of covers supported
#define COVER_FILE "cover.bin"

// This structure represents the header for each cover image in the cover index.
// About adler32 hash, see https://github.com/tpunix/SAROO/blob/65d8764c24aa3a55f6c57f05c45b97f550d1fb49/Firm_MCU/Saturn/saturn_utils.c#L702
typedef struct cover_hdr
{
	char serial_id[12];
	uint32_t ip_hash; // disc IP.BIN adler32 hash
	uint32_t img_offset;
	uint16_t width;
	uint16_t height;
	char unused[8];
} cover_hdr_t;

typedef struct img_palette
{
	uint32_t rgba[256];
} bmp_palette_t;

typedef struct __attribute__((packed)) bmp_hdr
{
	uint16_t Signature;		//0000h	'BM'
	uint32_t FileSize;		//0002h	File size in bytes
	uint32_t reserved;		//0006h	unused (=0)
	uint32_t DataOffset;	//000Ah	Offset from beginning of file to the beginning of the bitmap data
	uint32_t Size;			//000Eh	Size of InfoHeader =40
	uint32_t Width;			//0012h	Horizontal width of bitmap in pixels
	uint32_t Height;		//0016h	Vertical height of bitmap in pixels
	uint16_t Planes;		//001Ah	Number of Planes (=1)
	uint16_t BitsPerPixel;	//001Ch	Bits per Pixel used to store palette entry information. 8 = 8bit palletized. NumColors = 256
	uint32_t Compression;	//001Eh	Type of Compression. 0 = BI_RGB   no compression
	uint32_t ImageSize;		//0022h	(compressed) Size of Image
	uint32_t XpixelsPerM;	//0026h	horizontal resolution: Pixels/meter
	uint32_t YpixelsPerM;	//002Ah	vertical resolution: Pixels/meter
	uint32_t Colors;		//002Eh	Number of actually used colors. For a 8-bit / pixel bitmap this will be 100h or 256.
	uint32_t ImpColors;		//0032h	Number of important colors. 0 = all
	bmp_palette_t Palette;	//0036h	present only if Info.BitsPerPixel <= 8
} bmp_header_t;


/* This is the basic Adler-32 hash calculation */
uint32_t adler32(uint8_t *data, int len) 
{
	uint32_t a = 1, b = 0;
	int i;

	for(i=0; i<len; i++) {
		a = (a + data[i]) % 65521;
		b = (b + a) % 65521;
	}

	return (b<<16) | a;
}

void get_cover_hash(FILE *f_in)
{
	char gid[16];
	uint8_t ipbuf[256];
	uint8_t *ip = ipbuf;

	if(fread(ipbuf, sizeof(ipbuf), 1, f_in) != 1){
		printf("Error! get_disc_ip failed!\n");
		return;
	}

	if(memcmp(ipbuf, "SEGA SEGASATURN ", 16))
		ip = ipbuf+0x10;

	// Check if the file is a valid SEGA SATURN disc image
	if(memcmp(ip, "SEGA SEGASATURN ", 16)){
		printf("Error! Not a SEGA SATURN disc image!\n");
		return;
	}

	memcpy(gid, ip+0x20, 16);
	char *p = strrchr(gid, 'V');
	if(p)
		*p = 0;
	p = strchr(gid, ' ');
	if(p)
		*p = 0;

	printf("Game ID: %s\n", gid);
	printf("Data   : %.64s\n", ip+0x30);
	printf("IP Hash: %08X\n", adler32(ip+0x30, 64));
}

// Function to export a single image from the cover index
void export_image(FILE* f_in, cover_hdr_t *idx)
{
	FILE *f_out;
	bmp_header_t bmp;
	uint8_t *img_data;
	char fname[128];

	img_data = malloc(idx->width * idx->height);
	if (!img_data) {
		printf(" -> Failed to allocate memory for image data.\n");
		return;
	}

	snprintf(fname, sizeof(fname), "cover_%s~%08X.bmp", idx->serial_id, idx->ip_hash);
	f_out = fopen(fname, "wb");
	if (!f_out) {
		free(img_data);
		printf(" -> Failed to open output file.\n");
		return;
	}

	memset(&bmp, 0, sizeof(bmp_header_t));
	bmp.Signature = 0x4D42; // 'BM'
	bmp.FileSize = sizeof(bmp_header_t) + (idx->width * idx->height);
	bmp.DataOffset = sizeof(bmp_header_t);
	bmp.Size = 40; // Size of InfoHeader
	bmp.Width = idx->width;
	bmp.Height = idx->height;
	bmp.Planes = 1; // Number of Planes
	bmp.BitsPerPixel = 8; // 8 bits per pixel for palette
	bmp.XpixelsPerM = 0x0B13; // Horizontal resolution: 2835 pixels/meter (72 DPI)
	bmp.YpixelsPerM = 0x0B13; // Vertical resolution: 2835 pixels/meter (72 DPI)
	bmp.Colors = 256; // Number of actually used colors

	fseek(f_in, idx->img_offset, SEEK_SET);
	fread(&bmp.Palette, sizeof(bmp_palette_t), 1, f_in);
	fread(img_data, 1, idx->width * idx->height, f_in);

	// Write the BMP header
	fwrite(&bmp, sizeof(bmp_header_t), 1, f_out);
	// Write the image data
	for (int i = (idx->height - 1); i >= 0; i--)
		fwrite(img_data + (i * idx->width), 1, idx->width, f_out);

	fclose(f_out);
	free(img_data);

	printf(" -> Exported to %s\n", fname);
}

uint32_t bmp_hash[MAX_COVERS] = {0};

// Function to import a BMP image and fill the cover index
void import_image(FILE *fbin, const char* fname, cover_hdr_t *idx, int pos)
{
	FILE *bmp_file;
	bmp_header_t bmp;
	uint8_t *img_data;

	// Read the BMP file
	bmp_file = fopen(fname, "rb");
	if (!bmp_file)
	{
		printf("ERROR! Can't open %s\n", fname);
		return;
	}

	fread(&bmp, sizeof(bmp_header_t), 1, bmp_file);
	if (bmp.Signature != 0x4D42 || bmp.BitsPerPixel != 8 || bmp.Width > 128 || bmp.Height > 192 || bmp.Compression != 0)
	{
		printf("ERROR! Invalid BMP file: %s\n", fname);
		fclose(bmp_file);
		return;
	}

	// Read the image data
	img_data = malloc(bmp.Width * bmp.Height);
	fread(img_data, 1, bmp.Width * bmp.Height, bmp_file);
	fclose(bmp_file);

	// Fill cover index
	sscanf(fname, "cover_%11[^~]~%08X.bmp", idx[pos].serial_id, &idx[pos].ip_hash);
	idx[pos].serial_id[11] = '\0'; // Ensure null-termination
	idx[pos].img_offset = ftell(fbin);
	idx[pos].width = bmp.Width;
	idx[pos].height = bmp.Height;

	bmp_hash[pos] = adler32(img_data, bmp.Width * bmp.Height);

	for (int i = 0; i < pos; i++)
		if (bmp_hash[i] == bmp_hash[pos])
		{
			idx[pos].img_offset = idx[i].img_offset; // Use the previous image offset
			printf("%04d: Duplicate image -> using %s [%08X]\n", pos, idx[i].serial_id, idx[i].ip_hash);
			free(img_data);
			return;
		}

	// Write palette
	fwrite(&bmp.Palette, sizeof(bmp_palette_t), 1, fbin);
	// Write image data
	for (int i = (bmp.Height - 1); i >= 0; i--)
		fwrite(img_data + (i * bmp.Width), 1, bmp.Width, fbin);

	free(img_data);
}

void print_usage(const char* arg)
{
	printf("Usage: %s [option] cover_file\n", arg);
	printf("    -u          Unpack a cover.bin file to BMP images for each game\n");
	printf("    -p          Pack BMP images and generate a cover.bin\n");
	printf("    -i          Generate IP.BIN Hash for a Saturn game disc image\n\n");
	printf("Example:\n");
	printf("    %s -u cover.bin\n", arg);
	printf("    %s -p coverlist.txt\n", arg);
	printf("    %s -i SaturnGame.bin\n", arg);
}

int main(int argc, char* argv[])
{
	FILE *fp, *f_out;
	cover_hdr_t cover_idx[MAX_COVERS];

	printf("\nSAROO Cover Tool v%s by Bucanero\n\n", CT_VERSION);
	if (argc < 3 || (strcmp(argv[1], "-u") != 0 && strcmp(argv[1], "-p") != 0 && strcmp(argv[1], "-i")) || argc > 3)
	{
		print_usage(argv[0]);
		return -1;
	}

	fp = fopen(argv[2], "rb");
	if (!fp)
	{
		printf("ERROR! Can't open %s\n", argv[2]);
		return -1;
	}

	if (argv[1][1] == 'i')
	{
		// Hash mode
		printf("Generating IP.BIN hash for %s...\n", argv[2]);
		get_cover_hash(fp);
		fclose(fp);
		return 0;
	}

	if (argv[1][1] == 'u')
	{
		// Unpack mode
		f_out = fopen("coverlist.txt", "w");
		if (!f_out)
		{
			printf("ERROR! Can't create coverlist.txt\n");
			return -1;
		}

		printf("Unpacking %s...\n", argv[2]);
		if (fread(cover_idx, sizeof(cover_hdr_t), MAX_COVERS, fp) != MAX_COVERS)
		{
			printf("ERROR! Failed to read cover index.\n");
			fclose(fp);
			return -1;
		}

		for (int i = 0; i < MAX_COVERS && cover_idx[i].serial_id[0]; i++)
		{
			printf("%04d: %12s [%08X] %dx%d 0x%08X", i, cover_idx[i].serial_id, cover_idx[i].ip_hash, cover_idx[i].width, cover_idx[i].height, cover_idx[i].img_offset);
			export_image(fp, &cover_idx[i]);
			fprintf(f_out, "cover_%s~%08X.bmp\n", cover_idx[i].serial_id, cover_idx[i].ip_hash);
		}
	} else {
		// Pack mode
		f_out = fopen(COVER_FILE, "wb");
		if (!f_out)
		{
			printf("ERROR! Can't create %s\n", COVER_FILE);
			return -1;
		}

		// Initialize cover index
		memset(cover_idx, 0, sizeof(cover_hdr_t) * MAX_COVERS);
		fwrite(cover_idx, sizeof(cover_hdr_t), MAX_COVERS, f_out);
		printf("Packing images from %s...\n", argv[2]);

		char *line = NULL;
		size_t linecap = 0;
		ssize_t linelen;
		int j = 0;
		while (j < MAX_COVERS && (linelen = getline(&line, &linecap, fp)) > 0)
		{
			line[linelen - 1] = '\0'; // Remove newline character
			import_image(f_out, line, cover_idx, j);
			printf("%04d: %12s [%08X] %dx%d 0x%08X\n", j, cover_idx[j].serial_id, cover_idx[j].ip_hash, cover_idx[j].width, cover_idx[j].height, cover_idx[j].img_offset);
			j++;
		}

		free(line);
		fseek(f_out, 0, SEEK_SET);
		fwrite(cover_idx, sizeof(cover_hdr_t), j, f_out); // Write cover index
		printf("Packed %d images into %s\n", j, COVER_FILE);
	}

	fclose(fp);
	fclose(f_out);
	printf("Done!\n");
	return 0;
}
