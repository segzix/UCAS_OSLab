/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
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
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <pgtable.h>
#include <os/net.h>

#define USER_ADDR 0xffffffc052000000
#define VERSION_BUF 50
#define task_info_addr 0xffffffc0502001fc//对于block中的数据
// #define KERNEL_STACK 0xffffffc050501000

uint16_t task_num;
uint16_t num_threads;
//uint16_t swap_block_id;//换进磁盘的地址
uint16_t num_tasks;//记录已经有过的进程数
int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
extern void ret_from_exception();//conflict

// Task info array
task_info_t tasks[TASK_MAXNUM];



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

    //swap_block_id = task_info_block_id + task_info_block_num;

    bios_sd_read(TASK_MEM_BASE, task_info_block_num, task_info_block_id);
    memcpy(tasks, TASK_MEM_BASE + task_info_block_offset, task_info_block_size);
    //将task_info数组拷贝到tasks数组中

    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

/************************************************************/
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb,int argc, char *argv[])//这里的userstack是虚地址，并且是内核平坦映射下的虚地址
{
    int i;
    
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    uintptr_t argv_base = user_stack - sizeof(uintptr_t)*(argc + 1);
    pcb->user_sp = pcb->user_sp - sizeof(uintptr_t)*(argc + 1);

    //准备将该进程运行时的一些需要写寄存器的值全部放入栈中
    pt_regs->regs[1]    = (reg_t)entry_point;             //ra
    pt_regs->regs[4]    = (reg_t)pcb;                     //tp
    pt_regs->regs[10]   = (reg_t)argc;                    //a0寄存器，命令行参数长度
    // pt_regs->regs[11]   = argv_base;
    pt_regs->regs[11]   = pcb->user_sp;//这里是传的命令行参数的地址(后面user_sp还得减)
    pt_regs->sepc       = (reg_t)entry_point;             //sepc
    pt_regs->sstatus    = (reg_t)((SR_SPIE & ~SR_SPP) | SR_SUM);     //sstatus
    pt_regs->sbadaddr   = 0;
    pt_regs->scause     = 0;

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
    pt_switchto->regs[1] = (reg_t)(pt_regs);      //sp

    uint64_t user_sp_now = argv_base;
    uintptr_t * argv_ptr = (ptr_t)argv_base;
    for(i=0; i<argc; i++){
        uint32_t len = strlen(argv[i]);
        user_sp_now = user_sp_now - len - 1;
        pcb->user_sp = pcb->user_sp - len - 1;

        (*argv_ptr) = pcb->user_sp;
        strcpy((char*)user_sp_now,argv[i]);
        argv_ptr++;
    }
    (*argv_ptr) = 0;

    //  uint64_t user_sp_va = argv_va_base;
    //                 uint64_t user_sp_pa = argv_pa_base;
    //                 uintptr_t * argv_ptr = (ptr_t)argv_va_base;
    //                 for(int i=0; i<argc; i++){
    //                     uint32_t len = strlen(argv[i]);
    //                     user_sp_va = user_sp_va - len - 1;
    //                     user_sp_pa = user_sp_pa - len - 1;
    //                     (*argv_ptr) = user_sp_pa;
    //                     strcpy(user_sp_va,argv[i]);
    //                     argv_ptr ++;
    //                 }
    // int user_stack_size =  user_stack - user_sp_now;
    // int siz_alignment = ((user_stack_size/16) + !(user_stack_size%128==0))*16;
    // user_sp_now = user_stack - siz_alignment;

    // user_sp_now = user_sp_now & (~0xf);
    pcb->user_sp = pcb->user_sp & (~0xf);

    pt_regs->regs[2]  = (reg_t)pcb->user_sp;     //sp
    // pcb->user_sp=(reg_t)user_sp_now;

    pcb->kernel_sp = (reg_t)pt_switchto;
}

void init_tcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    tcb_t *tcb,int arg)
{
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    //准备将该进程运行时的一些需要写寄存器的值全部放入栈中
    pt_regs->regs[1]    = (reg_t)entry_point;             //ra
    pt_regs->regs[2]    = (reg_t)user_stack;              //sp//注意这个地方由于不需要放命令行参数因此直接将用户虚地址放进来
    pt_regs->regs[4]    = (reg_t)tcb;                     //tp
    pt_regs->regs[10]   = (reg_t)arg;                     //a0传参
    pt_regs->sepc       = (reg_t)entry_point;             //sepc
    pt_regs->sstatus    = (reg_t)((SR_SPIE & ~SR_SPP) | SR_SUM);     //sstatus
    pt_regs->sbadaddr   = 0;
    pt_regs->scause     = 0;

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
    pt_switchto->regs[1] = (reg_t)(pt_regs);      //sp

    tcb->kernel_sp = pt_switchto;
}

