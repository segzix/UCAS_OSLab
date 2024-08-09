/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

/* maybe you need another stack for booting? */
.equ kernel_stack0_for_boot, 		0x50900000
.equ kernel_stack1_for_boot,   0x508ff000

.section ".bootkernel_entry","ax"
ENTRY(_boot)
  /*
   * this is the entrypoint of the kernel. 
   * Don't clear BSS here
   */

  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr a0, CSR_MHARTID
  bnez a0, secondary

  la sp, kernel_stack0_for_boot
  // this will never return
  call boot_kernel

  secondary:

  la sp, kernel_stack1_for_boot
  // this will never return
  call boot_kernel


loop:
  wfi
  j loop

END(_boot)
