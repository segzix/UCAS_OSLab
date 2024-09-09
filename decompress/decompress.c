#define KERNEL_INFO_LOC 0x502001ec
#define SECTOR_SIZE 512

#define BIOS_SDREAD 11
#define BIOS_FUNC_ENTRY 0x50150000
#define IGNORE 0
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))
#include <mminit.h>
#include <tinylibdeflate.h>

static long call_bios(long which, long arg0, long arg1, long arg2, long arg3, long arg4) {
    long (*bios_func)(long, long, long, long, long, long, long, long) =
        (long (*)(long, long, long, long, long, long, long, long))BIOS_FUNC_ENTRY;

    return bios_func(arg0, arg1, arg2, arg3, arg4, IGNORE, IGNORE, which);
}

int sd_read(unsigned mem_address, unsigned num_of_blocks, unsigned block_id) {
    return (int)call_bios((long)BIOS_SDREAD, (long)mem_address, (long)num_of_blocks, (long)block_id,
                          IGNORE, IGNORE);
}

void memcpy(char *dest, const char *src, unsigned len) {
    for (; len != 0; len--) {
        *dest++ = *src++;
    }
}

int main() {
    //用来记录KERNEL所占扇区字节数以及在扇区中的物理地址
    unsigned KERNEL_compressed_block_size;
    unsigned KERNEL_compressed_block_phyaddr;
    //通过上面两个变量，得到的结果
    unsigned KERNEL_compressed_block_id;
    unsigned KERNEL_compressed_block_num;
    unsigned long KERNEL_compressed_block_offset;

    KERNEL_compressed_block_size = *(unsigned *)(KERNEL_INFO_LOC + 0x4);
    KERNEL_compressed_block_phyaddr = *(unsigned *)(KERNEL_INFO_LOC);

    KERNEL_compressed_block_id = KERNEL_compressed_block_phyaddr / SECTOR_SIZE;
    KERNEL_compressed_block_num =
        NBYTES2SEC(KERNEL_compressed_block_phyaddr + KERNEL_compressed_block_size) -
        KERNEL_compressed_block_id;
    KERNEL_compressed_block_offset = KERNEL_compressed_block_phyaddr % SECTOR_SIZE;

    // prepare environment
    deflate_set_memory_allocator((void *)0, (void *)0);
    struct libdeflate_decompressor *decompressor = deflate_alloc_decompressor();

    // read decompressed kernel && do decompress
    int restore_nbytes = 0;
    sd_read(KERNEL_DEFLATE, KERNEL_compressed_block_num, KERNEL_compressed_block_id);
    deflate_deflate_decompress(decompressor, (char *)(KERNEL_DEFLATE + KERNEL_compressed_block_offset), KERNEL_compressed_block_size,
                               (char *)KERNEL, 0x01000000, &restore_nbytes);
    return 0;
}