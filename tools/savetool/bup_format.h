/*
 * bup_header.h - structure definition for the .BUP file header used
 * by various Sega Saturn tools.
 *
 * Format developed by Cafe-Alpha (https://segaxtreme.net/threads/save-game-metadata-questions.24625/post-178534)
 *
 */

/** .BUP extension is required to work wih PS Kai */
#define BUP_EXTENSION ".BUP"

/** The BUP header structure (sizeof(vmem_bup_header_t)) is exactly 64 bytes */
#define BUP_HEADER_SIZE 64

/** BUP header magic */
#define VMEM_MAGIC_STRING_LEN 4
#define VMEM_MAGIC_STRING "Vmem"

/** Max backup filename length */
#define JO_BACKUP_MAX_FILENAME_LENGTH      (12)

/** Max backup file comment length */
#define JO_BACKUP_MAX_COMMENT_LENGTH       (10)

/*
 * Language of backup save in jo_backup_file
 * taken from Jo Engine backup.c
 */
enum jo_backup_language
{
    backup_japanese = 0,
    backup_english = 1,
    backup_french = 2,
    backup_deutsch = 3,
    backup_espanol = 4,
    backup_italiano = 5,
};

/**
 * Backup file metadata information. Taken from Jo Engine.
 **/
typedef struct _jo_backup_file
{
    unsigned char filename[JO_BACKUP_MAX_FILENAME_LENGTH];
    unsigned char comment[JO_BACKUP_MAX_COMMENT_LENGTH + 1];
    unsigned char language; // jo_backup_language
    unsigned int date;
    unsigned int datasize;
    unsigned short blocksize;
    unsigned short padding;
} jo_backup_file;

typedef struct _jo_backup_date {
    unsigned char year;       // year - 1980
    unsigned char month;
    unsigned char day;
    unsigned char time;
    unsigned char min;
    unsigned char week;
} jo_backup_date;

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

/**
 * Vmem usage statistics structure.
 * Statistics are reset on each vmem session, ie when Saturn is reset,
 * or when game calls BUP_Init function.
 **/
typedef struct _vmem_bup_stats_t
{
    /* Number of times BUP_Dir function is called. */
    unsigned char dir_cnt;

    /* Number of times BUP_Read function is called. */
    unsigned char read_cnt;

    /* Number of times BUP_Write function is called. */
    unsigned char write_cnt;

    /* Number of times BUP_Verify function is called. */
    unsigned char verify_cnt;
} vmem_bup_stats_t;


/**
 * Backup data header. Multibyte values are stored as big-endian.
 **/
typedef struct _vmem_bup_header_t
{
    /* Magic string.
     * Used in order to verify that file is in vmem format.
     */
    char magic[VMEM_MAGIC_STRING_LEN];

    /* Save ID.
     * "Unique" ID for each save data file, the higher, the most recent.
     */
    unsigned int save_id;

    /* Vmem usage statistics. */
    vmem_bup_stats_t stats;

    /* Unused, kept for future use. */
    char unused1[8 - sizeof(vmem_bup_stats_t)];

    /* Backup Data Informations Record (34 bytes + 2 padding bytes). */
    jo_backup_file dir;

    /* Date stamp, in Saturn's BUP library format.
     * Used in order to verify which save data is the most
     * recent one when rebuilding index data.
     * Note #1 : this information is already present in BupDir structure,
     * but games are setting it, so it may be incorrect (typically, set
     * to zero).
     * Note #2 : this date information is the date when Pseudo Saturn Kai
     * last started, not the time the save was saved, so if information in
     * dir structure is available, it is more accurate.
     */
    unsigned int date;

    /* Unused, kept for future use. */
    char unused2[8];
} vmem_bup_header_t;

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif
