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
// #include <stdint.h>
// #include <stdint.h>

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

    search_PTE_swap = search_and_set_PTE(stval,(*current_running)->pgdir,(*current_running)->pid);//找寻到对应的页表项
    
    
    if(*search_PTE_swap % 2 == 1){//p位有效
        set_attribute(search_PTE_swap, get_attribute(*search_PTE_swap,PA_ATTRIBUTE_MASK) |_PAGE_PRESENT |_PAGE_ACCESSED |_PAGE_READ);
    }
    else{
        if((*search_PTE_swap & _PAGE_SOFT)){//软件位有，则是在硬盘上
            search_block_id = (uint16_t)get_pfn(*search_PTE_swap);//确定扇区上的扇区号
            uint64_t kva = allocPage(1,0,stval,0,(*current_running)->pid);//分配出一块空间,并且这里肯定不是页表，也不用被pin住

            bios_sd_read(kva2pa(kva), 8, search_block_id);

            set_pfn(search_PTE_swap,kva2pa(kva) >> NORMAL_PAGE_SHIFT);//
            set_attribute(search_PTE_swap,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                        |_PAGE_ACCESSED | _PAGE_USER);
            //将硬盘中的内容读到内存中(内存中可能被换出的内容在allocpage中已经被换出)，然后再将页表映射建立好
        }
        else{//软件位无，则需要新分配物理页
            uint64_t kva = allocPage(1,0,stval,0,(*current_running)->pid);//分配出一块空间

            set_pfn(search_PTE_swap,kva2pa(kva) >> NORMAL_PAGE_SHIFT);//
            set_attribute(search_PTE_swap,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                        |_PAGE_ACCESSED | _PAGE_USER);
            //将硬盘中的内容读到内存中(内存中可能被换出的内容在allocpage中已经被换出)，然后再将页表映射建立好
        }
    }

    local_flush_tlb_all();
}

void handle_pagefault_store(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uint16_t search_block_id;//从磁盘中取出时的扇区号
    PTE * search_PTE_swap;

    search_PTE_swap = search_and_set_PTE(stval,(*current_running)->pgdir,(*current_running)->pid);
    
    if(*search_PTE_swap % 2 == 1){//p位有效
        if(*search_PTE_swap & _PAGE_WRITE)
            ;
        else {
            uint64_t src_kva = pa2kva(get_pa(*search_PTE_swap));//已经找到的表项，将其中的物理地址提取出来
            uint64_t dest_kva = allocPage(1,1,stval,0,(*current_running)->pid);//分配出一块空间

            memcpy(dest_kva, src_kva, PAGE_SIZE);

            //bios_sd_read(kva2pa(kva), 8, search_block_id);

            set_pfn(search_PTE_swap,kva2pa(dest_kva) >> NORMAL_PAGE_SHIFT);//

            uint32_t node_index = (src_kva - FREEMEM_KERNEL)/PAGE_SIZE;
            page_general[node_index].using--;//对应的物理页的使用数量会增加！
            screen_move_cursor(0,4);
            printk("doing copy_on_write!");
        }
        set_attribute(search_PTE_swap, get_attribute(*search_PTE_swap,PA_ATTRIBUTE_MASK) |_PAGE_PRESENT |_PAGE_ACCESSED |_PAGE_READ |_PAGE_DIRTY |_PAGE_WRITE);
    }
    else{
        if((*search_PTE_swap & _PAGE_SOFT)){//软件位有，则是在硬盘上
            search_block_id = (uint16_t)get_pfn(*search_PTE_swap);//确定扇区上的扇区号
            uint64_t kva = allocPage(1,0,stval,0,(*current_running)->pid);//分配出一块空间

            bios_sd_read(kva2pa(kva), 8, search_block_id);

            set_pfn(search_PTE_swap,kva2pa(kva) >> NORMAL_PAGE_SHIFT);//
            set_attribute(search_PTE_swap,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                    |_PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
            //将硬盘中的内容读到内存中(内存中可能被换出的内容在allocpage中已经被换出)，然后再将页表映射建立好
        }
        else{//软件位无，则需要新分配物理页
            if(stval >= 0xffffffc000000000){
                screen_move_cursor(0,7);
                printk("address fault !");
                do_exit();
            }else{
                uint64_t kva = allocPage(1,0,stval,0,(*current_running)->pid);//分配出一块空间

                set_pfn(search_PTE_swap,kva2pa(kva) >> NORMAL_PAGE_SHIFT);//
                set_attribute(search_PTE_swap,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                    |_PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
            //将硬盘中的内容读到内存中(内存中可能被换出的内容在allocpage中已经被换出)，然后再将页表映射建立好
            }
        }
    }

    local_flush_tlb_all();
    //这里相当于也是完成了一次啊allocpage_helper的工作
}
