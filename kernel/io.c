#include <io.h>
#include <common.h>
#include <os/sched.h>


void do_getchar() {
    int c = -1;
    while (1) {
        c = port_read_ch();
        if (c != -1) { 
            if (c == '\r') c = '\n'; 
            break;
        }
    }
    mythread()->trapframe->a0() = c;
}