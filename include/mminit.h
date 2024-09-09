#ifndef INCLUDE_MMINIT_H_
#define INCLUDE_MMINIT_H_

// bios 
#define BIOS_FUNC_ENTRY_KVA 0xffffffc050150000
#define BIOS_FUNC_ENTRY 0x50150000

// pid0与pid1的栈
#define PID0_STACK 0x50900000
#define PID1_STACK 0x508ff000
#define PID0_STACK_KVA 0xffffffc050900000
#define PID1_STACK_KVA 0xffffffc0508ff000

// 内核开始地址&&taskinfo加载地址&&内核自由分配地址
#define KERNEL 0x50201000
#define TASK_MEM_BASE PID0_STACK_KVA
#define FREEMEM_KERNEL 0xffffffc051050000

// 解压缩地址 
#define DEFLATE_ADDR 0x53000000
#define DEFLATE_STACK 0x53100000
#define KERNEL_DEFLATE 0x54000000

//用户程序暂存地址
#define TMPLOADADDR 0x59000000

#endif
