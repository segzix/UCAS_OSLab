#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/string.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/task.h>
#include <os/loader.h>
#include <os/net.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <pgtable.h>

pcb_t pcb[NUM_MAX_TASK];
tcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = 0xffffffc050900000;
const ptr_t pid1_stack = 0xffffffc0508ff000;
pcb_t pid0_pcb = {
    .pid = 0,
    .tid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .hart_mask = 0x1,
    .current_mask = 0x1,
    .pcb_name = "pid0",
    //每个核对应的都有可以跑的

    .pgdir = PGDIR_PA + KVA_OFFSET,
    .recycle = 0,
    .list = {NULL,NULL}, 
    .status = TASK_RUNNING
};
pcb_t pid1_pcb = {
    .pid = 1,
    .tid = 0,
    .kernel_sp = (ptr_t)pid1_stack,
    .user_sp = (ptr_t)pid1_stack,
    .hart_mask = 0x2,
    .current_mask = 0x2,
    .pcb_name = "pid1",
    //每个核对应的都有可以跑的

    .pgdir = PGDIR_PA + KVA_OFFSET,
    .recycle = 0,
    .list = {NULL,NULL}, 
    .status = TASK_RUNNING
};
//初始化内核进程pcb

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
spin_lock_t ready_spin_lock = {UNLOCKED};
spin_lock_t sleep_spin_lock = {UNLOCKED};

/* current running task PCB */
pcb_t ** current_running;
pcb_t * current_running_0;
pcb_t * current_running_1;
//在main.c中初始化为只想pid0_pcb的pcb指针！

/* global process id */
pid_t process_id = 1;
//初始进程pid号为1，为内核进程

void clean_temp_page(uint64_t pgdir_addr){
    PTE * pgdir = pgdir_addr;
    for(uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu){
        va &= VA_MASK;
        uint64_t vpn2 =
            va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);//根目录页中的临时映射清空
        pgdir[vpn2] = 0;
    }

}

