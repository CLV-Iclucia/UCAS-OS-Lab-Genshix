#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <sys/syscall.h>
#include <assert.h>
#include <atomic.h>
#include <debugs.h>

mutex_lock_t mlocks[LOCK_NUM];
static int lock_idx_hash(int key)
{
    // TODO: change this to a better one
    return key % LOCK_NUM;
}

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++) {
        mlocks[i].lock.status = UNLOCKED;
        mlocks[i].block_queue.next = mlocks[i].block_queue.prev = &mlocks[i].block_queue;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int idx = lock_idx_hash(key);
    mlocks[idx].key = key;
    return idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    tcb_t* t = mythread();
    log_line(LOCK, "process %d (%s), thread %d tries to acquire lock %d\n", t->pcb->pid, t->pcb->name, t->tid, mlock_idx);
    assert_msg(!HOLD_LOCK(t, mlock_idx), 
        "lock is already held by the current process!");
    // print a log
    if (mlocks[mlock_idx].lock.status == UNLOCKED) {
        mlocks[mlock_idx].lock.status = LOCKED;
        t->lock_bitmask |= LOCK_MASK(mlock_idx);
        log_line(LOCK, "lock %d held by %s, thread %d", mlock_idx, t->pcb->name, t->tid);
    }
    else do_block(&(t->list), &mlocks[mlock_idx].block_queue);
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    assert_msg(mlocks[mlock_idx].lock.status == LOCKED, "releasing an unlocked lock!");
    tcb_t* t = mythread();
    log_line(LOCK, "process %d (%s), thread %d is releasing lock %d\n", t->pcb->pid, t->pcb->name, t->tid, mlock_idx);
    assert_msg(HOLD_LOCK(t, mlock_idx), "lock is not held by the current process!");
    mlocks[mlock_idx].lock.status = UNLOCKED;
    t->lock_bitmask ^= LOCK_MASK(mlock_idx);
    // if no proc is blocking on the lock, push the lock index to the queue and return
    log_line(LOCK, "process %d (%s), thread %d released lock %d\n", t->pcb->pid, t->pcb->name, t->tid, mlock_idx);
    if (LIST_EMPTY(mlocks[mlock_idx].block_queue))
        return;
    tcb_t* nt = list_tcb(LIST_FIRST(mlocks[mlock_idx].block_queue));
    do_unblock(LIST_FIRST(mlocks[mlock_idx].block_queue));
    assert(mlocks[mlock_idx].lock.status == UNLOCKED);
    assert_msg(!HOLD_LOCK(nt, mlock_idx), "the awoken process holds the lock");
    mlocks[mlock_idx].lock.status = LOCKED;
    nt->lock_bitmask |= LOCK_MASK(mlock_idx);
    log_line(LOCK, "lock %d held by %s\n", mlock_idx, nt->pcb->name);
}

syscall_transfer_i_i(do_mutex_init, do_mutex_lock_init)
syscall_transfer_v_i(do_mutex_acquire, do_mutex_lock_acquire)
syscall_transfer_v_i(do_mutex_release, do_mutex_lock_release)
void dump_lock(int mlock_idx)
{
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    printl("****************************\n"
           "lock %d: \nstatus: %s \nblocked processes:\n",
            mlock_idx, lock_status_str(mlocks[mlock_idx].lock.status));
    list_node_t* node = LIST_FIRST(mlocks[mlock_idx].block_queue);
    while(node != &mlocks[mlock_idx].block_queue) {
        tcb_t* t = list_tcb(node);
        printl("pid: %d, name: %s, tid: %d\n", t->pcb->pid, t->pcb->name, t->tid);
        node = node->next;
    }
    printl("****************************\n");
}