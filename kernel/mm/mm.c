#include <assert.h>
#include <pgtable.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <printk.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;
page_allocated page_general[PAGE_NUM];
share_page share_pages[SHARE_PAGE_NUM];
extern void ret_from_exception();

//用户给出一个虚地址和根目录页，将映射全部建立好(如果已经有映射则不用建立)，最终返回末级页表的地址
//所有的对页表的分配都是在这个函数里进行(被封装起来)，并且页表一定会被pin住
//注意页表的分配，其中的va并不像分配物理页中的va具有任何意义
uintptr_t walk(uintptr_t va, PTE* pgdir, enum WALK walk)
{
    va &= VA_MASK;
    PTE* pgdir_t = pgdir;
    int pid = get_pcb()->pid;
    unsigned level;
    
    for(level=2; level>0; level--){
        // 得到PTE表项地址
        PTE* pte = &pgdir_t[PX(level, va)];
        
        // _PAGE_PRESENT有效，则直接有pgdir_t
        if(*pte & _PAGE_PRESENT){
            pgdir_t = (PTE *)pa2kva(get_pa(*pte));
        }
        else{
            // ALLOC的情况才会继续往下进行分配
            if(walk == ALLOC){
                palloc(pte);
                pgdir_t = (PTE *)pa2kva(get_pa(*pte));
            }else{
                return 0;
            }
        }
    }

    /* 
     * ALLOC与VOID的情况，返回对应的PTE表项即可(因为可能还没有对应的页表，需要外部进行set)
     * FIND的情况，确定有末级页面了，所以直接返回最末级的页面内核虚地址
     */
    if(walk == ALLOC || walk == VOID){
        return (uintptr_t)&pgdir_t[PX(level, va)];
    }else if(walk == FIND){
        return pa2kva(get_pa(pgdir_t[PX(level, va)]));
    }

    return 0;
}

void clear_pagearray(uint32_t node_index){
    page_general[node_index].valid = 0;
    page_general[node_index].pin = 0;
}

