#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .tid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,

    .list = {NULL,NULL}, 
    .status = TASK_RUNNING
};
//初始化内核进程pcb

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
spin_lock_t ready_spin_lock = {UNLOCKED};
spin_lock_t sleep_spin_lock = {UNLOCKED};

/* current running task PCB */
pcb_t * volatile current_running;
//在main.c中初始化为只想pid0_pcb的pcb指针！

/* global process id */
pid_t process_id = 1;
//初始进程pid号为1，为内核进程

void do_scheduler(void)
{   
    spin_lock_acquire(&ready_spin_lock);
    // bios_set_timer(get_ticks() + TIMER_INTERVAL);
    check_sleeping();
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    pcb_t* prev_running = current_running;

    if(current_running->status == TASK_RUNNING){
        list_add(&(current_running->list), &ready_queue);
        current_running->status = TASK_READY;
    } 
    //注意这里如果status为BLOCKED则不用执行上面的操作，因为已经加入到对应lock的block_queue中了
    current_running = list_entry(ready_queue.next, pcb_t, list);
    list_del(&(current_running->list));
    current_running->status = TASK_RUNNING;
    //将当前执行的进程对应的pcb加入ready队列之中，同时将ready队列的最前端进程取下来，由prev_running指向它。
    //注意将两个pcb的进程状态修改。

    process_id = current_running->pid;
    //修改当前执行进程ID

    /*vt100_move_cursor(current_running->cursor_x, current_running->cursor_y);
    screen_cursor_x = current_running->cursor_x;
    screen_cursor_y = current_running->cursor_y;*/
    // 将当前执行进程的screen_cursor修改一下？

    // TODO: [p2-task1] Modify the current_running pointer.
    spin_lock_release(&ready_spin_lock);

    switch_to(prev_running, current_running);
    // TODO: [p2-task1] switch_to current_running

}

void do_thread_scheduler(void)
{   
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    spin_lock_acquire(&ready_spin_lock);
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    list_node_t* list_check;
    pcb_t* prev_running = current_running;

    if(current_running->status == TASK_RUNNING){
        list_add(&(current_running->list), &ready_queue);
        current_running->status = TASK_READY;
    } 
    //注意这里如果status为BLOCKED则不用执行上面的操作，因为已经加入到对应lock的block_queue中了
    list_check = ready_queue.next;
    current_running = list_entry(list_check, pcb_t, list);

    while((current_running->pid != prev_running->pid) || (current_running->tid == 0) || (current_running->tid ==  prev_running->tid)){
        list_check = list_check->next;
        current_running = list_entry(list_check, pcb_t, list);
    }
    list_del(&(current_running->list));
    current_running->status = TASK_RUNNING;
    //将当前执行的进程对应的pcb加入ready队列之中，同时将ready队列的最前端进程取下来，由prev_running指向它。
    //注意将两个pcb的进程状态修改。

    process_id = current_running->pid;
    //修改当前执行进程ID

    /*vt100_move_cursor(current_running->cursor_x, current_running->cursor_y);
    screen_cursor_x = current_running->cursor_x;
    screen_cursor_y = current_running->cursor_y;*/
    // 将当前执行进程的screen_cursor修改一下？

    // TODO: [p2-task1] Modify the current_running pointer.
    spin_lock_release(&ready_spin_lock);
    
    switch_to(prev_running, current_running);
    // TODO: [p2-task1] switch_to current_running

}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    spin_lock_acquire(&sleep_spin_lock);
    current_running->wakeup_time = get_timer() + sleep_time;
    do_block(&current_running->list, &sleep_queue,&sleep_spin_lock);
    spin_lock_release(&sleep_spin_lock);
    //do_scheduler();
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock)
{
    list_del(pcb_node);
    list_add(pcb_node, queue);
    (list_entry(pcb_node, pcb_t, list))->status = TASK_BLOCKED;
    spin_lock_release(lock);
    do_scheduler();

    //在之前全部执行完毕之后，再次切到自己是，再acquire自旋锁
    spin_lock_acquire(lock);
    //return;
    //将正在运行的进程pcb加入到对应锁的阻塞队列里，同时将pcb状态更改为TASK_BLOCKED
    //这里有queue参数，是因为不知道要加入哪个锁的阻塞队列之中
    // TODO: [p2-task2] block the pcb task into the block queue
}

void do_unblock(list_node_t *pcb_node)
{
    list_del(pcb_node);

    spin_lock_acquire(&ready_spin_lock);
    list_add(pcb_node, &ready_queue);
    spin_lock_release(&ready_spin_lock);

    (list_entry(pcb_node, pcb_t, list))->status = TASK_READY;
    //return;
    // TODO: [p2-task2] unblock the `pcb` from the block queue
}
