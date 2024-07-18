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
                *pte = 0;
                pgdir_t = (PTE*)allocPage(1,1,pgdir);
                set_pfn(pte, kva2pa((uintptr_t)pgdir_t) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
                set_attribute(pte,_PAGE_PRESENT);
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
    //page_general[node_index].using
    //page_general[node_index].kva

    page_general[node_index].va  = -1;
    page_general[node_index].table_not  = 0;
}

unsigned swap_out(){//swapout函数只负责选中一页然后换出，不负责将某一页重新填入或者分配出去

    static unsigned swap_block_id = 0x200;
    static unsigned swap_index = 0;
    PTE * swap_PTE;//将要换出去的物理页对应的表项
    for(unsigned i = swap_index;i < PAGE_NUM;i = (i+1)%PAGE_NUM)
    {
        if(!page_general[i].pin && !page_general[i].table_not){
            page_general[i].valid = 0;

            // page_general[i].pin   = pin;
            // page_general[i].using = 1;

            // page_general[i].pid   = (*current_running)->pid;
            // page_general[i].pgdir = (*current_running)->pgdir;
            // page_general[i].va    = va;
            PTE* pgdir = get_pcb()->pgdir;
            swap_PTE = (PTE*)walk(page_general[i].va, pgdir,ALLOC);
            assert(swap_PTE);
            *swap_PTE = 0;

            set_pfn(swap_PTE, swap_block_id);//将硬盘上的扇区号写入ppn
            set_attribute(swap_PTE, _PAGE_SOFT | _PAGE_USER);//软件位拉高
            local_flush_tlb_all();

            bios_sd_write(kva2pa(page_general[i].kva),8,swap_block_id);//将物理地址和所写的扇区号传入

            printl("\npage[%d](kva : 0x%x) has been swapped to block id[%d]\n",i,page_general[i].kva,swap_block_id);

            swap_block_id += 8;//扇区号加8
            swap_index = (i+1)%PAGE_NUM;//从下一个开始算起，为fifo算法
            return i;//返回数组中的下标
        }
    }

    return 0;
}