unsigned swap_out(){//swapout函数只负责选中一页然后换出，不负责将某一页重新填入或者分配出去

    static unsigned swap_block_id = 0x200;
    static unsigned swap_index = 0;
    PTE * swap_PTE;//将要换出去的物理页对应的表项
    for(unsigned i = swap_index;i < PAGE_NUM;i = (i+1)%PAGE_NUM)
    {
        if(!page_general[i].pin){
            page_general[i].valid = 0;

            // page_general[i].pin   = pin;
            // page_general[i].using = 1;

            // page_general[i].pid   = (*current_running)->pid;
            // page_general[i].pgdir = (*current_running)->pgdir;
            // page_general[i].va    = va;
            swap_PTE = page_general[i].pte;
            assert(swap_PTE);
            *swap_PTE = 0;

            set_pfn(swap_PTE, swap_block_id);//将硬盘上的扇区号写入ppn
            set_attribute(swap_PTE, _PAGE_SOFT | _PAGE_USER);//软件位拉高
            local_flush_tlb_all();

            bios_sd_write(kva2pa(pgindex2kva(i)),8,swap_block_id);//将物理地址和所写的扇区号传入

            printl("\npage[%d](kva : 0x%x) has been swapped to block id[%d]\n",i,pgindex2kva(i),swap_block_id);

            swap_block_id += 8;//扇区号加8
            swap_index = (i+1)%PAGE_NUM;//从下一个开始算起，为fifo算法
            return i;//返回数组中的下标
        }
    }

    return 0;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void free_all_pagemapping(PTE * baseAddr){//通过根目录页基址，取消所有映射
    PTE *pgdir = baseAddr;
    PTE *pmd, *pmd2, *pmd3;
    // int last = fm_head;
    for(int i=0; i<512; i++){
        if(pgdir[i] % 2){
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            
            if(pmd < FREEMEM_KERNEL) continue; //kernal no release
            // printl("pmd %ld \n",pmd);

            for(int j = 0; j<512; j++){
                if(pmd[j] % 2){
                    pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
                    for(int k = 0; k<512; ++k){
                        if(pmd2[k] % 2){
                            pmd3 = (PTE *)pa2kva(get_pa(pmd2[k]));
                            uint32_t node_index = ((uint64_t)pmd3 - FREEMEM_KERNEL)/PAGE_SIZE;//由此得知数组中的对应页的下标

                            set_attribute(pmd2, get_attribute(*pmd2,PA_ATTRIBUTE_MASK) & ~_PAGE_PRESENT);
                            //取消映射

                            //取消映射不代表取消物理页，如果using为0则取消物理页
                            page_general[node_index].using--;
                            if(page_general[node_index].using == 0)
                                clear_pagearray(node_index);
                        }
                    }
                }
            }
        }
    }
}

void free_all_pagetable(PTE* baseAddr){//取消并回收所有的页表
    PTE *pgdir = baseAddr;
    PTE *pmd, *pmd2;
    // int last = fm_head;
    for(int i=0; i<512; i++){
        if(pgdir[i] % 2){//如果该位拉高则证明对应的页表有映射
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            
            if(pmd < FREEMEM_KERNEL) continue; //kernal no release
            // printl("pmd %ld \n",pmd);

            for(int j = 0; j<512; j++){
                if(pmd[j] % 2){//如果该位拉高则证明对应的页表有映射
                    pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
                    uint32_t node_index = ((uint64_t)pmd2 - FREEMEM_KERNEL)/PAGE_SIZE;//由此得知数组中的对应页的下标

                    clear_pagearray(node_index);
                    page_general[node_index].using = 0;
                }
            }

            uint32_t node_index = ((uint64_t)pmd - FREEMEM_KERNEL)/PAGE_SIZE;//由此得知数组中的对应页的下标

            clear_pagearray(node_index);
            page_general[node_index].using = 0;
        }
    }

    uint32_t node_index = ((uint64_t)pgdir - FREEMEM_KERNEL)/PAGE_SIZE;//由此得知数组中的对应页的下标

    clear_pagearray(node_index);
    page_general[node_index].using = 0;

    local_flush_tlb_all();
}

void free_all_pageframe(ptr_t baseAddr){//取消并回收所有的物理页
    PTE *pgdir = (PTE *)baseAddr;
    PTE *pmd, *pmd2, *pmd3;
    // int last = fm_head;
    for(int i=0; i<512; i++){
        if(pgdir[i] % 2){
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            
            if(pmd < FREEMEM_KERNEL) continue; //kernal no release
            // printl("pmd %ld \n",pmd);

            for(int j = 0; j<512; j++){
                if(pmd[j] % 2){
                    pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
                    for(int k = 0; k<512; ++k){
                        if(pmd2[k] % 2){
                            pmd3 = (PTE *)pa2kva(get_pa(pmd2[k]));
                            uint32_t node_index = ((uint64_t)pmd3 - FREEMEM_KERNEL)/PAGE_SIZE;//由此得知数组中的对应页的下标

                            page_general[node_index].valid = 0;
                            page_general[node_index].pin = 0;

                            //取消映射
                        }
                    }
                }
            }
        }
    }
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(PTE* dest_pgdir, PTE* src_pgdir)
{
    uint64_t *dest_pgdir_addr = (uint64_t *) dest_pgdir;
    uint64_t *src_pgdir_addr  = (uint64_t *) src_pgdir;
    for(int i = 0; i < 512; i++){
        if(i != (TEMP_PAGE_START >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)))//如果满足不是临时映射则拷贝
            *dest_pgdir_addr = *src_pgdir_addr;
        dest_pgdir_addr++;
        src_pgdir_addr++;
    }
}

uintptr_t shm_page_get(int key)
{
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    uintptr_t va;
    int find = 0;
    int id=0;
    for(int i=0;i<SHARE_PAGE_NUM;i++){
        if(share_pages[i].valid && (share_pages[i].key == key)){//已经被分配过并且符合key值
            find = 1;
            id = i;
            break;
        }
        else if(!share_pages[i].valid){//还没有被分配过
            find = 0;
            id = i;
            break;
        }
        else //被分配过且不符合key值则继续寻找
            continue;
    }

    if(find){//之前就已经有，即找到key值
        share_pages[id].using++;
        //这里变换成为用户的虚地址固定，通过walk函数去给用户的虚地址建立页表

        /*
         * 首先得到用户的虚地址，然后建立用户虚地址的映射并返回PTE表项
         */
        va = SHM_PAGE_STARTVA+id*NORMAL_PAGE_SIZE;
        PTE* pte = (PTE*)walk(va, (*current_running)->pgdir, ALLOC);
        assert(pte);

        /*
         * 对返回的PTE表项进行赋物理地址和标志位的操作 
         */
        memset(pte, 0, sizeof(PTE));
        set_pfn(pte, kva2pa(share_pages[id].kva) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
        set_attribute(pte,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                                            | _PAGE_USER);

        local_flush_tlb_all();
    }
    else{
        share_pages[id].valid = 1;
        
        share_pages[id].key = key;
        share_pages[id].using = 1;
        share_pages[id].pin = 1;

        clear_pgdir(share_pages[id].kva);//共享物理页要求在没有人用，全部解除映射时就清空

        /*
         * 首先得到用户的虚地址，然后建立用户虚地址的映射并返回PTE表项
         */
        va = SHM_PAGE_STARTVA+id*NORMAL_PAGE_SIZE;
        PTE* pte = (PTE*)walk(va, (*current_running)->pgdir, ALLOC);
        assert(pte);

        /*
         * 对返回的PTE表项进行赋物理地址和标志位的操作 
         */
        memset(pte, 0, sizeof(PTE));
        set_pfn(pte, kva2pa(share_pages[id].kva) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
        set_attribute(pte,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                                            | _PAGE_USER);

        local_flush_tlb_all();       
    }

    return va; 
    // TODO [P4-task4] shm_page_get:
}


uintptr_t free_pagemapping(uintptr_t va,PTE* pgdir){//通过根目录页基址和用户虚地址来取消对应的映射
    PTE* pmd;
    PTE* pmd2;
    uintptr_t pmd3;

    PTE * pgdir_t = pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));//页目录虚地址
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));//二级页表虚地址
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT));//三级页表虚地址
    // int last = fm_head;
    pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
    pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    set_attribute(pmd2, get_attribute(*pmd2,PA_ATTRIBUTE_MASK) & ~_PAGE_PRESENT);
    //set_attribute(pmd2, 0);
    pmd3 = pa2kva(get_pa(pmd2[vpn0]));

    return pmd3;
}

