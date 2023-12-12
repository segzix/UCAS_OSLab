#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *buffer, int *nbytes);
void do_wake_up_recv_stream(void);
void do_resend_RSD();
void do_resend_ACK();
void init_stream();

#endif  // !__INCLUDE_NET_H__