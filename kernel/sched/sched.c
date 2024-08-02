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
tcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = 0xffffffc050900000;
const ptr_t pid1_stack = 0xffffffc0508ff000;
pcb_t pid0_pcb = {.pid = 0,
                  .tid = 0,
                  .kernel_sp = (ptr_t)pid0_stack,
                  .user_sp = (ptr_t)pid0_stack,
                  .hart_mask = 0x1,
                  .current_mask = 0x1,
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
                  .current_mask = 0x2,
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

/* global process id */
pid_t process_id = 1;
//初始进程pid号为1，为内核进程

void clean_temp_page(uint64_t pgdir_addr) {
    PTE *pgdir = (PTE *)pgdir_addr;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu) {
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS); //根目录页中的临时映射清空
        pgdir[vpn2] = 0;
    }
}

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock) {
    list_del(pcb_node);
    list_add(pcb_node, queue);
    (list_entry(pcb_node, pcb_t, list))->status = TASK_BLOCKED;
    spin_lock_release(lock);
    do_scheduler();

    //在之前全部执行完毕之后，再次切到自己是，再acquire自旋锁
    spin_lock_acquire(lock);
    // return;
    //将正在运行的进程pcb加入到对应锁的阻塞队列里，同时将pcb状态更改为TASK_BLOCKED
    //这里有queue参数，是因为不知道要加入哪个锁的阻塞队列之中
    // TODO: [p2-task2] block the pcb task into the block queue
}

void do_unblock(list_node_t *pcb_node) {
    list_del(pcb_node);

    // spin_lock_acquire(&ready_spin_lock);
    list_add(pcb_node, &ready_queue);
    // spin_lock_release(&ready_spin_lock);

    (list_entry(pcb_node, pcb_t, list))->status = TASK_READY;
    // return;
    // TODO: [p2-task2] unblock the `pcb` from the block queue
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

void init_pcb_mm(int id, int taskid, enum FORK fork) {
    uintptr_t kva_user_stack;

    /*指定根目录页地址，堆地址，内核栈地址(直接分配完毕)，用户栈地址*/
    pcb[id].pgdir = (PTE *)kalloc();
    pcb[id].heap = HEAP_STARTVA;
    pcb[id].kernel_sp = kalloc() + 1 * PAGE_SIZE;
    pcb[id].user_sp = USER_STACK_ADDR;

    //内核地址映射拷贝
    share_pgtable(pcb[id].pgdir, (PTE *)pa2kva(PGDIR_PA));

    /*
     * fork: 加载程序，分配用户栈，初始化用户栈
     * notfork: 建立页表映射到父进程的物理页，其他工作由写时复制保证
     */
    if (fork == NOTFORK) {
        load_task_img(taskid, (uintptr_t)pcb[id].pgdir);
        kva_user_stack =
            uvmalloc(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,
                     _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER) +
            1 * PAGE_SIZE;
        init_pcb_stack(pcb[id].kernel_sp, kva_user_stack, tasks[taskid].task_entrypoint, &pcb[id],
                       kernel_argc, kernel_argv);
    } else {
        pcb_t *curpcb = get_pcb();
        pgcopy(pcb[id].pgdir, curpcb->pgdir, 2);
        
        regs_context_t *pt_regs = (regs_context_t *)(pcb[id].kernel_sp - sizeof(regs_context_t));
        switchto_context_t *pt_switchto =
            (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
        //trapframe段拷贝
        memcpy((void *)pt_regs, (void *)(curpcb->kernel_sp - sizeof(regs_context_t)),
               sizeof(regs_context_t));

        //fork进程独有：tp寄存器，a0返回0，switchto后返回ret_from_ex，switchto设置trapframe内核栈地址
        pt_regs->regs[4] = (reg_t)(&pcb[id]); // tp
        pt_regs->regs[10] = (reg_t)0;
        pt_switchto->regs[0] = (reg_t)ret_from_exception;
        pt_switchto->regs[1] = (reg_t)(pt_regs); // sp

        pcb[id].kernel_sp = (reg_t)pt_switchto;
    }
}

void init_tcb_mm(int id, void *thread_entrypoint, void *arg) {
    uintptr_t kva_user_stack;

    /*指定根目录页地址，堆地址，内核栈地址(直接分配完毕)，用户栈地址(一个线程分配一页)*/
    pcb[id].pgdir = get_pcb()->pgdir;
    pcb[id].heap = HEAP_STARTVA;
    pcb[id].kernel_sp = kalloc() + 1 * PAGE_SIZE;
    pcb[id].user_sp = USER_STACK_ADDR + PAGE_SIZE * pcb[id].tid;

    /*
     * 分配用户栈空间
     */
    kva_user_stack = uvmalloc(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,
                              _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER) +
                     1 * PAGE_SIZE;

    /*初始化用户栈*/
    init_tcb_stack(pcb[id].kernel_sp, pcb[id].user_sp, (uintptr_t)thread_entrypoint, &pcb[id],
                   (uint64_t)arg); //这里直接传用户的虚地址即可
}

int pid2id(int pid) {
    int id = hash(pid, NUM_MAX_TASK);
    for (int i = 0; i < NUM_MAX_TASK; i++, id = (id + 1) % NUM_MAX_TASK)
        if (pid == pcb[id].pid)
            return id;
    return -1;
}
