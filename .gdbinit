set confirm off
set disassemble-next-line auto	
set output-radix 16			
add-symbol-file build/bootblock
add-symbol-file build/decompress
b bootblock.S:main
b bootblock.S:26
b arch/riscv/kernel/head.S:_start
b start.S:_boot

