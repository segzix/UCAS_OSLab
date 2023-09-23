#include<unistd.h>
#include<stdio.h>
#include<string.h>
#define LOOP 2

int main(int argc,char *argv[])
{
   pid_t pid;
   int loop;
   int status = 0;

   for(loop=0;loop<LOOP;loop++) {

      if((pid=fork()) < 0)
        fprintf(stderr, "fork failed\n");
      else if(pid == 0) {
        status++;
        printf(" I am child process\n");
        break;
      }
      else {
        sleep(1);
		printf(" I am parent process\n");
      }
    }
    return 0;
}