/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define DEFLATE_STACK		0x53100000
#define KERNEL		      0x50202000
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
  //为deflate函数创造环境，创造完环境并执行完后再直接跳转到kernel
  la    sp,DEFLATE_STACK
  call main

  /* TODO: [p1-task2] setup C environment */
  la    t1,KERNEL  # jump to   KERNEL
  jalr  t1
  

loop:
  wfi
  j loop

END(_start)
