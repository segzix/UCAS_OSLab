#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
int main()
{
    int result = 0;
    int element[10] = {1,2,3,4,5,6,7,8,9,10};
    pid_t pid;
    
    pid=fork();
    
    if(pid==0)
    {
        int i;
        for(i = 0;i < 10;i++)
        {
            result += element[i];
        }
        printf("result : %d\n",result);
        execl("/home/stu/chengzixuan21a/homework_3", "ls", "-l",NULL);
    }
    else if(pid>0)
    {
        int* status;
        int ret=wait(status);
        printf("parent process finishes\n");
    }

    return 0;

}