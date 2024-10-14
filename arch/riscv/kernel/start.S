/** SPDX-License-Identifier: GPL-2.0-only */
/**
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>
#include <mminit.h>

.section ".bootkernel_entry","ax"
ENTRY(_boot)
  /**
   * this is the entrypoint of the kernel. 
   * Don't clear BSS here
   */

  /** Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr a0, CSR_MHARTID
  bnez a0, secondary

  li sp, PID0_STACK
  // this will never return
  call boot_kernel

  secondary:

  li sp, PID1_STACK
  // this will never return
  call boot_kernel


loop:
  wfi
  j loop

END(_boot)
