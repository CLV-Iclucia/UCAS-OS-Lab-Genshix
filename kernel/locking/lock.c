#include <assert.h>
#include <atomic.h>
#include <debugs.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <sys/syscall.h>


#define TABLE_SIZE 16

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conds[CONDITION_NUM];
semaphore_t semaphores[SEMAPHORE_NUM];
mailbox_t mbox[MBOX_NUM];

typedef struct hash_node {
  char name[MAX_NAME_LENGTH];
  int idx;
  int next;
} hash_node_t;

static hash_node_t hash_nodes[MBOX_NUM];
static int hash_table[TABLE_SIZE];
static spin_lock_t hash_lock;
static int hash_idx_queue[MBOX_NUM];
static int hash_idx_queue_head = 0, hash_idx_queue_tail = 0;

static void init_hash_table() {
  spin_lock_init(&hash_lock);
  for (int i = 0; i < TABLE_SIZE; i++) hash_table[i] = -1;
  for (int i = 0; i < MBOX_NUM; i++) hash_nodes[i].next = -1;
  for (int i = 0; i < MBOX_NUM; i++) hash_idx_queue[i] = i;
  hash_idx_queue_head = 0;
  hash_idx_queue_tail = MBOX_NUM - 1;
}

static int alloc_hash_node() {
  assert(spin_lock_holding(&hash_lock));
  if (hash_idx_queue_head == hash_idx_queue_tail) return -1;
  int idx = hash_idx_queue[hash_idx_queue_head];
  hash_idx_queue_head = (hash_idx_queue_head + 1) % MBOX_NUM;
  return idx;
}

static void free_hash_node(int idx) {
  assert(spin_lock_holding(&hash_lock));
  hash_idx_queue_tail = (hash_idx_queue_tail + 1) % MBOX_NUM;
  hash_idx_queue[hash_idx_queue_tail] = idx;
}

static unsigned int hash(const char *key, int len) {
  uint64_t hash = 0;
  for (int i = 0; i < len; i++) hash = hash * 1331 + key[i];
  return hash % TABLE_SIZE;
}

void insert(const char *key, int len, int idx) {
  unsigned int index = hash(key, len);
  spin_lock_acquire(&hash_lock);
  int new_idx = alloc_hash_node();
  for (int i = 0; i < len; i++) hash_nodes[new_idx].name[i] = key[i];
  hash_nodes[new_idx].idx = idx;
  hash_nodes[new_idx].next = -1;

  if (hash_table[index] == -1) {
    hash_table[index] = new_idx;
  } else {
    int current = hash_table[index];
    while (hash_nodes[current].next != -1) current = hash_nodes[current].next;
    hash_nodes[current].next = new_idx;
  }

  spin_lock_release(&hash_lock);
}

int hash_table_get(const char *name, int len) {
  unsigned int index = hash(name, len);
  spin_lock_acquire(&hash_lock);
  int current = hash_table[index];
  while (current != -1) {
    if (strcmp(name, hash_nodes[current].name) == 0) {
      spin_lock_release(&hash_lock);
      return hash_nodes[current].idx;
    }
    current = hash_nodes[current].next;
  }
  spin_lock_release(&hash_lock);
  return -1;
}

void hash_table_remove(const char *name, int len) {
  unsigned int index = hash(name, len);
  spin_lock_acquire(&hash_lock);
  int current = hash_table[index];
  if (current == -1) {
    spin_lock_release(&hash_lock);
    return;
  }
  if (strcmp(name, hash_nodes[current].name) == 0) {
    hash_table[index] = hash_nodes[current].next;
    free_hash_node(current);
    spin_lock_release(&hash_lock);
    return;
  }
  while (hash_nodes[current].next != -1) {
    if (strcmp(name, hash_nodes[hash_nodes[current].next].name) == 0) {
      int tmp = hash_nodes[current].next;
      hash_nodes[current].next = tmp == -1 ? -1 : hash_nodes[tmp].next;
      free_hash_node(tmp);
      spin_lock_release(&hash_lock);
      return;
    }
    current = hash_nodes[current].next;
  }
  spin_lock_release(&hash_lock);
}

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
  while (atomic_swap(LOCKED, (ptr_t)&lock->status) == LOCKED)
    ;
  cpu_t *c = mycpu();
  c->spin_lock_cnt++;
  lock->cpuid = c->id;
}

void spin_lock_release(spin_lock_t *lock) {
  assert(lock->status == LOCKED);
  cpu_t *c = mycpu();
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
  if (LIST_EMPTY(mlocks[mlock_idx].block_queue)) return;
  tcb_t *nt = list_tcb(LIST_FIRST(mlocks[mlock_idx].block_queue));
  assert_msg(!HOLD_LOCK(nt, mlock_idx), "the awoken process holds the lock");
  nt->lock_bitmask |=
      LOCK_MASK(mlock_idx);  // currently it is not running on any cpu
  do_unblock(LIST_FIRST(mlocks[mlock_idx].block_queue));
  assert(mlocks[mlock_idx].status == UNLOCKED);
  mlocks[mlock_idx].status = LOCKED;
  spin_lock_release(&mlocks[mlock_idx].lock);
}

