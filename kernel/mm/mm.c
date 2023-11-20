#include "type.h"
#include <os/mm.h>
#include <os/sched.h>
#include <os/lock.h>
#include <printk.h>
#include <debugs.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;
static ptr_t released_kernel_pages[NUM_MAX_THREADS * 3];
static ptr_t released_user_pages[NUM_MAX_THREADS * 3];
static spin_lock_t kernMemLock = {
    .cpuid = -1,
    .status = UNLOCKED,
};
static spin_lock_t userMemLock = {
    .cpuid = -1,
    .status = UNLOCKED,
};

static int num_free_kernel_pages = 0;
static int num_free_user_pages = 0;

void initMemoryAllocator()
{
    spin_lock_init(&kernMemLock);
    spin_lock_init(&userMemLock);
}

// no need to hold lock, this can be done atomically
ptr_t allocKernelPage(int numPage)
{
    spin_lock_acquire(&kernMemLock);
    if (num_free_kernel_pages > 0)
    {
        ptr_t ret = released_kernel_pages[--num_free_kernel_pages];
        return ret;
    }
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    spin_lock_release(&kernMemLock);
    return ret;
}

// no need to hold lock, this can be done atomically
ptr_t allocUserPage(int numPage)
{
    spin_lock_acquire(&userMemLock);
    if (num_free_user_pages > 0)
    {
        ptr_t ret = released_user_pages[--num_free_user_pages];
        return ret;
    }
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    spin_lock_release(&userMemLock);
    return ret;
}

// no need to hold lock, this can be done atomically
void freeKernelPage(ptr_t addr)
{
    // assert that addr must be page aligned
    if (LOWBIT(addr) < PAGE_SIZE)
        panic("freeKernelPage: addr must be page aligned");
    spin_lock_acquire(&kernMemLock);
    released_kernel_pages[num_free_kernel_pages++] = addr;
    spin_lock_release(&kernMemLock);
}

// no need to hold lock, this can be done atomically
void freeUserPage(ptr_t addr)
{
    if (LOWBIT(addr) < PAGE_SIZE)
        panic("freeKernelPage: addr must be page aligned");
    spin_lock_acquire(&userMemLock);
    released_user_pages[num_free_user_pages++] = addr;
    spin_lock_release(&userMemLock);
}

void freeUserPages(ptr_t addr, int numPage)
{
    if (LOWBIT(addr) < PAGE_SIZE)
        panic("freeKernelPage: addr must be page aligned");
    spin_lock_acquire(&userMemLock);
    for (int i = 0; i < numPage; i++)
        released_user_pages[num_free_user_pages++] = addr + i * PAGE_SIZE;
    spin_lock_release(&userMemLock);
}