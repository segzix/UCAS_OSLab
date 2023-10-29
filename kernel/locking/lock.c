#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    int i;
    for(i = 0;i < LOCK_NUM;i++)
    {
        mlocks[i].key = -1;
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
    }
    /* TODO: [p2-task2] initialize mlocks */
}

void spin_lock_init(spin_lock_t *lock)
{
    lock->status = UNLOCKED;
    /* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    while(lock->status == LOCKED)
        ;
    lock->status = LOCKED;
    return;
    //不断地去查询锁是否处于LOCKED状态，如果出于该状态，则加入该锁的队列中；如果不处于则上锁
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    lock->status = UNLOCKED;

    return;
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    int i;
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == key)
            return key;
    }
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == -1)
        {
            mlocks[i].key = key;
            return key;
        }
    }
    //每一个程序在调用该函数时寻找是否有已经可以匹配的锁，有则直接返回，没有分配一把新的
    /* TODO: [p2-task2] initialize mutex lock */
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    int i;
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == mlock_idx)
        {
            spin_lock_acquire(&mlocks[i].lock);
            while(mlocks[i].mutex_status == LOCKED)
            {
                do_block(&current_running->list,&mlocks[i].block_queue,&mlocks[i].lock);
                // spin_lock_release(&mlocks[i].lock);
                // do_scheduler();
                // spin_lock_acquire(&mlocks[i].lock);
            }
            mlocks[i].mutex_status = LOCKED;
            spin_lock_release(&mlocks[i].lock);

            return;
        }
    }
    /* TODO: [p2-task2] acquire mutex lock */
}

void do_mutex_lock_release(int mlock_idx)
{
    int i;
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == mlock_idx)
        {
            spin_lock_acquire(&mlocks[i].lock);
            mlocks[i].mutex_status = UNLOCKED;
            while((mlocks[i].block_queue).next != &(mlocks[i].block_queue))
                do_unblock((mlocks[i].block_queue).next);
            spin_lock_release(&mlocks[i].lock);

            return;
        }
    }
    /* TODO: [p2-task2] release mutex lock */
}
