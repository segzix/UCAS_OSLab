#define KERNEL_compressed 0x54000000
#define KERNEL            0x50202000
#define KERNEL_compressed_phyaddr         0x502001ec
#define KERNEL_compressed_size            0x502001f0
#define SECTOR_SIZE       512

#define BIOS_SDREAD 11
#define BIOS_FUNC_ENTRY 0x50150000
#define IGNORE 0
// #include <common.h>
// #include <os/string.h>
#include <tinylibdeflate.h>

static long call_bios(long which, long arg0, long arg1, long arg2, long arg3, long arg4)
{
    long (*bios_func)(long,long,long,long,long,long,long,long) = \
        (long (*)(long,long,long,long,long,long,long,long))BIOS_FUNC_ENTRY;

    return bios_func(arg0, arg1, arg2, arg3, arg4, IGNORE, IGNORE, which);
}

int sd_read(unsigned mem_address, unsigned num_of_blocks, unsigned block_id)
{
    return (int)call_bios((long)BIOS_SDREAD, (long)mem_address, \
                            (long)num_of_blocks, (long)block_id, IGNORE, IGNORE);
}

void memcpy(char *dest, const char *src, unsigned len)
{
    for (; len != 0; len--) {
        *dest++ = *src++;
    }
}

int main()
{
    unsigned KERNEL_compressed_block_size;
    unsigned KERNEL_compressed_block_phyaddr; 
    //用来记录KERNEL所占扇区字节数以及在扇区中的物理地址
    unsigned KERNEL_compressed_block_id;
    unsigned KERNEL_compressed_block_num;
    unsigned KERNEL_compressed_block_offset;
    //通过上面两个变量，得到的结果
    KERNEL_compressed_block_size    = (unsigned)*(unsigned *)(KERNEL_compressed_size);
    KERNEL_compressed_block_phyaddr = (unsigned)*(unsigned *)(KERNEL_compressed_phyaddr);

    KERNEL_compressed_block_id      = KERNEL_compressed_block_phyaddr / SECTOR_SIZE;
    KERNEL_compressed_block_num     = (KERNEL_compressed_block_phyaddr + KERNEL_compressed_block_size) / SECTOR_SIZE - KERNEL_compressed_block_id + ((KERNEL_compressed_block_phyaddr + KERNEL_compressed_block_size) % SECTOR_SIZE != 0);
    KERNEL_compressed_block_offset  = KERNEL_compressed_block_phyaddr % SECTOR_SIZE;


    sd_read(KERNEL_compressed,KERNEL_compressed_block_num,KERNEL_compressed_block_id);
    memcpy(KERNEL_compressed, KERNEL_compressed + KERNEL_compressed_block_offset, KERNEL_compressed_block_size);
    //将压缩的kernel读到KERNEL_compressed处并且让其对齐

    // prepare environment
    deflate_set_memory_allocator((void*)0, (void*)0);
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();


    // do decompress
    int restore_nbytes = 0;
    deflate_deflate_decompress(decompressor, (char *)KERNEL_compressed, KERNEL_compressed_block_size, (char *)KERNEL, 0x01000000, &restore_nbytes);
    //bios_putchar(restore_nbytes);
    //解压缩并最终输出解压缩后的字节数
    return 0;
}