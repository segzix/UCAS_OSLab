#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/time.h>
#define STREAM_BUFFER_NUM 64
#define TEMP_BUFFER_NUM 2048
#define RESEND_BUFFER_NUM 62
#define RESEND_HEAD_NUM 54

#define MAGIC_NUM 0x45
#define _DAT (1lu << 0)
#define _RSD (1lu << 1)
#define _ACK (1lu << 2)

#define RSD_TIME_GAP 1
#define ACK_TIME_GAP 1

#define MAGIC_NUM_SEGMENT_OFFSET 54
#define FLAGS_SEGMENT_OFFSET 55
#define LENGTH_SEGMENT_OFFSET 56
#define SEQ_SEGMENT_OFFSET 58
#define SIZE_SEGMENT_OFFSET 62
#define DATA_SEGMENT_OFFSET 66

#define START_OFFSET MAGIC_NUM_SEGMENT_OFFSET

// static LIST_HEAD(send_block_queue);
// static LIST_HEAD(recv_block_queue);
typedef struct stream_buffer{
    uint64_t seq;
    uint32_t length;

    int prev;
    int next;
    int valid;
}stream_buffer; 
stream_buffer stream_buffer_array[STREAM_BUFFER_NUM];

char temp_buffer[TEMP_BUFFER_NUM];
char resend_buffer[RESEND_BUFFER_NUM];
char resend_head[RESEND_HEAD_NUM];
uint64_t resend_ACK_time;
uint64_t resend_RSD_time;

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

uint64_t min(uint64_t a,uint64_t b){
    return (a > b) ? b : a;
}

uint64_t max(uint64_t a,uint64_t b){
    return (a > b) ? a : b;
}

void init_stream(void){
    stream_buffer_array[0].valid = 1;
    stream_buffer_array[0].seq = 0;
    stream_buffer_array[0].length = 0;
    stream_buffer_array[0].prev = -1;
    stream_buffer_array[0].next = -1;

    for(int i = 1;i < STREAM_BUFFER_NUM;i++){
        stream_buffer_array[i].valid = 0;
        stream_buffer_array[i].seq = 0;
        stream_buffer_array[i].length = 0;
        stream_buffer_array[i].prev = -1;
        stream_buffer_array[i].next = -1;
    }

    resend_ACK_time = get_timer();
    resend_RSD_time = get_timer();
}

void do_resend_RSD(){
    if(resend_RSD_time - get_timer() >= RSD_TIME_GAP){
        uint32_t resend_RSD_seq = stream_buffer_array[0].seq + stream_buffer_array[0].length;
        uint32_t resend_RSD_length = stream_buffer_array[0].length;

        memcpy((void*)resend_buffer, (void*)resend_head, RESEND_HEAD_NUM);

        resend_buffer[MAGIC_NUM_SEGMENT_OFFSET] = MAGIC_NUM;
        resend_buffer[FLAGS_SEGMENT_OFFSET] = _RSD;

        *(uint8_t*)(resend_buffer + LENGTH_SEGMENT_OFFSET)     = ((resend_RSD_length & 0x0000ff00) >> 8);
        *(uint8_t*)(resend_buffer + LENGTH_SEGMENT_OFFSET + 1) = ((resend_RSD_length & 0x000000ff) >> 0);

        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET)     = ((resend_RSD_seq & 0xff000000) >> 24);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 1) = ((resend_RSD_seq & 0x00ff0000) >> 16);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 2) = ((resend_RSD_seq & 0x0000ff00) >> 8);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 3) = ((resend_RSD_seq & 0x000000ff) >> 0);

        e1000_transmit(resend_buffer,RESEND_BUFFER_NUM);
        resend_RSD_time += RSD_TIME_GAP;
        printl("RSD : %u\n",resend_RSD_seq);
    }
}

