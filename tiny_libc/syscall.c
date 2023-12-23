#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    long return_value;
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    asm volatile(
        "ecall\n\t"
        "mv %0,a0\n\t"
        "nop\n\t"
        :"=r"(return_value)
    );

    return return_value;
}

void sys_yield(void)
{
    //call_jmptab(YIELD,0,0,0,0,0);
    invoke_syscall(SYSCALL_YIELD,0,0,0,0,0);
    return;
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
}

void sys_move_cursor(int x, int y)
{
    //call_jmptab(MOVE_CURSOR,x,y,0,0,0);
    invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
}

void sys_write(char *buff)
{
    //call_jmptab(WRITE,buff,0,0,0,0);
    invoke_syscall(SYSCALL_WRITE, (uintptr_t)buff, IGNORE, IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
}

void sys_reflush(void)
{
    //call_jmptab(REFLUSH,0,0,0,0,0);
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
}

int sys_mutex_init(int key)
{
    //return call_jmptab(MUTEX_INIT,key,0,0,0,0);
    return (int)invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE,IGNORE,IGNORE);
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
}

void sys_mutex_acquire(int mutex_idx)
{
    //call_jmptab(MUTEX_ACQ,mutex_idx,0,0,0,0);
    invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
}

void sys_mutex_release(int mutex_idx)
{
    //call_jmptab(MUTEX_RELEASE,mutex_idx,0,0,0,0);
    invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE,IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
}

long sys_get_timebase(void)
{
    return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    //return 0;
}

