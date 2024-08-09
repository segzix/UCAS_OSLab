#include "os/sched.h"
#include "printk.h"
#include <csr.h>
#include <hash.h>
#include <os/mm.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>

uint32_t genpid = 2;

/************************************************************/
/*
 * shell(所有用户态进程的父进程初始化)
 */
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

/*
 * 创建进程
 */
pid_t do_exec(char *name, int argc, char *argv[]) {
    int taskid = -1;
    int id = -1;

    pcb_t* current_running = get_pcb();
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
        return -2;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].ppid = current_running->pid;
    pcb[id].tid = 0;

    init_pcb_mm(id, taskid, NOTFORK);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = current_running->hart_mask;
    pcb[id].pwd = current_running->pwd;
    // pwd_dir只用管shell显示，这里不做拷贝

    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[id].mutex_lock_key[k] = 0;
    }
    // printl("%d\n",pcb[id].user_sp);

    list_add(&(pcb[id].list), &ready_queue);
    pcb[id].status = TASK_READY;
    return pcb[id].pid;
}

/*
 * 创建线程
 */
void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg) {
    pcb_t* current_running = get_pcb();
    int id = -1;

    /*找寻pcbid*/
    for (int i = hash(genpid, NUM_MAX_TASK), j = 0; j < NUM_MAX_TASK;
         i = (i + 1) % NUM_MAX_TASK, j++)
        if (pcb[i].status == TASK_EXITED) {
            id = i;
            memcpy((void *)pcb[id].pcb_name, (void *)current_running->pcb_name, 32);
            break;
        }
    if (id == -1)
        return;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].tid = current_running->tid + 1;
    pcb[id].ppid = current_running->pid;

    init_tcb_mm(id, thread_entrypoint, arg);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = current_running->hart_mask;

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
    pcb_t* current_running = get_pcb();

    /*找寻pcbid*/
    for (int i = hash(genpid, NUM_MAX_TASK), j = 0; j < NUM_MAX_TASK;
         i = (i + 1) % NUM_MAX_TASK, j++)
        if (pcb[i].status == TASK_EXITED) {
            id = i;
            memcpy((void *)pcb[id].pcb_name, (void *)current_running->pcb_name, 32);
            break;
        }
    if (id == -1)
        return -1;

    /*pid,tid和父进程的pid*/
    pcb[id].pid = genpid++;
    pcb[id].tid = 0;
    pcb[id].ppid = current_running->pid;

    //这里taskid无用
    init_pcb_mm(id, 0, FORK);

    pcb[id].recycle = 0;
    pcb[id].cursor_x = 0;
    pcb[id].cursor_y = 0;
    pcb[id].wakeup_time = 0;
    pcb[id].wait_list.prev = &pcb[id].wait_list;
    pcb[id].wait_list.next = &pcb[id].wait_list;
    pcb[id].kill = 0;
    pcb[id].hart_mask = current_running->hart_mask;

    for (int k = 0; k < TASK_LOCK_MAX; k++) {
        pcb[id].mutex_lock_key[k] = 0;
    }

    list_add(&(pcb[id].list), &ready_queue);
    pcb[id].status = TASK_READY;

    return pcb[id].pid;
}

/************************************************************/
void do_task_set_p(pid_t pid, int mask, char* buf) {
    int id = pid2id(pid);
    if (id==-1){
        sprintk(buf, "> [Taskset] PID number not in use. \n");
        return;
    }
    if (mask > 3 || mask < 1) {
        sprintk(buf, "> [Taskset] Core number not in use. \n");
        return;
    }
    if (pcb[id].status == TASK_EXITED){
        sprintk(buf, "> [Taskset] Process has exited. \n");
        return;
    }
    pcb[id].hart_mask = mask;
}

int do_task_set(int mask, char *name, int argc, char *argv[]) {
    int pid = do_exec(name, argc, argv);
    if(pid < 0)
        return pid;
    do_task_set_p(pid, mask, NULL);
    return pid;
}

pid_t do_getpid() {
    return get_pcb()->pid;
}

int do_process_show(char* buf) {
    int i = 0;
    int add_lines = 1;
    sprintk(buf, "[Process Table]: \n");
    while (i < NUM_MAX_TASK) {
        switch (pcb[i].status) {
        case TASK_RUNNING:
            sprintk(buf, "[%d] NAME: %s PID: %d TID: %d STATUS: RUNNING ", i, pcb[i].pcb_name,
                   pcb[i].pid, pcb[i].tid);
            if (pcb[i].cpu == 0x1)
                sprintk(buf, "Running on core 0\n");
            else if (pcb[i].cpu == 0x2)
                sprintk(buf, "Running on core 1\n");
            break;
        case TASK_READY:
            sprintk(buf, "[%d] NAME: %s PID: %d TID: %d STATUS: READY\n", i, pcb[i].pcb_name, pcb[i].pid,
                   pcb[i].tid);
            break;
        case TASK_BLOCKED:
            sprintk(buf, "[%d] NAME: %s PID: %d TID: %d STATUS: BLOCKED\n", i, pcb[i].pcb_name,
                   pcb[i].pid, pcb[i].tid);
            break;
        default:
            break;
        }
        if (pcb[i].status != TASK_EXITED) {
            sprintk(buf, "hart_mask : 0x%x\n", pcb[i].hart_mask);
            add_lines += 2;
        }
        i++;
    }

    return add_lines;
}