#include "os/mm.h"
#include "os/sched.h"
#include "os/task.h"
#include "os/loader.h"
#include <os/string.h>
#include "pgtable.h"
#include "csr.h"
#include <type.h>

extern void ret_from_exception();

/************************************************************/
/*
 * 进程内存空间分配
 */
void init_pcb_mm(int id, int taskid, enum FORK fork) {
    uintptr_t kva_user_stack;

    /*指定根目录页地址，堆地址，内核栈地址(直接分配完毕)，用户栈地址*/
    pcb[id].pgdir = (PTE *)kalloc();
    pcb[id].heap = HEAP_STARTVA;
    pcb[id].kernel_sp = kalloc() + 1 * PAGE_SIZE;
    pcb[id].user_sp = USER_STACK_ADDR;

    //内核地址映射拷贝
    share_pgtable(pcb[id].pgdir, (PTE *)pa2kva(PGDIR_PA));

    /*
     * fork: 加载程序，分配用户栈，初始化用户栈
     * notfork: 建立页表映射到父进程的物理页，其他工作由写时复制保证
     */
    if (fork == NOTFORK) {
        load_task_img(taskid, (uintptr_t)pcb[id].pgdir);
        kva_user_stack =
            uvmalloc(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,
                     _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER) +
            1 * PAGE_SIZE;
        init_pcb_stack(pcb[id].kernel_sp, kva_user_stack, tasks[taskid].task_entrypoint, &pcb[id],
                       kernel_argc, kernel_argv);
    } else {
        pcb_t *curpcb = get_pcb();
        pgcopy(pcb[id].pgdir, curpcb->pgdir, 2);

        regs_context_t *pt_regs = (regs_context_t *)(pcb[id].kernel_sp - sizeof(regs_context_t));
        switchto_context_t *pt_switchto =
            (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
        // trapframe段拷贝
        memcpy((void *)pt_regs, (void *)(curpcb->kernel_sp - sizeof(regs_context_t)),
               sizeof(regs_context_t));

        // fork进程独有：tp寄存器，a0返回0，switchto后返回ret_from_ex，switchto设置trapframe内核栈地址
        pt_regs->regs[4] = (reg_t)(&pcb[id]); // tp
        pt_regs->regs[10] = (reg_t)0;
        pt_switchto->regs[0] = (reg_t)ret_from_exception;
        pt_switchto->regs[1] = (reg_t)(pt_regs); // sp

        pcb[id].kernel_sp = (reg_t)pt_switchto;
    }
}

/*
 * 线程内存空间分配
 */
void init_tcb_mm(int id, void *thread_entrypoint, void *arg) {

    /*指定根目录页地址，堆地址，内核栈地址(直接分配完毕)，用户栈地址(一个线程分配一页)*/
    pcb[id].pgdir = get_pcb()->pgdir;
    pcb[id].heap = HEAP_STARTVA;
    pcb[id].kernel_sp = kalloc() + 1 * PAGE_SIZE;
    pcb[id].user_sp = USER_STACK_ADDR + PAGE_SIZE * pcb[id].tid;

    /*分配用户栈空间*/
    uvmalloc(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,
             _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

    /*初始化用户栈*/
    init_tcb_stack(pcb[id].kernel_sp, pcb[id].user_sp, (uintptr_t)thread_entrypoint, &pcb[id],
                   arg); //这里直接传用户的虚地址即可
}

/************************************************************/
/*
 * 进程内核用户栈初始化
 */
void init_pcb_stack(ptr_t kernel_stack, ptr_t kva_user_stack, ptr_t entry_point, pcb_t *pcb,
                    int argc, char *argv[]) {
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    /*
     * !!!
     * kva_user_stack与user_sp对应的是同一个物理页面的内核与用户虚地址，要区别对待，同时做减法
     * 同时注意传递的一定是用户虚地址
     */
    uintptr_t argv_base = kva_user_stack - sizeof(uintptr_t) * (argc + 1);
    pcb->user_sp = pcb->user_sp - sizeof(uintptr_t) * (argc + 1);

    /*初始化ra,tp,a0,a1寄存器，传递命令行参数长度和地址*/
    pt_regs->regs[1] = (reg_t)entry_point;
    pt_regs->regs[4] = (reg_t)pcb;
    pt_regs->regs[10] = (reg_t)argc;
    pt_regs->regs[11] = pcb->user_sp;

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
        user_sp_now -= (len + 1);
        pcb->user_sp -= (len + 1);

        //设置栈
        (*argv_ptr) = pcb->user_sp;
        strcpy((char *)user_sp_now, argv[i]);
        argv_ptr++;
    }
    (*argv_ptr) = 0;
    pcb->user_sp &= (~0xf);

    /*初始化trapframe中用户sp指针，进程控制块中的内核sp指针*/
    pt_regs->regs[2] = (reg_t)pcb->user_sp; // sp
    pcb->kernel_sp = (reg_t)pt_switchto;
}

/*
 * 线程内核用户栈初始化
 */
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