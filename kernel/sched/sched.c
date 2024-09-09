#include "os/list.h"
#include <hash.h>
#include <os/kernel.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <pgtable.h>
#include <printk.h>
#include <mminit.h>

extern void ret_from_exception();
//初始化内核进程pcb
#ifdef MLFQ
#define MLFQNUM 3
#define PRIOR2TIME(prior) 2 * prior + 1
#define MLFQDOWN(prior)                                                                            \
    if (prior < MLFQNUM - 1)                                                                       \
    prior++
#define MLFQUP(prior)                                                                              \
    if (prior > 0)                                                                                 \
    prior--
#define MLFQGAP 1
static uint8_t MLFQuptime = 0;
list_node_t ready_queues[MLFQNUM];
#endif
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
spin_lock_t ready_spin_lock = {UNLOCKED};
spin_lock_t sleep_spin_lock = {UNLOCKED};

/* current running task PCB */
pcb_t *current_running_0;
pcb_t *current_running_1;
//在main.c中初始化为只想pid0_pcb的pcb指针！

pcb_t pid0_pcb = {.pid = 0,
                  .tid = 0,
                  .kernel_sp = (ptr_t)PID0_STACK_KVA,
                  .user_sp = (ptr_t)PID0_STACK_KVA,
                  .hart_mask = 0x1,
                  .cpu = 0x1,
                  .cputime = 0x1,
                  .prior = 0,
                  .pcb_name = "pid0",
                  //每个核对应的都有可以跑的

                  .pgdir = (PTE *)(PGDIR_PA + KVA_OFFSET),
                  .recycle = 0,
                  .list = {NULL, NULL},
                  .status = TASK_RUNNING};
pcb_t pid1_pcb = {.pid = 1,
                  .tid = 0,
                  .kernel_sp = (ptr_t)PID1_STACK_KVA,
                  .user_sp = (ptr_t)PID1_STACK_KVA,
                  .hart_mask = 0x2,
                  .cpu = 0x2,
                  .cputime = 0x1,
                  .prior = 0,
                  .pcb_name = "pid1",
                  //每个核对应的都有可以跑的

                  .pgdir = (PTE *)(PGDIR_PA + KVA_OFFSET),
                  .recycle = 0,
                  .list = {NULL, NULL},
                  .status = TASK_RUNNING};

void clean_temp_page(uint64_t pgdir_addr) {
    PTE *pgdir = (PTE *)pgdir_addr;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu) {
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS); //根目录页中的临时映射清空
        pgdir[vpn2] = 0;
    }
}

/*
 * 阻塞进程至queue队列
 * 这里采用mesa风格写小锁(虽然最终没实现小锁)
 */
void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock) {
    list_del(pcb_node);
    list_add(pcb_node, queue);
    (list_entry(pcb_node, pcb_t, list))->status = TASK_BLOCKED;
    spin_lock_release(lock);
    do_scheduler();

    //在之前全部执行完毕之后，再次切到自己时，再acquire自旋锁
    spin_lock_acquire(lock);
}

/*
 * 进程阻塞解除
 */
void do_unblock(list_node_t *pcb_node) {
    list_del(pcb_node);
#ifndef MLFQ
    list_add(pcb_node, &ready_queue);
#else
    list_add(pcb_node, &ready_queues[0]);
    (list_entry(pcb_node, pcb_t, list))->prior = 0;
#endif
    (list_entry(pcb_node, pcb_t, list))->status = TASK_READY;
}

/*
 * 释放相关锁资源，标记recycle方便后续物理页回收，标记为退出状态
 */
void srcrel(int id) {
    //标记状态
    pcb[id].status = TASK_EXITED;
    pcb[id].recycle = 1;

    //释放互斥锁资源
    for (int i = 0; i < TASK_LOCK_MAX; i++) {
        if (pcb[id].mutex_lock_key[i] != 0) {
            do_mutex_lock_release(pcb[id].mutex_lock_key[i]);
        }
    }

    //释放等待锁资源
    spin_lock_acquire(&pcb[id].wait_lock);
    while (pcb[id].wait_list.next != &pcb[id].wait_list)
        do_unblock(pcb[id].wait_list.next);
    spin_lock_release(&pcb[id].wait_lock);
}

pcb_t *RRsched(int curcpu, pcb_t* currunning) {
    // RUNNING->READY(BLOCKED不用改，已在队列中)
    if (currunning->status == TASK_RUNNING) {
        list_add(&(currunning->list), &ready_queue);
        currunning->status = TASK_READY;
    }

    list_node_t *list_check = ready_queue.next;
    pcb_t *nxtproc;
    nxtproc = list_entry(list_check, pcb_t, list);
    while ((curcpu & nxtproc->hart_mask) == 0) {
        list_check = list_check->next;
        nxtproc = list_entry(list_check, pcb_t, list);
    }
    set_pcb(nxtproc);
    nxtproc->cpu = curcpu;
    list_del(&(nxtproc->list));
    nxtproc->status = TASK_RUNNING;

    return nxtproc;
}

#ifdef MLFQ
void init_queues() {
    for (unsigned i = 0; i < MLFQNUM; i++) {
        list_init(&ready_queues[i]);
    }
}

pcb_t *MLFQsched(int curcpu, pcb_t *currunning) {
    currunning->cputime--;
    if (!currunning->cputime && currunning->pid != 1 && currunning->pid != 2) {
        if (currunning->pid == 1 || currunning->pid == 2)
            list_add(&currunning->list, &ready_queues[MLFQNUM - 1]);
        else {
            MLFQDOWN(currunning->prior);
            list_add(&currunning->list, &ready_queues[currunning->prior]);
        }

        for (unsigned i = 0; i < MLFQNUM; i++) {
            if (list_check(&ready_queues[i]))
                continue;
            list_node_t *list_check = ready_queues[i].next;
            pcb_t *nxtproc;
            while (list_check != &ready_queues[i]) {
                nxtproc = list_entry(list_check, pcb_t, list);
                if (curcpu & nxtproc->hart_mask) {
                    set_pcb(nxtproc);
                    list_del(&(nxtproc->list));

                    nxtproc->cpu = curcpu;
                    nxtproc->status = TASK_RUNNING;
                    nxtproc->cputime = PRIOR2TIME(currunning->prior);
                    return nxtproc;
                }
                list_check = list_check->next;
            }
        }
        return NULL;
    } else {
        return currunning;
    }
}

void MLFQupprior() {
    if (get_timer() >= MLFQuptime) {
        for (int i = 0; i < NUM_MAX_TASK; i++)
            if (pcb[i].status != TASK_EXITED)
                pcb[i].prior = 0 ;
        MLFQuptime += PERF_GAP;
    }
}
#endif