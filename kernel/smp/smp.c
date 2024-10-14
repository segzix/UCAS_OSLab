#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

spin_lock_t kernel_lock = {UNLOCKED};

void smp_init()
{
    /** TODO: P3-TASK3 multicore*/
}

void wakeup_other_hart()
{
    /** TODO: P3-TASK3 multicore*/
}

void lock_kernel()
{
    spin_lock_acquire(&kernel_lock);
    /** TODO: P3-TASK3 multicore*/
}

void unlock_kernel()
{
    spin_lock_release(&kernel_lock);
    /** TODO: P3-TASK3 multicore*/
}