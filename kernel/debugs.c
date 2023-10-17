#include <os/mm.h>
#include <debugs.h>
#include <assert.h>
#define read_reg(reg) ({ \
    uint64_t __val; \
    asm volatile ("mv %0, " #reg : "=r"(__val)); \
    __val; \
})


void kernel_self_check()
{
    // first turn off all the interrupt
    assert(read_reg(sp) < FREEMEM_USER); // sp should be in kernel space
}