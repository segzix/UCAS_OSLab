#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
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
    spin_lock_acquire(&mutex_init_lock);
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == key){
            spin_lock_release(&mutex_init_lock);
            return key;
        }
    }
    for(i = 0;i < LOCK_NUM;i++)
    {
        if(mlocks[i].key == -1)
        {
            mlocks[i].key = key;
            spin_lock_release(&mutex_init_lock);
            return key;
        }
    }
    spin_lock_release(&mutex_init_lock);
    //每一个程序在调用该函数时寻找是否有已经可以匹配的锁，有则直接返回，没有分配一把新的
    /* TODO: [p2-task2] initialize mutex lock */
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    int i;
    int k;
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

            for(k = 0;k < TASK_LOCK_MAX;k++){
                if(!current_running->mutex_lock_key[k])
                {
                    current_running->mutex_lock_key[k] = mlock_idx;
                    break;
                }
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



barrier_t barriers[BARRIER_NUM];

void init_barriers(void){
    int i;
    for(i=0; i < BARRIER_NUM; i++){
        barriers[i].key = -1;
        barriers[i].queue_size = 0;
        barriers[i].target_size = 0;
        barriers[i].barrier_queue.prev = &(barriers[i].barrier_queue);
        barriers[i].barrier_queue.next = &(barriers[i].barrier_queue);
    }
}

int do_barrier_init(int key,int goal){
    int i;
    int id;

    spin_lock_acquire(&barrier_init_lock);
    for(i=0; i < BARRIER_NUM; i++){
        if(barriers[i].key==key){
            spin_lock_release(&barrier_init_lock);
            return -1;
        }
    }
    for(i=0; i < BARRIER_NUM; i++){
        if(barriers[i].key == -1){
            id = i;
            barriers[id].key = key;
            barriers[id].queue_size = 0;
            barriers[id].target_size = goal;
            spin_lock_release(&barrier_init_lock);
            return id;
        }
    }
    spin_lock_release(&barrier_init_lock);
    //每一个程序在调用该函数时寻找是否有已经可以匹配的屏障，有则说明该key已经被分配过故返回-1，没有则选择barrier进行分配
    return -1;
}

void do_barrier_wait(int bar_idx){
    // current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    barrier_t * barrier_now = &(barriers[bar_idx]);

    spin_lock_acquire(&(barrier_now->lock));
    barrier_now->queue_size++;

    if(barrier_now->queue_size < barrier_now->target_size)
        do_block(&current_running->list, &barrier_now->barrier_queue, &barrier_now->lock);

    while(barrier_now->barrier_queue.next != &(barrier_now->barrier_queue))
        do_unblock(barrier_now->barrier_queue.next);

    barrier_now->queue_size = 0;
    spin_lock_release(&(barrier_now->lock));
}

void do_barrier_destroy(int bar_idx){
    barrier_t * barrier_now = &(barriers[bar_idx]);

    spin_lock_acquire(&(barrier_now->lock));
    while(barrier_now->barrier_queue.next != &(barrier_now->barrier_queue))
        do_unblock(barrier_now->barrier_queue.next);

    barrier_now->key=-1;
    barrier_now->queue_size=0;
    barrier_now->target_size=0;

    spin_lock_release(&(barrier_now->lock));
}

semaphore_t semaphores[SEMAPHORE_NUM];

void init_semaphores(void){
    int i;
    for(i=0; i < SEMAPHORE_NUM; i++){
        semaphores[i].key = -1;
        semaphores[i].semaphore_size = 0;
        semaphores[i].semaphore_queue.prev = &(semaphores[i].semaphore_queue);
        semaphores[i].semaphore_queue.next = &(semaphores[i].semaphore_queue);
    }
}

int do_semaphore_init(int key,int init){
    int i;
    int id;

    spin_lock_acquire(&semaphore_init_lock);
    for(i=0; i < SEMAPHORE_NUM; i++){
        if(semaphores[i].key==key){
            spin_lock_release(&semaphore_init_lock);
            return -1;
        }
    }
    for(i=0; i < SEMAPHORE_NUM; i++){
        if(semaphores[i].key == -1){
            id = i;
            semaphores[id].key = key;
            semaphores[id].semaphore_size = init;
            spin_lock_release(&semaphore_init_lock);
            return id;
        }
    }
    spin_lock_release(&semaphore_init_lock);
    //每一个程序在调用该函数时寻找是否有已经可以匹配的屏障，有则说明该key已经被分配过故返回-1，没有则选择barrier进行分配
    return -1;
}

void do_semaphore_up(int sema_idx){
    // current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    semaphore_t * semaphore_now = &(semaphores[sema_idx]);

    spin_lock_acquire(&(semaphore_now->lock));

    if(semaphore_now->semaphore_size < 0)
        // if(semaphore_now->semaphore_queue.next != &(semaphore_now->semaphore_queue))
            do_unblock(semaphore_now->semaphore_queue.next);
    semaphore_now->semaphore_size++;

    // if(semaphore_now->semaphore_queue.next != &(semaphore_now->semaphore_queue))
    //     do_unblock(semaphore_now->semaphore_queue.next);
    // else
    //     semaphore_now->semaphore_size++;

    spin_lock_release(&(semaphore_now->lock));
}

void do_semaphore_down(int sema_idx){
    // current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    semaphore_t * semaphore_now = &(semaphores[sema_idx]);

    spin_lock_acquire(&(semaphore_now->lock));

    semaphore_now->semaphore_size--;
    if(semaphore_now->semaphore_size < 0)
        do_block(&current_running->list,&semaphore_now->semaphore_queue,&semaphore_now->lock);

    // if(semaphore_now->semaphore_size > 0)
    //     semaphore_now->semaphore_size--;
    // if(semaphore_now->semaphore_size == 0)
    //     do_block(&current_running->list,&semaphore_now->semaphore_queue,&semaphore_now->lock);

    spin_lock_release(&(semaphore_now->lock));
}

void do_semaphore_destroy(int sema_idx){
    semaphore_t * semaphore_now = &(semaphores[sema_idx]);

    spin_lock_acquire(&(semaphore_now->lock));

    // semaphore_now->semaphore_queue.prev = &(semaphore_now->semaphore_queue);
    // semaphore_now->semaphore_queue.next = &(semaphore_now->semaphore_queue);
    while(semaphore_now->semaphore_queue.next != &(semaphore_now->semaphore_queue))
        do_unblock(semaphore_now->semaphore_queue.next);
    semaphore_now->key=-1;
    semaphore_now->semaphore_size=0;

    spin_lock_release(&(semaphore_now->lock));
}


mailbox_t mailboxs[MBOX_NUM];

void init_mbox(){
    for(int i=0; i < MBOX_NUM; i++){
        mailboxs[i].siz  = 0;
        mailboxs[i].head = 0;
        mailboxs[i].tail = 0;
        mailboxs[i].using = 0;
        mailboxs[i].name[0] = '\0';

        mailboxs[i].mailbox_recv_queue.next = &(mailboxs[i].mailbox_recv_queue);
        mailboxs[i].mailbox_recv_queue.prev = &(mailboxs[i].mailbox_recv_queue);
        mailboxs[i].mailbox_send_queue.next = &(mailboxs[i].mailbox_send_queue);
        mailboxs[i].mailbox_send_queue.prev = &(mailboxs[i].mailbox_send_queue);
    }
}

int do_mbox_open(char *name){
    int i;
    int id;

    spin_lock_acquire(&mailbox_init_lock);
    for(i=0; i < MBOX_NUM; i++){
        if(strcmp(mailboxs[i].name,name) == 0){
            id = i;
            mailboxs[i].using++;
            spin_lock_release(&mailbox_init_lock);
            return id;
        }
    }
    for(i=0; i < MBOX_NUM; i++){
        if(mailboxs[i].name[0] == '\0'){
            id = i;
            strcpy(mailboxs[i].name,name);
            spin_lock_release(&mailbox_init_lock);
            return id;
        }
    }

    spin_lock_release(&mailbox_init_lock);
    return -1;
}

void do_mbox_close(int mbox_idx){
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);

    spin_lock_acquire(&(mailbox_now->lock));

    mailbox_now->using -- ;
    if(mailbox_now->using == 0){
        mailbox_now->siz = 0;
        mailbox_now->head = 0;
        mailbox_now->tail = 0;
        mailbox_now->name[0] = '\0'; 

        mailbox_now->mailbox_recv_queue.next = &(mailbox_now->mailbox_recv_queue);
        mailbox_now->mailbox_recv_queue.prev = &(mailbox_now->mailbox_recv_queue);
        mailbox_now->mailbox_send_queue.next = &(mailbox_now->mailbox_send_queue);
        mailbox_now->mailbox_send_queue.prev = &(mailbox_now->mailbox_send_queue);
    }

    spin_lock_release(&(mailbox_now->lock));
}

