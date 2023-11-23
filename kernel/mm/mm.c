#include <pgtable.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/kernel.h>
// #include <stdint.h>
// #include <stdint.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;
page_allocated page_general[PAGE_NUM];
share_page share_pages[SHARE_PAGE_NUM];
extern void ret_from_exception();
// unsigned page_head = 0;

//用户给出一个虚地址和根目录页，将映射全部建立好(如果已经有映射则不用建立)，最终返回末级页表的地址
//所有的对页表的分配都是在这个函数里进行(被封装起来)，并且页表一定会被pin住
//注意页表的分配，其中的va并不像分配物理页中的va具有任何意义
PTE * search_and_set_PTE(uintptr_t va, uintptr_t pgdir,int pid)
{
    va &= VA_MASK;
    PTE * pgdir_t = (PTE *)pgdir;
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));//页目录虚地址
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));//二级页表虚地址
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << (PPN_BITS)) ^
                    (va   >> (NORMAL_PAGE_SHIFT));//三级页表虚地址
    
    if(pgdir_t[vpn2] % 2 == 0){//页表对应的p位必然是1，除非没有被分配过
        // printl("IN ! %d\n",pgdir_t[vpn2]);
        // alloc second - level page
        pgdir_t[vpn2] = 0;
        set_pfn(&pgdir_t[vpn2], kva2pa(allocPage(1,1,va,1,pid)) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
        //set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT);
        //clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));//事实上就是将刚刚allocpage的页清空
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
    
    if(pmd[vpn1] % 2 == 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
        // alloc third - level page
        pmd[vpn1] = 0;
        set_pfn(&pmd[vpn1], kva2pa(allocPage(1,1,va,1,pid)) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
        //set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        set_attribute(&pmd[vpn1],_PAGE_PRESENT);
        //clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));  

    return (pmd2 + vpn0);//直接返回了对应的页表项地址
}

uintptr_t search_PTE(uintptr_t kva, uintptr_t pgdir,int pid)//这个函数只会单纯的去找表项，不会在没有找到的时候分配页表
//传进的是内核虚地址，并且由于是sharepages数组因此不会再这个函数里进行分配而是直接把虚地址传进来
{
    uintptr_t va;
    PTE * pgdir_t = (PTE *)pgdir;
    // uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));//页目录虚地址
    // uint64_t vpn1 = (vpn2 << PPN_BITS) ^
    //                 (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));//二级页表虚地址
    // uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
    //                 (vpn1 << (PPN_BITS)) ^
    //                 (va   >> (NORMAL_PAGE_SHIFT));//三级页表虚地址
    
    for(unsigned vpn2 = 0;vpn2 < 512;vpn2++){
        if(pgdir_t[vpn2] % 2 == 0){//页表对应的p位必然是1，除非没有被分配过
            // printl("IN ! %d\n",pgdir_t[vpn2]);
            // alloc second - level page
            pgdir_t[vpn2] = 0;
            set_pfn(&pgdir_t[vpn2], kva2pa(allocPage(1,1,0,1,pid)) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
            set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT);
            //clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));//事实上就是将刚刚allocpage的页清空
        }
        else 
            continue;

        for(unsigned vpn1 = 0;vpn1 < 512;vpn1++){
            PTE *pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
    
            if(pmd[vpn1] % 2 == 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
            // alloc third - level page
                pmd[vpn1] = 0;
                set_pfn(&pmd[vpn1], kva2pa(allocPage(1,1,0,1,pid)) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
                set_attribute(&pmd[vpn1],_PAGE_PRESENT);
                //clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
            }
            else 
                continue;

            for(unsigned vpn0 = 0;vpn0 < 512;vpn0++){
                PTE *pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));

                if(pmd2[vpn0] % 2 == 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
                // alloc third - level page
                    pmd2[vpn0] = 0;
                    set_pfn(&pmd2[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
                    // set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                    //          | _PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
                    set_attribute(&pmd2[vpn0],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                                            | _PAGE_USER);
                    //clear_pgdir(pa2kva(get_pa(pmd2[vpn0])));

                    va = (vpn2 << (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) |
                         (vpn1 << (NORMAL_PAGE_SHIFT + PPN_BITS)) |
                         (vpn0 << (NORMAL_PAGE_SHIFT));

                    return va;
                }
                else 
                    continue; 
            }
        }
    }

    printk("search PTE error");
    // if(pgdir_t[vpn2] % 2 == 0){//页表对应的p位必然是1，除非没有被分配过
    //     // printl("IN ! %d\n",pgdir_t[vpn2]);
    //     // alloc second - level page
    //     pgdir_t[vpn2] = 0;
    //     set_pfn(&pgdir_t[vpn2], kva2pa(allocPage(1,1,va,1,pid)) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
    //     set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    //     clear_pgdir(pa2kva(get_pa(pgdir_t[vpn2])));//事实上就是将刚刚allocpage的页清空
    // }

    // PTE *pmd = (PTE *)pa2kva(get_pa(pgdir_t[vpn2]));
    
    // if(pmd[vpn1] % 2 == 0){//然后对二级页表的虚地址进行操作//可能会出现前面几级页表一样，最后一级不一样
    //     // alloc third - level page
    //     pmd[vpn1] = 0;
    //     set_pfn(&pmd[vpn1], kva2pa(allocPage(1,1,va,1,pid)) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
    //     set_attribute(&pmd[vpn1],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    //     clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    // }

    // PTE *pmd2 = (PTE *)pa2kva(get_pa(pmd[vpn1]));  

    // return (pmd2 + vpn0);//直接返回了对应的页表项地址
}

void clear_pagearray(uint32_t node_index){
    page_general[node_index].valid = 0;
    page_general[node_index].pin = 0;
    //page_general[node_index].using
    //page_general[node_index].kva

    page_general[node_index].pid = -1;
    page_general[node_index].pgdir = -1;
    page_general[node_index].va  = -1;
    page_general[node_index].table_not  = 0;
}

unsigned swap_out(){//swapout函数只负责选中一页然后换出，不负责将某一页重新填入或者分配出去

    static swap_block_id = 0x200;
    PTE * swap_PTE;//将要换出去的物理页对应的表项
    for(unsigned i = 0;i < PAGE_NUM;i++)
    {
        if(!page_general[i].pin && !page_general[i].table_not){
            page_general[i].valid = 0;

            // page_general[i].pin   = pin;
            // page_general[i].using = 1;

            // page_general[i].pid   = (*current_running)->pid;
            // page_general[i].pgdir = (*current_running)->pgdir;
            // page_general[i].va    = va;
            swap_PTE = search_and_set_PTE(page_general[i].va, page_general[i].pgdir,0);
            *swap_PTE = 0;

            set_pfn(swap_PTE, swap_block_id);//将硬盘上的扇区号写入ppn
            set_attribute(swap_PTE, _PAGE_SOFT | _PAGE_USER);//软件位拉高
            local_flush_tlb_all();

            bios_sd_write(kva2pa(page_general[i].kva),8,swap_block_id);//将物理地址和所写的扇区号传入
            swap_block_id += 8;//扇区号加8

            return i;//返回数组中的下标
        }
    }
    // while(am_siz > 0 && am_pool[am_head].valid == 0){
    //     am_head ++;
    //     am_siz--;
    // }
    // if(am_siz == 0){
    //     swap_error();
    //     return;
    // }
    // for(int i=0; i<4096; i++){
    //     if(sw_pool[i].valid == 0){
    //         if(i > sw_top)
    //             sw_top = i;
    //         uint64_t pa = am_pool[am_head].pa;
    //         sw_pool[i].valid = 1;
    //         sw_pool[i].pid   = (*current_running)->parent_id ? (*current_running)->parent_id : (*current_running)->pid;
    //         sw_pool[i].va    = am_pool[am_head].va;
    //         sw_pool[i].pmd3  = am_pool[am_head].pmd3;
    //         am_head++;
    //         if(am_head == 4096)
    //             am_head = 0;
    //         am_siz--;
    //         *(sw_pool[i].pmd3) = (PTE) 0; 
    //         local_flush_tlb_all();
    //         uint64_t bias = padding_ADDR/512;
    //         bios_sdwrite(pa,8,bias+8*i); 
    //         printl("swap successful ! %ld\n",sw_pool[i].va);
    //         return pa2kva(pa);          
    //     }
    // }    
    // swap_error();
    return 0;
}

ptr_t allocPage(int numPage,int pin,uintptr_t va,int table_not,int pid)//返回的是物理地址对应的内核虚地址
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

                page_general[i].pid   = pid;
                page_general[i].pgdir = pcb[pid-2].pgdir;//这里找寻的还是pcb数组
                page_general[i].va    = va;
                page_general[i].table_not    = table_not;

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

        page_general[index].pid   = pid;
        page_general[index].pgdir = pcb[pid-2].pgdir;//这里找寻的还是pcb数组
        page_general[index].va    = va;
        page_general[index].table_not    = table_not;
        ret = page_general[index].kva;
        clear_pgdir(page_general[index].kva);
    }

    return ret;
    // ptr_t ret;
    // if(fm_head >= 0){
    //     int temp = fm_head;
    //     ret = fm_head * PAGE_SIZE + FREEMEM_KERNEL;
    //     fm_head = fm_pool[fm_head].nxt;
    //     fm_pool[temp].nxt = -1;
    //     fm_pool[temp].valid = 0;
    //     // printl("ret node %ld\n",ret);
    //     return ret;
    // }
    
    // ret = ROUND(kernMemCurr, PAGE_SIZE);
    // if(ret + numPage * PAGE_SIZE <= FREEMEM_END){
    //     kernMemCurr = ret + numPage * PAGE_SIZE;
    //     return ret;        
    // }
    // ret = swap_page();
    // return ret;
    // // align PAGE_SIZE
    // ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    // kernMemCurr = ret + numPage * PAGE_SIZE;
    // return ret;
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

void free_all_pagemapping(ptr_t baseAddr){//通过根目录页基址，取消所有映射
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

void free_all_pagetable(ptr_t baseAddr){//取消并回收所有的页表
    PTE *pgdir = (PTE *)baseAddr;
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

// typedef struct page_allocated{
//     int valid;//是否被分配，已经被分配则置为1
    
//     int pin;//如果置成1，则该页不允许被换出
//     int using;//如果是共享内存，该变量记录有多少个进程正在共享

//     uintptr_t kva;//对应的内核虚地址

//     int pid;//对应的进程号
//     uintptr_t pgdir;//对应进程的根目录页
//     uintptr_t va;//对应进程对该物理地址的虚地址
// }page_allocated;
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

                            page_general[node_index].pid = -1;
                            page_general[node_index].pgdir = -1;
                            page_general[node_index].va  = -1;

                            //取消映射
                        }
                    }
                }
            }
        }
    }
}
// void freePage(ptr_t baseAddr)
// {
//     PTE *pgdir = (PTE *)baseAddr;
//     PTE *pmd, *pmd2, *pmd3;
//     int last = fm_head;
//     current_running = get_current_cpu_id() ? &current_running_1 : &current_running_0;
//     for(int i=0; i<512; i++){
//         if(pgdir[i] % 2){
//             pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            
//             if(pmd < FREEMEM_KERNEL) continue; //kernal no release
//             // printl("pmd %ld \n",pmd);

