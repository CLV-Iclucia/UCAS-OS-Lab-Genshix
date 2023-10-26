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
    list_node_t* p = sleep_queue.next;
    list_node_t* p_next = p->next;
    while (p != &sleep_queue) {
        pcb_t* pcb = list_pcb(p);
        if (pcb->wakeup_time <= get_ticks())
            do_unblock(p);
        p = p_next;
        p_next = p->next;
    }
}