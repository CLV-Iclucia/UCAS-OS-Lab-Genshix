#ifndef OS_CSR_H
#define OS_CSR_H

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

#endif