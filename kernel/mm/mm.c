#include <assert.h>
#include <hash.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <pgtable.h>
#include <printk.h>

// NOTE: A/C-core
uint16_t swap_block_id = 0x200;
uint16_t swap_page_id;

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage) {
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;
}
#endif

/********** basic mm function ***********/

/*
 * 设置页表项
 */
void setPTE(PTE *pte, uintptr_t pa, uint64_t perm) {
    memset(pte, 0, sizeof(PTE));
    set_pfn(pte, pa >> NORMAL_PAGE_SHIFT);
    set_attribute(pte, perm);
}

/*
 * walk根据根目录页基址与用户虚拟地址进行页表项的映射与分配(不分配真正物理页，只返回末级页表的PTE表项)
 * ALLOC:中间如果有映射不成立，分配后继续walk
 * VOID:中间如果有映射不成立，walk失败，返回0
 */
uintptr_t walk(uintptr_t va, PTE *pgdir, enum WALK walk) {
    va &= VA_MASK;
    PTE *pgdir_t = pgdir;
    unsigned level;

    for (level = 2; level > 0; level--) {
        // 得到PTE表项地址
        PTE *pte = &pgdir_t[PX(level, va)];

        // _PAGE_PRESENT有效，则直接有pgdir_t
        if (*pte & _PAGE_PRESENT) {
            pgdir_t = (PTE *)pa2kva(get_pa(*pte));
        } else {
            // ALLOC的情况才会继续往下进行分配
            if (walk == ALLOC) {
                palloc(pte, _PAGE_PRESENT);
                pgdir_t = (PTE *)pa2kva(get_pa(*pte));
            } else {
                return 0;
            }
        }
    }

    /*
     * ALLOC与VOID的情况，返回对应的PTE表项即可(因为可能还没有对应的页表，需要外部进行set)
     */
    if (walk == ALLOC || walk == VOID)
        return (uintptr_t)&pgdir_t[PX(level, va)];

    return 0;
}

/*
 * swapout函数负责选中一页然后换出，返回这页对应的index，有kmalloc函数将这页重新分配出去
 * swapout采用clock算法，根据fqy来进行判断
 */
unsigned swap_out() {

    PTE *swap_PTE;
    uint8_t judge_pg;
    static int k=0;

    /*
     * 先看既没读过也没写过的页，再看只读过没写过的页，然后看读过写过的页
     */
    for (;;) {
        if (page_general[swap_page_id].pin == UNPINED && page_general[swap_page_id].used <= 1 &&
            page_general[swap_page_id].pte != NULL) {

            swap_PTE = page_general[swap_page_id].pte;
            judge_pg = !(*swap_PTE & (_PAGE_EXEC | _PAGE_READ | _PAGE_WRITE));

            // 目前采取保守策略，不换出页表(否则性能会很差)
            if (!judge_pg) {
                if (page_general[swap_page_id].fqy == 0) {
                    // 物理页free，设置表项，将物理页写进硬盘
                    uintptr_t kva = pgindex2kva(swap_page_id);
                    kmfree(kva);
                    setPTE(swap_PTE, swap_block_id << NORMAL_PAGE_SHIFT, _PAGE_SOFT | _PAGE_USER);
                    bios_sd_write(kva2pa(kva), 8,
                                  swap_block_id); //将物理地址和所写的扇区号传入
                    printl("\npage[%d](kva : 0x%x) has been swapped to block id[%d]\n",
                           swap_page_id, kva, swap_block_id);
                    k++;
                    if(k==234)
                        k++;

                    // block与page编号增加
                    swap_block_id += 8; //扇区号加8
                    uint16_t temp_id = swap_page_id;
                    swap_page_id = (swap_page_id + 1) % PAGE_NUM; 

                    local_flush_tlb_all();
                    return temp_id; 
                } else
                    page_general[swap_page_id].fqy--;
            }
        }
        swap_page_id = (swap_page_id + 1) % PAGE_NUM;
    }

    return -1;
}

/**********alloc***********/

/*
 * kmalloc()负责分配出一个物理页并返回内核虚地址，在分配的过程中可能会涉及到页的换入换出
 */
uintptr_t kmalloc() {

    for (int j = 0; j < PAGE_NUM; j++) {
        for (unsigned i = 0; i < PAGE_NUM; i++) {
            //若有还没用到的页，直接返回即可
            if (page_general[i].valid == 0) {
                page_general[i].valid = 1;
                page_general[i].used++;
                cleanpage(pgindex2kva(i));
                return pgindex2kva(i);
            }
        }
        // swap_out负责换出某一页；并将这一页的index返回
        unsigned index = swap_out();
        assert(index != -1);

        page_general[index].valid = 1;
        page_general[index].used++;
        cleanpage(pgindex2kva(index));
        return pgindex2kva(index);
    }

    return 0;
}

