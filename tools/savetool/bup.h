
#ifndef _BUP_H_
#define _BUP_H_

/*****************************************************************************/

typedef unsigned  char u8;
typedef unsigned short u16;
typedef unsigned   int u32;
typedef unsigned long long u64;


typedef struct {
	char file_name[12];
	char comment[11];
	char language;
	u32  date;
	u32  data_size;
	u8  *dbuf;
}SAVEINFO;

extern u8 save_buf[0x100000];

/*****************************************************************************/



u8 *load_file(char *name, int *size);
int write_file(char *file, void *buf, int size);

u32 get_be32(void *p);
u32 get_be16(void *p);
void put_be32(void *p, u32 v);
void put_be16(void *p, u32 v);

void set_bitmap(u8 *bmp, int index, int val);
SAVEINFO *load_saveraw(char *save_name);
void bup_flush(void);

int sr_bup_init(u8 *buf);
int sr_bup_list(int slot_id);
int sr_bup_export(int slot_id, int save_id, int exp_type);
int sr_bup_import(int slot_id, int save_id, char *save_name);
int sr_bup_delete(int slot_id, int save_id);
int sr_bup_create(char *game_id);

int ss_bup_init(u8 *buf);
int ss_bup_list(int slot_id);
int ss_bup_export(int slot_id, int save_id, int exp_type);
int ss_bup_import(int slot_id, int save_id, char *save_name);
int ss_bup_delete(int slot_id, int save_id);
int ss_bup_create(char *game_id);

int sr_mems_init(u8 *buf);
int sr_mems_list(int slot_id);
int sr_mems_export(int slot_id, int save_id, int exp_type);
int sr_mems_import(int slot_id, int save_id, char *save_name);
int sr_mems_delete(int slot_id, int save_id);
int sr_mems_create(char *game_id);


/*****************************************************************************/

#endif

