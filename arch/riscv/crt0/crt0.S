#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    la    t2, __bss_start
    la    t1, __BSS_END__

crt0_loop:
    blt   t1, t2, crt0_endloop # if t0 <= t1 then endloop
    sb    zero,0(t2)
    addi  t2, t2, 1
    j     crt0_loop
crt0_endloop:
    
    /** TODO: [p1-task3] setup C runtime environment for the user program */

    call main
    call sys_exit
    RET
    
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
