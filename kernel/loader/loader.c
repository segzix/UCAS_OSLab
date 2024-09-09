#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <type.h>
#include <os/mm.h>
#include <os/loader.h>
#include "mminit.h"

uint64_t load_task_img(int taskid, PTE* pgdir)
{
    uint32_t task_block_id;
    uint32_t task_block_num;
    uint32_t task_block_offset;

    uint32_t page_number = 0;
    uint64_t load_entrypoint_kva = pa2kva(TMPLOADADDR);

    task_block_id       =  tasks[taskid].task_block_phyaddr / SECTOR_SIZE;
    task_block_num      =  NBYTES2SEC(tasks[taskid].task_block_phyaddr + tasks[taskid].task_block_size) - task_block_id;
    task_block_offset   =  tasks[taskid].task_block_phyaddr % SECTOR_SIZE;

    bios_sd_read(TMPLOADADDR, task_block_num, task_block_id);
    memcpy((uint8_t *)load_entrypoint_kva, (uint8_t *)(load_entrypoint_kva + task_block_offset),tasks[taskid].task_block_size);

    //根据它的扇区数可以推断出占几个页；建立好映射之后，就将其拷贝到其对应的虚地址处
    for(uint32_t i = 0; i < task_block_num; i += (NORMAL_PAGE_SIZE >> 9),page_number++){
        uint64_t kva = uvmalloc(tasks[taskid].task_entrypoint + page_number * NORMAL_PAGE_SIZE, pgdir, 
                            _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);
        //一定要注意，这里返回的是内核中对该物理页映射的虚地址！
        memcpy((uint8_t *)kva,(uint8_t *)pa2kva(TMPLOADADDR + page_number * NORMAL_PAGE_SIZE),NORMAL_PAGE_SIZE);
    }

    return 0;
}
