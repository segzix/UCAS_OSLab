#include <stdio.h>
#include <unistd.h>

int thread_num[2];
int thread_exetimes[2];
int yield_times = 0;

void thread_start(int rank_id)
{
    thread_num[rank_id] = 0;
    while(1){
        thread_exetimes[rank_id]++;
        sys_move_cursor(0, 9+rank_id);;
        printf("> thread_child[%d] exetimes is [%d]",rank_id,thread_exetimes[rank_id]);
        while(thread_num[rank_id] < (thread_num[(rank_id+1)%2]+2)){
            sys_move_cursor(0, 7+rank_id);
            printf("> [TASK] thread_child[%d] is running! thread_num is [%d]",rank_id,thread_num[rank_id]);
            thread_num[rank_id]++;
        }

        yield_times++;
        sys_thread_yield();
    }
}
int main()
{
    int i;
    for(i = 0;i < 2;i++){
        sys_thread_create(thread_start,i);
    }
    while(1){
        sys_move_cursor(0, 6);
        printf("> [TASK] thread_parent is running! yield_times is [%d]",yield_times);
    }
}