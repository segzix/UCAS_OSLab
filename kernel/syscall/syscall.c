#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    regs->sepc = regs->sepc + 4;
    regs->regs[10] = syscall[regs->regs[10]](regs->regs[11], regs->regs[12],regs->regs[13],regs->regs[14],regs->regs[15]);
    //这里interrupt和cause都没有用
}
