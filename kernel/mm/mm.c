#include <debugs.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <printk.h>
#include <type.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;
typedef struct release_record {
  ptr_t addr;
  int numPage;
} release_record_t;
static release_record_t released_kernel_pages[NUM_MAX_THREADS * 3];
static release_record_t released_user_pages[NUM_MAX_THREADS * 3];
static spin_lock_t kernMemLock = {
    .cpuid = -1,
    .status = UNLOCKED,
};
static spin_lock_t userMemLock = {
    .cpuid = -1,
    .status = UNLOCKED,
};

static int num_free_kernel_records = 0;
static int num_free_user_records = 0;

void initMemoryAllocator() {
  spin_lock_init(&kernMemLock);
  spin_lock_init(&userMemLock);
}

// no need to hold lock, this can be done atomically
ptr_t allocKernelPage(int numPage) {
  spin_lock_acquire(&kernMemLock);
  for (int i = 0; i < num_free_kernel_records; i++) {
    if (released_kernel_pages[i].numPage == numPage) {
      // found a record
      ptr_t ret = released_kernel_pages[i].addr;
      // swap with the last record, and decrease the number of records
      if (num_free_kernel_records > 1)
        released_kernel_pages[i] =
            released_kernel_pages[num_free_kernel_records - 1];
      num_free_kernel_records--;
      return ret;
    }
  }
  ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
  kernMemCurr = ret + numPage * PAGE_SIZE;
  spin_lock_release(&kernMemLock);
  return ret;
}

// no need to hold lock, this can be done atomically
ptr_t allocUserPage(int numPage) {
  spin_lock_acquire(&userMemLock);
  for (int i = 0; i < num_free_user_records; i++) {
    if (released_user_pages[i].numPage == numPage) {
      // found a record
      ptr_t ret = released_user_pages[i].addr;
      // swap with the last record, and decrease the number of records
      if (num_free_user_records > 1)
        released_user_pages[i] = released_user_pages[num_free_user_records - 1];
      num_free_user_records--;
      return ret;
    }
  }
  ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
  userMemCurr = ret + numPage * PAGE_SIZE;
  spin_lock_release(&userMemLock);
  return ret;
}

// no need to hold lock, this can be done atomically
void freeKernelPage(ptr_t addr) {
  // assert that addr must be page aligned
  if (LOWBIT(addr) < PAGE_SIZE)
    panic("freeKernelPage: addr must be page aligned");
  spin_lock_acquire(&kernMemLock);
  released_kernel_pages[num_free_kernel_records++] = (release_record_t){addr, 1};
  spin_lock_release(&kernMemLock);
}

// no need to hold lock, this can be done atomically
void freeUserPage(ptr_t addr) {
  if (LOWBIT(addr) < PAGE_SIZE)
    panic("freeKernelPage: addr must be page aligned");
  spin_lock_acquire(&userMemLock);
  released_user_pages[num_free_user_records++] = (release_record_t){addr, 1};
  spin_lock_release(&userMemLock);
}

void freeUserPages(ptr_t addr, int numPage) {
  if (LOWBIT(addr) < PAGE_SIZE)
    panic("freeKernelPage: addr must be page aligned");
  spin_lock_acquire(&userMemLock);
    released_user_pages[num_free_user_records++] = (release_record_t){addr, numPage};
  spin_lock_release(&userMemLock);
}