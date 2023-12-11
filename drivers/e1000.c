#include "os/lock.h"
#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <os/sched.h>
#include <assert.h>
#include <pgtable.h>

LIST_HEAD(send_block_queue);
LIST_HEAD(recv_block_queue);
static spin_lock_t send_block_spin_lock = {UNLOCKED};
static spin_lock_t recv_block_spin_lock = {UNLOCKED};

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    //e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for(int i=0; i<TXDESCS; i++){
        tx_desc_array[i].addr   = (uint32_t)kva2pa((uintptr_t)(tx_pkt_buffer[i]));
        tx_desc_array[i].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    
    uint32_t LOW  = (uint32_t)(((uint64_t)kva2pa(tx_desc_array)) & 0xffffffff);
    uint32_t HIGH = (uint32_t)(((uint64_t)kva2pa(tx_desc_array)) >> 32); 
    // uint32_t SIZE = 16*TXDESCS;
    uint32_t SIZE = sizeof(tx_desc_array);
    
    e1000_write_reg(e1000,E1000_TDBAL,LOW);
    e1000_write_reg(e1000,E1000_TDBAH,HIGH);
    e1000_write_reg(e1000,E1000_TDLEN,SIZE);

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */

    e1000_write_reg(e1000,E1000_TDH,0);
    e1000_write_reg(e1000,E1000_TDT,0);

    /* TODO: [p5-task1] Program the Transmit Control Register */

    //EN
    uint32_t TCTL_write = E1000_TCTL_EN |
                          E1000_TCTL_PSP|
                          0x100         |
                          0x40000       ;

    e1000_write_reg(e1000,E1000_TCTL,TCTL_write);
    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    //RAL0
    uint32_t RAL0_v = (enetaddr[3]<<24) |
                      (enetaddr[2]<<16) |
                      (enetaddr[1]<< 8) |
                      (enetaddr[0])
                      ;
    
    uint32_t RAH0_v = E1000_RAH_AV      |
                      (enetaddr[5]<< 8) |
                      (enetaddr[4])
                      ;

    e1000_write_reg_array(e1000,E1000_RA,0,RAL0_v);
    e1000_write_reg_array(e1000,E1000_RA,1,RAH0_v);

    /* TODO: [p5-task2] Initialize rx descriptors */

    for(int i=0; i<RXDESCS; i++){
        rx_desc_array[i].addr = (uint32_t)kva2pa((uintptr_t)(rx_pkt_buffer[i]));
        rx_desc_array[i].status = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].length = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
        // rx_desc_array[i].status |= (0<<1);//EOP
    }//全部清空

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    
    // uint32_t LOW  = (uint32_t)(((uint64_t)kva2pa(rx_desc_array)) & 0xffffffff);
    // uint32_t HIGH = (uint32_t)(((uint64_t)kva2pa(rx_desc_array)) >> 32); 
    // uint32_t SIZE = 16*RXDESCS;
    
    // e1000_write_reg(e1000,E1000_RDBAL,LOW);
    // e1000_write_reg(e1000,E1000_RDBAH,HIGH);
    // e1000_write_reg(e1000,E1000_RDLEN,SIZE);

    e1000_write_reg(e1000, E1000_RDBAH, kva2pa((uint64_t)rx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_RDBAL, kva2pa((uint64_t)rx_desc_array) << 32 >> 32);
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */

    e1000_write_reg(e1000,E1000_RDH,0);
    e1000_write_reg(e1000,E1000_RDT,RXDESCS -1);

    /* TODO: [p5-task2] Program the Receive Control Register */

    uint32_t RCTL_write = E1000_RCTL_EN    | 
                          E1000_RCTL_BAM   ;

    e1000_write_reg(e1000,E1000_RCTL,RCTL_write);

    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */

    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

// void e1000_block_send(){
//     current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
//     (*current_running)->status = TASK_BLOCKED;
//     list_del(&((*current_running)->list));
//     list_add(&((*current_running)->list), &send_queue);
//     do_scheduler(); 
// }

// void e1000_check_send(){
//     current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
//     local_flush_dcache();
//     if(send_queue.prev == &send_queue) return;
//     list_node_t *now = &send_queue;
//     list_node_t *nxt = now->prev;

//     int tail = e1000_read_reg(e1000,E1000_TDT);
//     if(tx_desc_array[tail].status & E1000_TXD_STAT_DD){
//         list_del(nxt);
//         list_add(nxt,&ready_queue);
//         pcb_t *pcb_now = LIST_to_PCB(nxt);
//         pcb_now->status = TASK_READY;        
//     }
// }
// void do_unblock(list_node_t *pcb_node)
// {
//     list_del(pcb_node);

//     spin_lock_acquire(&ready_spin_lock);
//     list_add(pcb_node, &ready_queue);
//     spin_lock_release(&ready_spin_lock);

//     (list_entry(pcb_node, pcb_t, list))->status = TASK_READY;
//     //return;
//     // TODO: [p2-task2] unblock the `pcb` from the block queue
// }

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;

    uint32_t tail = e1000_read_reg(e1000,E1000_TDT);
    // int head = e1000_read_reg(e1000,E1000_TDH);
    // int TCTL = e1000_read_reg(e1000,E1000_TCTL);
    // nxt = (tail + 1) % RXDESCS;//看下一个是不是有效的，如果有效说明可以读出来

    // cnt ++;

    tx_desc_array[tail].length = length;
    // printl("L = %d\n",length);
    tx_desc_array[tail].status &= ~(E1000_TXD_STAT_DD);
    memcpy((void*)tx_pkt_buffer[tail],txpacket,length);//完成当前对于tail指针指向的发送描述符的一系列标志位等
    
    tail = (tail + 1) % TXDESCS;//注意在进入这个函数的时候，tail指向的位置一定是可以写入的
    //只不过往后走可能不能写入了，那么这个时候需要被阻塞，并且不可以把新的tail指针写入

    spin_lock_acquire(&send_block_spin_lock);
    while(!(tx_desc_array[tail].status & E1000_TXD_STAT_DD)){
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        do_block(&(*current_running)->list, &send_block_queue,&send_block_spin_lock);
    }
    spin_lock_release(&send_block_spin_lock);

    e1000_write_reg(e1000,E1000_TDT,tail);

    local_flush_dcache();
    return 0;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;

    uint32_t tail = e1000_read_reg(e1000,E1000_RDT);

    tail = (tail + 1) % RXDESCS;//看下一个是不是有效的，如果有效说明可以读出来

    spin_lock_acquire(&recv_block_spin_lock);
    while(!(rx_desc_array[tail].status & E1000_RXD_STAT_DD)){//如果DD位有效的，则说明这里已经被硬件置过，在这种情况下是可以往后走的
        // e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
        do_block(&(*current_running)->list, &recv_block_queue,&recv_block_spin_lock);
    }
    spin_lock_release(&recv_block_spin_lock);
    
    uint32_t len = rx_desc_array[tail].length;
    memcpy(rxbuffer,(void*)rx_pkt_buffer[tail],len);
    rx_desc_array[tail].status = 0;
    rx_desc_array[tail].length = 0;
    
    // nxt = (nxt + 1) % RXDESCS;

    e1000_write_reg(e1000,E1000_RDT,tail);

    local_flush_dcache();
    return len;
}
//两个函数最不同的地方就在于，对于发送描述符，时进入函数之后一定可以发送，需要考虑的是发送完后tail能不能往前走
//接收则首先需要考虑前面的能不能走，如果不能走那么就应该直接被阻塞