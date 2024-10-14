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
extern void ret_from_exception();

/***********syscall**********/

/**
 * 根据key值建立共享页(默认共享页用户虚地址从0x80000000开始)
 */
uintptr_t shm_page_get(int key) {
    pcb_t* current_running = get_pcb();
    uint8_t find;
    uintptr_t va;
    uint32_t index = hash(key, SHARE_PAGE_NUM);

    //寻找对应的共享页
    for (unsigned i = 0; i < SHARE_PAGE_NUM; index = (index + 1) % SHARE_PAGE_NUM, i++) {
        if (share_pages[index].key == key) {
            find = 1;
            goto startshm;
        } else
            continue;
    }

    //没有则寻找未被分配的共享页
    for (unsigned i = 0; i < SHARE_PAGE_NUM; index = (index + 1) % SHARE_PAGE_NUM, i++) {
        if (share_pages[index].key == -1) {
            find = 0;
            goto startshm;
        } else
            continue;
    }

startshm:
    va = SHM_PAGE_STARTVA + index * NORMAL_PAGE_SIZE;
    if (find) {
        //提取出已有的pgindex
        uint32_t pgindex = share_pages[index].pg_index;

        //直接建立用户虚地址的映射,并将对应的页面used++
        PTE *pte = mappages(va, current_running->pgdir, kva2pa(pgindex2kva(pgindex)),
                            _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);
        assert(pte);
        page_general[pgindex].used++;
    } else {
        //分配出用户对应虚地址的共享页
        uintptr_t kva =
            uvmalloc(va, current_running->pgdir,
                     _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

        //给共享页打上key和对应的页面(初始化share_pages)
        share_pages[index].key = key;
        share_pages[index].pg_index = kva2pgindex(kva);
    }
    local_flush_tlb_all();

    return va;
}

/**
 * 根据用户虚地址取消共享页
 */
void shm_page_dt(uintptr_t va) {
    uintptr_t kva;
    kva = uvmfree(get_pcb()->pgdir, va);

    unsigned pgindex = kva2pgindex(kva);
    uint32_t shindex;
    //如果已经无人使用，将此页清除掉
    if (!page_general[pgindex].used) {
        for (shindex = 0; shindex < SHARE_PAGE_NUM; shindex++) {
            if (share_pages[shindex].pg_index == pgindex)
                break;
        }
        share_pages[shindex].key = -1;
    }

    local_flush_tlb_all();
}

/**
 * 用户内存分配
 */
uintptr_t mmalloc(uint32_t size) {
    //旧堆地址与新堆地址
    uintptr_t old_heap = get_pcb()->heap;
    uintptr_t new_heap = get_pcb()->heap + size;

    //分配页面开始虚地址与结束虚地址
    uintptr_t startkva = ((old_heap - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
    uintptr_t endkva = (new_heap / PAGE_SIZE) * PAGE_SIZE;

    //建立映射但不写入具体物理页地址
    PTE *pgdir = get_pcb()->pgdir;
    for (unsigned kva = startkva; kva <= endkva; kva += PAGE_SIZE) {
        mappages(kva, pgdir, 0, 0);
    }

    get_pcb()->heap += size;
    return old_heap;
}