.global main
.LC:
	.string "%d "
main:

	li t0,0

	addi t2,t0,1
	addi t1,t0,100
//设置边界
loop:
	blt t1,t2,endloop
	add t0,t0,t2
	addi t2,t2,1
	j loop
endloop:

// TODO: [p0-task1] Add your code here!

end:
	j end