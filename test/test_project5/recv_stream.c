

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#define  BUFFER_NUM 19534
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
    int nbytes = BUFFER_NUM;
    //int nbytes = phdr->p_memsz;

    // sys_net_recv_stream(buffer, &nbytes);
    sys_net_recv_stream(buffer, &nbytes);
    uint32_t size = *(uint32_t*)buffer;

    // memcpy((void*)buffer, (void*)buffer+4, nbytes-4);
    // uint16_t ret_flet = fletcher16(buffer,nbytes-4);
    memcpy((void*)buffer, (void*)buffer+4, size-4);
    uint16_t ret_flet = fletcher16(buffer,size-4);
    printf("fletcher num : %d",ret_flet);
}