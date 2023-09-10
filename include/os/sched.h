/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
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

#include "os/net.h"
#include "pgtable.h"
#include <os/lock.h>
#include <type.h>
#include <os/list.h>

#define NUM_MAX_TASK 32
#define TASK_LOCK_MAX 16

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    char  pcb_name[32];//for debugging


    /* process id */
    pid_t truepid;//这个字段用来标记如果是线程，它是由哪个线程启的
    pid_t pid;
    tid_t tid;
    unsigned thread_num;

    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;

    list_head wait_list;
    spin_lock_t wait_lock;
    int mutex_lock_key[TASK_LOCK_MAX];

    int kill;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    /* mask 
     * 0x01 core 0
     * 0x02 core 1
     * 0x03 core 0/1
     */
    int hart_mask;
    int current_mask;

    /* pgdir */
    PTE* pgdir;
    uintptr_t heap;
    unsigned recycle;

    uint32_t pwd;
    char pwd_dir[64];//用来记录当前工作路径

    netStream netstream;
} pcb_t,tcb_t;

/*main.c中定义的变量和函数*/
extern uint16_t num_tasks;
extern uint16_t num_threads;
extern uint16_t task_num;
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb,int argc, char *argv[]);
void init_tcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, 
    tcb_t *tcb,int arg);




/* ready queue to run */
extern list_head ready_queue;
extern spin_lock_t ready_spin_lock;

/* sleep queue to be blocked in */
extern list_head sleep_queue;
extern spin_lock_t sleep_spin_lock;

/* current running task PCB */
// extern pcb_t * volatile current_running;
pcb_t ** current_running;
pcb_t * current_running_0;
pcb_t * current_running_1;

extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern tcb_t tcb[NUM_MAX_TASK];

extern pcb_t pid0_pcb;
extern pcb_t pid1_pcb;
extern const ptr_t pid0_stack;
extern const ptr_t pid1_stack;

void clean_temp_page(uint64_t pgdir_addr);
extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
// void do_thread_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock);
void do_unblock(list_node_t *);

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif

extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern int do_process_show();
extern pid_t do_getpid();

void do_task_set_p(pid_t pid, int mask);
int do_task_set(int mask,char *name, int argc, char *argv[]);

// void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg);
void do_thread_create(pid_t *thread, void *thread_entrypoint, void *arg);
pcb_t* get_pcb();
/************************************************************/

#endif
