#include "os/lock.h"
#include "os/mm.h"
#include "os/sched.h"
#include "os/smp.h"
#include "os/time.h"
#include "printk.h"

/*
 * !!!
 * 为什么这里不能用get_pcb()，因为要明白现在curRun1与curRun2这两个地方存放的值需要进行变化，
 * 而要变化就一定得知道这两个的地址
 * 因此需要通过curRun根据当前所处的核来指向curRun1或curRun2，然后进行修改
 * 如果直接调用get_pcb()，将直接获得当前curRun1(或curRun2)的值，
 * 后续根据switchto进行对curRun1(或curRun2)修改时将无法做到
 */

void do_scheduler(void) {
    spin_lock_acquire(&ready_spin_lock);
    list_node_t *list_check;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    int curcpu = get_current_cpu_id() ? 0x2 : 0x1;
    check_sleeping();

    do_resend();

    if ((*current_running)->pid == 2) {
        for (unsigned i = 0; i < NUM_MAX_TASK; i++) {
            if ((pcb[i].pid != 2) && pcb[i].recycle && !pcb[i].tid) {
                //取消末级页，取消映射，回收内核栈
                uvmfreeall(pcb[i].pgdir);
                mapfree(pcb[i].pgdir);
                kmfree(pcb[i].kernel_sp);
                pcb[i].recycle = 0;
            }
            //当前是shell,并且经过了exit需要被回收，同时不是子线程，同时shell的不允许被回收
        }
    }
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/
    pcb_t *prev_running = (*current_running);

    if ((*current_running)->status == TASK_RUNNING) {
        list_add(&((*current_running)->list), &ready_queue);
        (*current_running)->status = TASK_READY;
    }
    //注意这里如果status为BLOCKED则不用执行上面的操作，因为已经加入到对应lock的block_queue中了

    list_check = ready_queue.next;
    //此时对curRun1(curRun2)进行了修改
    (*current_running) = list_entry(ready_queue.next, pcb_t, list);
    while ((curcpu & (*current_running)->hart_mask) == 0) {
        list_check = list_check->next;
        (*current_running) = list_entry(list_check, pcb_t, list);
    }
    (*current_running)->cpu = curcpu;

    list_del(&((*current_running)->list));
    (*current_running)->status = TASK_RUNNING;
    //将当前执行的进程对应的pcb加入ready队列之中，同时将ready队列的最前端进程取下来，由prev_running指向它。
    //注意将两个pcb的进程状态修改。

    process_id = (*current_running)->pid;
    //修改当前执行进程ID

    // TODO: [p2-task1] Modify the current_running pointer.
    set_satp(SATP_MODE_SV39, (*current_running)->pid,
             kva2pa((uintptr_t)(*current_running)->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    local_flush_icache_all();

    spin_lock_release(&ready_spin_lock);

    switch_to(prev_running, (*current_running));
    // TODO: [p2-task1] switch_to current_running
}

void do_sleep(uint32_t sleep_time) {
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    spin_lock_acquire(&sleep_spin_lock);
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    (*current_running)->wakeup_time = get_timer() + sleep_time;
    printl("\nadd (%s %d) to sleep_queue\n", (*current_running)->pcb_name, (*current_running)->pid);
    do_block(&(*current_running)->list, &sleep_queue, &sleep_spin_lock);
    spin_lock_release(&sleep_spin_lock);
    // do_scheduler();
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

void do_exit(void) {
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;

    int id = pid2id((*current_running)->pid);
    srcrel(id);

    do_scheduler();
}

int do_kill(pid_t pid) {
    int id = pid2id(pid);
    if (id == -1)
        return 1;

    if (pcb[id].status == TASK_BLOCKED) {
        list_del(&pcb[id].list);
        srcrel(id);
    } else {
        pcb[id].kill = 1;
    }

    return 0;
}

int do_waitpid(pid_t pid) {
    int id;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    for (id = 0; id < NUM_MAX_TASK; id++) {
        if ((pcb[id].pid == pid) && (pcb[id].status != TASK_EXITED))
            break;
    }

    if (id == NUM_MAX_TASK)
        return 0;
    else {
        spin_lock_acquire(&(pcb[id].wait_lock));
        do_block(&((*current_running)->list), &(pcb[id].wait_list), &(pcb[id].wait_lock));
        spin_lock_release(&(pcb[id].wait_lock));

        return 1;
    }
}