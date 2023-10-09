#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <assert.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
// a queue to store the available lock indices
static int lock_index_queue[LOCK_NUM];
static int lock_index_queue_head = 0;
static int lock_index_queue_tail = LOCK_NUM - 1;

static inline int pop_lock_index(void)
{
    if (lock_index_queue_head == lock_index_queue_tail)
        return -1;
    int lock_index = lock_index_queue[lock_index_queue_head];
    lock_index_queue_head = (lock_index_queue_head + 1) % LOCK_NUM;
    return lock_index;
}

static inline void push_lock_index(int lock_index)
{
    lock_index_queue_tail = (lock_index_queue_tail + 1) % LOCK_NUM;
    lock_index_queue[lock_index_queue_tail] = lock_index;
}

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++)
        lock_index_queue[i] = i;
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
    // TODO: what can key be used for?
    return pop_lock_index();
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    pcb_t* p = myproc();
    printl("process %d (%s) tries to acquire lock %d\n", p->pid, p->name, mlock_idx);
    assert_msg(!HOLD_LOCK(p, mlock_idx), 
        "lock is already held by the current process!");
    // print a log
    if (mlocks[mlock_idx].lock.status == UNLOCKED) {
        mlocks[mlock_idx].lock.status = LOCKED;
        p->lock_bitmask |= LOCK_MASK(mlock_idx);
        printl("lock %d held by %s\n", mlock_idx, p->name);
    }
    else do_block(&(p->list), &mlocks[mlock_idx].block_queue);
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    assert_msg(mlocks[mlock_idx].lock.status == LOCKED, "releasing an unlocked lock!");
    pcb_t* p = myproc();
    printl("process %d (%s) is releasing lock %d\n", p->pid, p->name, mlock_idx);
    assert_msg(HOLD_LOCK(p, mlock_idx), "lock is not held by the current process!");
    mlocks[mlock_idx].lock.status = UNLOCKED;
    p->lock_bitmask ^= LOCK_MASK(mlock_idx);
    // if no proc is blocking on the lock, push the lock index to the queue and return
    printl("process %d (%s) released lock %d\n", p->pid, p->name, mlock_idx);
    if (LIST_EMPTY(mlocks[mlock_idx].block_queue)) {
        push_lock_index(mlock_idx);
        return;
    }
    pcb_t* np = list_pcb(LIST_FIRST(mlocks[mlock_idx].block_queue));
    do_unblock(LIST_FIRST(mlocks[mlock_idx].block_queue));
    assert(mlocks[mlock_idx].lock.status == UNLOCKED);
    assert_msg(!HOLD_LOCK(np, mlock_idx), "the awoken process holds the lock");
    mlocks[mlock_idx].lock.status = LOCKED;
    np->lock_bitmask |= LOCK_MASK(mlock_idx);
    printl("lock %d held by %s\n", mlock_idx, np->name);
}

void dump_lock(int mlock_idx)
{
    assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
    printl("****************************\n"
           "lock %d: \nstatus: %s \nblocked processes:\n",
            mlock_idx, lock_status_str(mlocks[mlock_idx].lock.status));
    list_node_t* node = LIST_FIRST(mlocks[mlock_idx].block_queue);
    while(node != &mlocks[mlock_idx].block_queue) {
        pcb_t* p = list_pcb(node);
        printl("pid: %d, name: %s\n", p->pid, p->name);
        node = node->next;
    }
    printl("****************************\n");
}