#include "e1000.h"
#include "printk.h"
#include "screen.h"
#include <os/net.h>
#include <os/smp.h>
#include <os/time.h>

/****** 初始化进程的netstream结构体 ******/

void init_stream(void) {
    stream_buffer *sbawin = get_pcb()->netstream.sbawin;

    sbawin[0].valid = 1;
    sbawin[0].seq = 0;
    sbawin[0].length = 0;
    sbawin[0].prev = -1;
    sbawin[0].next = -1;

    for (int i = 1; i < SBAWIN_NUM; i++) {
        sbawin[i].valid = 0;
        sbawin[i].seq = 0;
        sbawin[i].length = 0;
        sbawin[i].prev = -1;
        sbawin[i].next = -1;
    }

    get_pcb()->netstream.genlen = 0xffffffff;
    get_pcb()->netstream.curlen = 0;
    get_pcb()->netstream.valid = 1;
    get_pcb()->netstream.resend_time = get_ticks();
    get_pcb()->netstream.prevByte = 0;
    get_pcb()->netstream.CurByte = 0;
    get_pcb()->netstream.perf_time = get_timer();
}

/****** 定时重传，唤醒。打印性能信息相关操作 ******/

/*
 * RSD重传
 */
void do_resend_RSD(int id) {
    for (int i = 0; i < SBAWIN_NUM; i++) {
        //如果有需求就申请重传
        if (pcb[id].netstream.sbawin[i].valid) {
            //获得需要的buffer参数
            uint32_t resend_RSD_seq = pcb[id].netstream.sbawin[i].seq + pcb[id].netstream.sbawin[i].length;
            char *resend_buffer = pcb[id].netstream.resend_buffer;

            //填充头部
            resend_buffer[MAGIC_NUM_SEGMENT_OFFSET] = MAGIC_NUM;
            resend_buffer[FLAGS_SEGMENT_OFFSET] = _RSD;

            //填充序号
            *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET) =
                ((resend_RSD_seq & 0xff000000) >> 24);
            *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 1) =
                ((resend_RSD_seq & 0x00ff0000) >> 16);
            *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 2) =
                ((resend_RSD_seq & 0x0000ff00) >> 8);
            *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 3) =
                ((resend_RSD_seq & 0x000000ff) >> 0);

            e1000_transmit(resend_buffer, RESEND_BUFFER_NUM);
            printl("RSD : %u\n", resend_RSD_seq);
        }
    }
}

/*
 * ACK确认
 */
void do_resend_ACK(int id) {
    //获得需要的buffer参数
    uint32_t resend_ACK_seq = pcb[id].netstream.CurByte;
    char *resend_buffer = pcb[id].netstream.resend_buffer;

    //填充头部
    resend_buffer[MAGIC_NUM_SEGMENT_OFFSET] = MAGIC_NUM;
    resend_buffer[FLAGS_SEGMENT_OFFSET] = _ACK;

    //填充序号
    *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET) = ((resend_ACK_seq & 0xff000000) >> 24);
    *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 1) = ((resend_ACK_seq & 0x00ff0000) >> 16);
    *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 2) = ((resend_ACK_seq & 0x0000ff00) >> 8);
    *(uint8_t *)(resend_buffer + SEQ_SEGMENT_OFFSET + 3) = ((resend_ACK_seq & 0x000000ff) >> 0);

    e1000_transmit(resend_buffer, RESEND_BUFFER_NUM);
    printl("ACK : %u\n", resend_ACK_seq);
    pcb[id].netstream.ACK_valid = 0;
}

/*
 * 接收队列定时唤醒
 */
void do_wake_up_recv_stream(void) { //定时唤醒
    e1000_handle_rxdmt0();          //调用该函数，将阻塞队列里的进程取出来
    printl("wakeup stream\n");
}

/*
 * 性能打印
 */
void do_perfm(int id) {
    pcb[id].netstream.CurByte = pcb[id].netstream.sbawin[0].length;
    screen_clear(0, 1);
    printk("%u[Bytes] %u[B/s]\n", pcb[id].netstream.CurByte,
           (pcb[id].netstream.CurByte - pcb[id].netstream.prevByte) / PERF_GAP);
    pcb[id].netstream.prevByte = pcb[id].netstream.CurByte;
}

/*
 * 定时重传集成函数
 */
void do_resend() {
    for (int id = 0; id < NUM_MAX_TASK; id++) {
        if (pcb[id].netstream.valid && pcb[id].status != TASK_EXITED) {
            /*打印性能*/
            if (get_timer() >= pcb[id].netstream.perf_time) {
                do_perfm(id);
                pcb[id].netstream.perf_time += PERF_GAP;
            }

            /*进行重传和进程唤醒，以便对接收区进行操作*/
            if (get_ticks() >= pcb[id].netstream.resend_time) {
                do_resend_RSD(id);
                do_resend_ACK(id);
                do_wake_up_recv_stream();
                pcb[id].netstream.resend_time += RESEND_TIME_INTERVAL;
            }
        }
    }
}


/****** 发送接收阻塞队列操作 ******/

/*
 * 发送中断并从阻塞队列中取出，同时置零中断位
 */
void e1000_handle_txpe(void) {
    while (send_block_queue.next != &send_block_queue)
        do_unblock(send_block_queue.next);
    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
}

/*
 * 接收中断并从阻塞队列中取出，同时置零中断位
 */
void e1000_handle_rxdmt0(void) {
    while (recv_block_queue.next != &recv_block_queue)
        do_unblock(recv_block_queue.next);
    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_RXDMT0);
}