char *mboxncpy(char *dest, const char *src, int n)
{
    char *tmp = dest;

    while (n-- > 0) {
        *dest++ = *src++;
    }
    return tmp;
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length){
    // current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    int block = 0;
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);

    spin_lock_acquire(&(mailbox_now->lock));

    while(msg_length + mailbox_now->siz > MAX_MBOX_LENGTH){//生产者发现当前空余量已经不够生产
        block++;
        do_block(&current_running->list, &mailbox_now->mailbox_send_queue, &mailbox_now->lock);
    }
        
    mailbox_now->siz += msg_length;
    if(msg_length + mailbox_now->tail <= MAX_MBOX_LENGTH){//如果发现不用循环，直接往后加即可
        mboxncpy((mailbox_now->buffer + mailbox_now->tail),msg,msg_length);
        // printl("send buffer=%s,msg=%s,len=%d",msg,((*mailbox_now).buffer + mailbox_now->tail),msg_length);
        if(mailbox_now->tail + msg_length == MAX_MBOX_LENGTH)
            mailbox_now->tail = 0;
        else
            mailbox_now->tail = mailbox_now->tail + msg_length;
    }
    else{
        int prelen = MAX_MBOX_LENGTH - (mailbox_now->tail);//记录要放在循环数组后面的字符串长度
        mboxncpy((mailbox_now->buffer + mailbox_now->tail),msg,prelen);//放在循环数组后面
        mboxncpy(mailbox_now->buffer,msg+prelen,msg_length-prelen);//放在循环数组前面
        mailbox_now->tail = msg_length - prelen;
    }

    while(mailbox_now->mailbox_recv_queue.next != &(mailbox_now->mailbox_recv_queue))//生产成功唤醒所有消费者
        do_unblock(mailbox_now->mailbox_recv_queue.next);

    spin_lock_release(&(mailbox_now->lock));
    return block;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    // current_running = get_current_cpu_id()? &current_running_1 : &current_running_0;
    int block = 0;
    mailbox_t * mailbox_now = &(mailboxs[mbox_idx]);

    spin_lock_acquire(&(mailbox_now->lock));

    while(mailbox_now->siz < msg_length){
        block++;
        do_block(&current_running->list, &mailbox_now->mailbox_recv_queue, &mailbox_now->lock);
    }

    mailbox_now->siz -= msg_length;
    if(mailbox_now->head + msg_length <= MAX_MBOX_LENGTH){//消费是改变头指针，消费完之后看是否需要掉头循环
        mboxncpy(msg,(mailbox_now->buffer + mailbox_now->head),msg_length);
        // printl("recv buffer=%s,msg=%s,len=%d",msg,((*mailbox_now).buffer + mailbox_now->head),msg_length);
        if(mailbox_now->head + msg_length == MAX_MBOX_LENGTH)
            mailbox_now->head = 0;
        else
            mailbox_now->head = mailbox_now->head + msg_length;
    }
    else{
        int prelen = MAX_MBOX_LENGTH - (mailbox_now->head);
        mboxncpy(msg,(mailbox_now->buffer + mailbox_now->head),prelen);//数组后
        mboxncpy(msg+prelen,mailbox_now->buffer,msg_length-prelen);//数组前
        mailbox_now->head = msg_length - prelen;
    }

    while(mailbox_now->mailbox_send_queue.next != &(mailbox_now->mailbox_send_queue))//消费成功唤醒所有生产者
        do_unblock(mailbox_now->mailbox_send_queue.next);

    spin_lock_release(&(mailbox_now->lock));
    return block;    
}