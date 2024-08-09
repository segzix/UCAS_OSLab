#ifndef SMP_H
#define SMP_H

#include <os/sched.h>
#define NR_CPUS 2
extern void smp_init();
extern void wakeup_other_hart();
extern unsigned long get_current_cpu_id();
extern void lock_kernel();
extern void unlock_kernel();
extern void clear_sip();
pcb_t *get_pcb();
void set_pcb(pcb_t *pcb);

#endif /* SMP_H */
