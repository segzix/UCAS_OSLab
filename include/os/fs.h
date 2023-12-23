#include "pgtable.h"
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

//这列全部采用块管理而非扇区管理，即一个块是4K,一个扇区是512，一个块为8个扇区；这里所有的偏移量均采用块来进行技术
#define SUPER_START (1lu << 17)//文件系统分配出1M个block，从第512M开始，
#define SUPER_MAGIC 0x398
#define BCACHE_NUM 512 //bcache的数量 

#define BLOCK_SIZ (1lu << 12)
#define BLOCK_BIT_SIZ (BLOCK_SIZ << 3)//每个块的byte数和bit数
#define SECTOR_SIZ (1lu << 9)
#define SECTOR_BIT_SIZ (BLOCK_SIZ << 3)//每个块的byte数和bit数

#define FS_MAX_BLOCKSIZ (1lu << 29)/BLOCK_SIZ//整个文件系统的最大块数为512K块

#define DIRECT_NUM 3//直接
#define INDIRECT1_NUM 1//一级间接
#define INDIRECT2_NUM 1//二级间接
#define POINT_PER_BLOCK (BLOCK_SIZ>>2)//一个数据块1k个blockid项
#define DIRECT_BLOCK_SIZ DIRECT_NUM//直接对应块的最大块标号
#define INDIRECT1_BLOCK_SIZ (DIRECT_BLOCK_SIZ+POINT_PER_BLOCK*INDIRECT1_NUM)//直接对应和一级块的最大块标号
#define INDIRECT2_BLOCK_SIZ (INDIRECT1_BLOCK_SIZ+POINT_PER_BLOCK*POINT_PER_BLOCK*INDIRECT1_NUM)//直接对应和一级二级块的最大块标号
#define NUM_FDESCS 32

#define O_RD (1lu << 0)
#define O_WR (1lu << 1)

char super_block[BLOCK_SIZ];//super可能会在内存中长期驻留
char check_block[BLOCK_SIZ];

typedef enum {
    INODE_FILE,
    INODE_DIR,
} inode_status_t;

typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} whence_status_t;

typedef struct bcache{
    uint8_t bcache_valid;//是否有效
    uint32_t block_id;//对应数据块的id
    uint8_t bcache_block[BLOCK_SIZ];
} bcache_t;
uint8_t bcache_point;
bcache_t bcaches[BCACHE_NUM];

typedef struct superblock{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;
    uint32_t siz;//整个文件系统的大小
    uint32_t start;

    uint32_t blockbitmap_offset;
    uint32_t blockbitmap_siz;

    uint32_t inodebytemap_offset;
    uint32_t inodebytemap_siz;

    uint32_t inodetable_offset;
    uint32_t inodetable_siz;

    uint32_t datablock_offset;
    uint32_t datablock_siz;

} superblock_t;

typedef struct inode{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint8_t mode;//形式，目录或文件
    uint8_t owner_pid;//拥有的进程
    uint8_t hardlinks;//硬链接数
    int8_t  fd_index;//如果为文件，则指向文件描述符下标

    uint32_t filesz;//文件大小
    uint32_t mtime;//创建时间

    uint32_t direct[3];
    uint32_t indirect_1;
    uint32_t indirect_2;
} inode_t;
//一共32bytes大小

typedef struct dentry{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t ino;
    uint8_t dentry_valid;
    char name[27];
} dentry_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint64_t memsiz;//对于文件而言，有一个真正的memsiz，用来记录如果有空洞，则除去空洞的总字节数
    uint32_t used;
    uint32_t ino;
    uint32_t mode;//文件描述符里面做权限标志
    uint32_t r_cursor;
    uint32_t w_cursor;
} fdesc_t;
fdesc_t fdescs[NUM_FDESCS];
//同时在读取之前会先从bcache中取，取不到再从硬盘中读到bcache中并且读出

uint32_t blockid2sectorid(uint32_t block_id);
uint32_t sectorid2blockid(uint32_t sector_id);
void init_bcache(void);
uint8_t* bread(uint32_t block_id);
void bwrite(uint32_t block_id, uint8_t* bcache_write);
uint8_t* balloc(uint32_t block_id);

int write_file(inode_t*inode, char* string, uint32_t start, uint32_t length);
uint8_t* init_block(uint32_t block_id);
int inopath2ino(uint32_t base_ino, char * name);
inode_t* ino2inode_t(uint32_t ino);
int check_dir(inode_t* inode);
int strcheck(char *src, char c);
uint32_t countbyte(uint8_t* count_block);
uint32_t countbit(uint8_t* count_block);
int check_fs();
unsigned alloc_inode(uint8_t mode);
uint8_t* search_datapoint(inode_t* inode, uint32_t offset);
int read_file(inode_t* inode, char* string, uint32_t start, uint32_t length);
dentry_t* name_search_dentry(inode_t* inode_current, char* name);
int name_search_offset(inode_t* inode_current, char* name);
int rmfile(uint32_t ino);

int do_mkfs(void);
int do_statfs(void);
int do_cd(char *path);
int do_mkdir(char *dir_name);
int do_ls(int argc, char *argv[]);
int do_rmdirfile(char *dir_name);
void do_getpwdname(char* pwd_name);

int do_fopen(char *path, int mode);
int do_fread(int fd, char *buff, int length);
int do_fwrite(int fd, char *buff, int length);
int do_fclose(int fd);
int do_touch(char *filename);
int do_cat(char *filename);
int do_lseek(int fd, int offset, int whence);