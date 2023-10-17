#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0
  csrr t0, sie
  andi t0, t0, 0
  csrw sie, t0
  li t1, SR_SIE
  or t0, t0, t1
  csrw sstatus, t0
  ret
ENDPROC(setup_exception)
