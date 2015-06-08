
#include "../main.h"

/******************************************************************************/

#define STIRQ_CDC  0x0001
#define STIRQ_CMD  0x0002
#define STIRQ_DAT  0x0004

#define ST_CTRL   REG16(0x60000004)
#define ST_STAT   REG16(0x60000006)

#define BLK_ADDR  REG  (0x60000008)
#define BLK_SIZE  REG16(0x6000000c)
#define FIFO_CTRL REG16(0x60000010)
#define FIFO_STAT REG16(0x60000012)

#define SS_CTRL   REG16(0x60000014)
#define HIRQ_CLR  REG16(0x60000016)

#define RESP1     REG16(0x60000018)
#define RESP2     REG16(0x6000001a)
#define RESP3     REG16(0x6000001c)
#define RESP4     REG16(0x6000001e)

#define SSCR1     REG16(0x60000020)
#define SSCR2     REG16(0x60000022)
#define SSCR3     REG16(0x60000024)
#define SSCR4     REG16(0x60000026)
#define HIRQ      REG16(0x60000028)
#define HMSK      REG16(0x6000002a)
#define SCMD      REG16(0x6000002e)

#define FIFO_RCNT REG  (0x60000030)





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


#define PLAYTYPE_SECTOR 1
#define PLAYTYPE_FILE   2
#define PLAYTYPE_DIR    3


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
	u32 fad_start;
	u32 fad_end;
	u32 file_offset;
	u16 sector_size;
	u8  ctrl_addr;
	u8  unuse;
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
	
	// 写CR4后置1. 读CR4后清0.
	u8 in_cmd;

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

	u8 last_buffer;

	int sector_size;
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
	FIL fp;

	// TOC缓存, 共102项
	u32 *TOC;
	// Subcode缓存
	u8  *SUBH;


	int play_type;
	int play_wait;
	u32 play_fad_start;
	u32 play_fad_end;
	int pause_request;
	int max_repeat;

}CDBLOCK;









extern CDBLOCK cdb;
extern TRACK_INFO tracks[MAX_TRACKS];

extern OS_SEM sem_wait_cmd;
extern OS_SEM sem_wait_cdc;
extern OS_SEM sem_wait_disc;

extern int in_isr;


__task void scmd_task(void);
__task void scdc_task(void);
void cdc_cmd_process(void);


void set_report(u8 status);

BLOCK *find_block(PARTITION *part, int index);
void remove_block(PARTITION *part, int index);

void trans_start(void);
void trans_handle(void);
int handle_diread(void);

BLOCK *alloc_block(void);
void free_block(BLOCK *block);

int filter_sector(BLOCK *wblk);

TRACK_INFO *get_track_info(int tn);
u32 track_to_fad(u16 track_index);
int fad_to_track(u32 fad);
u32 bswap32(u32 d);


extern CDBLOCK cdb;







/******************************************************************************/

