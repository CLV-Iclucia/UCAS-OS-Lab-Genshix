#include <os/smp.h>
#include <assert.h>
#include <atomic.h>
#include <debugs.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <sys/syscall.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conds[CONDITION_NUM];
mailbox_t mbox[MBOX_NUM];
static int lock_idx_hash(int key) {
  // TODO: change this to a better one
  return key % LOCK_NUM;
}

void init_locks(void) {
  /* TODO: [p2-task2] initialize mlocks */
  for (int i = 0; i < LOCK_NUM; i++) {
    spin_lock_init(&mlocks[i].lock);
    mlocks[i].block_queue.next = mlocks[i].block_queue.prev =
        &mlocks[i].block_queue;
  }
}

void spin_lock_init(spin_lock_t *lock) {
  lock->cpuid = -1;
  lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock) {
  /* TODO: [p2-task2] try to acquire spin lock */
  return 0;
}

void spin_lock_acquire(spin_lock_t *lock) {
  while (atomic_swap(LOCKED, (ptr_t)&lock->status) == LOCKED);
  cpu_t* c = mycpu();
  c->spin_lock_cnt++;
  lock->cpuid = c->id;
}

void spin_lock_release(spin_lock_t *lock) {
  assert(lock->status == LOCKED);
  cpu_t* c = mycpu();
  assert(lock->cpuid == c->id);
  c->spin_lock_cnt--;
  lock->cpuid = -1;
  atomic_swap(UNLOCKED, (ptr_t)&lock->status);
}

int do_mutex_lock_init(int key) {
  int idx = lock_idx_hash(key);
  mlocks[idx].key = key;
  return idx;
}

void do_mutex_lock_acquire(int mlock_idx) {
  assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
  tcb_t *t = mythread();
  spin_lock_acquire(&t->lock);
  spin_lock_acquire(&mlocks[mlock_idx].lock);
  assert_msg(!HOLD_LOCK(t, mlock_idx),
             "lock is already held by the current process!");
  if (mlocks[mlock_idx].status == UNLOCKED) {
    mlocks[mlock_idx].status = LOCKED;
    t->lock_bitmask |= LOCK_MASK(mlock_idx);
    spin_lock_release(&mlocks[mlock_idx].lock);
    spin_lock_release(&t->lock);
  } else
    do_block(&(t->list), &mlocks[mlock_idx].block_queue,
             &mlocks[mlock_idx].lock);
}

void do_mutex_lock_release(int mlock_idx) {
  assert(mlock_idx >= 0 && mlock_idx < LOCK_NUM);
  tcb_t *t = mythread();
  spin_lock_acquire(&mlocks[mlock_idx].lock);
  assert_msg(mlocks[mlock_idx].lock.status == LOCKED,
             "releasing an unlocked lock!");
  assert_msg(HOLD_LOCK(t, mlock_idx),
             "lock is not held by the current process!");
  mlocks[mlock_idx].status = UNLOCKED;
  t->lock_bitmask ^= LOCK_MASK(mlock_idx);
  // if no proc is blocking on the lock, push the lock index to the queue and
  // return
  log_line(LOCK, "process %d (%s), thread %d released lock %d\n", t->pcb->pid,
           t->pcb->name, t->tid, mlock_idx);
  if (LIST_EMPTY(mlocks[mlock_idx].block_queue))
    return;
  tcb_t *nt = list_tcb(LIST_FIRST(mlocks[mlock_idx].block_queue));
  assert_msg(!HOLD_LOCK(nt, mlock_idx), "the awoken process holds the lock");
  nt->lock_bitmask |= LOCK_MASK(mlock_idx); // currently it is not running on any cpu
  do_unblock(LIST_FIRST(mlocks[mlock_idx].block_queue));
  assert(mlocks[mlock_idx].status == UNLOCKED);
  mlocks[mlock_idx].status = LOCKED;
  spin_lock_release(&mlocks[mlock_idx].lock);
}

syscall_transfer_i_i(do_mutex_init, do_mutex_lock_init) 
syscall_transfer_v_i(do_mutex_acquire, do_mutex_lock_acquire)
syscall_transfer_v_i(do_mutex_release, do_mutex_lock_release)