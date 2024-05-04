/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "main.h"


/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

int sd_read_sector(u32 block, int count, u8 *buf);
int sd_write_sector(u32 block, int count, u8 *buf);

static osSemaphoreId_t sem_diskio;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (BYTE pdrv)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (BYTE pdrv)
{
	sem_diskio = osSemaphoreNew(1, 1, NULL);
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
	int result, retry;

	result = osSemaphoreAcquire(sem_diskio, 100);
	if(result==osErrorTimeout)
		return RES_ERROR;

	//printk("disk_read: pdrv=%d sector=%08x count=%d buf=%08x\n", pdrv, sector, count, buff);
	retry = 4;
	while(retry){
		result = sd_read_sector(sector, count, buff);
		if(result){
			printk("disk_read: pdrv=%d sector=%08x count=%d\n", pdrv, sector, count);
			printk("    retv=%08x\n", result);
			retry -= 1;
			//sdio_reset();
		}else{
			if(retry<4)
				printk("    retry read OK.\n");
			break;
		}
	}

	osSemaphoreRelease(sem_diskio);

	if(result)
		return RES_ERROR;
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
	int result, retry;

	result = osSemaphoreAcquire(sem_diskio, 100);
	if(result==osErrorTimeout)
		return RES_ERROR;

	retry = 4;
	while(retry){
		result = sd_write_sector(sector, count, (BYTE *)buff);
		if(result){
			printk("disk_write: pdrv=%d sector=%08x count=%d\n", pdrv, sector, count);
			printk("    retv=%08x\n", result);
			retry -= 1;
			//sdio_reset();
		}else{
			if(retry<4)
				printk("    retry write OK.\n");
			break;
		}
	}

	osSemaphoreRelease(sem_diskio);

	if(result)
		return RES_ERROR;
	return RES_OK;

}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
	if(cmd==GET_SECTOR_SIZE)
		*(int*)buff = 512;

	return RES_OK;
}


