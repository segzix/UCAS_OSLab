#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define O_RD (1lu << 0)
#define O_WR (1lu << 1)

static char buff[64];

int main(void)
{
    int fd = sys_fopen("1.txt", O_RD | O_WR);
    int fd2 = sys_fopen("2.txt", O_RD | O_WR);

    // write 'hello world!' * 10
    for (int i = 0; i < 10; i++)
    {
        sys_fwrite(fd, "hello world!\n", 13);
    }

    for (int i = 0; i < 5; i++)
    {
        sys_fwrite(fd2, "Byebye world!\n", 14);
    }

    // read
    sys_move_cursor(0, 0);
    for (int i = 0; i < 10; i++)
    {
        sys_fread(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_fclose(fd);
    sys_fclose(fd2);

    return 0;
}