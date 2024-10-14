/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
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

#include <asm.h>
#include <asm/unistd.h>
#include <common.h>
#include <cpparg.h>
#include <csr.h>
#include <e1000.h>
#include <os/fs.h>
#include <os/ioremap.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <pgtable.h>
#include <plic.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>
#include <type.h>
#include "mminit.h"

#define TASK_INFO_LOC 0xffffffc0502001f4 //记录taskinfo位置信息

uint16_t task_num;
int version = 2;    // version must between 0 and 9 
task_info_t tasks[TASK_MAXNUM]; // Task info array

static void init_jmptab(void) {
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (volatile long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
    jmptab[SD_READ] = (volatile long (*)())sd_read;
    jmptab[SD_WRITE] = (volatile long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (volatile long (*)())qemu_logging;
    jmptab[SET_TIMER] = (volatile long (*)())set_timer;
    jmptab[READ_FDT] = (volatile long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (volatile long (*)())screen_move_cursor;
    jmptab[PRINT] = (volatile long (*)())printk;
    jmptab[YIELD] = (volatile long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (volatile long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (volatile long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (volatile long (*)())do_mutex_lock_release;
    jmptab[WRITE] = (volatile long (*)())screen_write;
    jmptab[REFLUSH] = (volatile long (*)())screen_reflush;

    // TODO: [p2-task1] (S-core) initialize system call table.
}

static void init_task_info(void) {
    uint32_t task_info_block_phyaddr;
    uint32_t task_info_block_size;

    uint32_t task_info_block_id;
    uint32_t task_info_block_num;
    uint32_t task_info_block_offset;

    task_info_block_phyaddr = *(uint32_t *)(TASK_INFO_LOC);
    task_info_block_size = *(uint32_t *)(TASK_INFO_LOC + 0x4);
    task_num = *(uint16_t *)(TASK_INFO_LOC + 0x8);

    //得到task_info的一系列信息，下面从sd卡读到内存中
    task_info_block_id = task_info_block_phyaddr / SECTOR_SIZE;
    task_info_block_num = NBYTES2SEC(task_info_block_phyaddr + task_info_block_size) - task_info_block_id;
    task_info_block_offset = task_info_block_phyaddr % SECTOR_SIZE;

    //将task_info数组拷贝到tasks数组中
    bios_sd_read((unsigned)TASK_MEM_BASE, task_info_block_num, task_info_block_id);
    memcpy((uint8_t *)tasks, (uint8_t *)TASK_MEM_BASE + task_info_block_offset,
           task_info_block_size);
}

static void init_pcb(void) {
    /** TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int num_pcbs;

    for (num_pcbs = 0; num_pcbs < NUM_MAX_TASK; num_pcbs++) {
        pcb[num_pcbs].status = TASK_EXITED;
        pcb[num_pcbs].pid = -1;
        pcb[num_pcbs].kill = 0;
        pcb[num_pcbs].hart_mask = 0x0;
        pcb[num_pcbs].cpu = 0x0;
        pcb[num_pcbs].pwd = 0;
        strcpy(pcb[num_pcbs].pwd_dir, "/");
    }

    current_running_0 = &pid0_pcb;
    current_running_1 = &pid1_pcb;
}

static void init_page_general(void) {
    /** TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int num_pages;

    for (num_pages = 0; num_pages < PAGE_NUM; num_pages++) {
        page_general[num_pages].valid = 0;
        page_general[num_pages].pin = UNPINED;
        page_general[num_pages].used = 0;
        page_general[num_pages].pte = NULL;
        page_general[num_pages].fqy = 0;
    }
}

static void init_share_page(void) {
    /** TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    int share_num;

    for (share_num = 0; share_num < SHARE_PAGE_NUM; share_num++) {
        share_pages[share_num].key = -1;
        share_pages[share_num].pg_index = -1;
    }
}

static void init_syscall(void) {
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_READCH] = (long (*)())port_read_ch;

    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_FORK] = (long (*)())do_fork;
    syscall[SYSCALL_THREAD_CREATE] = (long (*)())do_thread_create;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_TASK_SET] = (long (*)())do_task_set;
    syscall[SYSCALL_TASK_SETP] = (long (*)())do_task_set_p;

    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;

    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;

    syscall[SYSCALL_SEMA_INIT] = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP] = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN] = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY] = (long (*)())do_semaphore_destroy;

    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;

    syscall[SYSCALL_SHM_GET] = (long (*)())shm_page_get;
    syscall[SYSCALL_SHM_DT] = (long (*)())shm_page_dt;

    syscall[SYSCALL_NET_SEND] = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV] = (long (*)())do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM] = (long (*)())do_net_recv_stream;

    syscall[SYSCALL_MKFS] = (long (*)())do_mkfs;
    syscall[SYSCALL_STATFS] = (long (*)())do_statfs;
    syscall[SYSCALL_CD] = (long (*)())do_cd;
    syscall[SYSCALL_MKDIR] = (long (*)())do_mkdir;
    syscall[SYSCALL_LS] = (long (*)())do_ls;
    syscall[SYSCALL_RMDIR] = (long (*)())do_rmdirfile;
    syscall[SYSCALL_GETPWDNAME] = (long (*)())do_getpwdname;

    syscall[SYSCALL_TOUCH] = (long (*)())do_touch;
    syscall[SYSCALL_CAT] = (long (*)())do_cat;
    syscall[SYSCALL_FOPEN] = (long (*)())do_fopen;
    syscall[SYSCALL_FREAD] = (long (*)())do_fread;
    syscall[SYSCALL_FWRITE] = (long (*)())do_fwrite;
    syscall[SYSCALL_FCLOSE] = (long (*)())do_fclose;
    syscall[SYSCALL_RMFILE] = (long (*)())do_rmdirfile;
    syscall[SYSCALL_LSEEK] = (long (*)())do_lseek;
    syscall[SYSCALL_LN] = (long (*)())do_ln;

    syscall[SYSCALL_MMALLOC] = (long (*)())mmalloc;
    // syscall[SYSCALL_FREE]           = (long(*)())mmfree;

    // TODO: [p2-task3] initialize system call table.
}
/*************************************************************/

int main(void) {
    if (get_current_cpu_id() == 0) {
        // lock_kernel();
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        init_locks();
        init_barriers();
        init_semaphores();
        init_mbox();
        init_page_general();
        init_share_page();

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
        printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr,
               nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");
#endif
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        init_shell();
        printk("> [INIT] PCB initialization succeeded.\n");

        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");
#ifdef MLFQ
        init_queues();
#endif
        init_screen();

        if (!check_fs())
            do_mkfs();
        enable_interrupt();

        send_ipi((unsigned long *)0);
        clear_sip();

        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        while (1) {
            enable_preempt();
            asm volatile("wfi");
        }
    } else {
        setup_exception();
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        enable_interrupt();
        while (1) {
            enable_preempt();
            asm volatile("wfi");
        }
    }

    return 0;
}
