#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    uint32_t task_block_id;
    uint32_t task_block_num;
    uint32_t task_block_offset;

    task_block_id       =  tasks[taskid].task_block_phyaddr / SECTOR_SIZE;
    task_block_num      =  (tasks[taskid].task_block_phyaddr + tasks[taskid].task_block_size) / SECTOR_SIZE - task_block_id + 1;
    task_block_offset   =  tasks[taskid].task_block_phyaddr % SECTOR_SIZE;

    bios_sd_read(tasks[taskid].task_entrypoint, task_block_num, task_block_id);
    memcpy(tasks[taskid].task_entrypoint,tasks[taskid].task_entrypoint + task_block_offset,tasks[taskid].task_block_size);
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    return 0;
}

/*uint64_t load_task_img_sched1(int taskid)
{
    uint32_t task_block_id;
    uint32_t task_block_num;
    uint32_t task_block_offset;

    task_block_id       =  sched1_tasks[taskid].task_block_phyaddr / SECTOR_SIZE;
    task_block_num      =  (sched1_tasks[taskid].task_block_phyaddr + sched1_tasks[taskid].task_block_size) / SECTOR_SIZE - task_block_id + 1;
    task_block_offset   =  sched1_tasks[taskid].task_block_phyaddr % SECTOR_SIZE;

    bios_sd_read(sched1_tasks[taskid].task_entrypoint, task_block_num, task_block_id);
    memcpy(sched1_tasks[taskid].task_entrypoint,sched1_tasks[taskid].task_entrypoint + task_block_offset,sched1_tasks[taskid].task_block_size);
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /*return 0;
}*/