void do_scheduler(void)
{   
    spin_lock_acquire(&ready_spin_lock);
    list_node_t* list_check;
    int cpu_hartmask;
    // bios_set_timer(get_ticks() + TIMER_INTERVAL);
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    cpu_hartmask = get_current_cpu_id() ? 0x2 : 0x1;
    check_sleeping();

    if((*current_running)->pid == 2){
        for(unsigned i = 0;i < NUM_MAX_TASK;i++){
            if((pcb[i].pid != 2) && pcb[i].recycle && pcb[i].status == TASK_EXITED && !pcb[i].tid){
                free_all_pagemapping(pcb[i].pgdir);
                free_all_pagetable(pcb[i].pgdir);
                pcb[i].recycle = 0;
            }
            //当前是shell,并且经过了exit需要被回收，同时不是子线程，同时shell的不允许被回收
        }
    }
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/
    pcb_t* prev_running = (*current_running);

    if((*current_running)->status == TASK_RUNNING){
        list_add(&((*current_running)->list), &ready_queue);
        (*current_running)->status = TASK_READY;
    } 
    //注意这里如果status为BLOCKED则不用执行上面的操作，因为已经加入到对应lock的block_queue中了

    list_check = ready_queue.next;
    (*current_running) = list_entry(ready_queue.next, pcb_t, list);
    while((cpu_hartmask & (*current_running)->hart_mask) == 0){
        list_check = list_check->next;
        (*current_running) = list_entry(list_check, pcb_t, list);
    }
    (*current_running)->current_mask = cpu_hartmask;

    list_del(&((*current_running)->list));
    (*current_running)->status = TASK_RUNNING;
    //将当前执行的进程对应的pcb加入ready队列之中，同时将ready队列的最前端进程取下来，由prev_running指向它。
    //注意将两个pcb的进程状态修改。

    process_id = (*current_running)->pid;
    //修改当前执行进程ID

    // list_node_t* list_debug = ready_queue.next;
    // printl("ready_queue: ");
    // while(list_debug != &ready_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s:%d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = sleep_queue.next;
    // printl("sleep_queue:");
    // while(list_debug != &sleep_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[0].mailbox_send_queue.next;
    // printl("mboxs[0].send_queue:");
    // while(list_debug != &mailboxs[0].mailbox_send_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[0].mailbox_recv_queue.next;
    // printl("mboxs[0].recv_queue:");
    // while(list_debug != &mailboxs[0].mailbox_recv_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[1].mailbox_send_queue.next;
    // printl("mboxs[1].send_queue:");
    // while(list_debug != &mailboxs[1].mailbox_send_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[1].mailbox_recv_queue.next;
    // printl("mboxs[1].recv_queue:");
    // while(list_debug != &mailboxs[1].mailbox_recv_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[2].mailbox_send_queue.next;
    // printl("mboxs[2].send_queue:");
    // while(list_debug != &mailboxs[2].mailbox_send_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // list_debug = mailboxs[2].mailbox_recv_queue.next;
    // printl("mboxs[2].recv_queue:");
    // while(list_debug != &mailboxs[2].mailbox_recv_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s %d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");

    // printl("current running:(%s %d)",(*current_running)->pcb_name,(*current_running)->pid);
    // printl("\n\n");   
    // printl("ready_queue:\n");
    // while(list_debug != &ready_queue){
    //     pcb_t* pcb_debug = list_entry(list_debug, pcb_t, list);
    //     printl("%s:%d  ",pcb_debug->pcb_name,pcb_debug->pid);
    //     list_debug = list_debug->next;
    // }
    // printl("\n");
    /*vt100_move_cursor(current_running->cursor_x, current_running->cursor_y);
    screen_cursor_x = current_running->cursor_x;
    screen_cursor_y = current_running->cursor_y;*/
    // 将当前执行进程的screen_cursor修改一下？

    // TODO: [p2-task1] Modify the current_running pointer.
    set_satp(SATP_MODE_SV39, (*current_running)->pid, kva2pa((*current_running)->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    local_flush_icache_all();
    
    spin_lock_release(&ready_spin_lock);

    switch_to(prev_running, (*current_running));
    // TODO: [p2-task1] switch_to current_running

}

// void do_thread_scheduler(void)
// {   
//     // TODO: [p2-task3] Check sleep queue to wake up PCBs

//     spin_lock_acquire(&ready_spin_lock);
//     current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
//     /************************************************************/
//     /* Do not touch this comment. Reserved for future projects. */
//     /************************************************************/
//     list_node_t* list_check;
//     pcb_t* prev_running = (*current_running);

//     if((*current_running)->status == TASK_RUNNING){
//         list_add(&((*current_running)->list), &ready_queue);
//         (*current_running)->status = TASK_READY;
//     } 
//     //注意这里如果status为BLOCKED则不用执行上面的操作，因为已经加入到对应lock的block_queue中了
//     list_check = ready_queue.next;
//     (*current_running) = list_entry(list_check, pcb_t, list);

//     while(((*current_running)->pid != prev_running->pid) || ((*current_running)->tid == 0) || ((*current_running)->tid ==  prev_running->tid)){
//         list_check = list_check->next;
//         (*current_running) = list_entry(list_check, pcb_t, list);
//     }
//     list_del(&((*current_running)->list));
//     (*current_running)->status = TASK_RUNNING;
//     //将当前执行的进程对应的pcb加入ready队列之中，同时将ready队列的最前端进程取下来，由prev_running指向它。
//     //注意将两个pcb的进程状态修改。

//     process_id = (*current_running)->pid;
//     //修改当前执行进程ID

//     /*vt100_move_cursor(current_running->cursor_x, current_running->cursor_y);
//     screen_cursor_x = current_running->cursor_x;
//     screen_cursor_y = current_running->cursor_y;*/
//     // 将当前执行进程的screen_cursor修改一下？

//     // TODO: [p2-task1] Modify the current_running pointer.
//     spin_lock_release(&ready_spin_lock);
    
//     switch_to(prev_running, (*current_running));
//     // TODO: [p2-task1] switch_to current_running

// }

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    spin_lock_acquire(&sleep_spin_lock);
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    (*current_running)->wakeup_time = get_timer() + sleep_time;
    printl("\nadd (%s %d) to sleep_queue\n",(*current_running)->pcb_name,(*current_running)->pid);
    do_block(&(*current_running)->list, &sleep_queue,&sleep_spin_lock);
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

pid_t do_exec(char *name, int argc, char *argv[])
{
    uintptr_t kva_user_stack;

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/
    for(int i=1;i<task_num;i++){
        // printl("%s %s\n",tasks[i].name,name);
        if(strcmp(tasks[i].task_name,name) == 0){
            for(int id=0; id < NUM_MAX_TASK; id++){
                if(pcb[id].status == TASK_EXITED){

                    pcb[id].recycle = 0;
                    pcb[id].pgdir = allocPage(1,1,0,1,id+2);//分配根目录页//这里的给出的用户映射的虚地址没有任何意义
                    //clear_pgdir(pcb[id].pgdir); //清空根目录页
                    share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
                    load_task_img(i,pcb[id].pgdir,id+2);//load进程并且为给进程建立好地址映射(这一步实际上包括了建立好除了根目录页的所有页表以及除了栈以外的所有映射)


                    pcb[id].kernel_sp  = allocPage(1,1,0,0,id+2) + 1 * PAGE_SIZE;//这里的给出的用户映射的虚地址没有任何意义
                    pcb[id].user_sp    = USER_STACK_ADDR;

                    kva_user_stack = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,1,id+2) + 1 * PAGE_SIZE;//比栈地址低的一张物理页
                    alloc_page_helper(pcb[id].user_sp - 2*PAGE_SIZE, pcb[id].pgdir,1,id+2);//比栈地址低的第二张物理页
                    // uintptr_t va = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir) + PAGE_SIZE;
                    //内核对应的映射到这张物理页的地址，后面对于该用户栈的操作全部通过内核映射表进行
                    //并且考虑到后面要加一个东西导致真实的
                    //这列应该可以直接用用户的也页表映射去访问
                    // pcb[id].kernel_sp  = allocKernelPage(1) + PAGE_SIZE;
                    // pcb[id].user_sp    = allocUserPage(1) +   PAGE_SIZE;
                    pcb[id].cursor_x   = 0;
                    pcb[id].cursor_y   = 0;
                    pcb[id].wakeup_time = 0;
                    pcb[id].truepid = id + 2;
                    pcb[id].pid = id + 2;
                    pcb[id].tid = 0;
                    pcb[id].thread_num = 0;
                    pcb[id].wait_list.prev = &pcb[id].wait_list;
                    pcb[id].wait_list.next = &pcb[id].wait_list;
                    pcb[id].kill = 0;
                    pcb[id].hart_mask = (*current_running)->hart_mask;

                    // clean_temp_page(pcb[id].pgdir);

                    for(int k = 0;k < TASK_LOCK_MAX;k++){
                        pcb[id].mutex_lock_key[k] = 0;
                    }

                    memcpy((void*)pcb[id].pcb_name, (void*)tasks[i].task_name, 32);
                    // load_task_img(tasks[i].name);
                    init_pcb_stack( pcb[id].kernel_sp,kva_user_stack,
                                    tasks[i].task_entrypoint,&pcb[id],argc,argv);
                    // printl("%d\n",pcb[id].user_sp);


                    list_add(&(pcb[id].list),&ready_queue);
                    pcb[id].status     = TASK_READY;
                    num_tasks++;
                    return pcb[id].pid;
                }
            }
        }
    }
    return 0;
}




void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg){
    uintptr_t kva_user_stack;
    // int id = tasknum;
    // pcb[id].pid = id + 1;
    // pcb[id].kernel_sp = allocKernelPage(1) + 1 * PAGE_SIZE;
    // pcb[id].user_sp   = allocUserPage(1) +   1 * PAGE_SIZE;
    // pcb[id].status = TASK_READY;
    // pcb[id].cursor_x = 0;
    // pcb[id].cursor_y = 0;
    // pcb[id].wakeup_time = 0;
    // init_pcb_stack(pcb[id].kernel_sp,pcb[id].user_sp,entrypoint,pcb+id,0,NULL);
    // list_add(&(pcb[id].list),&ready_queue);
    current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    int id;
    for(id = 0; id < NUM_MAX_TASK; id++){
        if(pcb[id].status == TASK_EXITED){
            (*current_running)->thread_num ++;
            pcb[id].tid = (*current_running)->thread_num;

            pcb[id].recycle = 0;
            pcb[id].pgdir = (*current_running)->pgdir;

            pcb[id].kernel_sp  = allocPage(1,1,0,0,id+2) + 1 * PAGE_SIZE;//这里的给出的用户映射的虚地址没有任何意义
            pcb[id].user_sp    = USER_STACK_ADDR + 2 * PAGE_SIZE * pcb[id].tid;//必须是分配两页用户栈

            kva_user_stack = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,1,id+2) + 1 * PAGE_SIZE;//比栈地址低的一张物理页
            alloc_page_helper(pcb[id].user_sp - 2*PAGE_SIZE, pcb[id].pgdir,1,id+2);//比栈地址低的第二张物理页
            // uintptr_t va = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir) + PAGE_SIZE;
            //内核对应的映射到这张物理页的地址，后面对于该用户栈的操作全部通过内核映射表进行
            //并且考虑到后面要加一个东西导致真实的
            //这列应该可以直接用用户的也页表映射去访问
            // pcb[id].kernel_sp  = allocKernelPage(1) + PAGE_SIZE;
            // pcb[id].user_sp    = allocUserPage(1) +   PAGE_SIZE;
            pcb[id].cursor_x   = 0;
            pcb[id].cursor_y   = 0;
            pcb[id].wakeup_time = 0;
            pcb[id].truepid = (*current_running)->truepid;
            pcb[id].pid = id + 2;
            pcb[id].thread_num = 0;
            pcb[id].wait_list.prev = &pcb[id].wait_list;
            pcb[id].wait_list.next = &pcb[id].wait_list;
            pcb[id].kill = 0;
            pcb[id].hart_mask = (*current_running)->hart_mask;

            // clean_temp_page(pcb[id].pgdir);

            for(int k = 0;k < TASK_LOCK_MAX;k++){
                pcb[id].mutex_lock_key[k] = 0;
            }

            ptr_t entry_point = (ptr_t)thread_entrypoint;

            memcpy((void*)pcb[id].pcb_name, (void*)(*current_running)->pcb_name, 32);
            // load_task_img(tasks[i].name);
            init_tcb_stack( pcb[id].kernel_sp,pcb[id].user_sp,
                            entry_point,&pcb[id],(uint64_t)arg);//这里直接传用户的虚地址即可
            // printl("%d\n",pcb[id].user_sp);


            list_add(&(pcb[id].list),&ready_queue);
            *thread = pcb[id].pid;

            pcb[id].status     = TASK_READY;

            num_tasks++;
            return ;
        }
    }
    return;
}