// static void init_pcb(void)
// {
//     /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

//     int num_tasks; 
    
//     // task1: sched1_tasks
//     //init_sched1_tasks();
//     //先将sched1_tasks数组做一遍初始化
//     for(num_tasks = 0; num_tasks < task_num; num_tasks++){
//         //pcb[num_tasks].kernel_sp = KERNEL_STACK + (num_tasks + 1) * 0x1000;
//         //pcb[num_tasks].user_sp = pcb[num_tasks].kernel_sp;
//         pcb[num_tasks].kernel_sp = allocKernelPage(1) + PAGE_SIZE;
//         pcb[num_tasks].user_sp   = allocUserPage(1) + PAGE_SIZE;
//         list_add(&pcb[num_tasks].list, &ready_queue);
//         pcb[num_tasks].pid = num_tasks + 1;
//         pcb[num_tasks].tid = 0;
//         pcb[num_tasks].status = TASK_READY;
        
//         load_task_img(num_tasks);
//         init_pcb_stack( pcb[num_tasks].kernel_sp, pcb[num_tasks].user_sp, 
//                         tasks[num_tasks].task_entrypoint, &pcb[num_tasks]); 
//     }
    
//     current_running = &pid0_pcb;
//     /* TODO: [p2-task1] remember to initialize 'current_running' */

// }

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int num_pcbs; 
    
    for(num_pcbs = 0; num_pcbs < NUM_MAX_TASK; num_pcbs++){
        pcb[num_pcbs].status = TASK_EXITED;
        pcb[num_pcbs].pid = -1;
        pcb[num_pcbs].kill = 0;
        pcb[num_pcbs].hart_mask = 0x0;
        pcb[num_pcbs].current_mask = 0x0;
    }

    current_running_0 = &pid0_pcb;
    current_running_1 = &pid1_pcb;
}

static void init_page_general(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int num_pages; 
    
    for(num_pages = 0; num_pages < PAGE_NUM; num_pages++){

        page_general[num_pages].valid = 0;

        page_general[num_pages].pin = 0;
        page_general[num_pages].using = 0;
        page_general[num_pages].kva = num_pages * PAGE_SIZE + FREEMEM_KERNEL;

        page_general[num_pages].pid = -1;
        page_general[num_pages].pgdir = -1;
        page_general[num_pages].va = -1;

         page_general[num_pages].table_not = 0;//这一项专门用来判断是不是页表项
    }

}

static void init_share_page(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int share_num; 
    
    for(share_num = 0; share_num < SHARE_PAGE_NUM; share_num++){

        share_pages[share_num].valid = 0;

        share_pages[share_num].key = -1;
        share_pages[share_num].pin = 0;
        share_pages[share_num].using = 0;

        share_pages[share_num].kva = (share_num + PAGE_NUM) * PAGE_SIZE + FREEMEM_KERNEL;
    }

}
// static void init_memory(void)
// {
//     int mem_tasks; 
    
//     for(mem_tasks = 0; mem_tasks < task_num; mem_tasks++)
//         load_task_img(mem_tasks);

// }
//初始时将所有的程序加载到内存中来

