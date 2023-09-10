

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#define  BUFFER_NUM 703189
char buffer[BUFFER_NUM];

uint16_t fletcher16(uint8_t *data, int n){
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;

    int i;
    for(i = 0;i < n;i++){
        sum1 = (sum1 + data[i]) % 0xff;
        sum2 = (sum1 + sum2) % 0xff;
    }
    return (sum2 << 8) | sum1;
}

int main(void){

    FILE* fp = NULL;
    fp = fopen("/home/stu/oslab/testP5.txt","r");
    fread((void *)buffer, sizeof(char), BUFFER_NUM-4, fp);
    uint16_t ret_flet = fletcher16(buffer,BUFFER_NUM-4);
    printf("fletcher num : %d\n",ret_flet);
}