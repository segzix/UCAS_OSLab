/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include "hash.h"
#include "os/lock.h"
#include "os/net.h"
#include "pgtable.h"

#define NUM_MAX_TASK 32
#define TASK_LOCK_MAX 16
extern uint16_t task_num;

/** used to save register infomation */
typedef struct regs_context {
    /** Saved main processor registers.*/
    reg_t regs[32];

    /** Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/** used to save register infomation in switch_to */
typedef struct switchto_context {
    /** Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

enum FORK {
    FORK,
    NOTFORK,
};

/** Process Control Block */
typedef struct pcb {
    /** register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    char pcb_name[32]; // for debugging

    /** process id */
    pid_t pid;
    tid_t tid;
    pid_t ppid;

    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /** previous, next pointer */
    list_node_t list;

    list_head wait_list;
    spin_lock_t wait_lock;
    int mutex_lock_key[TASK_LOCK_MAX];

    int kill;

    /** BLOCK | READY | RUNNING */
    task_status_t status;

    /** cursor position */
    int cursor_x;
    int cursor_y;

    /** time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    /** mask
     * 0x01 core 0
     * 0x02 core 1
     * 0x03 core 0/1
     */
    uint8_t hart_mask;
    uint8_t cpu;
    uint8_t cputime;
    uint8_t prior;

    /** pgdir */
    PTE *pgdir;
    uintptr_t heap;
    unsigned recycle;

    uint32_t pwd;
    char pwd_dir[64]; //用来记录当前工作路径

    netStream netstream;
} pcb_t, tcb_t;

/** ready queue to run */
extern list_head ready_queue;
extern spin_lock_t ready_spin_lock;
#ifdef MLFQ
extern list_node_t ready_queues[];
#endif

/** sleep queue to be blocked in */
extern list_head sleep_queue;
extern spin_lock_t sleep_spin_lock;

/** current running task PCB */
pcb_t *current_running_0;
pcb_t *current_running_1;
extern pcb_t pid0_pcb;
extern pcb_t pid1_pcb;
pcb_t pcb[NUM_MAX_TASK];

/**sched function for shedular*/
#ifdef MLFQ
pcb_t *MLFQsched(int curcpu, pcb_t *currunning);
void MLFQupprior();
void init_queues();
#else
pcb_t *RRsched(int curcpu, pcb_t *currunning);
#endif
extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);
void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock);
void do_unblock(list_node_t *);

/**proc function for process*/
void clean_temp_page(uint64_t pgdir_addr);
void srcrel(int id);
// pcb与tcb的初始化
void init_pcb_stack(ptr_t kernel_stack, ptr_t kva_user_stack, ptr_t entry_point, pcb_t *pcb,
                    int argc, char *argv[]);
void init_tcb_stack(ptr_t kernel_stack, ptr_t kva_user_stack, ptr_t entry_point, tcb_t *tcb,
                    void *arg);
void init_pcb_mm(int id, int taskid, enum FORK fork);
void init_tcb_mm(int id, void *thread_entrypoint, void *arg);

/*************************************************************/
/** exec exit kill waitpid ps */
void init_shell(void);
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg);
void do_exit(void);
int do_kill(pid_t pid);
int do_waitpid(pid_t pid);
int do_process_show(char *buf);

// pid&&id
pid_t do_getpid();
static inline int pid2id(int pid) {
    int id = hash(pid, NUM_MAX_TASK);
    for (int i = 0; i < NUM_MAX_TASK; i++, id = (id + 1) % NUM_MAX_TASK)
        if (pid == pcb[id].pid)
            return id;
    return -1;
};

// cpu task
void do_task_set_p(pid_t pid, int mask, char *buf);
int do_task_set(int mask, char *name, int argc, char *argv[]);

/*************************************************************/

/**kernel mem for argc&&argv*/
int kernel_argc;
char kernel_arg[5][200];
char *kernel_argv[5];

#endif
