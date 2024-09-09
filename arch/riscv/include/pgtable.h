#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)
#define PA_MASK ((1lu << 54) - 1)
#define PA_ATTRIBUTE_MASK ((1lu << 10) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

#define KVA_OFFSET 0xffffffc000000000lu//内核虚地址相对于实地址的偏移

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    return kva - KVA_OFFSET;
    /* TODO: [P4-task1] */
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    return pa + KVA_OFFSET;
    /* TODO: [P4-task1] */
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    return ((entry & PA_MASK) >> 10) << 12;//物理页表的地址
    /* TODO: [P4-task1] */
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    return (entry & PA_MASK) >> 10;
    /* TODO: [P4-task1] */
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    uint64_t mask = PA_MASK ^ PA_ATTRIBUTE_MASK;
    *entry = (mask & (pfn << 10)) | (~mask & *entry);
    /* TODO: [P4-task1] */
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    return entry & mask;
    /* TODO: [P4-task1] */
}

static inline void set_attribute(PTE *entry, uint64_t bits)
{

    *entry = ((*entry >> 10) << 10) | bits;
    /* TODO: [P4-task1] */
}

static inline void cleanpage(uintptr_t pgdir_addr)
{
    uint64_t *pgdir = (uint64_t *)pgdir_addr;
    for(int i = 0; i < 512; i++){
        *pgdir = 0;
        pgdir++;
    }
    /* TODO: [P4-task1] */
}

#endif  // PGTABLE_H
