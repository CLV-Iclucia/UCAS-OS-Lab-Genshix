#ifndef OS_CSR_H
#define OS_CSR_H

#include <os/irq.h>
#include <csr.h>
static inline long long
r_sstatus()
{
  long long x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(long long x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

static inline long long r_sscratch()
{
  long long x;
  asm volatile("csrr %0, sscratch" : "=r" (x) );
  return x;
}

static inline void w_sscratch(long long x)
{
  asm volatile("csrw sscratch, %0" : : "r" (x));
}

static inline void w_scause(long long x)
{
  asm volatile("csrw scause, %0" : : "r" (x));
}

static inline bool is_supervisor_mode() 
{
    return (r_sstatus() & SR_SPP) != 0;
}

static inline void w_stvec(long long x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline void set_supervisor_mode()
{
  long long x = r_sstatus();
  x |= SR_SPP;
  w_sstatus(x);
}

static inline void set_user_mode()
{
  long long x = r_sstatus();
  x &= ~SR_SPP;
  w_sstatus(x);
}

#endif