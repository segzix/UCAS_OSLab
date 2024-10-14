/** SPDX-License-Identifier: GPL-2.0-only */
/**
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>
#include <mminit.h>

.section ".entry_function","ax"
ENTRY(_start)
  /** Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bnez t0, secondary

  la    t0, __bss_start
  la    t1, __BSS_END__

head_loop:
  blt   t1, t0, head_endloop # if t0 <= t1 then endloop
  sb    zero,0(t0)
  addi  t0,t0,1
  j     head_loop
head_endloop:

  /** setup C environment */
  la    tp, pid0_pcb
  li    sp, PID0_STACK_KVA

  call  main

secondary:
  la    tp, pid1_pcb
  li    sp, PID1_STACK_KVA

  call  main

loop:
  wfi
  j loop

END(_start)