void do_exit(void)
{
    int i;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    (*current_running)->status = TASK_EXITED;
    (*current_running)->recycle = 1;

    for(i = 0;i < TASK_LOCK_MAX;i++){
        if((*current_running)->mutex_lock_key[i] != 0)
        {
            do_mutex_lock_release((*current_running)->mutex_lock_key[i]);
        }
    }

    // list_del(&(current_running->list));

    spin_lock_acquire(&((*current_running)->wait_lock));
    while(((*current_running)->wait_list).next != &((*current_running)->wait_list))
        do_unblock(((*current_running)->wait_list).next);
    spin_lock_release(&((*current_running)->wait_lock));

    do_scheduler();
}   
int do_kill(pid_t pid)
{
    int id;
    for(id = 0;id < NUM_MAX_TASK;id++){
        if((pcb[id].pid == pid) && (pcb[id].status != TASK_EXITED))
            break;
    }

    if(id == NUM_MAX_TASK)
        return 0;
    else{
        pcb[id].kill = 1;
        return 1;
    } 
}
int do_waitpid(pid_t pid)
{
    int id;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    for(id = 0;id < NUM_MAX_TASK;id++){
        if((pcb[id].pid == pid) && (pcb[id].status != TASK_EXITED))
            break;
    }

    if(id == NUM_MAX_TASK)
        return 0;
    else{
        spin_lock_acquire(&(pcb[id].wait_lock));
        do_block(&((*current_running)->list),&(pcb[id].wait_list),&(pcb[id].wait_lock));
        spin_lock_release(&(pcb[id].wait_lock));

        return 1;
    } 
}

