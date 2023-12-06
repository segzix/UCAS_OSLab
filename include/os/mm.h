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

#include <type.h>
#include <pgtable.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
// <<<<<<< HEAD
// #define INIT_KERNEL_STACK 0x50500000
// #define INIT_USER_STACK 0x52500000
// #define FREEMEM_KERNEL (INIT_KERNEL_STACK+2*PAGE_SIZE)
// #define FREEMEM_USER INIT_USER_STACK
// =======
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+PAGE_SIZE)//由这里开始内核自由分配
#define TEMP_PAGE_START 0x50000000
#define PAGE_NUM 4096
#define SHARE_PAGE_NUM 32
// >>>>>>> start/Project4-Virtual_Memory_Management

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

typedef struct page_allocated{
    int valid;//是否被分配，已经被分配则置为1
    
    int pin;//如果置成1，则该页不允许被换出
    int using;//如果是共享内存，该变量记录有多少个进程正在共享

    uintptr_t kva;//对应的内核虚地址

    int pid;//对应的进程号
    uintptr_t pgdir;//对应进程的根目录页
    uintptr_t va;//对应进程对该物理地址的虚地址
    int table_not;//这一项专门用来判断是不是页表项
}page_allocated;

typedef struct share_page{
    int valid;//是否被分配，已经被分配则置为1
    
    int key;
    int using;
    int pin;//如果置成1，则该页不允许被换出

    uintptr_t kva;//对应的内核虚地址
}share_page;

extern ptr_t allocPage(int numPage,int pin,uintptr_t va,int table_not,int pid);
void free_all_pagemapping(ptr_t baseAddr);
void free_all_pagetable(ptr_t baseAddr);
void free_all_pageframe(ptr_t baseAddr);
extern uint16_t swap_block_id;
extern page_allocated page_general[PAGE_NUM];
extern share_page share_pages[SHARE_PAGE_NUM];
// TODO [P4-task1] */
void freePage(ptr_t baseAddr);

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

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, int pin,int pid);
PTE * search_and_set_PTE(uintptr_t va, uintptr_t pgdir,int pid);
uintptr_t search_PTE(uintptr_t kva, uintptr_t pgdir,int pid);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
pid_t do_fork();



#endif /* MM_H */
