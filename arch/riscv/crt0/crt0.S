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
  
    mv    t2,zero
    mv    t1,zero

    /*mv    t1,sp
    la    sp,_start
    li    t4,0x10000
    add   sp,sp,t4
    addi  sp,sp,-8
    //设置栈指针

    addi  sp,sp,-32
    //分配栈空间

    sd    t1, 0(sp) 
    sd    ra, 8(sp) 
    
    /* TODO: [p1-task3] setup C runtime environment for the user program */

    la  t1,main
    jalr  t1
    /* TODO: [p1-task3] enter main function */

    /*ld    ra, 8(sp)  
    ld    sp, 0(sp)*/

    RET
    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
