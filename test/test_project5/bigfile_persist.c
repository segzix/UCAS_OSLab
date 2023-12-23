#include <stdio.h>
#include <string.h>
#include <unistd.h>
uint32_t * write_buffer = 0x10000000;
#define POINT_DIRECT 0x2000//(4K*2)
#define POINT_INDIRECT1 0x400000//(4M)
#define POINT_INDIRECT2 0x800000//(8M)
#define O_RD (1lu << 0)
#define O_WR (1lu << 1)
typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} whence_status_t;
int main(void)
{
    int fd = sys_fopen("bigfile.txt", O_RD | O_WR);
    
    sys_move_cursor(0, 0);
    printf("test begin !");
    uint32_t b;

//直接指针测试
    printf("\npersist direct: ");
    sys_lseek(fd,POINT_DIRECT,SEEK_SET);
    for (int i = 0; i < 10; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d ",b);
    }
    sys_sleep(1);

//一级间接指针测试
    printf("\npersist indirect1: ");
    sys_lseek(fd,POINT_INDIRECT1,SEEK_SET);
    for (int i = 10; i < 20; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d ",b);        
    }
    sys_sleep(1);

//二级间接指针测试
    printf("\npersist indirect2: ");
    sys_lseek(fd,POINT_INDIRECT2,SEEK_SET);
    for (int i = 20; i < 30; i++)
    {
        sys_fread(fd, &b, sizeof(uint32_t));
        printf("%d ",b);        
    }
    sys_sleep(1);

    printf("\ntest end !\n");

    sys_fclose(fd);

    return 0;
}