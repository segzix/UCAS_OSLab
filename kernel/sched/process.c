#include "os/sched.h"
#include "printk.h"
#include <csr.h>
#include <hash.h>
#include <os/mm.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>

extern void ret_from_exception();
uint32_t genpid = 2;

void init_shell(void) {
    int taskid = 0;
    strcpy(pcb[0].pcb_name, "shell");

    /*pid,tid和父进程的pid*/
    pcb[0].pid = genpid++;
    pcb[0].tid = 0;
    pcb[0].ppid = get_pcb()->pid;

    init_pcb_mm(0, taskid, NOTFORK);

    pcb[0].recycle = 0;
    pcb[0].cursor_x = 0;
    pcb[0].cursor_y = 0;
    pcb[0].wakeup_time = 0;
    pcb[0].wait_list.prev = &pcb[0].wait_list;
    pcb[0].wait_list.next = &pcb[0].wait_list;
    pcb[0].kill = 0;
    pcb[0].hart_mask = 0x3;
    pcb[0].cpu = 0x0;
    pcb[0].pwd = 0;
    strcpy(pcb[0].pwd_dir, "/");
    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[0].mutex_lock_key[k] = 0;
    }

    list_add(&pcb[0].list, &ready_queue);
    pcb[0].status = TASK_READY;
    /* TODO: [p2-task1] remember to initialize 'current_running' */
}

pid_t do_exec(char *name, int argc, char *argv[]) {
    int taskid = -1;
    int id = -1;

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    /* TODO [P3-TASK1] exec exit kill waitpid ps*/

    /*将用户的arg参数先拷贝到内核中，以免换页换出*/
    kernel_argc = argc;
    for (int i = 0; i < argc; i++) {
        memcpy((void *)kernel_arg[i], (void *)argv[i], strlen(argv[i]) + 1);
        kernel_argv[i] = kernel_arg[i];
    }

    /*找寻taskid*/
    for (int i = 1; i < task_num; i++)
        if (strcmp(tasks[i].task_name, name) == 0) {
            taskid = i;
            break;
        }
    if (taskid == -1)
        return -1;

    /*找寻pcbid*/
    for (int i = hash(genpid, NUM_MAX_TASK), j = 0; j < NUM_MAX_TASK;
         i = (i + 1) % NUM_MAX_TASK, j++)
        if (pcb[i].status == TASK_EXITED) {
            id = i;
            memcpy((void *)pcb[id].pcb_name, (void *)tasks[taskid].task_name, 32);
            break;
        }
    if (id == -1)
        return -1;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].ppid = (*current_running)->pid;
    pcb[id].tid = 0;

    init_pcb_mm(id, taskid, NOTFORK);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = (*current_running)->hart_mask;
    pcb[id].pwd = (*current_running)->pwd;
    // pwd_dir只用管shell显示，这里不做拷贝

    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[id].mutex_lock_key[k] = 0;
    }
    // printl("%d\n",pcb[id].user_sp);

    list_add(&(pcb[id].list), &ready_queue);
    pcb[id].status = TASK_READY;
    return pcb[id].pid;
}

void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg) {
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    int id = -1;

    /*找寻pcbid*/
    for (int i = hash(genpid, NUM_MAX_TASK), j = 0; j < NUM_MAX_TASK;
         i = (i + 1) % NUM_MAX_TASK, j++)
        if (pcb[i].status == TASK_EXITED) {
            id = i;
            memcpy((void *)pcb[id].pcb_name, (void *)(*current_running)->pcb_name, 32);
            break;
        }
    if (id == -1)
        return;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].tid = (*current_running)->tid + 1;
    pcb[id].ppid = (*current_running)->pid;

    init_tcb_mm(id, thread_entrypoint, arg);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = (*current_running)->hart_mask;

    // clean_temp_page(pcb[id].pgdir);

    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[id].mutex_lock_key[k] = 0;
    }

    list_add(&(pcb[id].list), &ready_queue);
    *thread = pcb[id].pid;

    pcb[id].status = TASK_READY;
    return;
}

/*
 * 进程fork
 */
pid_t do_fork() {
    int id = -1;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;

    /*找寻pcbid*/
    for (int i = hash(genpid, NUM_MAX_TASK), j = 0; j < NUM_MAX_TASK;
         i = (i + 1) % NUM_MAX_TASK, j++)
        if (pcb[i].status == TASK_EXITED) {
            id = i;
            memcpy((void *)pcb[id].pcb_name, (void *)(*current_running)->pcb_name, 32);
            break;
        }
    if (id == -1)
        return -1;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].tid = 0;
    pcb[id].ppid = (*current_running)->pid;

    //这里taskid无用
    init_pcb_mm(id, 0, FORK);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = (*current_running)->hart_mask;

    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[id].mutex_lock_key[k] = 0;
    }

    list_add(&(pcb[id].list), &ready_queue);
    pcb[id].status = TASK_READY;

    return pcb[id].pid;
}

