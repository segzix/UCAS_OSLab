#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void main(int argc, char* argv[])
{
    void* mem = 0x10000000;
    for(unsigned i = 0;i < 0x80000;i+=0x1000){
        *(unsigned *)(mem+i) = i;
    }

    mem = 0x10000000;
    for(unsigned i = 0;i < 0x80000;i+=0x1000){
        if(*(unsigned *)(mem+i) == i)
            continue;
        else {
            printf("swap is error!\n");
            return;
        }
    }

    printf("swap is success!!!");
    return;
}