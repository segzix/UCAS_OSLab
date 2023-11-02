#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  csrw  CSR_SSCRATCH, sp
  sd sp, PCB_USER_SP(tp)
  //将用户栈的sp放入暂存寄存器中
  //将用户栈的sp保存到pcb的user_stack中

  ld  sp, PCB_KERNEL_SP(tp)
  addi sp, sp, -(OFFSET_SIZE)
  //将内核栈对应的此时的sp放在sp中
  //对sp做减法，准备保存trapframe

  //sd sp, PCB_KERNEL_SP(tp)
  //将内核栈的sp保存到pcb的kernel_stack中

  sd ra, OFFSET_REG_RA(sp)   
  //将用户栈的sp保存到trapframe中    
  sd gp, OFFSET_REG_GP(sp)
  sd tp, OFFSET_REG_TP(sp) 
  //保存一下tp，尽管switchto里就会把tp置好，但这里压到栈中，做双重保险 
  sd t0, OFFSET_REG_T0(sp)
  sd t1, OFFSET_REG_T1(sp)
  sd t2, OFFSET_REG_T2(sp)
  sd s0, OFFSET_REG_S0(sp)
  sd s1, OFFSET_REG_S1(sp)
  sd a0, OFFSET_REG_A0(sp)
  sd a1, OFFSET_REG_A1(sp)
  sd a2, OFFSET_REG_A2(sp)
  sd a3, OFFSET_REG_A3(sp)
  sd a4, OFFSET_REG_A4(sp)
  sd a5, OFFSET_REG_A5(sp)
  sd a6, OFFSET_REG_A6(sp)
  sd a7, OFFSET_REG_A7(sp)
  sd s2, OFFSET_REG_S2(sp)
  sd s3, OFFSET_REG_S3(sp)
  sd s4, OFFSET_REG_S4(sp)
  sd s5, OFFSET_REG_S5(sp)
  sd s6, OFFSET_REG_S6(sp)
  sd s7, OFFSET_REG_S7(sp)
  sd s8, OFFSET_REG_S8(sp)
  sd s9, OFFSET_REG_S9(sp)
  sd s10, OFFSET_REG_S10(sp)
  sd s11, OFFSET_REG_S11(sp)
  sd t3, OFFSET_REG_T3(sp)
  sd t4, OFFSET_REG_T4(sp)
  sd t5, OFFSET_REG_T5(sp)
  sd t6, OFFSET_REG_T6(sp)
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  //目前P2有用吗？
  csrr t0, CSR_SSTATUS
  sd t0, OFFSET_REG_SSTATUS(sp)
  csrr t0, CSR_SEPC
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, CSR_SCAUSE
  sd t0, OFFSET_REG_SCAUSE(sp)
  //保存以上四个寄存器
  csrr t0, CSR_SSCRATCH
  sd t0, OFFSET_REG_SP(sp)
  //sscratch暂存sp，现在存回到栈中

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

.endm

