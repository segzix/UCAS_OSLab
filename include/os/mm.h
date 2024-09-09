/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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
#ifndef MM_H
#define MM_H

#include <mminit.h>
#include <pgtable.h>
#include <type.h>

#define PAGE_SIZE 4096 // 4K
#define PAGE_NUM 512
#define SHARE_PAGE_NUM 16
#define TEMP_PAGE_START 0x50000000
#define SHM_PAGE_STARTVA 0x80000000
#define HEAP_STARTVA 0x90000000

enum WALK {
    ALLOC, //根据给出的虚地址返回最终的pte表项(中间若发现没有映射则进行分配)
    VOID //根据给出的虚地址返回最终的pte表项(中间若发现没有映射直接返回0)
};

enum PIN { UNPINED, PINED };

/* Rounding; only works for n = power of two */
#define PXSHIFT(level) (NORMAL_PAGE_SHIFT + (9 * (level)))
#define PX(level, va) ((((uint64_t)(va)) >> PXSHIFT(level)) & PXMASK)
#define PXMASK 0x1FF

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

typedef struct page_allocated {
    uint8_t valid; //是否被分配，已经被分配则置为1
    uint8_t used;  //如果是共享内存，该变量记录有多少个进程正在共享
    enum PIN pin;
    PTE *pte;

    uint8_t fqy; //记录访问频繁次数，用于clock算法
} page_allocated;

typedef struct share_page {
    int key;
    int pg_index; //对应的内核虚地址
} share_page;

page_allocated page_general[PAGE_NUM];
share_page share_pages[SHARE_PAGE_NUM];

/* mm function for memory */
extern void share_pgtable(PTE *dest_pgdir, PTE *src_pgdir);
uintptr_t walk(uintptr_t va, PTE *pgdir, enum WALK walk);
PTE *mappages(uintptr_t va, PTE *pgdir, uintptr_t pa, uint64_t perm);
void pgcopy(PTE *dest_pgdir, PTE *src_pgdir, uint8_t level);
void uvmcopy(PTE *expte);
void pin_page(uintptr_t kva);
unsigned swap_out();
void setPTE(PTE *pte, uintptr_t pa, uint64_t perm);

/*
 * alloc && free
 */
uintptr_t kmalloc();
uintptr_t kalloc();
uintptr_t palloc(PTE *pte, uint64_t perm);
uintptr_t uvmalloc(uintptr_t va, PTE *pgdir, uint64_t perm);

void uvmfreeall(PTE *pgdir);
uintptr_t uvmfree(PTE *pgdir, uintptr_t va);
void mapfree(PTE *pgdir);
void kmfree(uintptr_t kva);

/*syscall*/
uintptr_t mmalloc(uint32_t size);
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t va);
pid_t do_fork();

static inline uint32_t kva2pgindex(uintptr_t kva) {
    return (((kva >> NORMAL_PAGE_SHIFT) << NORMAL_PAGE_SHIFT) - FREEMEM_KERNEL) / PAGE_SIZE;
}

static inline uintptr_t pgindex2kva(uint32_t index) {
    return FREEMEM_KERNEL + index * PAGE_SIZE;
}

#endif /* MM_H */
