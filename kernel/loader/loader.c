#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <type.h>
#include <os/mm.h>

uint64_t load_task_img(int taskid,uintptr_t pgdir,int pid)
{
    uint32_t task_block_id;
    uint32_t task_block_num;
    uint32_t task_block_offset;

    uint32_t page_number = 0;//记录除开页表分配的物理页框数
    uint32_t load_entrypoint = 0x59000000;//记录除开页表分配的物理页框数
    uint64_t load_entrypoint_kva;

    task_block_id       =  tasks[taskid].task_block_phyaddr / SECTOR_SIZE;
    task_block_num      =  (tasks[taskid].task_block_phyaddr + tasks[taskid].task_block_size) / SECTOR_SIZE - task_block_id + 1;
    task_block_offset   =  tasks[taskid].task_block_phyaddr % SECTOR_SIZE;

    bios_sd_read(load_entrypoint, task_block_num, task_block_id);
    load_entrypoint_kva = pa2kva(load_entrypoint);
    memcpy((uint8_t *)load_entrypoint_kva,(uint8_t *)(load_entrypoint_kva + task_block_offset),tasks[taskid].task_block_size);
    // memcpy(load_entrypoint_kva,pa2kva(load_entrypoint + task_block_offset),tasks[taskid].task_block_size);
    //先从sd卡中读出并拷贝到一个地方

    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // uint64_t offset = tasks[id].offset;
    for(uint32_t i = 0; i < task_block_num; i += (NORMAL_PAGE_SIZE >> 9),page_number++){
        uint64_t va = alloc_page_helper(tasks[taskid].task_entrypoint + page_number * NORMAL_PAGE_SIZE,pgdir,1,pid);
        //一定要注意，这里返回的是内核中对该物理页映射的虚地址！
        memcpy((uint8_t *)va,(uint8_t *)pa2kva(load_entrypoint + page_number * NORMAL_PAGE_SIZE),NORMAL_PAGE_SIZE);
    }
    //根据它的扇区数可以推断出占几个页
    //对于每一页在alloc_page_helper建立好映射之后，就将其拷贝到其对应的虚地址处
    // return tasks[id].p_vaddr;

    return 0;
}
