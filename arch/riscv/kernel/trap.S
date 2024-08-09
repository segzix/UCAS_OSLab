#include <asm.h>
#include <csr.h>

//将例外入口跳转地址设置号，以后硬件自动跳转时将跳转到该函数的入口处
ENTRY(setup_exception)
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0
  jr ra
  //返回init_exception函数

ENDPROC(setup_exception)
