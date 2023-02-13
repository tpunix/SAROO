/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2013        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control module to the FatFs module with a defined API.        */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "../main.h"

int sd_read_blocks(u32 block, int count, u8 *buf);
int sd_write_blocks(u32 block, int count, u8 *buf);

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (BYTE pdrv)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (BYTE pdrv)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

#if 0
DRESULT disk_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	int result, retry;

	retry = 4;
	while(retry){
		result = sd_read_blocks(sector, count, buff);
		if(result){
			printk("disk_read: pdrv=%d sector=%08x count=%d\n", pdrv, sector, count);
			printk("    retv=%08x\n", result);
			retry -= 1;
		}else{
			if(retry<4)
				printk("    retry read OK.\n");
			break;
		}
	}

	if(result)
		return RES_ERROR;
	return RES_OK;
}

#else

DRESULT disk_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	int i, result, retry;

	result = 0;
	for(i=0; i<count; i++){
		retry = 8;
		while(retry){
			result = sd_read_blocks(sector, 1, buff);
			if(result){
				printk("disk_read: pdrv=%d sector=%08x count=%d retv=%08x\n", pdrv, sector, count, result);
				printk("retry read %d...\n", retry);
				retry -= 1;
			}else{
				sector += 1;
				buff += 512;
				break;
			}
		}
		
	}

	if(result)
		return RES_ERROR;
	return RES_OK;
}

#endif

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	int result;

	result = sd_write_blocks(sector, count, buff);
	if(result)
		return RES_ERROR;
	return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
	if(cmd==GET_SECTOR_SIZE)
		*(int*)buff = 512;

	return RES_OK;
}

