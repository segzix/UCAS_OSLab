#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void main(int argc, char* argv[])
{
    uintptr_t mem;
    for(unsigned i = 0;i < 0x30000;i+=0x1000){
        mem = sys_mmalloc(0x1000);
        *(unsigned *)(mem) = i;
        if(*(unsigned *)(mem) == i)
            continue;
        else {
            printf("swap is error!\n");
            return;
        }
    }

    printf("swap is success!!!");
    return;
}