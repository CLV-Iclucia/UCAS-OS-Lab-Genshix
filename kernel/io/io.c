#include <common.h>
#include <os/io.h>
#include <os/sched.h>

void do_getchar() {
  mythread()->trapframe->a0() = port_read_ch();
}