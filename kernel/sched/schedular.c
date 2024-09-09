#include "os/lock.h"
#include "os/mm.h"
#include "os/sched.h"
#include "os/smp.h"
#include "os/time.h"

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
    pcb_t *current_running = get_pcb();
    int curcpu = get_current_cpu_id() ? 0x2 : 0x1;

    /*检查睡眠队列，网络重传检测，回收物理页*/
    check_sleeping();
    do_resend();
    //当前是shell,并且经过了exit需要被回收，同时不是子线程，同时shell的不允许被回收
    if (current_running->pid == 2) {
        for (unsigned i = 0; i < NUM_MAX_TASK; i++) {
            if ((pcb[i].pid != 2) && pcb[i].recycle && !pcb[i].tid) {
                //取消末级页，取消映射，回收内核栈
                uvmfreeall(pcb[i].pgdir);
                mapfree(pcb[i].pgdir);
                kmfree(pcb[i].kernel_sp);
                pcb[i].recycle = 0;
            }
        }
    }

    pcb_t *prev_running = current_running;

// READY->RUNNING(简易调度算法，考虑mask是否允许在该核上运行)
#ifndef MLFQ
    current_running = RRsched(curcpu, current_running);
#else
    MLFQupprior();
    current_running = MLFQsched(curcpu, current_running);
    assert(current_running != NULL);
#endif

    //设置根目录页，刷tlb，icache
    set_satp(SATP_MODE_SV39, current_running->pid,
             kva2pa((uintptr_t)((current_running)->pgdir)) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    local_flush_icache_all();

    spin_lock_release(&ready_spin_lock);

    switch_to(prev_running, current_running);
}

/*
 * 进程退出，释放相关资源
 */
void do_exit(void) {
    int id = pid2id(get_pcb()->pid);
    srcrel(id);
    do_scheduler();
}

/*
 * ???
 */
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

/*
 * 等待某个进程，阻塞至对应进程的队列中
 */
int do_waitpid(pid_t pid) {
    int id = pid2id(pid);
    if (id == -1 || pcb[id].status == TASK_EXITED)
        return 1;

    spin_lock_acquire(&(pcb[id].wait_lock));
    do_block(&(get_pcb()->list), &(pcb[id].wait_list), &(pcb[id].wait_lock));
    spin_lock_release(&(pcb[id].wait_lock));
    return 0;
}