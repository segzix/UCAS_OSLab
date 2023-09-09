.global main
main:

    li s2,0
	li t5,1
	li t6,100
//设置边界
loop:
	blt t6,t5,endloop
	mv a0,t5
    jal ra,judge_prime

    beq zero,s1,notadd
    addi s2,s2,1
notadd:
    
    addi t5,t5,1
	j loop
endloop:

// TODO: [p0-task1] Add your code here!

end:
	j end

judge_prime:

    li t0,1
    li t3,0
    li t4,2

loop_prime:
    blt a0,t0,endloop_prime

    rem t2,a0,t0
    blt zero,t2,notadd_prime
    addi t3,t3,1
notadd_prime:

    blt t4,t3,notisprime
    addi t0,t0,1
    j loop_prime

endloop_prime:
    bne t4,t3,notisprime
    li s1,1
    ret

notisprime:
    li s1,0
    ret