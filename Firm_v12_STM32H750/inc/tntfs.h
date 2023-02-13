
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#if 0
typedef unsigned  char u8;
typedef unsigned short u16;
typedef unsigned   int u32;
typedef unsigned  long long u64;
typedef long long s64;
#endif

#define SIZE_T long long


int printk(char *fmt, ...);
#define logmsg(...)  printk(__VA_ARGS__)
#define logerr(...)  printk(__VA_ARGS__)


#define FS_MALLOC(s)    malloc(s)
#define FS_FREE(s)      free(s)


/******************************************************************************/


extern u8 lfn_utf8[256];
extern int lfn_utf8_len;


int unicode_to_utf8(u16 *u, int ulen, u8 *utf8);


/******************************************************************************/

#define ENOENT     2
#define EIO        5
#define EBADF      9
#define ENOMEM     12
#define EACCES     13
#define EFAULT     14
#define ENODEV     19
#define EINVAL     22
#define ENOSYS     40



typedef struct _partition_t
{
	u32 lba_start;
	u32 lba_size;
	struct _filesystem_t *fs;
}PARTITION;


typedef struct _block_t
{
	char name[8];
	int sectors;

	PARTITION parts[4];

	void *itf;
	int (*read_sector )(void *itf, u8 *buf, u32 start, int size);
	int (*write_sector)(void *itf, u8 *buf, u32 start, int size);
}BLKDEV;


#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


#define S_DIR     0x10000000
#define S_RDONLY  0x00000001
#define S_HIDDEN  0x00000002
#define S_SYSTEM  0x00000004
#define S_ARCHIVE 0x00000020


typedef struct {
	u16 year;
	u8  mon, d;
	u8  h, m, s;
	u8  pad;
}TIME;


typedef struct _stat_t
{
	SIZE_T size;
	TIME mtime;
	TIME ctime;
	int flags;
}STAT_T;


typedef void* FSD;
typedef void** FD;


typedef struct _filesystem_t
{
	char *name;

	void  *(*mount)(BLKDEV *bdev, int part);
	int   (*umount)(void *fs_super);
	FSD    (*find) (void *fs_super, FSD fsd, char *name, char *next, int do_list);
	int    (*stat) (void *fs_super, FSD fsd, STAT_T *st);
	int    (*open) (void *fs_super, FSD fsd);
	int    (*close)(void *fs_super, FSD fsd);
	SIZE_T (*lseek)(void *fs_super, FSD fsd, SIZE_T offset, int whence);
	int    (*read) (void *fs_super, FSD fsd, void *buf, int size);
	int    (*write)(void *fs_super, FSD fsd, void *buf, int size);

	void *super;
	FSD fsd;

}FSDESC;


int    f_list(char *file);
int    f_stat(char *file, STAT_T *st);
FD     f_open(char *file);
int    f_close(FD fd);
SIZE_T f_lseek(FD, SIZE_T offset, int whence);
int    f_read (FD, void *buf, int size);
int    f_write(FD, void *buf, int size);

int    f_mount(BLKDEV *bdev, int part);
int    f_umount(BLKDEV *bdev, int part);

int    f_initdev(BLKDEV *bdev, char *devname, int index);
int    f_removedev(BLKDEV *bdev);