//             for(int j = 0; j<512; ++j){
//                 if(pmd[j] % 2){
//                     pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
//                     for(int k = 0; k<512; ++k){
//                         if(pmd2[k] % 2){
//                             pmd3 = (PTE *)pa2kva(get_pa(pmd2[k]));
//                             uint32_t node_index = ((uint64_t)pmd3 - FREEMEM_KERNEL)/PAGE_SIZE;
//                             if(fm_pool[node_index].valid)continue;
//                             fm_pool[node_index].valid = 1;
//                             fm_pool[node_index].nxt = -1;
//                             if(fm_head == -1){
//                                 fm_head = node_index;
//                                 last = fm_head;
//                             }
//                             else{
//                                 for(;fm_pool[last].nxt >= 0; last = fm_pool[last].nxt);
//                                 fm_pool[last].nxt = node_index;
//                             }
//                         }
//                     }
//                     uint32_t node_index = ((uint64_t)pmd2 - FREEMEM_KERNEL)/PAGE_SIZE;
//                     if(fm_pool[node_index].valid)continue;
//                     fm_pool[node_index].valid = 1;
//                     fm_pool[node_index].nxt = -1;
//                     if(fm_head == -1){
//                         fm_head = node_index;
//                         last = fm_head;
//                     }
//                     else{
//                         for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
//                         fm_pool[last].nxt = node_index;
//                     }
//                 }
//             }
//             uint32_t node_index = ((uint64_t)pmd - FREEMEM_KERNEL)/PAGE_SIZE;
//             if(fm_pool[node_index].valid)continue;
//             fm_pool[node_index].valid = 1;
//             fm_pool[node_index].nxt = -1;
//             if(fm_head == -1){
//                 fm_head = node_index;
//                 last = fm_head;
//             }
//             else{
//                 for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
//                 fm_pool[last].nxt = node_index;
//             }
//         }
//     }
//     uint32_t node_index = ((uint64_t)pgdir - FREEMEM_KERNEL)/PAGE_SIZE;
//     if(fm_pool[node_index].valid == 0){
//         fm_pool[node_index].valid = 1;
//         fm_pool[node_index].nxt = -1;
//         if(fm_head == -1){
//             fm_head = node_index;
//             last = fm_head;
//         }
//         else{
//             for(;fm_pool[last].nxt >= 0;last = fm_pool[last].nxt);
//             fm_pool[last].nxt=node_index;
//         }        
//     }