void shm_page_dt(uintptr_t addr)
{
    uintptr_t kva;
    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
    // printl("DT pgdir = %ld addr = %d\n",(*current_running)->pgdir,addr);
    kva = free_pagemapping(addr,(*current_running)->pgdir);
    local_flush_tlb_all();

    unsigned node_index = (kva - PAGE_NUM * PAGE_SIZE - FREEMEM_KERNEL)/PAGE_SIZE;
    share_pages[node_index].using--;
    if(!share_pages[node_index].using){
        share_pages[node_index].valid = 0;

        share_pages[node_index].key = -1;
        share_pages[node_index].pin = 0;
        //clear_pgdir(share_pages[node_index].kva);//共享物理页要求在没有人用，全部解除映射时就清空
    }
}

void pgcopy(PTE* dest_pgdir, PTE* src_pgdir, uint8_t level){
    PTE * src_pgdir_t = src_pgdir;
    PTE * dest_pgdir_t = dest_pgdir;

    for(unsigned vpn = 0;vpn < NUM_PTE_ENTRY; vpn++){
        //页表对应的p位是1,则需要进行拷贝
        if(src_pgdir_t[vpn] & _PAGE_PRESENT){
            if(level){
                //首先给目标项分配一页，然后继续进入进行memcopy
                palloc(&dest_pgdir_t[vpn]);
                pgcopy((PTE *)pa2kva(get_pa(dest_pgdir_t[vpn])), (PTE *)pa2kva(get_pa(src_pgdir_t[vpn])), level-1);
            }
            else{
                //这里不能调用palloc函数，要直接对PTE表项进行操作，指向同一个物理页
                memset(&dest_pgdir_t[vpn], 0, sizeof(PTE));
                set_pfn(&dest_pgdir_t[vpn], get_pa(src_pgdir_t[vpn]) >> NORMAL_PAGE_SHIFT);
                set_attribute(&dest_pgdir_t[vpn],get_attribute(src_pgdir_t[vpn],PA_ATTRIBUTE_MASK) & ~_PAGE_WRITE);

                page_general[kva2pgindex(pa2kva(get_pa(src_pgdir_t[vpn])))].using++;//对应的物理页的使用数量会增加！
            }
        }
        else 
            continue;
    }

    return;
}

