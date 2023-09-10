#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__
#include <type.h>

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *buffer, int *nbytes);
void net_handle_irq(void);

void init_stream();
void do_resend_ACK(int id);
void do_resend();
void do_perfm();

void e1000_handle_txpe(void);
void e1000_handle_rxdmt0(void);

#define SBAWIN_NUM 32
#define TEMP_BUFFER_NUM 1200
#define RESEND_BUFFER_NUM 62
#define RESEND_HEAD_NUM 54

#define MAGIC_NUM 0x45
#define _DAT (1lu << 0)
#define _RSD (1lu << 1)
#define _ACK (1lu << 2)

#define RESEND_TIME_INTERVAL 2000000
#define PERF_GAP 3

#define MAGIC_NUM_SEGMENT_OFFSET 54
#define FLAGS_SEGMENT_OFFSET 55
#define LENGTH_SEGMENT_OFFSET 56
#define SEQ_SEGMENT_OFFSET 58
#define SIZE_SEGMENT_OFFSET 62
#define DATA_SEGMENT_OFFSET 66

#define START_OFFSET MAGIC_NUM_SEGMENT_OFFSET

typedef struct {
    uint64_t seq;
    uint32_t length;

    int prev;
    int next;
    int valid;
} stream_buffer;

typedef struct{
    //接收端缓存
    stream_buffer sbawin[SBAWIN_NUM];
    char temp_buffer[TEMP_BUFFER_NUM];

    //发送端缓存
    char resend_buffer[RESEND_BUFFER_NUM];

    //相关参数
    uint8_t valid;
    uint8_t ACK_valid;
    uint32_t genlen;
    uint32_t curlen;
    uint64_t resend_time;

    //计算性能相关参数
    uint32_t prevByte;
    uint32_t CurByte;
    uint64_t perf_time;
} netStream;

#endif  // !__INCLUDE_NET_H__