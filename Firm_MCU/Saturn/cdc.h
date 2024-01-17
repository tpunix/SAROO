
#include "main.h"

/******************************************************************************/

#define REG8(x)  ( *(volatile unsigned char*) (x) )
#define REG16(x) ( *(volatile unsigned short*)(x) )
#define REG(x)   ( *(volatile unsigned int*)  (x) )

#define FPGA_BASE 0x60000000

#define FIRM_ADDR 0x61000000
#define IMGINFO_ADDR 0x61080000
#define SYSINFO_ADDR 0x610a0000
#define TMPBUFF_ADDR 0x610c0000


#define STIRQ_CDC  0x0001
#define STIRQ_CMD  0x0002
#define STIRQ_DAT  0x0004

#define ST_CTRL   REG16(FPGA_BASE+0x04)
#define ST_STAT   REG16(FPGA_BASE+0x06)
#define FIFO_DATA REG16(FPGA_BASE+0x08)
#define FIFO_STAT REG16(FPGA_BASE+0x0c)
#define FIFO_RCNT REG16(FPGA_BASE+0x0e)
#define SS_CMD    REG16(FPGA_BASE+0x10)
#define SS_ARG    REG16(FPGA_BASE+0x12)
#define SS_CTRL   REG16(FPGA_BASE+0x14)

#define RESP1     REG16(FPGA_BASE+0x18)
#define RESP2     REG16(FPGA_BASE+0x1a)
#define RESP3     REG16(FPGA_BASE+0x1c)
#define RESP4     REG16(FPGA_BASE+0x1e)
#define SSCR1     REG16(FPGA_BASE+0x20)
#define SSCR2     REG16(FPGA_BASE+0x22)
#define SSCR3     REG16(FPGA_BASE+0x24)
#define SSCR4     REG16(FPGA_BASE+0x26)
#define HIRQ      REG16(FPGA_BASE+0x28)
#define HMSK      REG16(FPGA_BASE+0x2a)
#define HIRQ_CLR  REG16(FPGA_BASE+0x2c)
#define SS_MRGB   REG16(FPGA_BASE+0x2e)



#define HIRQ_CMOK      0x0001
#define HIRQ_DRDY      0x0002
#define HIRQ_CSCT      0x0004
#define HIRQ_BFUL      0x0008
#define HIRQ_PEND      0x0010
#define HIRQ_DCHG      0x0020
#define HIRQ_ESEL      0x0040
#define HIRQ_EHST      0x0080
#define HIRQ_ECPY      0x0100
#define HIRQ_EFLS      0x0200
#define HIRQ_SCDQ      0x0400
#define HIRQ_MPED      0x0800
#define HIRQ_MPCM      0x1000
#define HIRQ_MPST      0x2000

#define STAT_BUSY      0x00
#define STAT_PAUSE     0x01
#define STAT_STANDBY   0x02
#define STAT_PLAY      0x03
#define STAT_SEEK      0x04
#define STAT_SCAN      0x05
#define STAT_OPEN      0x06
#define STAT_NODISC    0x07
#define STAT_RETRY     0x08
#define STAT_ERROR     0x09
#define STAT_FATAL     0x0A
#define STAT_PERI      0x20
#define STAT_TRNS      0x40
#define STAT_WAIT      0x80
#define STAT_REJECT    0xFF




#define MAX_BLOCKS      200
#define MAX_SELECTORS   24
#define MAX_FILES       256

#define MAX_TRACKS      32



#define TRANS_DATA      1
#define TRANS_DATA_DEL  2
#define TRANS_TOC       3
#define TRANS_FINFO_ALL 4
#define TRANS_FINFO_ONE 5
#define TRANS_SUBQ      6
#define TRANS_SUBRW     7
#define TRANS_PUT       8


#define PLAYTYPE_SECTOR 1
#define PLAYTYPE_FILE   2
#define PLAYTYPE_DIR    3


#define SSCMD_PRINTF   0x0001
#define SSCMD_LISTDISC 0x0002
#define SSCMD_LOADDISC 0x0003
#define SSCMD_CHECK    0x0004
#define SSCMD_UPDATE   0x0005
#define SSCMD_FILERD   0x0006
#define SSCMD_FILEWR   0x0007
#define SSCMD_LISTBIN  0x0008
#define SSCMD_SSAVE    0x0009
#define SSCMD_LSAVE    0x000a
#define SSCMD_LMEMS    0x000b
#define SSCMD_SMEMS    0x000c


#define MSF_TO_FAD(m,s,f) ((m * 4500) + (s * 75) + f)


typedef struct BLOCK_T
{
	s32 size;
	u32 fad;
	u8 cn;
	u8 fn;
	u8 sm;
	u8 ci;
	struct BLOCK_T *next;
	u8 *data;
} BLOCK;

typedef struct
{
	u32 fad;
	u32 range;
	u8 mode;
	u8 chan;
	u8 smmask;
	u8 cimask;
	u8 fid;
	u8 smval;
	u8 cival;
	u8 c_true;
	u8 c_false;
} FILTER;