pid_t do_fork(){

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;

    for(int id=0; id < NUM_MAX_TASK; id++){
        if(pcb[id].status == TASK_EXITED){

            pcb[id].recycle = 0;
            pcb[id].pgdir = (PTE*)kalloc();//分配根目录页//这里的给出的用户映射的虚地址没有任何意义
            //clear_pgdir(pcb[id].pgdir); //清空根目录页
            // share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
            // load_task_img(i,pcb[id].pgdir,id+2);//load进程并且为给进程建立好地址映射(这一步实际上包括了建立好除了根目录页的所有页表以及除了栈以外的所有映射)
            share_pgtable(pcb[id].pgdir,(PTE*)pa2kva(PGDIR_PA));//内核地址映射拷贝
            pgcopy(pcb[id].pgdir, (PTE*)pa2kva(PGDIR_PA), 2);

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

/*
 * kalloc()负责分配出一个物理页并返回内核虚地址，在分配的过程中可能会涉及到页的换入换出
 */
uintptr_t malloc(){

    for(int j = 0;j < PAGE_NUM;j++)
    {
        for(unsigned i = 0;i < PAGE_NUM;i++)
        {
            //若有还没用到的页，直接返回即可
            if(page_general[i].valid == 0){
                page_general[i].valid = 1;
                clear_pgdir(pgindex2kva(i));
                return pgindex2kva(i);
            }
        }
        //swap_out负责换出某一页；并将这一页的index返回
        unsigned index= swap_out();
        assert(index!=-1);

        page_general[index].valid = 1;
        clear_pgdir(pgindex2kva(index));
        return pgindex2kva(index);
    }

    return 0;
}

/*
 * kalloc()负责根目录页的分配以及内核栈的分配；即所有只局限于内核中的，不涉及到用户页表的分配
 */
uintptr_t kalloc(){
    uintptr_t kva = malloc();
    assert(kva);

    //将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    page_general[kva2pgindex(kva)].pte = NULL;
    page_general[kva2pgindex(kva)].pin = 1;
    page_general[kva2pgindex(kva)].using = 1;
    return kva;
}    

/*
 * palloc()负责页表的分配，要考虑pte项的置位
 */
uintptr_t palloc(PTE* pte){
    uintptr_t kva = malloc();
    assert(kva);

    memset(pte, 0, sizeof(PTE));
    set_pfn(pte, kva2pa(kva) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
    set_attribute(pte,_PAGE_PRESENT);

    //将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    page_general[kva2pgindex(kva)].pte = pte;
    page_general[kva2pgindex(kva)].pin = 1;
    page_general[kva2pgindex(kva)].using = 1;
    return kva;
}  

/*
 * va是要映射的虚地址(必然是用户的虚地址)，pgdir是用户的根目录页，pa是实地址，perm是对应的权限项
 * mappages负责把给出的虚实地址映射全部建立好，并返回最后一级的pte表项地址
 */
PTE* mappages(uintptr_t va, PTE* pgdir, uintptr_t pa, uint64_t perm){
    //首先中间页表建立好
    PTE* pte = (PTE*)walk(va, pgdir, ALLOC);
    assert(pte);

    //然后将最后一级的pte项写入物理地址和权限位
    memset(pte, 0, sizeof(PTE));
    set_pfn(pte, pa >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, perm);

    return pte;
}

/*
 * uvmalloc主要负责建立用户末级页的映射，同时对倒排数组进行注册
 */
uintptr_t uvmalloc(uintptr_t va, PTE* pgdir, uint64_t perm){
    //分配并建立完所有映射
    uintptr_t kva = malloc();
    assert(kva);
    PTE* pte = mappages(va, pgdir, kva2pa(kva), perm);

    //将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    page_general[kva2pgindex(kva)].pte = pte;
    page_general[kva2pgindex(kva)].pin = 1;
    page_general[kva2pgindex(kva)].using = 1;

    return kva;
}


