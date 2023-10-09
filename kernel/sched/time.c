#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    list_node_t* sleep_queue_check = sleep_queue.next;
    list_node_t* temp_queue_check;
    //用于承接pcb，以及检查睡眠队列是否已空

    while(sleep_queue_check != &sleep_queue){
        pcb_t* sleep_pcb = list_entry(sleep_queue_check, pcb_t, list);
        //用于sleep_pcb指向对应的pcb
        if(get_timer() >= sleep_pcb->wakeup_time){
            temp_queue_check = sleep_queue_check->next;
            list_del(sleep_queue_check);
            list_add(sleep_queue_check, &ready_queue);
            sleep_pcb->status = TASK_READY;

            sleep_queue_check = temp_queue_check;
            //先用一个承接一下下一个，然后再等于
        }
        else{
            sleep_queue_check = sleep_queue_check->next;
        } 
    }
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
}