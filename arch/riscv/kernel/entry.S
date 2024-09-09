#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  //便签寄存器暂存sp
  csrw  CSR_SSCRATCH, sp

  //保存用户的sp,加载内核的sp并减去偏移
  sd sp, PCB_USER_SP(tp)
  ld sp, PCB_KERNEL_SP(tp)
  addi sp, sp, -(OFFSET_SIZE)

  //store trapframe
  sd ra, OFFSET_REG_RA(sp)    
  sd gp, OFFSET_REG_GP(sp)
  //保存一下tp，尽管switchto里就会把tp置好，但这里压到栈中，做双重保险 
  sd tp, OFFSET_REG_TP(sp) 
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

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  //保存以下三个特殊寄存器
  csrr t0, CSR_SSTATUS
  sd t0, OFFSET_REG_SSTATUS(sp)
  csrr t0, CSR_SEPC
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, CSR_SCAUSE
  sd t0, OFFSET_REG_SCAUSE(sp)

  //用户栈的sp不仅在pcb中，也在内核栈保存的内容中
  csrr t0, CSR_SSCRATCH
  sd t0, OFFSET_REG_SP(sp)
.endm

.macro RESTORE_CONTEXT
  //注意这里的kernel sp是第三层的sp,但是sp已经回归到第一层的sp
  
  //restore trapframe
  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  ld tp, OFFSET_REG_TP(sp)
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

  //恢复三个特殊寄存器
  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC, t0
  ld t0, OFFSET_REG_SCAUSE(sp)
  csrw CSR_SCAUSE, t0

  /*
   * 1.t0协助处理，CSR_SSCRATCH暂存user sp
   * 2.存进kernel sp
   * 3.恢复user sp
   */
  ld t0, OFFSET_REG_SP(sp)
  csrw CSR_SSCRATCH, t0
  ld t0, OFFSET_REG_T0(sp)
  addi sp, sp, (OFFSET_SIZE)
  sd sp, PCB_KERNEL_SP(tp)
  csrr sp, CSR_SSCRATCH
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
  li t0, SR_SIE | SR_SUM
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
  //保存在栈中的sp，是保存上下文之前的sp(t1暂存sp)
  mv t1, sp
  addi sp, sp, -(SWITCH_TO_SIZE)

  //store context
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
  //保存上下文之后的栈指针放在kerner_stack中,方便后续切换至该进程时恢复上下文
  sd sp, PCB_KERNEL_SP(a0)

  //先获得已有的当前的栈指针(保存上下文之后的)，然后根据sp将所有的寄存器恢复(sp寄存器也恢复到保存上下文之前的sp)
  ld sp, PCB_KERNEL_SP(a1)
  // restore context
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

  //tp寄存器指向pcb变化
  mv tp, a1
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  //释放内核大锁，恢复trapframe
  call unlock_kernel
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)
  //保存trapframe，内核上锁
  SAVE_CONTEXT
  call lock_kernel

  //以后返回时，直接返回到ret_from_exception(trick)
  la ra, ret_from_exception

  /* 
   * sp此时对应的时内核栈的sp(经过了保存trapframe)
   * a1,a2传参，进入interrupt_helper处理
   */
  mv a0, sp
  csrr a1, CSR_STVAL
  csrr a2, CSR_SCAUSE
  j interrupt_helper
ENDPROC(exception_handler_entry)

ENTRY(clear_sip)
  csrw CSR_SIP, zero
  jr ra
ENDPROC(clear_sip)