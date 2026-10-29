
#include "main.h"

static u32 CRC32[256];
static int init = 0;

//初始化表
// Initialize table
static void init_table(void)
{
	int i,j;
	u32 crc;

	for(i=0; i<256; i++){
		crc = i;
		for(j=0; j<8; j++){
			if(crc&1){
				crc = (crc >> 1) ^ 0xEDB88320;
			}else{
				crc = crc >> 1;
			}
		}
		CRC32[i] = crc;
	}
}

//crc32实现函数
// crc32 implementation function
u32 crc32(u8 *buf, int len, u32 crc)
{
	int   i;
	
	if(!init){
		init_table();
		init = 1;
	}

	crc ^= 0xffffffff;

	for(i = 0; i < len;i++){
		crc = CRC32[((crc&0xFF)^buf[i])] ^(crc>>8);
	}

	crc ^= 0xffffffff;
	return crc;
}



