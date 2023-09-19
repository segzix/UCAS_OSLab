#define COUNT 10
#include <time.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    int i;

    pid_t pid_1;
    pid_t pid_2;
    pid_t pid_3;

    double diff_1 = 0;
    double diff_2 = 0;
    double diff_3 = 0;

    struct timespec time1;
    struct timespec time2;
    struct timespec time3;
    struct timespec time4;
    struct timespec time5;
    struct timespec time6;

    for(i = 0;i < COUNT;i++)
    {

        clock_gettime(CLOCK_REALTIME, &time1);
        pid_1 = getpid(); 
        clock_gettime(CLOCK_REALTIME, &time2);

        clock_gettime(CLOCK_REALTIME, &time3);
        pid_2 = syscall(SYS_getpid);
        clock_gettime(CLOCK_REALTIME, &time4);

        clock_gettime(CLOCK_REALTIME, &time5);
        asm(
            "mov $20, %%eax\n\t"
            "syscall\n\t"
            : "=a"(pid_3)
        );
        clock_gettime(CLOCK_REALTIME, &time6);

        diff_1 += time2.tv_nsec - time1.tv_nsec;
        diff_2 += time4.tv_nsec - time3.tv_nsec;
        diff_3 += time6.tv_nsec - time5.tv_nsec;
    }

    printf("pid1 = %d\n", pid_1);
    printf("pid2 = %d\n", pid_2);
    printf("pid3 = %d\n", pid_3);

    printf("time_getpid = %f ns\n", diff_1/COUNT);
    printf("time_syscall = %f ns\n", diff_2/COUNT);
    printf("time_syscall_getpid = %f ns\n", diff_3/COUNT);

    return 0;
}

