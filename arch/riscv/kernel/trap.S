#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0
  //将例外入口跳转地址设置号，以后硬件自动跳转时将跳转到该函数的入口处
  /* TODO: [p2-task3] save exception_handler_entry into STVEC */

  //not t0, zero
  //csrs CSR_SIE, t0

  //li t0,SR_SIE
  //csrs CSR_SSTATUS, t0
  /* TODO: [p2-task4] enable interrupts globally */
  jr ra
  //返回init_exception函数

ENDPROC(setup_exception)
