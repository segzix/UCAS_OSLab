#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ deflate_block_size_loc, 0x502001fc
# =======
# .equ os_size_loc, 0x502001fc
# >>>>>>> start/Project3-Interactive_OS_and_Process_Management

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
.equ deflate,0x53000000

.text
.global main

main:
	// fence on all memory and I/O
	fence
	// a0 is mhartid
	bnez a0, secondary

	li   	a7,BIOS_PUTSTR
	lui  	a0,%hi(msg)
	addi 	a0,a0,%lo(msg)
	jal  	bios_func_entry 

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"

	li   	a7,BIOS_SDREAD
	la 		a0,deflate

	la 	 	t1,deflate_block_size_loc
	lh	 	a1,0(t1)
	mv   	t1,zero

	li		a2,1

	jal  	bios_func_entry 

	la 		t1,deflate
	jr		t1
	//将deflate函数加载到内存中

	// TODO: [p1-task2] call BIOS to read kernel in SD card


	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */

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

