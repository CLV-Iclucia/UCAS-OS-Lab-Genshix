#include <os/list.h>
#include <common.h>
#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/csr.h>
cpu_t cpus[NR_CPUS];

void smp_init()
{
    for (int i = 0; i < NR_CPUS; i++) {
        cpus[i].id = i;
        cpus[i].spin_lock_cnt = 0;
        cpus[i].current_running = &sched_tcb[i];
        cpus[i].sched_ctx = &sched_ctx[i];
        cpus[i].sleep_queue = (list_node_t){ &cpus[i].sleep_queue, &cpus[i].sleep_queue };
        cpus[i].current_running->trapframe = (regs_context_t*)allocKernelPage(1);
        cpus[i].current_running->trapframe->kernel_sp = cpus[i].current_running->kernel_sp + PAGE_SIZE;
    }
}

void wakeup_other_hart()
{
    send_ipi(NULL);
    w_sip(0);
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
}