//     for(int i=0; i <= sw_top; i++){
//         if(sw_pool[i].pid == (*current_running)->pid)
//             sw_pool[i].valid = 0; 

//     }
//     while(sw_top >=0&&sw_pool[sw_top].valid ==0)
//         sw_top--;
//     for(int i=0; i < 4096; i++){
//         if(am_pool[i].pid == (*current_running)->pid)
//             am_pool[i].valid = 0;
//     }
//     // TODO [P4-task1] (design you 'freePage' here if you need):
// }

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
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
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, int pin,int pid)//用户给出一个虚地址以及页目录地址，返回给出一个内核分配的物理页的虚地址映射
//helper的作用在于由他分配出去的肯定不是页表，或者说除了例外处理的时候不用他多分配，其他时候都用它帮忙进行分配(要求分配的东西不是内核的东西)
//注意这个函数的精髓之处在于因为是用户，因此要建立映射，而对于内核栈和页表而言，不需要重新建立映射，因此不会调用此函数
{
    PTE * set_PTE;  

    set_PTE = search_and_set_PTE(va, pgdir,pid);//这里不仅会进行寻找表项，如果没有还会将页表全部建立起来

    *set_PTE = 0;
    uint64_t pa = kva2pa(allocPage(1,pin,va,0,pid));//为给页表项分配出一个物理页，并且根据传参确定是否pin住
    
    set_pfn(set_PTE,(pa >> NORMAL_PAGE_SHIFT));
    // set_attribute(set_PTE,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
    //                          | _PAGE_ACCESSED| _PAGE_DIRTY| _PAGE_USER);
    set_attribute(set_PTE,_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                        | _PAGE_USER);
    // printl("vpn2 %d %d vpn1 %d %d vpn0 %d %d pa %d | %d %d %d\n",vpn2,&pgdir_t[vpn2],vpn1,&pmd[vpn1],vpn0,&pmd2[vpn0],pa,pgdir_t[vpn2],pmd[vpn1],pmd2[vpn0]);                         
    
    // if(swap && am_siz < 4096){
    //     am_pool[am_tail].pid  = (*current_running)->parent_id ? (*current_running)->parent_id : (*current_running)->pid;
    //     am_pool[am_tail].pa   = pa;
    //     am_pool[am_tail].pmd3 = &pmd2[vpn0];
    //     am_pool[am_tail].va   = (va >> 12) << 12;
    //     am_pool[am_tail].valid=1;
    //     am_tail++;
    //     if(am_tail == 4096)
    //         am_tail = 0;
    //     am_siz++;
    //     printl("get valid swap page ! %d %ld\n",va,pmd2[vpn0]);
    // }
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
        va = search_PTE(share_pages[id].kva, (*current_running)->pgdir,(*current_running)->pid);
        //shm_map(va,shms[id].pa,(*current_running)->pgdir);
        local_flush_tlb_all();
    }
    else{
        share_pages[id].valid = 1;
        
        share_pages[id].key = key;
        share_pages[id].using = 1;
        share_pages[id].pin = 1;

        clear_pgdir(share_pages[id].kva);//共享物理页要求在没有人用，全部解除映射时就清空
        va = search_PTE(share_pages[id].kva, (*current_running)->pgdir,(*current_running)->pid);
        // uintptr_t va = USER_STACK_ADDR - (2 + ((*current_running)->shm_num)) * PAGE_SIZE;
        // printl("MAP pgdir = %ld addr = %d\n",(*current_running)->pgdir,va);
        // shm_map(va,shms[id].pa,(*current_running)->pgdir);
        // printl("va = %ld pa =%ld\n",va,shms[id].pa);
        local_flush_tlb_all();       
    }

    return va; 
    // TODO [P4-task4] shm_page_get:
}


