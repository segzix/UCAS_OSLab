#include <asm/biosdef.h>
#include <csr.h>
#include <mminit.h>
// deflate size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ deflate_block_size_loc, 0x502001fe

.text
.global main

main:
	// fence on all memory and I/O
	fence
	// a0 is mhartid
	bnez a0, secondary

	// call BIOS to load deflate
	li   	a7, BIOS_SDREAD
	la 		a0, DEFLATE_ADDR
	la 	 	t1, deflate_block_size_loc
	lh	 	a1, 0(t1)
	li		a2, 1
	li		t0, BIOS_FUNC_ENTRY
	jalr  	t0

	// start deflate
	li 		t1, DEFLATE_ADDR
	jr		t1

	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	csrw CSR_SIE, zero
	csrw CSR_SIP, zero

	li t0, KERNEL
	csrw CSR_STVEC, t0

	li t0, SIE_SSIE
	csrs CSR_SIE, t0

	li t0, SR_SIE
	csrw CSR_SSTATUS, t0

wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"
msg2: .string "It's a bootloader... core2\n\r"

