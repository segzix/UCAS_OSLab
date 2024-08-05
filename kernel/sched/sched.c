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

extern void ret_from_exception();
pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = 0xffffffc050900000;
const ptr_t pid1_stack = 0xffffffc0508ff000;
pcb_t pid0_pcb = {.pid = 0,
                  .tid = 0,
                  .kernel_sp = (ptr_t)pid0_stack,
                  .user_sp = (ptr_t)pid0_stack,
                  .hart_mask = 0x1,
                  .cpu = 0x1,
                  .pcb_name = "pid0",
                  //每个核对应的都有可以跑的

                  .pgdir = (PTE *)(PGDIR_PA + KVA_OFFSET),
                  .recycle = 0,
                  .list = {NULL, NULL},
                  .status = TASK_RUNNING};
pcb_t pid1_pcb = {.pid = 1,
                  .tid = 0,
                  .kernel_sp = (ptr_t)pid1_stack,
                  .user_sp = (ptr_t)pid1_stack,
                  .hart_mask = 0x2,
                  .cpu = 0x2,
                  .pcb_name = "pid1",
                  //每个核对应的都有可以跑的

                  .pgdir = (PTE *)(PGDIR_PA + KVA_OFFSET),
                  .recycle = 0,
                  .list = {NULL, NULL},
                  .status = TASK_RUNNING};
//初始化内核进程pcb

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
spin_lock_t ready_spin_lock = {UNLOCKED};
spin_lock_t sleep_spin_lock = {UNLOCKED};

/* current running task PCB */
pcb_t **current_running;
pcb_t *current_running_0;
pcb_t *current_running_1;
//在main.c中初始化为只想pid0_pcb的pcb指针！

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
    spin_lock_acquire(&ready_spin_lock);
    list_add(pcb_node, &ready_queue);
    spin_lock_release(&ready_spin_lock);
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

pcb_t *get_pcb() {
    return *(get_current_cpu_id() ? &current_running_1 : &current_running_0);
}

int pid2id(int pid) {
    int id = hash(pid, NUM_MAX_TASK);
    for (int i = 0; i < NUM_MAX_TASK; i++, id = (id + 1) % NUM_MAX_TASK)
        if (pid == pcb[id].pid)
            return id;
    return -1;
}
