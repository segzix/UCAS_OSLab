set confirm off
set disassemble-next-line auto	
set output-radix 16			
add-symbol-file build/bootblock
b bootblock.S:main
b bootblock.S:28