#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
uint32_t * write_buffer = 0x10000000;
#define WRITE_BLOCK_NUM 32
#define FILE_256KB  18
#define POINT_8KB 0x2000
#define POINT_1MB 0x100000
#define POINT_2MB 0x200000
#define POINT_3MB 0x300000
#define POINT_4MB 0x400000
#define POINT_8MB 0x800000
#define POINT_10MB 0xa00000
#define POINT_12MB 0xc00000
#define POINT_16MB 0x1000000
#define POINT_32MB 0x2000000
#define POINT_64MB 0x4000000
#define O_RD (1lu << 0)
#define O_WR (1lu << 1)
typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} whence_status_t;
int print_location_y = 0;
char buff[32];
int main(int argc, char *argv[])
{
    int fd = sys_fopen("bigfile.txt", O_RD | O_WR);
    
    // if(!strcmp(argv[1], "0"))
    //     print_location_y = 0;
    // else 
    //     print_location_y = 6;

    sys_move_cursor(0, print_location_y);
    printf("test begin !");
    uint32_t b;
    
    clock_t test_start = clock();
    for(int i = 0;i < WRITE_BLOCK_NUM;i++){

        // printf("\npersist read %dMB: ",i<<2);
        sys_lseek(fd,(i << FILE_256KB),SEEK_SET);
        sys_fread(fd, &b, sizeof(int));
        if(b != i)
            printf("read error!");
    }
    clock_t test_end = clock();

    printf("\ntest end !\n");
    printf("bcache: %ld ticks\n\r", test_end - test_start);
    sys_fclose(fd);

    return 0;
}