pid_t do_getpid(){
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    return (*current_running)->pid;
}

// void do_thread_create(uint64_t addr,uint64_t thread_id)
// {
//     current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
//     tcb[num_threads].kernel_sp = allocKernelPage(1) + PAGE_SIZE;
//     tcb[num_threads].user_sp   = allocUserPage(1)   + PAGE_SIZE;
//     list_add(&tcb[num_threads].list, &ready_queue);
//     tcb[num_threads].pid = (*current_running)->pid;
//     tcb[num_threads].tid = thread_id+1;
//     tcb[num_threads].status = TASK_READY;
        
//     init_tcb_stack( tcb[num_threads].kernel_sp, tcb[num_threads].user_sp, 
//                     addr, thread_id, &tcb[num_threads]); 
//     num_threads++;
// }

int do_process_show()
{
    int i = 0;
    int add_lines = 1;
    printk("\n[Process Table]: \n");
    while(i < NUM_MAX_TASK){
        switch (pcb[i].status) {
            case TASK_RUNNING:
                printk("[%d] NAME : %s  PID : %d TID : %d TRUEPID : %d STATUS : TASK_RUNNING ",i,pcb[i].pcb_name,pcb[i].pid,pcb[i].tid,pcb[i].truepid);
                if(pcb[i].current_mask == 0x1)
                    printk("Running on core 0\n");
                else if(pcb[i].current_mask == 0x2)
                    printk("Running on core 1\n");
                printk("hart_mask : %d\n",pcb[i].hart_mask);
                add_lines+=2;
                break;
            case TASK_READY:
                printk("[%d] NAME : %s  PID : %d TID : %d TRUEPID : %d STATUS : TASK_READY\n",i,pcb[i].pcb_name,pcb[i].pid,pcb[i].tid,pcb[i].truepid);
                printk("hart_mask : %d\n",pcb[i].hart_mask);
                add_lines+=2;
                break;
            case TASK_BLOCKED:
                printk("[%d] NAME : %s  PID : %d TID : %d TRUEPID : %d STATUS : TASK_BLOCKED\n",i,pcb[i].pcb_name,pcb[i].pid,pcb[i].tid,pcb[i].truepid);
                printk("hart_mask : %d\n",pcb[i].hart_mask);
                add_lines+=2;
                break;
            default:
                break;
        }

        i++;
    }

    return add_lines;
}


void do_task_set_p(pid_t pid, int mask){
    int id = pid - 2;
    if(id < 0 || id >= NUM_MAX_TASK ){
        printk("> [Taskset] Pid number not in use. \n\r");
        return;
    }
    if(mask > 3 || mask < 1){
        printk("> [Taskset] Core number not in use. \n\r");
        return;
    }
    if(pcb[id].status == TASK_EXITED)return;
    pcb[id].hart_mask = mask;
}

int do_task_set(int mask,char *name, int argc, char *argv[]){
    int pid = do_exec(name,argc,argv);
    do_task_set_p(pid,mask);
    return pid;
}