/*
 * kalloc()负责根目录页的分配以及内核栈的分配；即所有只局限于内核中的，不涉及到用户页表的分配
 */
uintptr_t kalloc() {
    uintptr_t kva = kmalloc();
    assert(kva);

    // 将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    uint32_t idx = kva2pgindex(kva);
    page_general[idx].pte = NULL;
    page_general[idx].pin = PINED;
    return kva;
}

/*
 * palloc()负责已知PTE地址的条件下，分配一页对PTE进行置位(不用项uvmalloc从根目录页alloc一遍)
 */
uintptr_t palloc(PTE *pte, uint64_t perm) {
    uintptr_t kva = kmalloc();
    assert(kva);

    //设置pte表项
    setPTE(pte, kva2pa(kva), perm);

    //将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    uint32_t idx = kva2pgindex(kva);
    page_general[idx].pte = pte;
    page_general[idx].pin = UNPINED;
    return kva;
}

/*
 * uvmalloc主要负责建立用户末级页的映射，同时对倒排数组进行注册
 */
uintptr_t uvmalloc(uintptr_t va, PTE *pgdir, uint64_t perm) {
    // 分配并建立完所有映射
    uintptr_t kva = kmalloc();
    assert(kva);
    PTE *pte = mappages(va, pgdir, kva2pa(kva), perm);

    // 将pte表项写入倒排数组中，方便后续swap时直接进行修改；这里默认全部pin住
    uint32_t idx = kva2pgindex(kva);
    page_general[idx].pte = pte;
    page_general[idx].pin = UNPINED;

    return kva;
}

/*
 * va是要映射的虚地址(必然是用户的虚地址)，pgdir是用jj户的根目录页，pa是实地址，perm是对应的权限项
 * mappages负责把给出的虚实地址映射全部建立好，并返回最后一级的pte表项地址
 */
PTE *mappages(uintptr_t va, PTE *pgdir, uintptr_t pa, uint64_t perm) {
    // 首先中间页表建立好
    PTE *pte = (PTE *)walk(va, pgdir, ALLOC);
    assert(pte);

    // 设置pte表项
    setPTE(pte, pa, perm);

    return pte;
}

/**********free***********/

/*
 * 取消某个物理页的映射，used--
 */
void kmfree(uintptr_t kva) {
    uint32_t pgindex = kva2pgindex(kva);

    if (!page_general[pgindex].used)
        ;
    else {
        page_general[pgindex].used--;
        if (!page_general[pgindex].used) {
            page_general[pgindex].valid = 0;
            page_general[pgindex].pin = UNPINED;
            page_general[pgindex].pte = NULL;
            page_general[pgindex].fqy = 0;
        }
    }
}

/*
 * 取消所有的用户页表映射
 */
void uvmfreeall(PTE *pgdir) {
    PTE *pmd, *pmd2;
    uintptr_t freekva;

    for (int i = 0; i < NUM_PTE_ENTRY; i++) {
        if (pgdir[i] & _PAGE_PRESENT) {
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            // 这里kernel页表不能进行处理的主要原因，是因为kernel映射拷贝时只拷贝了根目录页，后面的页表没有进行拷贝
            if ((uint64_t)pmd < FREEMEM_KERNEL)
                continue; // kernal no release
            for (int j = 0; j < NUM_PTE_ENTRY; j++) {
                if (pmd[j] & _PAGE_PRESENT) {
                    pmd2 = (PTE *)pa2kva(get_pa(pmd[j]));
                    for (int k = 0; k < NUM_PTE_ENTRY; k++) {
                        if (pmd2[k] & _PAGE_PRESENT) {
                            // 取消叶子节点的映射
                            freekva = pa2kva(get_pa(pmd2[k]));
                            kmfree(freekva);

                            // 取消映射
                            set_attribute(&pmd2[k], get_attribute(pmd2[k], PA_ATTRIBUTE_MASK) &
                                                        ~_PAGE_PRESENT);
                        }
                    }
                }
            }
        }
    }
}

/*
 * 取消某个用户虚地址对应物理页的映射
 */
