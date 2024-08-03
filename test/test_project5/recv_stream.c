

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#define  BUFFER_NUM 720000
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

    uint64_t time1 = clock()/CLOCKS_PER_SEC;
    sys_net_recv_stream(buffer, &nbytes);
    uint64_t time2 = clock()/CLOCKS_PER_SEC;
    uint32_t size = *(uint32_t*)buffer;
    printf("%u[Bytes]: %lu[s]\n", size, time2-time1);

    uint16_t ret_flet = fletcher16((void*)buffer+4,size-4);
    printf("fletcher num : %d\n",ret_flet);
}