typedef struct
{
	s32 size;
	int numblocks;
	BLOCK *head;
	BLOCK *tail;
} PARTITION;

typedef struct
{
	FIL *fp;
	u32 fad_0;
	u32 fad_start;
	u32 fad_end;
	u32 file_offset;
	u16 sector_size;
	u8  ctrl_addr;
	u8  mode;
}TRACK_INFO;

typedef struct
{
	u32 lba;
	u32 size;
	u8  unit;
	u8  gap;
	u8  fid;
	u8  attr;
}FILE_INFO;

typedef struct
{
	u16 cr1;
	u16 cr2;
	u16 cr3;
	u16 cr4;

	// 有200个扇区缓存
	BLOCK block[MAX_BLOCKS];
	u8 block_free;

	// CD接口对应的过滤器.
	u8 cddev_filter;

	// 认证状态
	u8 saturn_auth;
	u8 mpeg_auth;


	// 有24个过滤器. 每个过滤器对应一个缓存分区
	FILTER filter[MAX_SELECTORS];
	PARTITION part[MAX_SELECTORS];



////// 状态与cdreport
	u32 fad;
	u8 status;
	u8 options;
	u8 repcnt;
	u8 ctrladdr;
	u8 track;
	u8 index;
	u8 unuse;


////// 数据传输类型 ///////////////////////////////////////////
	u8 trans_type;
	// 总传输长度
	int cdwnum;

	// 扇区数据传输
	u8 trans_part_index;    // 要传输哪个分区的数据
	u8 trans_block_start;   // 分区中第一个要传输的扇区
	u8 trans_bk;            // 当前传输的扇区
	u8 trans_block_end;     // 分区中最后一个要传输的扇区+1
	BLOCK *trans_block;
	u16 trans_size;
	u8 trans_finish;

	u8 last_buffer;

	int sector_size;
	int put_sector_size;
	int actsize;

////// 文件操作 ///////////////////////////////////////////////

	u32 cdir_lba;           // 当前目录的LBA地址
	u8  cdir_size;          // 当前目录的LBA大小
	u8  dir_filter;
	u8  cdir_drend;
	u8  rdir_state;
	u16 cdir_pfid;          // 当前目录页的文件开始编号
	u16 cdir_pfnum;         // 当前目录页的文件个数

	u32 root_lba;           // 根目录的LBA地址
	u32 root_size;          // 根目录的LBA大小

	FILE_INFO fi[256];
	u8 *FINFO;


////// ISO相关 ////////////////////////////////////////////////
	int track_num;
	TRACK_INFO *tracks;

	// TOC缓存, 共102项
	u32 *TOC;
	// Subcode缓存
	u8  *SUBH;


	int play_type;
	int play_wait;
	u32 play_fad_start;
	u32 play_fad_end;
	u32 old_fad;
	int pause_request;
	int max_repeat;
	TRACK_INFO *play_track;

}CDBLOCK;









extern CDBLOCK cdb;
extern TRACK_INFO tracks[MAX_TRACKS];

extern osSemaphoreId_t sem_wait_pause;
extern osSemaphoreId_t sem_wait_disc;

extern int in_isr;

extern int lang_id;
extern int debug_flags;
extern int auto_update;
extern int sector_delay;
extern int sector_delay_force;
extern int play_delay;
extern int play_delay_force;


/******************************************************************************/

#define _BUFIO     0x0001
#define _BUFINFO   0x0002
#define _FILEIO    0x0004
#define _CDRV      0x0008
#define _INFO      0x0010
#define _TRANS     0x0020
#define _DEBUG     0x0040
#define _FILTER    0x0080
#define _DTASK     0x0100

#define LOG_MASK_DEFAULT  (_FILEIO | _CDRV | _INFO | _FILTER | _DTASK)

extern int log_mask;

#define SSLOG(mask, fmt, ...)  {if(log_mask&mask) { printk(fmt, ##__VA_ARGS__);}}


/******************************************************************************/

void cdc_cmd_process(void);


void set_report(u8 status);
void set_status(u8 status);

void set_pause_ok(void);
void wait_pause_ok(void);
void disk_task_wakeup(void);
void cdc_delay(int ticks);

BLOCK *find_block(PARTITION *part, int index);
void remove_block(PARTITION *part, int index);

void trans_start(void);
void trans_handle(void);
int handle_diread(void);

BLOCK *alloc_block(void);
void free_block(BLOCK *block);

int filter_sector(TRACK_INFO *track, BLOCK *wblk);


/******************************************************************************/


char *get_token(char **str_in);
char *get_line(u8 *buf, int *pos, int size);

int parse_config(char *fname, char *gameid);


TRACK_INFO *get_track_info(int tn);
u32 track_to_fad(u16 track_index);
int fad_to_track(u32 fad);
u32 bswap32(u32 d);
void init_toc(void);

int list_bins(int show);
int list_disc(int show);
int load_disc(int index);
int unload_disc(void);

int open_savefile(void);
int load_savefile(char *gameid);
int flush_savefile(void);

int load_smems(int id);
int flush_smems(int flag);


/******************************************************************************/

