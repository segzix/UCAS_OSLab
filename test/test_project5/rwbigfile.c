#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
uint32_t * write_buffer = 0x10000000;
#define WRITE_BLOCK_NUM 64
#define FILE_4MB  22
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
char buff[32];
int main(void)
{
    sys_touch("bigfile.txt");
    int fd = sys_fopen("bigfile.txt", O_RD | O_WR);
    
    sys_move_cursor(0, 0);
    printf("test begin !");
    uint32_t b;

    for(int i = 0;i < WRITE_BLOCK_NUM;i++){
        if(!(i % 10))//每次打印10次更新一下光标
            sys_move_cursor(0, 1);

        sys_lseek(fd,(i << FILE_4MB),SEEK_SET);
        sys_fwrite(fd, &i, sizeof(int));
        printf("\nstart read %dMB: ",i<<2);
        sys_fread(fd, &b, sizeof(int));
        printf("%d ",b<<2);
    }

    sys_move_cursor(0, 12);
    printf("\ntest end !\n");

    sys_fclose(fd);

    return 0;
}