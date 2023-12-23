#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
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
int print_location_y;
char buff[32];
int main(int argc, char *argv[])
{
    int fd = sys_fopen("bigfile.txt", O_RD | O_WR);
    
    if(!strcmp(argv[1], "0"))
        print_location_y = 0;
    else 
        print_location_y = 6;

    sys_move_cursor(0, print_location_y);
    clock_t test_start = clock();
    printf("test begin !");
    uint32_t b;

//直接指针测试
    printf("\npersist direct: ");
    sys_lseek(fd,POINT_DIRECT,SEEK_SET);
    sys_fread(fd, buff, 12);
    printf("%s ",buff);

//一级间接指针测试
    printf("\npersist indirect1: ");
    sys_lseek(fd,POINT_INDIRECT1,SEEK_SET);
    sys_fread(fd, buff, 12);
    printf("%s ",buff);

//二级间接指针测试
    printf("\npersist indirect2: ");
    sys_lseek(fd,POINT_INDIRECT2,SEEK_SET);
    sys_fread(fd, buff, 12);
    printf("%s ",buff);

    printf("\ntest end !\n");
    clock_t test_end = clock();
    printf("bcache: %ld ticks\n\r", test_end - test_start);
    sys_fclose(fd);

    return 0;
}