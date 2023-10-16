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

void sys_thread_create(uint64_t addr,uint64_t rank_id)
{
    invoke_syscall(SYSCALL_THREAD_YIELD, addr, rank_id,IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
}

void sys_thread_yield(void)
{
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE,IGNORE,IGNORE,IGNORE);
    return;
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/