void do_resend_ACK()
{
    if(resend_ACK_time - get_timer() >= ACK_TIME_GAP){
        uint32_t resend_RSD_seq = stream_buffer_array[0].seq + stream_buffer_array[0].length;
        uint32_t resend_RSD_length = stream_buffer_array[0].length;

        memcpy((void*)resend_buffer, (void*)resend_head, RESEND_HEAD_NUM);

        resend_buffer[MAGIC_NUM_SEGMENT_OFFSET] = MAGIC_NUM;
        resend_buffer[FLAGS_SEGMENT_OFFSET] = _ACK;

        *(uint8_t*)(resend_buffer + LENGTH_SEGMENT_OFFSET) = ((resend_RSD_length & 0x0000ff00) >> 8);
        *(uint8_t*)(resend_buffer + LENGTH_SEGMENT_OFFSET + 1) = ((resend_RSD_length & 0x000000ff) >> 0);

        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET) = ((resend_RSD_seq & 0xff000000) >> 24);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 1) = ((resend_RSD_seq & 0x00ff0000) >> 16);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 2) = ((resend_RSD_seq & 0x0000ff00) >> 8);
        *(uint8_t*)(resend_buffer + SEQ_SEGMENT_OFFSET + 3) = ((resend_RSD_seq & 0x000000ff) >> 0);

        e1000_transmit(resend_buffer,RESEND_BUFFER_NUM);
        resend_ACK_time += ACK_TIME_GAP;
        printl("ACK : %u\n",resend_RSD_seq);
    }
}

