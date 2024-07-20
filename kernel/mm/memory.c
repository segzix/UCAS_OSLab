#include <hash.h>
#include <assert.h>
#include <pgtable.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <printk.h>

// NOTE: A/C-core
extern void ret_from_exception();

uintptr_t shm_page_get(int key)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uint8_t find = 0;
    uint32_t index = hash(key, SHARE_PAGE_NUM);

    //找寻对应的共享页
    for(;;index=(index+1)%SHARE_PAGE_NUM){
        if(share_pages[index].key == key){//已经被分配过并且符合key值
            find = 1;
            break;
        } else if(share_pages[index].key == -1){//还没有被分配过
            find = 0;
            break;
        } else //被分配过且不符合key值则继续寻找
            continue;
    }

    uintptr_t va = SHM_PAGE_STARTVA + index*NORMAL_PAGE_SIZE;
    if(find){
        //提取出已有的pgindex
        uint32_t pgindex = share_pages[index].pg_index;

        //直接建立用户虚地址的映射,并将对应的页面used++
        PTE* pte = mappages(va, (*current_running)->pgdir, kva2pa(pgindex2kva(pgindex)), 
                        _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);
        assert(pte);
        page_general[pgindex].used++;
    }
    else{
        //分配出用户对应虚地址的共享页
        uintptr_t kva = uvmalloc(va, (*current_running)->pgdir,
                 _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

        //给共享页打上key和对应的页面(初始化share_pages)
        share_pages[index].key = key;
        share_pages[index].pg_index = kva2pgindex(kva);
    }
    local_flush_tlb_all();

    return va; 
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    uintptr_t kva;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    kva = uvmfree((*current_running)->pgdir, addr);
    local_flush_tlb_all();

    unsigned pgindex = kva2pgindex(kva);

    uint32_t shindex;
    //如果已经无人使用，将此页清除掉
    if(!page_general[pgindex].used){
        for(shindex = 0;shindex < SHARE_PAGE_NUM;shindex++){
            if(share_pages[shindex].pg_index == pgindex)
                break;
        }
        share_pages[shindex].key = -1;
    }
}

pid_t do_fork(){

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;

    for(int id=0; id < NUM_MAX_TASK; id++){
        if(pcb[id].status == TASK_EXITED){

            pcb[id].recycle = 0;
            pcb[id].pgdir = (PTE*)kalloc();//分配根目录页//这里的给出的用户映射的虚地址没有任何意义
            pcb[id].heap = HEAP_STARTVA;
            //clear_pgdir(pcb[id].pgdir); //清空根目录页
            // share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
            // load_task_img(i,pcb[id].pgdir,id+2);//load进程并且为给进程建立好地址映射(这一步实际上包括了建立好除了根目录页的所有页表以及除了栈以外的所有映射)
            share_pgtable(pcb[id].pgdir,(PTE*)pa2kva(PGDIR_PA));//内核地址映射拷贝
            pgcopy(pcb[id].pgdir, (*current_running)->pgdir, 2);

            pcb[id].kernel_sp  = kalloc() + 1 * PAGE_SIZE;//这里的给出的用户映射的虚地址没有任何意义
            pcb[id].user_sp    = (*current_running)->user_sp;

            // kva_user_stack = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir,1,id+2) + 1 * PAGE_SIZE;//比栈地址低的一张物理页
            // alloc_page_helper(pcb[id].user_sp - 2*PAGE_SIZE, pcb[id].pgdir,1,id+2);//比栈地址低的第二张物理页
            // uintptr_t va = alloc_page_helper(pcb[id].user_sp - PAGE_SIZE, pcb[id].pgdir) + PAGE_SIZE;
            //内核对应的映射到这张物理页的地址，后面对于该用户栈的操作全部通过内核映射表进行
            //并且考虑到后面要加一个东西导致真实的
            //这列应该可以直接用用户的也页表映射去访问
            // pcb[id].kernel_sp  = allocKernelPage(1) + PAGE_SIZE;
            // pcb[id].user_sp    = allocUserPage(1) +   PAGE_SIZE;
            pcb[id].cursor_x   = 0;
            pcb[id].cursor_y   = 0;
            pcb[id].wakeup_time = 0;
            pcb[id].truepid = id + 2;
            pcb[id].pid = id + 2;
            pcb[id].tid = 0;
            pcb[id].thread_num = 0;
            pcb[id].wait_list.prev = &pcb[id].wait_list;
            pcb[id].wait_list.next = &pcb[id].wait_list;
            pcb[id].kill = 0;
            pcb[id].hart_mask = (*current_running)->hart_mask;

            // clean_temp_page(pcb[id].pgdir);

            for(int k = 0;k < TASK_LOCK_MAX;k++){
                pcb[id].mutex_lock_key[k] = 0;
            }

            memcpy((void*)pcb[id].pcb_name, (void*)(*current_running)->pcb_name, 32);
            // load_task_img(tasks[i].name);


            regs_context_t *pt_regs = (regs_context_t *)(pcb[id].kernel_sp - sizeof(regs_context_t));

            uintptr_t src_kernel_sp_start = (*current_running)->kernel_sp - sizeof(regs_context_t);//这个时候kernel_sp只减去了pt_regs一段
            uintptr_t dest_kernel_sp_start = (uintptr_t)pt_regs;

            memcpy((void*)dest_kernel_sp_start, (void*)src_kernel_sp_start, sizeof(regs_context_t));
            //准备将该进程运行时的一些需要写寄存器的值全部放入栈中
            pt_regs->regs[4]    = (reg_t)(&pcb[id]);                     //tp
            pt_regs->regs[10]   = (reg_t)0;

            switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
            pt_switchto->regs[0] = (reg_t)ret_from_exception;
            pt_switchto->regs[1] = (reg_t)(pt_regs);      //sp

            pcb[id].kernel_sp = (reg_t)pt_switchto;


            list_add(&(pcb[id].list),&ready_queue);
            pcb[id].status     = TASK_READY;
            num_tasks++;

            return pcb[id].pid;
        }
    }
}

uintptr_t mmalloc(uint32_t size){
    uintptr_t old_heap  = get_pcb()->heap;
    uintptr_t new_heap  = get_pcb()->heap + size;

    uintptr_t startkva  = ((old_heap-1)/PAGE_SIZE + 1)*PAGE_SIZE;
    uintptr_t endkva    = (new_heap/PAGE_SIZE)*PAGE_SIZE;

    PTE* pgdir = get_pcb()->pgdir;

    for(unsigned kva=startkva; kva<=endkva; kva+=PAGE_SIZE){
        mappages(kva, pgdir, 0, 0);
    }

    get_pcb()->heap+=size;
    return old_heap;
}