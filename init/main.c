#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define USER_ADDR 0x52000000
#define VERSION_BUF 50
#define task_info_addr 0x502001fc
#define KERNEL_STACK 0x50501000
#define num_sched1_tasks 7

uint16_t task_num;
int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
extern void ret_from_exception();//conflict

// Task info array
task_info_t tasks[TASK_MAXNUM];
task_info_t sched1_tasks[TASK_MAXNUM];


static void enter_app(uint32_t task_entrypoint)
{
    asm volatile(
        "jalr   %0\n\t"
        :
        :"r"(task_entrypoint)
        :"ra"
    );
}

/*static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}*/

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
    jmptab[WRITE]           = (long (*)())screen_write;
    jmptab[REFLUSH]         = (long (*)())screen_reflush;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    uint32_t task_info_block_phyaddr;
    uint32_t task_info_block_size;

    uint32_t task_info_block_id;
    uint32_t task_info_block_num;
    uint32_t task_info_block_offset;

    task_info_block_phyaddr = *(uint32_t *)(task_info_addr - 0x8);
    task_info_block_size    = *(uint32_t *)(task_info_addr - 0x4);
    task_num                = *(uint16_t *)(task_info_addr + 0x2);

    task_info_block_id      = task_info_block_phyaddr / SECTOR_SIZE;
    task_info_block_num     = (task_info_block_phyaddr + task_info_block_size) / SECTOR_SIZE- task_info_block_id + 1;
    task_info_block_offset  = task_info_block_phyaddr % SECTOR_SIZE;
    //得到task_info的一系列信息，下面从sd卡读到内存中

    bios_sd_read(TASK_MEM_BASE, task_info_block_num, task_info_block_id);
    memcpy(tasks, TASK_MEM_BASE + task_info_block_offset, task_info_block_size);
    //将task_info数组拷贝到tasks数组中

    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

static void init_sched1_tasks(void)
{
    memcpy(&sched1_tasks[0], &tasks[0], sizeof(task_info_t));
    memcpy(&sched1_tasks[1], &tasks[1], sizeof(task_info_t));
    memcpy(&sched1_tasks[2], &tasks[3], sizeof(task_info_t));
    memcpy(&sched1_tasks[3], &tasks[4], sizeof(task_info_t));
    memcpy(&sched1_tasks[4], &tasks[5], sizeof(task_info_t));
    //这里写的还非常的不完善！！！将task数组里面的某些用户程序的task_info_t选择性的按顺序写入sched1_tasks数组中
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    //准备将该进程运行时的一些需要写寄存器的值全部放入栈中
    pt_regs->regs[1] = (reg_t)entry_point;             //ra
    pt_regs->regs[2] = (reg_t)user_stack;              //sp
    pt_regs->regs[4] = (reg_t)pcb;                     //tp
    pt_regs->sepc    = (reg_t)entry_point;             //sepc
    pt_regs->sstatus = (reg_t)SR_SPIE;                 //sstatus

    pcb->kernel_sp -= sizeof(regs_context_t);
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    //初始化该进程控制块中的sp指针
    pt_switchto->regs[0] = (reg_t)ret_from_exception;    //ra
    pt_switchto->regs[1] = (reg_t)(pcb->kernel_sp);      //sp

    pcb->kernel_sp -= sizeof(switchto_context_t);
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int num_tasks; 
    
    // task1: sched1_tasks
    init_sched1_tasks();
    //先将sched1_tasks数组做一遍初始化
    for(num_tasks = 0; num_tasks < num_sched1_tasks; num_tasks++){
        //pcb[num_tasks].kernel_sp = KERNEL_STACK + (num_tasks + 1) * 0x1000;
        //pcb[num_tasks].user_sp = pcb[num_tasks].kernel_sp;
        pcb[num_tasks].kernel_sp = allocKernelPage(1) + PAGE_SIZE;
        pcb[num_tasks].user_sp   = allocUserPage(1) + PAGE_SIZE;
        list_add(&pcb[num_tasks].list, &ready_queue);
        pcb[num_tasks].pid = num_tasks + 1;
        pcb[num_tasks].status = TASK_READY;
        
        load_task_img(num_tasks);
        init_pcb_stack( pcb[num_tasks].kernel_sp, pcb[num_tasks].user_sp, 
                        tasks[num_tasks].task_entrypoint, &pcb[num_tasks]); 
    }
    
    current_running = &pid0_pcb;
    /* TODO: [p2-task1] remember to initialize 'current_running' */

}

static void init_syscall(void)
{
    syscall[SYSCALL_SLEEP]          = (long(*)())do_sleep;
    syscall[SYSCALL_LOCK_INIT]      = (long(*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]       = (long(*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]   = (long(*)())do_mutex_lock_release;
    syscall[SYSCALL_YIELD]          = (long(*)())do_scheduler;
    syscall[SYSCALL_WRITE]          = (long(*)())screen_write;
    syscall[SYSCALL_CURSOR]         = (long(*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]        = (long(*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]   = (long(*)())get_time_base;
    syscall[SYSCALL_GET_TICK]       = (long(*)())get_ticks;
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void)
{

    // Check whether .bss section is set to zero
    int input_valid;
    int buf_number = 0;
    int task_id = 0;
    //int check = bss_check();
    long c;
    //confilct

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    enable_interrupt();
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    

/*
    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);
    bios_putstr("$ ");
*/
    /*while (1){
        long c;
        if ((c = bios_getchar()) != -1)
            bios_putchar(c);
    }*/
/*
    while(1){
        if((c = bios_getchar()) != -1)
        {
            bios_putchar(c);

            if(c == '\t')
            {
                buf[buf_number] = '\0';
                for(task_id = 0;task_id < task_num;task_id++)
                {
                    input_valid = strcmp(buf,tasks[task_id].task_name);
                    if(input_valid == 0)
                        break;
                }

                if(input_valid == 0)
                {
                    load_task_img(task_id);
                    enter_app(tasks[task_id].task_entrypoint);
                    bios_putstr("$ ");
                }
                else
                    bios_putstr("invalid name!\n$ ");
                buf_number = 0;
            }/*
            /*else if(c == 0x8)
            {
                if(buf_number > 0)
                {
                    bios_putstr(" \b");
                    buf[--buf_number] = '\0';
                }
                else
                    bios_putstr("$ ");
            }*/
           /*else
                buf[buf_number++] = c;
        }
        task_id = 0;
    }
*/

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        //do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