uintptr_t uvmfree(PTE *pgdir, uintptr_t va) {
    // 先获得对应的pte表项
    PTE *pte = (PTE *)walk(va, pgdir, VOID);

    // 取消叶子节点的映射
    uintptr_t freekva = pa2kva(get_pa(*pte));
    kmfree(freekva);

    // 取消映射
    set_attribute(pte, get_attribute(*pte, PA_ATTRIBUTE_MASK) & ~_PAGE_PRESENT);
    return freekva;
}

/*
 * 取消所有页表并进行回收
 */
void mapfree(PTE *pgdir) {
    PTE *pgdir_t = pgdir;
    PTE *pmd;
    uintptr_t freekva;

    for (int i = 0; i < NUM_PTE_ENTRY; i++) {
        if (pgdir[i] & _PAGE_PRESENT) {
            pmd = (PTE *)pa2kva(get_pa(pgdir[i]));
            // 这里kernel页表不能进行处理的主要原因，是因为kernel映射拷贝时只拷贝了根目录页，后面的页表没有进行拷贝
            if ((uint64_t)pmd < FREEMEM_KERNEL)
                continue; // kernal no release
            for (int j = 0; j < NUM_PTE_ENTRY; j++) {
                if (pmd[j] & _PAGE_PRESENT) {
                    freekva = pa2kva(get_pa(pmd[j]));
                    kmfree(freekva);
                }
            }
            kmfree((uintptr_t)pmd);
        }
    }
    kmfree((uintptr_t)pgdir_t);
}

/**********share && copy***********/

/*
 * 将内核进程的根目录页拷贝给页表(注意并没有拷贝一级二级页表，仍然复用内核的页表)
 */
void share_pgtable(PTE *dest_pgdir, PTE *src_pgdir) {
    uint64_t *dest_pgdir_addr = (uint64_t *)dest_pgdir;
    uint64_t *src_pgdir_addr = (uint64_t *)src_pgdir;
    for (int i = 0; i < 512; i++) {
        //如果满足不是临时映射则拷贝
        if (i != (TEMP_PAGE_START >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)))
            *dest_pgdir_addr = *src_pgdir_addr;
        dest_pgdir_addr++;
        src_pgdir_addr++;
    }
}

/*
 * 拷贝所有的页表，末级映射指向同一个物理页，取消写资格
 */
void pgcopy(PTE *dest_pgdir, PTE *src_pgdir, uint8_t level) {
    PTE *src_pgdir_t = src_pgdir;
    PTE *dest_pgdir_t = dest_pgdir;

    for (unsigned vpn = 0; vpn < NUM_PTE_ENTRY; vpn++) {
        // 页表对应的p位是1,则需要进行拷贝
        if (src_pgdir_t[vpn] & _PAGE_PRESENT) {
            if (pa2kva(get_pa(src_pgdir_t[vpn])) < FREEMEM_KERNEL)
                continue; // kernal no release
            if (level) {
                // 首先给目标项分配一页，然后继续进入进行pgcopy
                palloc(&dest_pgdir_t[vpn], _PAGE_PRESENT);
                pgcopy((PTE *)pa2kva(get_pa(dest_pgdir_t[vpn])),
                       (PTE *)pa2kva(get_pa(src_pgdir_t[vpn])), level - 1);
            } else {
                /*
                 * 写父进程表项，同时将父子进程的写权限全部剥夺
                 */
                setPTE(&dest_pgdir_t[vpn], get_pa(src_pgdir_t[vpn]),
                       get_attribute(src_pgdir_t[vpn], PA_ATTRIBUTE_MASK) & ~_PAGE_WRITE);
                set_attribute(&src_pgdir_t[vpn],
                              get_attribute(src_pgdir_t[vpn], PA_ATTRIBUTE_MASK) & ~_PAGE_WRITE);

                page_general[kva2pgindex(pa2kva(get_pa(src_pgdir_t[vpn])))]
                    .used++; //对应的物理页的使用数量会增加！
            }
        } else
            continue;
    }

    return;
}

/*
 * 根据发生异常地址对应的PTE表项，重新分配一页并进行用户页的复制(写时复制)
 */
void uvmcopy(PTE *expte) {
    // 写时复制
    uint64_t src_kva = pa2kva(get_pa(*expte)); //已经找到的表项，将其中的物理地址提取出来
    uint64_t dest_kva =
        palloc(expte, get_attribute(*expte, PA_ATTRIBUTE_MASK) | _PAGE_PRESENT | _PAGE_ACCESSED |
                          _PAGE_READ | _PAGE_DIRTY | _PAGE_WRITE); //分配出一块空间
    memcpy((uint8_t *)dest_kva, (uint8_t *)src_kva, PAGE_SIZE);

    // 将原页面free
    kmfree(src_kva);
}