.macro RESTORE_CONTEXT
  //ld sp, PCB_KERNEL_SP(tp)
  //这里的tp已经被改变过指向当前运行pcb了，在switchto函数里
  //同时switchto函数做完之后，pcb中的kernelstack一定会随之更改
  
  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  ld t1, OFFSET_REG_T1(sp)
  ld t2, OFFSET_REG_T2(sp)
  ld s0, OFFSET_REG_S0(sp)
  ld s1, OFFSET_REG_S1(sp)
  ld a0, OFFSET_REG_A0(sp)
  ld a1, OFFSET_REG_A1(sp)
  ld a2, OFFSET_REG_A2(sp)
  ld a3, OFFSET_REG_A3(sp)
  ld a4, OFFSET_REG_A4(sp)
  ld a5, OFFSET_REG_A5(sp)
  ld a6, OFFSET_REG_A6(sp)
  ld a7, OFFSET_REG_A7(sp)
  ld s2, OFFSET_REG_S2(sp)
  ld s3, OFFSET_REG_S3(sp)
  ld s4, OFFSET_REG_S4(sp)
  ld s5, OFFSET_REG_S5(sp)
  ld s6, OFFSET_REG_S6(sp)
  ld s7, OFFSET_REG_S7(sp)
  ld s8, OFFSET_REG_S8(sp)
  ld s9, OFFSET_REG_S9(sp)
  ld s10, OFFSET_REG_S10(sp)
  ld s11, OFFSET_REG_S11(sp)
  ld t3, OFFSET_REG_T3(sp)
  ld t4, OFFSET_REG_T4(sp)
  ld t5, OFFSET_REG_T5(sp)
  ld t6, OFFSET_REG_T6(sp)

  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC, t0
  ld t0, OFFSET_REG_SCAUSE(sp)
  csrw CSR_SCAUSE, t0
  //保存以上四个寄存器
  ld t0, OFFSET_REG_SP(sp)
  csrw CSR_SSCRATCH, t0
  //将用户栈的地址先保存到t0中，再将SSCRATCH恢复到用户栈的地方
  //此时sp还没有恢复，还处于-OFFSET的位置。故先暂存到sscratch中
  /* 
   * restore all registers and sstatus, sepc, stval, scause, sscratch
   * register $zero, $sp, $tp are not restored here!
   */
  ld t0, OFFSET_REG_T0(sp)
  //用t0协助处理完成之后，再恢复t0
  ld tp, OFFSET_REG_TP(sp)
  //现在已经恢复完所有的trapframe了

  addi sp, sp, (OFFSET_SIZE)
  sd sp, PCB_KERNEL_SP(tp)
  //先恢复kernel_stack，并保存到pcb中
  //这里和switchto中的差别就在于，在trap_frame中存放的是用户的栈指针，因此在更新kernel_stack是要手动回正之后再更新
  csrr sp, CSR_SSCRATCH
  //再恢复用户栈的sp
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  mv t1, sp
  //保存在栈中的sp，是保存上下文之前的sp
  addi sp, sp, -(SWITCH_TO_SIZE)

  /*ld t0, PCB_KERNEL_SP(a0)
  sd sp, SWITCH_TO_SP(t0)
  ld sp, PCB_KERNEL_SP(a0)*///以上的这种方式，相当于是在栈中专门开辟了一个用来保存上下文的空间用于存放，
  //并将地址存放在对应pcb的kernel_stack中，但这种方法在这里不予采纳
  /* TODO: store all callee save registers,
   * see the definition of `struct switchto_context` in sched.h*/
  sd ra, SWITCH_TO_RA(sp)
  sd t1, SWITCH_TO_SP(sp)
  sd s0, SWITCH_TO_S0(sp)
  sd s1, SWITCH_TO_S1(sp)
  sd s2, SWITCH_TO_S2(sp)
  sd s3, SWITCH_TO_S3(sp)
  sd s4, SWITCH_TO_S4(sp)
  sd s5, SWITCH_TO_S5(sp)
  sd s6, SWITCH_TO_S6(sp)
  sd s7, SWITCH_TO_S7(sp)
  sd s8, SWITCH_TO_S8(sp)
  sd s9, SWITCH_TO_S9(sp)
  sd s10, SWITCH_TO_S10(sp)
  sd s11, SWITCH_TO_S11(sp)

  sd sp, PCB_KERNEL_SP(a0)
  //这列没有专门开辟保存上下文的空间，因此需要将保存上下文之后的栈指针放在kerner_stack中

  // restore next
  /* TODO: restore all callee save registers,
   * see the definition of `struct switchto_context` in sched.h*/
  ld sp, PCB_KERNEL_SP(a1)
  //先获得已有的当前的栈指针(保存上下文之后的)，然后根据sp将所有的寄存器恢复(sp寄存器也恢复到保存上下文之前的sp)
  ld ra, SWITCH_TO_RA(sp)
  ld s0, SWITCH_TO_S0(sp)
  ld s1, SWITCH_TO_S1(sp)
  ld s2, SWITCH_TO_S2(sp)
  ld s3, SWITCH_TO_S3(sp)
  ld s4, SWITCH_TO_S4(sp)
  ld s5, SWITCH_TO_S5(sp)
  ld s6, SWITCH_TO_S6(sp)
  ld s7, SWITCH_TO_S7(sp)
  ld s8, SWITCH_TO_S8(sp)
  ld s9, SWITCH_TO_S9(sp)
  ld s10, SWITCH_TO_S10(sp)
  ld s11, SWITCH_TO_S11(sp)
  ld sp, SWITCH_TO_SP(sp)
  //sp要最后load，因为sp不能提前变化

  //sd sp, PCB_KERNEL_SP(a1)
  //这列一定要把

  mv tp, a1
  jr ra

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  /*addi sp, sp, SWITCH_TO_SIZE
  jr ra*/
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  RESTORE_CONTEXT
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */

  sret
  /* HINT: remember to check your sp, does it point to the right address? */
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  SAVE_CONTEXT
  /* TODO: [p2-task3] save context via the provided macro */

  la ra, ret_from_exception
  //以后返回时，直接返回到ret_from_exception

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */

  mv a0, sp
  //这时的sp是已经将trapframe压过栈的sp
  csrr a1, CSR_STVAL
  csrr a2, CSR_SCAUSE

  la t0,interrupt_helper
  jr t0
  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */


ENDPROC(exception_handler_entry)