uintptr_t free_pagemapping(uintptr_t va,ptr_t pgdir){//通过根目录页基址和用户虚地址来取消对应的映射
    PTE* pmd;
    PTE* pmd2;
    uintptr_t pmd3;

    PTE * pgdir_t = (PTE *)pgdir;
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
    // share_pages[node_index].valid = 
    // for(int i=0;i<SHM_NUM;i++){
    //     if(share_pages[i].valid == 0)continue;
    //     printl("%ld %ld\n",share_pages[i].pa,pa);
    //     if(share_pages[i].pa == pa){
    //         printl("HAPPEN!\n");
    //         share_pages[i].siz--;
    //         if(share_pages[i].siz == 0)share_pages[i].valid = 0;
    //         break;
    //     }
    // }
    // TODO [P4-task4] shm_page_dt:
}

void copy_pagetable(uintptr_t dest_pgdir,uintptr_t src_pgdir,int pid){
    PTE * src_pgdir_t = (PTE *)src_pgdir;
    PTE * dest_pgdir_t = (PTE *)dest_pgdir;
    // uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));//页目录虚地址
    // uint64_t vpn1 = (vpn2 << PPN_BITS) ^
    //                 (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS));//二级页表虚地址
    // uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
    //                 (vpn1 << (PPN_BITS)) ^
    //                 (va   >> (NORMAL_PAGE_SHIFT));//三级页表虚地址
    
    for(unsigned vpn2 = 0;vpn2 < (NUM_PTE_ENTRY >> 1);vpn2++){
        if(src_pgdir_t[vpn2] % 2 != 0){//页表对应的p位是1,则需要进行拷贝
            // printl("IN ! %d\n",pgdir_t[vpn2]);
            // alloc second - level page
            dest_pgdir_t[vpn2] = 0;
            set_pfn(&dest_pgdir_t[vpn2], kva2pa(allocPage(1,1,0,1,pid)) >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
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
                set_pfn(&dest_pmd[vpn1], kva2pa(allocPage(1,1,0,1,pid)) >> NORMAL_PAGE_SHIFT);//这里分配出去的时页表页，并且一定会被pin住
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
                    // va = (vpn2 << (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) |
                    //      (vpn1 << (NORMAL_PAGE_SHIFT + PPN_BITS)) |
                    //      (vpn0 << (NORMAL_PAGE_SHIFT));

                    // return va;
                    
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
            pcb[id].pgdir = allocPage(1,1,0,1,id+2);//分配根目录页//这里的给出的用户映射的虚地址没有任何意义
            //clear_pgdir(pcb[id].pgdir); //清空根目录页
            // share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
            // load_task_img(i,pcb[id].pgdir,id+2);//load进程并且为给进程建立好地址映射(这一步实际上包括了建立好除了根目录页的所有页表以及除了栈以外的所有映射)
            share_pgtable(pcb[id].pgdir,pa2kva(PGDIR_PA));//内核地址映射拷贝
            copy_pagetable(pcb[id].pgdir,(*current_running)->pgdir,id+2);

            pcb[id].kernel_sp  = allocPage(1,1,0,0,id+2) + 1 * PAGE_SIZE;//这里的给出的用户映射的虚地址没有任何意义
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

// pid_t do_fork()
// {
//     int i, j;
//     char buff[32];
//     for (i = 0; i < NUM_MAX_TASK; i++){
//         if (pcb[i].status == TASK_EXITED){
//             pcb[i].kill = 0;
//             pcb[i].pid = i+2;
//             pcb[i].status = TASK_READY;
//             break;
//         }
//     }
//     if (i == NUM_MAX_TASK)
//         return 0;
    
//     uint64_t kernel_stack = (uint64_t)allocPage(1,1,0,0,i+2) + PAGE_SIZE;
//     pcb[i].kernel_stack_base = kernel_stack - PAGE_SIZE;
    
//     regs_context_t *father_pt_regs = current_running[get_current_cpu_id()]->kernel_sp - sizeof(regs_context_t);
//     regs_context_t *son_pt_regs    = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
//     memcpy((uint8_t*)son_pt_regs, (uint8_t*)father_pt_regs, sizeof(regs_context_t));
//     son_pt_regs->regs[4]  = &pcb[i];  
//     son_pt_regs->regs[10] = 0;
    
//     switchto_context_t *son_pt_switchto = (switchto_context_t *)((uint64_t)son_pt_regs - sizeof(switchto_context_t));
//     pcb[i].kernel_sp = (uint64_t)son_pt_switchto;
//     son_pt_switchto->regs[0] = ret_from_exception;
//     son_pt_switchto->regs[1] = (uint64_t)son_pt_switchto;
    
//     PTE* son_pgdir = (PTE*)allocPage(1,1,0,1,i+2);
//     pcb[i].pgdir = (uint64_t)son_pgdir;
//     share_pgtable(son_pgdir, (PTE*)pa2kva(PGDIR_PA));
    
//     PTE* father_pgdir = (PTE*)current_running[get_current_cpu_id()]->pgdir;
//     for (uint64_t i = 0; i < (NUM_PTE_ENTRY >> 1); i++){
//         if (father_pgdir[i] & _PAGE_PRESENT){
//             PTE* son_pmd = (PTE*)allocPage(1,1,0,1,pcb[i].pid);
//             set_pfn(&son_pgdir[i], kva2pa((uint64_t)son_pmd) >> NORMAL_PAGE_SHIFT);
//             set_attribute(&son_pgdir[i], _PAGE_PRESENT);
//             PTE* father_pmd = (PTE*)pa2kva(get_pfn(father_pgdir[i]));
//             for (uint64_t j = 0; j < NUM_PTE_ENTRY; j++){
//                 if (father_pmd[j] & _PAGE_PRESENT){
//                     PTE* son_pgt = (PTE*)allocPage(1,1,0,1,pcb[i].pid);
//                     set_pfn(&son_pmd[j], kva2pa((uint64_t)son_pgt) >> NORMAL_PAGE_SHIFT);
//                     set_attribute(&son_pmd[j], _PAGE_PRESENT);
//                     PTE* father_pgt = (PTE*)pa2kva(get_pfn(father_pmd[j]));
//                     for (uint64_t k = 0; k < NUM_PTE_ENTRY; k++){
//                         if (father_pgt[k] & _PAGE_PRESENT){
//                             memcpy((uint8_t*)&son_pgt[k], (uint8_t*)&father_pgt[k], sizeof(uint64_t));
//                             uint64_t perm = get_attribute(son_pgt[k],PA_ATTRIBUTE_MASK);
//                             set_attribute(&son_pgt[k], perm & ~_PAGE_WRITE);
//                         }
//                         else{
//                             continue;
//                         }
//                     }
//                 }
//                 else{
//                     continue;
//                 }
//             }
//         }
//         else{
//             continue;
//         }
//     }
    
//     list_add(&ready_queue, &pcb[i].list);
    
//     return pcb[i].pid;
// }