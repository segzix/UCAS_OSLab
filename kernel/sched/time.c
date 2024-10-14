#include "os/smp.h"
#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks() {
    __asm__ __volatile__("rdtime %0" : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer() {
    return get_ticks() / time_base;
}

uint64_t get_time_base() {
    return time_base;
}

void latency(uint64_t time) {
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time)
        ;
    return;
}

/**
 * 进入睡眠，设置苏醒时间并阻塞至睡眠队列中
 */
void do_sleep(uint32_t sleep_time) {
    spin_lock_acquire(&sleep_spin_lock);
    pcb_t *curpcb = get_pcb();
    curpcb->wakeup_time = get_timer() + sleep_time;
    do_block(&(curpcb->list), &sleep_queue, &sleep_spin_lock);
    spin_lock_release(&sleep_spin_lock);
}

/**
 * 检查睡眠队列
 */
void check_sleeping(void) {
    list_node_t *sleep_queue_check = sleep_queue.next;
    list_node_t *temp_queue_check;

    //检查睡眠队列，超出睡眠时间转入ready队列
    while (sleep_queue_check != &sleep_queue) {
        pcb_t *sleep_pcb = list_entry(sleep_queue_check, pcb_t, list);
        if (get_timer() >= sleep_pcb->wakeup_time) {
            temp_queue_check = sleep_queue_check->next;
            list_del(sleep_queue_check);
            list_add(sleep_queue_check, &ready_queue);
            sleep_pcb->status = TASK_READY;

            sleep_queue_check = temp_queue_check;
        } else {
            sleep_queue_check = sleep_queue_check->next;
        }
    }
}