#include <os/time.h>
#include <os/list.h>
#include <os/sched.h>
#include <sys/syscall.h>
#include <type.h>
uint64_t time_elapsed = 0;
uint64_t time_base = 0;
uint64_t next_time = 0;
uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

syscall_transfer_i_v(do_get_tick, get_ticks)
syscall_transfer_i_v(do_get_timebase, get_time_base)

void check_sleeping(void)
{
    // TODO: run through the list is too slow, manage the sleeping queue with other data structure
    list_node_t* t = sleep_queue.next;
    list_node_t* t_next = t->next;
    while (t != &sleep_queue) {
        tcb_t* tcb = list_tcb(t);
        if (tcb->wakeup_time <= get_ticks())
            do_unblock(t);
        t = t_next;
        t_next = t->next;
    }
}