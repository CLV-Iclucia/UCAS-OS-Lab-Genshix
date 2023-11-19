#ifndef SMP_H
#define SMP_H

#include <type.h>
#include <os/list.h>

#define NR_CPUS 2

typedef struct tcb tcb_t;
typedef struct switchto_context switchto_context_t;
typedef struct cpu {
    tcb_t* current_running;
    switchto_context_t* sched_ctx;
    list_node_t sleep_queue;
    uint64_t next_time;
    uint64_t time_elapsed;
    bool timer_needs_reset;
    uint8_t id;
    uint8_t spin_lock_cnt;
} cpu_t;

extern cpu_t cpus[NR_CPUS];
extern void smp_init();
extern void wakeup_other_hart();
static inline cpu_t* mycpu() {
    int cpuid;
    asm volatile("csrr %0, mhartid" : "=r"(cpuid));
    return cpus + cpuid;
}
static inline int mycpuid() {
    int cpuid;
    asm volatile("csrr %0, mhartid" : "=r"(cpuid));
    return cpuid;
}
extern uint64_t get_current_cpu_id();
extern void lock_kernel();
extern void unlock_kernel();

#endif /* SMP_H */
