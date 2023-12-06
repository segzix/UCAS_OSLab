#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    uint64_t va = io_base & VA_MASK;
    PTE* pgdir_t = (PTE *)pa2kva(PGDIR_PA);
    uint64_t vpn2 = (va   >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));//页目录虚地址
    
    set_pfn(&pgdir_t[vpn2], phys_addr >> NORMAL_PAGE_SHIFT);//allocpage作为内核中的函数是虚地址，此时为二级页表分配了空间
    //set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    set_attribute(&pgdir_t[vpn2],_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC
                                |_PAGE_ACCESSED| _PAGE_DIRTY);
    // TODO: [p5-task1] map one specific physical region to virtual address

    io_base += ONE_G_SIZE;

    local_flush_tlb_all();
    
    return (void*)(io_base - ONE_G_SIZE);
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