static void init_shell(void)
{
    uintptr_t kva_user_stack;

    pcb[num_tasks].recycle = 0;
    pcb[num_tasks].pgdir = allocPage(1,1,0,1,2);//分配根目录页//这里的给出的用户映射的虚地址没有任何意义  //这里是2因为能确定是shell
    //clear_pgdir(pcb[num_tasks].pgdir); //清空根目录页
    share_pgtable(pcb[num_tasks].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
    load_task_img(num_tasks,pcb[num_tasks].pgdir,2);//load进程并且为给进程建立好地址映射
    //pcb[num_tasks].kernel_sp = KERNEL_STACK + (num_tasks + 1) * 0x1000;
    //pcb[num_tasks].user_sp = pcb[num_tasks].kernel_sp;

    pcb[num_tasks].kernel_sp = allocPage(1,1,0,0,2) + 1 * PAGE_SIZE;//不是页表，但是要被pin住//只要没有所谓的用户对应的地址，va全部放0
    pcb[num_tasks].user_sp   = USER_STACK_ADDR;

    kva_user_stack = alloc_page_helper(pcb[num_tasks].user_sp - PAGE_SIZE, pcb[num_tasks].pgdir,1,2) + 1 * PAGE_SIZE;//比栈地址低的一张物理页
    alloc_page_helper(pcb[num_tasks].user_sp - 2*PAGE_SIZE, pcb[num_tasks].pgdir,1,2);//比栈地址低的第二张物理页

    pcb[num_tasks].wait_list.prev = &pcb[num_tasks].wait_list;
    pcb[num_tasks].wait_list.next = &pcb[num_tasks].wait_list;
    pcb[num_tasks].kill = 0;
    pcb[num_tasks].truepid = num_tasks + 2;
    pcb[num_tasks].pid = num_tasks + 2;
    pcb[num_tasks].tid = 0;
    pcb[num_tasks].thread_num = 0;
    pcb[num_tasks].hart_mask = 0x3;
    pcb[num_tasks].current_mask = 0x0;
    for(int k = 0;k < TASK_LOCK_MAX;k++){
        pcb[num_tasks].mutex_lock_key[k] = 0;
    }
    strcpy(pcb[num_tasks].pcb_name, "shell");
    init_pcb_stack( pcb[num_tasks].kernel_sp, kva_user_stack, 
                    tasks[num_tasks].task_entrypoint, &pcb[num_tasks],0,0);//记住这里的使用内核的映射去初始化栈的

    list_add(&pcb[num_tasks].list, &ready_queue);
    pcb[num_tasks].status = TASK_READY; 

    num_tasks++;
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
    
    syscall[SYSCALL_READCH]         = (long(*)())port_read_ch;
    syscall[SYSCALL_EXEC]           = (long(*)())do_exec;
    syscall[SYSCALL_EXIT]           = (long(*)())do_exit;
    syscall[SYSCALL_KILL]           = (long(*)())do_kill;
    syscall[SYSCALL_WAITPID]        = (long(*)())do_waitpid;
    syscall[SYSCALL_GETPID]         = (long(*)())do_getpid;
    syscall[SYSCALL_PS]             = (long(*)())do_process_show;
    syscall[SYSCALL_CLEAR]          = (long(*)())screen_clear;

    syscall[SYSCALL_BARR_INIT]      = (long(*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long(*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long(*)())do_barrier_destroy;

    syscall[SYSCALL_SEMA_INIT]      = (long(*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]        = (long(*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]      = (long(*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]   = (long(*)())do_semaphore_destroy;

    syscall[SYSCALL_MBOX_OPEN]      = (long(*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long(*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long(*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long(*)())do_mbox_recv;

    syscall[SYSCALL_SHM_GET]        = (long(*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]         = (long(*)())shm_page_dt;
    syscall[SYSCALL_FORK]           = (long(*)())do_fork;

    syscall[SYSCALL_THREAD_CREATE]  = (long(*)())do_thread_create;
    syscall[SYSCALL_NET_SEND]       = (long(*)())do_net_send;
    syscall[SYSCALL_NET_RECV]       = (long(*)())do_net_recv;
    // syscall[SYSCALL_THREAD_YIELD]   = (long(*)())do_thread_scheduler;

    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void)
{
    if(get_current_cpu_id() == 0){
        // lock_kernel();
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        // init_pcb();
        // init_memory();
        init_pcb();
        init_locks();
        init_barriers();
        init_semaphores();
        init_mbox();
        init_page_general();
        init_share_page();

// <<<<<<< HEAD
        current_running = &current_running_0;
// =======
        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");
// >>>>>>> 44b6afb9d17c726a4c190ffe4d7083ed3d511bd6

// <<<<<<< HEAD
        // Init lock mechanism o(´^｀)o
        printk("> [INIT] Lock mechanism initialization succeeded.\n");
// =======
        // TODO: [p5-task3] Init plic
        // plic_init(plic_addr, nr_irqs);
        // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");
// >>>>>>> 44b6afb9d17c726a4c190ffe4d7083ed3d511bd6

        init_shell();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        // printk("> [INIT] SCREEN initialization succeeded.\n");

        enable_interrupt();

        send_ipi((unsigned long *)0);
        clear_sip();

        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's

        // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
        //   and then execute them.

        // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
        // unlock_kernel();
        while (1)
        {
            // If you do non-preemptive scheduling, it's used to surrender control
            //do_scheduler();

            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }
    else{
        // lock_kernel();
        current_running = &current_running_1;
        setup_exception();
        // lock_kernel();
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        // unlock_kernel();
        enable_interrupt();
        //clean_temp_page(PGDIR_PA);//清空临时映射
        // unlock_kernel();
        while(1){
            enable_preempt();
            asm volatile("wfi");
        }
    }

    return 0;
}
