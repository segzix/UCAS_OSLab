#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

// static LIST_HEAD(send_block_queue);
// static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    e1000_transmit(txpacket,length);
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full

    return 0;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    char * pos = rxbuffer;
    // int tmp=0;
    *pos = 0;
    *pkt_lens = 0;

    for(int i = 0; i<pkt_num; i++){
        // pos = rxbuffer + tmp;
        int lennow = e1000_poll(pos);
        pos += lennow;
        pkt_lens[i] = lennow;
    }

    return 0;  // Bytes it has received
}

void e1000_handle_txpe(void){
    while(send_block_queue.next != &send_block_queue)
        do_unblock(send_block_queue.next);
    // e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
}

void e1000_handle_rxdmt0(void){
    while(recv_block_queue.next != &recv_block_queue)
        do_unblock(recv_block_queue.next);
    // e1000_write_reg(e1000, E1000_IMC, E1000_IMC_RXDMT0);
}

void net_handle_irq(void)
{
    uint32_t ICR_ID = e1000_read_reg(e1000,E1000_ICR);
    if(ICR_ID & E1000_ICR_TXQE)
        e1000_handle_txpe();
    else if(ICR_ID & E1000_ICR_RXDMT0)
        e1000_handle_rxdmt0();
    else
        ;
    // TODO: [p5-task3] Handle interrupts from network device
}