ptr_t allocPage(int numPage,int pin,PTE* pgdir)//返回的是物理地址对应的内核虚地址
{
    uintptr_t ret = 0;

    for(int j = 0;j < numPage;j++)
    {
        for(unsigned i = 0;i < PAGE_NUM;i++)
        {
            if(page_general[i].valid == 0){
                page_general[i].valid = 1;

                page_general[i].pin   = pin;
                page_general[i].using = 1;

                if(pgdir)
                    page_general[i].pgdir = pgdir;
                else
                    page_general[i].pgdir = (PTE*)page_general[i].kva;

                clear_pgdir(page_general[i].kva);
                if(j == 0)
                    ret = page_general[i].kva;//kva初始化的时候已经被设置好了
                break;
            }
        }
        if(ret != 0)//如果前面已经被分配出，则直接结束该循环
            continue;
        unsigned index= swap_out();//如果没有空余的，则在换出之后返回一个被换出的数组下标//这里只用单纯的去找能换出的页表项即可

        page_general[index].valid = 1;

        page_general[index].pin   = pin;
        page_general[index].using = 1;

        if(pgdir)
            page_general[index].pgdir = pgdir;
        else
            page_general[index].pgdir = (PTE*)page_general[index].kva;
        ret = page_general[index].kva;
        clear_pgdir(page_general[index].kva);
    }

    return ret;
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

                            page_general[node_index].va  = -1;

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

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
//这是专门为用户建立页表，用户进行映射准备的函数，其他的比如建立内核栈，建立映射页表都是用户不可见，因此不会使用该函数
uintptr_t alloc_page_helper(uintptr_t va, PTE* pgdir, int pin,int pid)//用户给出一个虚地址以及页目录地址，返回给出一个内核分配的物理页的虚地址映射
//helper的作用在于由他分配出去的肯定不是页表，或者说除了例外处理的时候不用他多分配，其他时候都用它帮忙进行分配(要求分配的东西不是内核的东西)
//注意这个函数的精髓之处在于因为是用户，因此要建立映射，而对于内核栈和页表而言，不需要重新建立映射，因此不会调用此函数
{
    PTE * set_PTE;  

    set_PTE = (PTE*)walk(va, pgdir, ALLOC);//这里不仅会进行寻找表项，如果没有还会将页表全部建立起来
    assert(set_PTE);

    *set_PTE = 0;
    uint64_t pa = kva2pa(allocPage(1,pin,pgdir));//为给页表项分配出一个物理页，并且根据传参确定是否pin住
    
    set_pfn(set_PTE,(pa >> NORMAL_PAGE_SHIFT));
    // set_attribute(set_PTE,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
    //                          | _PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
    set_attribute(set_PTE,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                        | _PAGE_USER);
    // printl("vpn2 %d %d vpn1 %d %d vpn0 %d %d pa %d | %d %d %d\n",vpn2,&pgdir_t[vpn2],vpn1,&pmd[vpn1],vpn0,&pmd2[vpn0],pa,pgdir_t[vpn2],pmd[vpn1],pmd2[vpn0]);                         
    
    local_flush_tlb_all();
    return pa2kva(pa);
    // TODO [P4-task1] alloc_page_helper:
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

void copy_pagetable(PTE* dest_pgdir,PTE* src_pgdir,int pid){
    PTE * src_pgdir_t = (PTE *)src_pgdir;
    PTE * dest_pgdir_t = (PTE *)dest_pgdir;
    
    for(unsigned vpn2 = 0;vpn2 < (NUM_PTE_ENTRY >> 1);vpn2++){
        if(src_pgdir_t[vpn2] % 2 != 0){//页表对应的p位是1,则需要进行拷贝
            // printl("IN ! %d\n",pgdir_t[vpn2]);
            // alloc second - level page
            dest_pgdir_t[vpn2] = 0;
            set_pfn(&dest_pgdir_t[vpn2], kva2pa(allocPage(1,1,dest_pgdir)) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
            set_attribute(&dest_pgdir_t[vpn2],_PAGE_PRESENT);
            //clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));//事实上就是将刚刚allocpage的页清空
        }
        else 
            continue;

        for(unsigned vpn1 = 0;vpn1 < 512;vpn1++){
            PTE *src_pmd = (PTE *)pa2kva(get_pa(src_pgdir_t[vpn2]));
            PTE *dest_pmd = (PTE *)pa2kva(get_pa(dest_pgdir_t[vpn2]));
    
            if(src_pmd[vpn1] % 2 != 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
            // alloc third - level page
                dest_pmd[vpn1] = 0;
                set_pfn(&dest_pmd[vpn1], kva2pa(allocPage(1,1,dest_pgdir)) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
                set_attribute(&dest_pmd[vpn1],_PAGE_PRESENT);
                //clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
            }
            else 
                continue;

            for(unsigned vpn0 = 0;vpn0 < 512;vpn0++){
                PTE *src_pmd2 = (PTE *)pa2kva(get_pa(src_pmd[vpn1]));
                PTE *dest_pmd2 = (PTE *)pa2kva(get_pa(dest_pmd[vpn1]));

                if(src_pmd2[vpn0] % 2 != 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
                // alloc third - level page
                    dest_pmd2[vpn0] = 0;
                    set_pfn(&dest_pmd2[vpn0], get_pa(src_pmd2[vpn0]) >> NORMAL_PAGE_SHIFT);//这里最后一级建立映射，把源的物理地址放进去即可
                    // set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                    //          | _PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
                    set_attribute(&dest_pmd2[vpn0],get_attribute(src_pmd2[vpn0],PA_ATTRIBUTE_MASK) & ~_PAGE_WRITE);
                    //clear_pgdir(pa2kva(get_pa(pmd2[vpn0])));

                    uint32_t node_index = (pa2kva(get_pa(src_pmd2[vpn0])) - FREEMEM_KERNEL)/PAGE_SIZE;
                    page_general[node_index].using++;//对应的物理页的使用数量会增加！
                    
                }
                else 
                    continue; 
            }
        }
    }

    //printk("search PTE error");
    return;
}

pid_t do_fork(){

    current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;

    for(int id=0; id < NUM_MAX_TASK; id++){
        if(pcb[id].status == TASK_EXITED){

            pcb[id].recycle = 0;
            pcb[id].pgdir = (PTE*)allocPage(1,1,NULL);//分配根目录页//这里的给出的用户映射的虚地址没有任何意义
            //clear_pgdir(pcb[id].pgdir); //清空根目录页
            // share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
            // load_task_img(i,pcb[id].pgdir,id+2);//load进程并且为给进程建立好地址映射(这一步实际上包括了建立好除了根目录页的所有页表以及除了栈以外的所有映射)
            share_pgtable(pcb[id].pgdir,(PTE*)pa2kva(PGDIR_PA));//内核地址映射拷贝
            copy_pagetable(pcb[id].pgdir,(*current_running)->pgdir,id+2);

            pcb[id].kernel_sp  = allocPage(1,1,pcb[id].pgdir) + 1 * PAGE_SIZE;//这里的给出的用户映射的虚地址没有任何意义
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
