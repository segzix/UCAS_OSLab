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

#include <os/smp.h>
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
#include <type.h>
#include <csr.h>
#include <pgtable.h>
#include <os/net.h>
#include <os/fs.h>
#include <plic.h>
#include <cpparg.h>

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

    jmptab[CONSOLE_PUTSTR]  = (volatile long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
    jmptab[SD_READ]         = (volatile long (*)())sd_read;
    jmptab[SD_WRITE]        = (volatile long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (volatile long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (volatile long (*)())set_timer;
    jmptab[READ_FDT]        = (volatile long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (volatile long (*)())screen_move_cursor;
    jmptab[PRINT]           = (volatile long (*)())printk;
    jmptab[YIELD]           = (volatile long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (volatile long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (volatile long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (volatile long (*)())do_mutex_lock_release;
    jmptab[WRITE]           = (volatile long (*)())screen_write;
    jmptab[REFLUSH]         = (volatile long (*)())screen_reflush;

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

    bios_sd_read((unsigned)TASK_MEM_BASE, task_info_block_num, task_info_block_id);
    memcpy((uint8_t*)tasks, (uint8_t*)TASK_MEM_BASE + task_info_block_offset, task_info_block_size);
    //将task_info数组拷贝到tasks数组中

    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

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
        pcb[num_pcbs].pwd = 0;
        strcpy(pcb[num_pcbs].pwd_dir,"/");
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
        page_general[num_pages].pin = UNPINED;
        page_general[num_pages].used = 0;
        page_general[num_pages].pte = NULL;
        page_general[num_pages].fqy = 0;
    }

}

static void init_share_page(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int share_num; 
    
    for(share_num = 0; share_num < SHARE_PAGE_NUM; share_num++){
        share_pages[share_num].key = -1;
        share_pages[share_num].pg_index = -1;
    }

}
// static void init_memory(void)
// {
//     int mem_tasks; 
    
//     for(mem_tasks = 0; mem_tasks < task_num; mem_tasks++)
//         load_task_img(mem_tasks);

// }
//初始时将所有的程序加载到内存中来

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
    syscall[SYSCALL_NET_RECV_STREAM]= (long(*)())do_net_recv_stream;
    // syscall[SYSCALL_THREAD_YIELD]   = (long(*)())do_thread_scheduler;

    syscall[SYSCALL_MKFS]           = (long(*)())do_mkfs;
    syscall[SYSCALL_STATFS]         = (long(*)())do_statfs;
    syscall[SYSCALL_CD]             = (long(*)())do_cd;
    syscall[SYSCALL_MKDIR]          = (long(*)())do_mkdir;
    syscall[SYSCALL_LS]             = (long(*)())do_ls;
    syscall[SYSCALL_RMDIR]          = (long(*)())do_rmdirfile;
    syscall[SYSCALL_GETPWDNAME]     = (long(*)())do_getpwdname;

    syscall[SYSCALL_TOUCH]          = (long(*)())do_touch;
    syscall[SYSCALL_CAT]            = (long(*)())do_cat;
    syscall[SYSCALL_FOPEN]          = (long(*)())do_fopen;
    syscall[SYSCALL_FREAD]          = (long(*)())do_fread;
    syscall[SYSCALL_FWRITE]         = (long(*)())do_fwrite;
    syscall[SYSCALL_FCLOSE]         = (long(*)())do_fclose;
    syscall[SYSCALL_RMFILE]         = (long(*)())do_rmdirfile;
    syscall[SYSCALL_LSEEK]          = (long(*)())do_lseek;
    syscall[SYSCALL_LN]             = (long(*)())do_ln;

    syscall[SYSCALL_MMALLOC]         = (long(*)())mmalloc;
    //syscall[SYSCALL_FREE]           = (long(*)())mmfree;

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

        current_running = &current_running_0;
        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        
        #ifdef NET
        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        printk("> [INIT] Lock mechanism initialization succeeded.\n");
        // TODO: [p5-task3] Init plic
        plic_init(plic_addr, nr_irqs);
        printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");
        #endif

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        init_shell();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        // printk("> [INIT] SCREEN initialization succeeded.\n");

        if(!check_fs())
            do_mkfs();
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
        current_running = &current_running_1;
        setup_exception();
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        enable_interrupt();
        while(1){
            enable_preempt();
            asm volatile("wfi");
        }
    }

    return 0;
}
