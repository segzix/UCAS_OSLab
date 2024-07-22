#include "os/smp.h"
#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <os/mm.h>
#include <os/net.h>
#include <pgtable.h>
#include <plic.h>
#include <e1000.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    //lock_kernel();

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    if((*current_running)->kill == 1)
        do_exit();

    if(scause & (1UL << 63)){
        scause = scause & (~(1UL << 63));
        (*irq_table[scause])(regs, stval, scause);
    }
    else{
        (*exc_table[scause])(regs, stval, scause);
    }

    if((*current_running)->kill == 1)
        do_exit();
    //stval传递为interrupt值，确定类型
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    //unlock_kernel();

}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    //disable_preempt();
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint32_t plic_ID = plic_claim();

    if(plic_ID == PLIC_E1000_QEMU_IRQ || plic_ID == PLIC_E1000_PYNQ_IRQ){
        net_handle_irq();
    }
    else if(plic_ID == 0)
        return;
    else {
        handle_other(regs, stval, scause);
    }

    plic_complete(plic_ID);
    // TODO: [p5-task3] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
}

void init_exception()
{
    exc_table[EXCC_SYSCALL]         = (handler_t)handle_syscall;
    exc_table[EXCC_INST_MISALIGNED] = (handler_t)handle_other;
    exc_table[EXCC_INST_ACCESS]     = (handler_t)handle_other;
    exc_table[EXCC_BREAKPOINT]      = (handler_t)handle_other;
    exc_table[EXCC_LOAD_ACCESS]     = (handler_t)handle_other;
    exc_table[EXCC_STORE_ACCESS]    = (handler_t)handle_other;
    exc_table[EXCC_INST_PAGE_FAULT] = (handler_t)handle_pagefault_access;
    exc_table[EXCC_LOAD_PAGE_FAULT] = (handler_t)handle_pagefault_access;
    exc_table[EXCC_STORE_PAGE_FAULT]= (handler_t)handle_pagefault_store;
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/

    irq_table[IRQC_U_SOFT] = (handler_t)handle_other;
    irq_table[IRQC_S_SOFT] = (handler_t)handle_other;
    irq_table[IRQC_M_SOFT] = (handler_t)handle_other;
    irq_table[IRQC_U_TIMER] = (handler_t)handle_other;
    irq_table[IRQC_S_TIMER] = (handler_t)handle_irq_timer;
    irq_table[IRQC_M_TIMER] = (handler_t)handle_other;
    irq_table[IRQC_U_EXT] = (handler_t)handle_other;
    irq_table[IRQC_S_EXT] = (handler_t)handle_irq_ext;
    irq_table[IRQC_M_EXT] = (handler_t)handle_other;
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/

    setup_exception();
    /* TODO: [p2-task3] set up the entrypoint of exceptions */
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}

void handle_pagefault_access(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uint16_t search_block_id;//从磁盘中取出时的扇区号
    PTE * search_PTE_swap;

    //若walk不到地址，说明映射还没有建立，报段错误
    search_PTE_swap = (PTE*)walk(stval,(*current_running)->pgdir,VOID);
    if(!search_PTE_swap){
        printk("Segmentation Fault");
        regs->sepc = regs->sepc + 4;
        return;
    }
    
    /*
     * p位有效：access位无，重新给出即可
     * p位无效：软件位有，在硬盘上
     * p位无效：软件位无，还未分配即进行访问，报错(对于一个地址，规定先malloc建立映射，然后对该地址进行访问时分配一个物理页)
     */
    if(*search_PTE_swap & _PAGE_PRESENT){
        set_attribute(search_PTE_swap, 
                    get_attribute(*search_PTE_swap,PA_ATTRIBUTE_MASK) |_PAGE_PRESENT |_PAGE_ACCESSED |_PAGE_READ);
    }
    else{
        if((*search_PTE_swap & _PAGE_SOFT)){
            //确定扇区上的扇区号，分配一页，并将扇区读入
            search_block_id = (uint16_t)get_pfn(*search_PTE_swap);//确定扇区上的扇区号
            uint64_t kva = palloc(search_PTE_swap, 
                _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_ACCESSED | _PAGE_USER);
            bios_sd_read(kva2pa(kva), 8, search_block_id);
        }
        else{
            palloc(search_PTE_swap, 
                _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_ACCESSED | _PAGE_USER);
            
        }
    }

    local_flush_tlb_all();
}

void handle_pagefault_store(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uint16_t search_block_id;//从磁盘中取出时的扇区号
    PTE * search_PTE_swap;

    //若walk不到地址，说明映射还没有建立，报段错误
    search_PTE_swap = (PTE*)walk(stval,(*current_running)->pgdir,VOID);
    if(!search_PTE_swap){
        printk("Segmentation Fault");
        regs->sepc = regs->sepc + 4;
        return;
    }
    
    /*
     * p位有效：wirte位无，说明是写时复制，需进行uvmcopy
     * p位有效：wirte位有，重新给出即可
     * p位无效：软件位有，在硬盘上
     * p位无效：软件位无，mmaloc完还未进行分配
     */
    if(*search_PTE_swap & _PAGE_PRESENT){
        if(*search_PTE_swap & _PAGE_WRITE)
            set_attribute(search_PTE_swap, 
                        get_attribute(*search_PTE_swap,PA_ATTRIBUTE_MASK) |_PAGE_PRESENT |_PAGE_ACCESSED |_PAGE_READ |_PAGE_DIRTY |_PAGE_WRITE);
        else {
            uvmcopy(search_PTE_swap);
        }
    }
    else{
        if((*search_PTE_swap & _PAGE_SOFT)){
            //确定扇区上的扇区号，分配一页，并将扇区读入
            search_block_id = (uint16_t)get_pfn(*search_PTE_swap);
            uint64_t kva = palloc(search_PTE_swap, 
                _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_ACCESSED | _PAGE_DIRTY| _PAGE_USER);
            bios_sd_read(kva2pa(kva), 8, search_block_id);
        }
        else{
            palloc(search_PTE_swap, 
                _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_ACCESSED | _PAGE_DIRTY| _PAGE_USER);
        }
    }

    local_flush_tlb_all();
}