/************************************************************/
void init_pcb_stack(ptr_t kernel_stack, ptr_t kva_user_stack, ptr_t entry_point, pcb_t *pcb,
                    int argc, char *argv[]) {
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    /*
     * kva_user_stack与user_sp对应的是同一个物理页面的内核与用户虚地址，要区别对待，同时做减法
     */
    uintptr_t argv_base = kva_user_stack - sizeof(uintptr_t) * (argc + 1);
    pcb->user_sp = pcb->user_sp - sizeof(uintptr_t) * (argc + 1);

    /*初始化ra,tp,a0,a1寄存器，传递命令行参数长度和地址*/
    pt_regs->regs[1] = (reg_t)entry_point;
    pt_regs->regs[4] = (reg_t)pcb;
    pt_regs->regs[10] = (reg_t)argc;
    pt_regs->regs[11] = argv_base;

    /*初始化sepc,sstatus,sbadaddr,scause寄存器*/
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = (reg_t)((SR_SPIE & ~SR_SPP) | SR_SUM);
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;

    /*初始化上下文中的ra寄存器,sp指针*/
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)(pt_regs);

    //对命令行参数进行处理
    uintptr_t user_sp_now = argv_base;
    uintptr_t *argv_ptr = (uintptr_t *)argv_base;
    for (int i = 0; i < argc; i++) {
        // sp--
        uint32_t len = strlen(argv[i]);
        user_sp_now = user_sp_now - len - 1;
        pcb->user_sp = pcb->user_sp - len - 1;

        //设置栈
        (*argv_ptr) = user_sp_now;
        strcpy((char *)user_sp_now, argv[i]);
        argv_ptr++;
    }
    (*argv_ptr) = 0;
    pcb->user_sp &= (~0xf);

    /*初始化trapframe中用户sp指针，进程控制块中的内核sp指针*/
    pt_regs->regs[2] = (reg_t)pcb->user_sp; // sp
    pcb->kernel_sp = (reg_t)pt_switchto;
}

void init_tcb_stack(ptr_t kernel_stack, ptr_t kva_user_stack, ptr_t entry_point, tcb_t *tcb,
                    void *arg) {
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    /*初始化trapframe中的ra,sp,tp,a0,寄存器，传递arg指针参数*/
    pt_regs->regs[1] = (reg_t)entry_point;
    pt_regs->regs[2] = (reg_t)kva_user_stack;
    pt_regs->regs[4] = (reg_t)tcb;
    pt_regs->regs[10] = (reg_t)arg;

    /*初始化sepc,sstatus,sbadaddr,scause寄存器*/
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->sstatus = (reg_t)((SR_SPIE & ~SR_SPP) | SR_SUM);
    pt_regs->sbadaddr = 0;
    pt_regs->scause = 0;

    /*初始化上下文中的spra寄存器,sp指针*/
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)(pt_regs);

    /*初始化进程控制块中的内核sp指针*/
    tcb->kernel_sp = (uint64_t)pt_switchto;
}

void do_task_set_p(pid_t pid, int mask) {
    int id = pid - 2;
    if (id < 0 || id >= NUM_MAX_TASK) {
        printk("> [Taskset] Pid number not in use. \n\r");
        return;
    }
    if (mask > 3 || mask < 1) {
        printk("> [Taskset] Core number not in use. \n\r");
        return;
    }
    if (pcb[id].status == TASK_EXITED)
        return;
    pcb[id].hart_mask = mask;
}

int do_task_set(int mask, char *name, int argc, char *argv[]) {
    int pid = do_exec(name, argc, argv);
    do_task_set_p(pid, mask);
    return pid;
}

pid_t do_getpid() {
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    return (*current_running)->pid;
}

int do_process_show() {
    int i = 0;
    int add_lines = 1;
    printk("\n[Process Table]: \n");
    while (i < NUM_MAX_TASK) {
        switch (pcb[i].status) {
        case TASK_RUNNING:
            printk("[%d] NAME : %s  PID : %d TID : %d STATUS : TASK_RUNNING ", i, pcb[i].pcb_name,
                   pcb[i].pid, pcb[i].tid);
            if (pcb[i].cpu == 0x1)
                printk("Running on core 0\n");
            else if (pcb[i].cpu == 0x2)
                printk("Running on core 1\n");
            printk("hart_mask : %d\n", pcb[i].hart_mask);
            add_lines += 2;
            break;
        case TASK_READY:
            printk("[%d] NAME : %s  PID : %d TID : %d STATUS : TASK_READY\n", i, pcb[i].pcb_name,
                   pcb[i].pid, pcb[i].tid);
            printk("hart_mask : %d\n", pcb[i].hart_mask);
            add_lines += 2;
            break;
        case TASK_BLOCKED:
            printk("[%d] NAME : %s  PID : %d TID : %d STATUS : TASK_BLOCKED\n", i, pcb[i].pcb_name,
                   pcb[i].pid, pcb[i].tid);
            printk("hart_mask : %d\n", pcb[i].hart_mask);
            add_lines += 2;
            break;
        default:
            break;
        }

        i++;
    }

    return add_lines;
}