syscall_transfer_i_i(do_mutex_init, do_mutex_lock_init)
    syscall_transfer_v_i(do_mutex_acquire, do_mutex_lock_acquire)
        syscall_transfer_v_i(do_mutex_release, do_mutex_lock_release)

            void init_conditions() {
  for (int i = 0; i < CONDITION_NUM; i++) {
    spin_lock_init(&conds[i].lock);
    conds[i].queue.next = conds[i].queue.prev = &conds[i].queue;
  }
}

int do_condition_init(int cond_idx) { return lock_idx_hash(cond_idx); }

void do_condition_wait(int cond_idx, int mutex_idx) {}

void init_barriers() {
  for (int i = 0; i < BARRIER_NUM; i++) {
    spin_lock_init(&barriers[i].lock);
    barriers[i].queue.next = barriers[i].queue.prev = &barriers[i].queue;
    barriers[i].num = 0;
  }
}

int do_barrier_init(int key, int goal) {
  if (goal <= 0) return -1;
  int idx = lock_idx_hash(key);
  barriers[idx].key = key;
  barriers[idx].goal = goal;
  barriers[idx].num = 0;
  return idx;
}

void do_barrier_wait(int barrier_idx) {
  assert(barrier_idx >= 0 && barrier_idx < BARRIER_NUM);
  spin_lock_acquire(&barriers[barrier_idx].lock);
  barriers[barrier_idx].num++;
  if (barriers[barrier_idx].num == barriers[barrier_idx].goal) {
    while (!LIST_EMPTY(barriers[barrier_idx].queue))
      do_unblock(LIST_FIRST(barriers[barrier_idx].queue));
    barriers[barrier_idx].num = 0;
    spin_lock_release(&barriers[barrier_idx].lock);
  } else {
    tcb_t *t = mythread();
    do_block(&(t->list), &barriers[barrier_idx].queue,
             &barriers[barrier_idx].lock);
  }
}

void do_barrier_destroy(int bar_idx) {
  assert(bar_idx >= 0 && bar_idx < BARRIER_NUM);
  spin_lock_acquire(&barriers[bar_idx].lock);
  assert_msg(LIST_EMPTY(barriers[bar_idx].queue),
             "destroying a barrier with blocked processes!");
  spin_lock_release(&barriers[bar_idx].lock);
}

void init_semaphores() {
  for (int i = 0; i < SEMAPHORE_NUM; i++) {
    spin_lock_init(&semaphores[i].lock);
    semaphores[i].queue.next = semaphores[i].queue.prev = &semaphores[i].queue;
    semaphores[i].value = 0;
  }
}

void do_semaphore_up(int sema_idx) {
  assert(sema_idx >= 0 && sema_idx < SEMAPHORE_NUM);
  spin_lock_acquire(&semaphores[sema_idx].lock);
  if (LIST_EMPTY(semaphores[sema_idx].queue)) {
    semaphores[sema_idx].value++;
    spin_lock_release(&semaphores[sema_idx].lock);
    return;
  }
  do_unblock(LIST_FIRST(semaphores[sema_idx].queue));
  spin_lock_release(&semaphores[sema_idx].lock);
}

void do_semaphore_down(int sema_idx) {
  assert(sema_idx >= 0 && sema_idx < SEMAPHORE_NUM);
  spin_lock_acquire(&semaphores[sema_idx].lock);
  if (semaphores[sema_idx].value > 0) {
    semaphores[sema_idx].value--;
    spin_lock_release(&semaphores[sema_idx].lock);
    return;
  }
  tcb_t *t = mythread();
  do_block(&(t->list), &semaphores[sema_idx].queue, &semaphores[sema_idx].lock);
}

void init_mbox() {
  for (int i = 0; i < MBOX_NUM; i++) {
    spin_lock_init(&mbox[i].lock);
    mbox[i].send_queue.next = mbox[i].send_queue.prev = &mbox[i].send_queue;
    mbox[i].recv_queue.next = mbox[i].recv_queue.prev = &mbox[i].recv_queue;
    mbox[i].length = 0;
    mbox[i].head = 0;
    mbox[i].tail = 0;
  }
  init_hash_table();
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length) {
  assert(mbox_idx >= 0 && mbox_idx < MBOX_NUM);
  spin_lock_acquire(&mbox[mbox_idx].lock);
  if (LIST_EMPTY(mbox[mbox_idx].recv_queue)) {
    spin_lock_release(&mbox[mbox_idx].lock);
    return 0;
  }
  tcb_t *nt = list_tcb(LIST_FIRST(mbox[mbox_idx].recv_queue));
  do_unblock(LIST_FIRST(mbox[mbox_idx].recv_queue));
  spin_lock_release(&mbox[mbox_idx].lock);
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length) {}