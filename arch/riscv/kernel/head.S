/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50501000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  la    t0, __bss_start
  la    t1, __BSS_END__

head_loop:
  blt   t1, t0, head_endloop # if t0 <= t1 then endloop
  sb    zero,0(t0)
  addi  t0,t0,1
  j     head_loop
head_endloop:
  
  mv    t0,zero 
  mv    t1,zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */

  la    tp, pid0_pcb
  ld    sp, pid0_stack
  #la    sp,KERNEL_STACK

  la    t1,main
  jalr  t1
  #call  main

  /* TODO: [p1-task2] setup C environment */


loop:
  wfi
  j loop

END(_start)
