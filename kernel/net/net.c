#include "os/net.h"
#include "screen.h"
#include <assert.h>
#include <e1000.h>
#include <os/list.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/time.h>
#include <printk.h>
#include <type.h>

/****** 系统调用函数 ******/

int do_net_send(void *txpacket, int length) {
    e1000_transmit(txpacket, length);
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full

    return 0; // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens) {
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    char *pos = rxbuffer;
    // int tmp=0;
    *pos = 0;
    *pkt_lens = 0;

    for (int i = 0; i < pkt_num; i++) {
        // pos = rxbuffer + tmp;
        int lennow = e1000_poll(pos);
        pos += lennow;
        pkt_lens[i] = lennow;
    }

    return 0; // Bytes it has received
}

int do_net_recv_stream(void *buffer, int *nbytes) {
    int current_bytes = *nbytes;
    char *temp_buffer = get_pcb()->netstream.temp_buffer;
    char *resend_head = get_pcb()->netstream.resend_buffer;
    stream_buffer *sbawin = get_pcb()->netstream.sbawin;

    init_stream();

    while (get_pcb()->netstream.curlen < get_pcb()->netstream.genlen) {

        //先暂时拷贝到这个字符数组中，并设置ACK_Valid保证每个序号只会确认一次
        e1000_poll(temp_buffer);
        get_pcb()->netstream.ACK_valid = 1;

        //数据包头开始的地方与数据段开始的地方
        char *start_buffer = temp_buffer + START_OFFSET;
        char *data_buffer = temp_buffer + SIZE_SEGMENT_OFFSET;

        //获得当前数据段的序号，长度和魔数标记
        uint8_t now_magic = *(uint8_t *)(temp_buffer + MAGIC_NUM_SEGMENT_OFFSET);
        uint16_t len1 = (uint16_t)(*(uint8_t *)(temp_buffer + LENGTH_SEGMENT_OFFSET) << 8);
        uint16_t len2 = (uint16_t)(*(uint8_t *)(temp_buffer + LENGTH_SEGMENT_OFFSET + 1) << 0);
        uint16_t now_len = len1 + len2;
        uint64_t seq1 = (uint64_t)(*(uint8_t *)(temp_buffer + SEQ_SEGMENT_OFFSET) << 24);
        uint64_t seq2 = (uint64_t)(*(uint8_t *)(temp_buffer + SEQ_SEGMENT_OFFSET + 1) << 16);
        uint64_t seq3 = (uint64_t)(*(uint8_t *)(temp_buffer + SEQ_SEGMENT_OFFSET + 2) << 8);
        uint64_t seq4 = (uint64_t)(*(uint8_t *)(temp_buffer + SEQ_SEGMENT_OFFSET + 3) << 0);
        uint64_t now_seq = seq1 + seq2 + seq3 + seq4;

        //通过上述方式，求得当前得到的字符buffer对应的数据包的起始地址和尾地址
        uint64_t now_head = now_seq;
        uint64_t now_tail = now_seq + now_len;

        int prev = 0;
        int next = 0;
        int node_index;
        int neidx = 0;

        if (now_magic == MAGIC_NUM) { //判断是对应的包才进入下面的判断
            if (now_seq == 0) {
                get_pcb()->netstream.genlen = *(uint32_t *)data_buffer;
                assert(get_pcb()->netstream.genlen <= current_bytes);

                //打印总字节数
                screen_move_cursor(0, 1);
                printk("GenBytes: %u[Bytes]", *(uint32_t *)data_buffer);
            }

            memcpy((void *)resend_head, (void *)temp_buffer, RESEND_HEAD_NUM); //拷贝包头

            /*找prev指针(head <= nowhead)*/
            node_index = 0;
            while (true) {
                uint64_t head = sbawin[node_index].seq;
                if (head > now_head) {
                    prev = sbawin[node_index].prev;
                    break;
                }
                if (sbawin[node_index].next == -1) {
                    prev = node_index;
                    break;
                }
                node_index = sbawin[node_index].next;
            }

            /*找next指针(tail >= now_tail)*/
            node_index = 0;
            while (true) {
                if (node_index == -1) {
                    next = node_index;
                    break;
                }
                uint64_t tail = sbawin[node_index].seq + sbawin[node_index].length;
                if (tail >= now_tail) {
                    next = node_index;
                    break;
                }
                node_index = sbawin[node_index].next;
            }

            /*找空闲的sbawin*/
            for (neidx = 0; sbawin[neidx].valid; neidx++)
                ;

            /*处理刚填入的sbawin，并根据情况进行静态链表的前后合并*/
            if (prev != next) {
                //将空闲节点填入
                memcpy((void *)buffer + now_seq, (void *)data_buffer, now_len);
                sbawin[neidx].prev = prev;
                sbawin[neidx].next = next;
                sbawin[neidx].seq = now_seq;
                sbawin[neidx].length = now_len;
                sbawin[neidx].valid = 1;
                get_pcb()->netstream.curlen += now_len;

                //空闲节点是否与后续节点合并
                if (next != -1) {
                    sbawin[next].prev = neidx;
                    if (sbawin[next].seq == now_tail) {
                        sbawin[neidx].length += sbawin[next].length;
                        sbawin[neidx].next = sbawin[next].next;
                        sbawin[next].valid = 0;
                        sbawin[sbawin[neidx].next].prev = neidx;
                    }
                }

                //前驱节点是否与空闲节点合并
                if (prev != -1) {
                    sbawin[prev].next = neidx;
                    if (sbawin[prev].seq + sbawin[prev].length == now_head) {
                        sbawin[prev].length += sbawin[neidx].length;
                        sbawin[prev].next = sbawin[neidx].next;
                        sbawin[neidx].valid = 0;
                        sbawin[sbawin[neidx].next].prev = prev;
                    }
                }
            }
        }

        // debug
        // printl("now_seq : %u now_length : %u\n", now_seq, now_len);
        // for (int i = 0, j = 0; i != -1; i = sbawin[i].next, j++) {
        //     printl("%d : sbawin[%d] seq : %u length : %u\n", j, i, sbawin[i].seq, sbawin[i].length);
        // } //循环获得之前这些就有节点加起来的长度，后面用新的长度减去旧的长度
        // printl("\n");
        // int valid = 0;
        // for (int i = 0; i < SBAWIN_NUM; i++)
        //     if (sbawin[i].valid)
        //         valid++;
        // printl("valid num: %d\n", valid);
    }

    /*结束完之后最后一次确认*/
    do_perfm((int)(((void *)get_pcb() - (void *)pcb) / sizeof(pcb_t)));
    do_resend_ACK(((void *)get_pcb() - (void *)pcb) / sizeof(pcb_t));

    return 0;
}

void net_handle_irq(void) {
    uint32_t ICR_ID = e1000_read_reg(e1000, E1000_ICR);
    if (ICR_ID & E1000_ICR_TXQE)
        e1000_handle_txpe();
    if (ICR_ID & E1000_ICR_RXDMT0)
        e1000_handle_rxdmt0();
    // TODO: [p5-task3] Handle interrupts from network device
}