int do_net_recv_stream(void *buffer, int *nbytes)
{
    int current_bytes = *nbytes;
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    while(current_bytes){

        int lennow = e1000_poll(temp_buffer);//先暂时拷贝到这个字符数组中

        char * start_buffer = temp_buffer + START_OFFSET;//数据包头开始的地方
        char * data_buffer = temp_buffer + SIZE_SEGMENT_OFFSET;//数据段开始的地方

        uint8_t  now_magic = *(uint8_t*)(temp_buffer + MAGIC_NUM_SEGMENT_OFFSET);

        uint16_t len1 = (uint16_t)(*(uint8_t*)(temp_buffer + LENGTH_SEGMENT_OFFSET) << 8);
        uint16_t len2 = (uint16_t)(*(uint8_t*)(temp_buffer + LENGTH_SEGMENT_OFFSET + 1) << 0);
        uint16_t now_len = len1 + len2;

        uint64_t seq1 = (uint64_t)(*(uint8_t*)(temp_buffer + SEQ_SEGMENT_OFFSET) << 24);
        uint64_t seq2 = (uint64_t)(*(uint8_t*)(temp_buffer + SEQ_SEGMENT_OFFSET + 1) << 16);
        uint64_t seq3 = (uint64_t)(*(uint8_t*)(temp_buffer + SEQ_SEGMENT_OFFSET + 2) << 8);
        uint64_t seq4 = (uint64_t)(*(uint8_t*)(temp_buffer + SEQ_SEGMENT_OFFSET + 3) << 0);
        uint64_t now_seq =  seq1 + seq2 + seq3 + seq4;
        
        uint64_t now_head = now_seq;
        uint64_t now_tail = now_seq + now_len;//通过上述方式，求得当前得到的字符buffer对应的数据包的起始地址和尾地址

        if(now_magic == MAGIC_NUM){//判断是对应的包才进入下面的判断

            memcpy((void*)resend_head, (void*)temp_buffer, RESEND_HEAD_NUM);//拷贝包头

            int node_index_p_overlap = 0;//第一个与当前插入有重叠的节点
            int node_index_p_notoverlap = 0;//不存在重叠，在此之前的节点
            int node_index_q = 0;//第一个与当前插入没有重叠的节点(p之后)

            while(node_index_p_overlap != -1){
                uint64_t head = stream_buffer_array[node_index_p_overlap].seq;
                uint64_t tail = stream_buffer_array[node_index_p_overlap].seq + stream_buffer_array[node_index_p_overlap].length;
                //得到当前数组元素对应的头和尾，然后判断是否应该进行合并
                int node_index_next = stream_buffer_array[node_index_p_overlap].next;
                //当前的node_index是准备可能被合并的点，front是要求来进行合并的点

                int overlap = (head <= now_tail && now_head <= tail);//重叠判断条件

                if(overlap)
                    break;

                node_index_p_overlap  = node_index_next;
            }//第一轮循环，找到是否有重叠的第一个节点

            if(node_index_p_overlap == -1){//没找到一个重叠的节点，因此需要再找一次
                while(stream_buffer_array[node_index_p_notoverlap].next != -1){
                    int node_index_next = stream_buffer_array[node_index_p_notoverlap].next;
                    uint64_t head = stream_buffer_array[node_index_next].seq;
                    //查询当前节点的下一个节点，如果比当前的要大，自然当前的节点就是最后一个不会与其重叠的节点

                    if(head >= now_head)
                        break;

                    node_index_p_notoverlap = node_index_next;
                }//第一轮循环，找到最后一个不与其重叠的节点

                int node_empty_index;
                for(node_empty_index = 0;stream_buffer_array[node_empty_index].valid;node_empty_index++);
                //找寻一个没有被占用过的节点
                int temp_next = stream_buffer_array[node_index_p_notoverlap].next;
                //没有被重叠的not_overlap的下一个节点，两边夹住node_empty_index

                stream_buffer_array[node_empty_index].valid = 1;
                stream_buffer_array[node_empty_index].seq = now_seq;
                stream_buffer_array[node_empty_index].length = now_len;//关于这个节点其他初始化的操作

                stream_buffer_array[node_empty_index].prev = node_index_p_notoverlap;
                stream_buffer_array[node_empty_index].next = temp_next;
                stream_buffer_array[node_index_p_notoverlap].next = node_empty_index;
                if(temp_next != -1)
                    stream_buffer_array[temp_next].prev = node_empty_index;//只有在不为-1的时候才会有它的前驱是新加的节点
        
                memcpy((void*)(buffer + now_seq), (void*)data_buffer, now_len);//这里节点新加进来了，需要拷贝
                current_bytes -= stream_buffer_array[node_empty_index].length;
                //这里是加的新的空节点的length,下面是加的新的length，但需要减去老的length
            }else{
                int head_node = node_index_p_overlap;
                int tail_node = node_index_p_overlap;

                node_index_q = node_index_p_overlap;
                while(node_index_q != -1){
                    uint64_t head = stream_buffer_array[node_index_q].seq;
                    uint64_t tail = stream_buffer_array[node_index_q].seq + stream_buffer_array[node_index_q].length;
                    //得到当前数组元素对应的头和尾，然后判断是否应该有重叠
                    int node_index_next = stream_buffer_array[node_index_q].next;
                    //当前的node_index是准备可能被合并的点，front是要求来进行合并的点

                    int overlap = (head <= now_tail && now_head <= tail);//重叠判断条件

                    if(overlap)
                        tail_node = node_index_q;//专门用来记录最后一个重叠的node
                    //只有当出现重叠时，tailnode才会跟上来

                    if(!overlap)//这里是找寻到第一个不重叠的然后跳出
                        break;

                    node_index_q = node_index_next;
                }//第二轮循环，找到第一个在过了重叠之后不与其重叠的节点

                //这里用来记录重叠的第一个节点和最后一个节点

                uint32_t general_old_length = 0;
                for(int i = node_index_p_overlap;i != node_index_q;i = stream_buffer_array[i].next){
                    general_old_length += stream_buffer_array[i].length;
                    if(i != node_index_p_overlap)
                        stream_buffer_array[i].valid = 0;
                }//循环获得之前这些就有节点加起来的长度，后面用新的长度减去旧的长度
                //这里index_q是-1也没有关系，都是完全一样的逻辑
                //这里在清空valid时，第一个不能清

                stream_buffer_array[node_index_p_overlap].next = node_index_q;
                if(node_index_q != -1)
                    stream_buffer_array[node_index_q].prev = node_index_p_overlap;//只有在不为-1的时候才会有它的前驱是新加的节点

                uint64_t head = stream_buffer_array[head_node].seq;
                uint64_t tail = stream_buffer_array[tail_node].seq + stream_buffer_array[tail_node].length;
                //再来做一次，方便确定合并之后的seq和length
                stream_buffer_array[node_index_p_overlap].seq = min(head,now_head);
                stream_buffer_array[node_index_p_overlap].length = max(tail,now_tail) - min(head,now_head);
                //关于这个节点其他seq和length重置的操作

                memcpy((void*)(buffer + now_seq), (void*)data_buffer, now_len);//这里节点新加进来了，需要拷贝
                current_bytes -= (stream_buffer_array[node_index_p_overlap].length - general_old_length);
            }
        }
    }

    return 0;
}

void e1000_handle_txpe(void){
    while(send_block_queue.next != &send_block_queue)
        do_unblock(send_block_queue.next);
    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
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
    if(ICR_ID & E1000_ICR_RXDMT0)
        e1000_handle_rxdmt0();
    // TODO: [p5-task3] Handle interrupts from network device
}