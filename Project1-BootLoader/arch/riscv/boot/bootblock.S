#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ deflate_block_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
.equ deflate,0x52000000

.text
.global main

main:
	// fence on all memory and I/O
	fence

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


	//j		kernel
	// TODO: [p1-task2] jump to kernel to start UCAS-OS

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