long sys_get_tick(void)
{
    return invoke_syscall(SYSCALL_GET_TICK, IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    //return 0;
}

void sys_sleep(uint32_t time)
{
    invoke_syscall(SYSCALL_SLEEP, time, IGNORE,IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
}

// void sys_thread_create(uint64_t addr,uint64_t thread_id)
// {
//     invoke_syscall(SYSCALL_THREAD_CREATE, addr, thread_id,IGNORE,IGNORE,IGNORE);
//     return;
//     /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
// }

// void sys_thread_yield(void)
// {
//     invoke_syscall(SYSCALL_THREAD_YIELD, IGNORE, IGNORE,IGNORE,IGNORE,IGNORE);
//     return;
//     /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
// }

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */

/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    return invoke_syscall(SYSCALL_EXEC, (uintptr_t)name,argc,(uintptr_t)argv,IGNORE,IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
}
#endif

void sys_exit(void)
{
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
}

int  sys_kill(pid_t pid)
{
    return invoke_syscall(SYSCALL_KILL, pid, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
}

int  sys_waitpid(pid_t pid)
{
    return invoke_syscall(SYSCALL_WAITPID, pid, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
}

int sys_ps()
{
    return invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
}

void sys_clear(int begin,int end)
{
    invoke_syscall(SYSCALL_CLEAR, begin, end, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid()
{
    return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
}

int  sys_getchar(void)
{
    return invoke_syscall(SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
}

int  sys_barrier_init(int key, int goal)
{
    return invoke_syscall(SYSCALL_BARR_INIT, key, goal, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
}

void sys_barrier_wait(int bar_idx)
{
    invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
}

void sys_barrier_destroy(int bar_idx)
{
    invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
}

int sys_semaphore_init(int key, int init)
{
    return invoke_syscall(SYSCALL_SEMA_INIT, key, init, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
}

void sys_semaphore_up(int sema_idx)
{
    invoke_syscall(SYSCALL_SEMA_UP, sema_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
}

void sys_semaphore_down(int sema_idx)
{
    invoke_syscall(SYSCALL_SEMA_DOWN, sema_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
}

void sys_semaphore_destroy(int sema_idx)
{
    invoke_syscall(SYSCALL_SEMA_DESTROY, sema_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
}

int sys_mbox_open(char * name)
{
    return invoke_syscall(SYSCALL_MBOX_OPEN, name, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
}

void sys_mbox_close(int mbox_id)
{
    invoke_syscall(SYSCALL_MBOX_CLOSE, mbox_id, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    return invoke_syscall(SYSCALL_MBOX_SEND, mbox_idx, msg, msg_length, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    return invoke_syscall(SYSCALL_MBOX_RECV, mbox_idx, msg, msg_length, IGNORE, IGNORE);
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
}


void sys_task_set_p(pid_t pid, int mask)
{
    invoke_syscall(SYSCALL_TASK_SETP, pid, mask, IGNORE, IGNORE, IGNORE);
}

int sys_task_set(int mask,char *name, int argc, char *argv[])
{
    return invoke_syscall(SYSCALL_TASK_SET, mask, name, argc, argv, IGNORE);
}

void* sys_shmpageget(int key)
{
    return invoke_syscall(SYSCALL_SHM_GET, key, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpageget */
}

void sys_shmpagedt(void *addr)
{
    invoke_syscall(SYSCALL_SHM_DT, addr, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpagedt */
}

pid_t sys_fork()
{
    return invoke_syscall(SYSCALL_FORK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpagedt */
}

void pthread_create(pthread_t *thread,void (*start_routine)(void*),void *arg){
    invoke_syscall(SYSCALL_THREAD_CREATE,(uintptr_t)thread,(uintptr_t)start_routine,(uintptr_t)arg,IGNORE,IGNORE);
}

int pthread_join(pid_t thread){
    sys_waitpid(thread);
    return 0;   
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    return invoke_syscall(SYSCALL_NET_SEND,txpacket,(uintptr_t)length,IGNORE,IGNORE,IGNORE);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    return invoke_syscall(SYSCALL_NET_RECV,rxbuffer,(uintptr_t)pkt_num,pkt_lens,IGNORE,IGNORE);
}

int sys_net_recv_stream(void *buffer, int *nbytes)
{
    return invoke_syscall(SYSCALL_NET_RECV_STREAM,buffer,nbytes,IGNORE,IGNORE,IGNORE);
}

int sys_mkfs(void)
{
    return invoke_syscall(SYSCALL_MKFS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_statfs(void)
{
    return invoke_syscall(SYSCALL_STATFS,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_cd(char *path)
{
    return invoke_syscall(SYSCALL_CD,path,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mkdir(char *dir_name)
{
    return invoke_syscall(SYSCALL_MKDIR,dir_name,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_ls(int argc, char *argv[])
{
    return invoke_syscall(SYSCALL_LS,argc,argv,IGNORE,IGNORE,IGNORE);
}

int sys_rmdir(char *dir_name)
{
    return invoke_syscall(SYSCALL_RMDIR,dir_name,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_getpwdname(char *pwd_name)
{
    invoke_syscall(SYSCALL_GETPWDNAME,pwd_name,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_fopen(char *path, int mode)
{
    return invoke_syscall(SYSCALL_FOPEN,path,mode,IGNORE,IGNORE,IGNORE);
}

int sys_fread(int fd, char *buff, int length)
{
    return invoke_syscall(SYSCALL_FREAD,fd,buff,length,IGNORE,IGNORE); 
}

int sys_fwrite(int fd, char *buff, int length)
{
    return invoke_syscall(SYSCALL_FWRITE,fd,buff,length,IGNORE,IGNORE);
}

int sys_fclose(int fd)
{
    return invoke_syscall(SYSCALL_FCLOSE,fd,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_touch(char *filename)
{
    return invoke_syscall(SYSCALL_TOUCH,filename,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_cat(char *filename)
{
    return invoke_syscall(SYSCALL_CAT,filename,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_rmfile(char *filename)
{
    return invoke_syscall(SYSCALL_RMFILE,filename,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_lseek(int fd, int offset, int whence)
{
    return invoke_syscall(SYSCALL_LSEEK,fd,offset,whence,IGNORE,IGNORE);
}

/************************************************************/