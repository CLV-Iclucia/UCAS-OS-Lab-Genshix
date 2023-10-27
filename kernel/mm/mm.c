#include <os/mm.h>
#include <os/sched.h>
#include <printk.h>
#include <debugs.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;
static ptr_t released_kernel_pages[NUM_MAX_THREADS * 3];
static ptr_t released_user_pages[NUM_MAX_THREADS * 3];
static int num_free_kernel_pages = 0;
static int num_free_user_pages = 0;
ptr_t allocKernelPage(int numPage)
{
    // align PAGE_SIZE
    if (num_free_kernel_pages > 0)
    {
        ptr_t ret = released_kernel_pages[--num_free_kernel_pages];
        return ret;
    }
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

ptr_t allocUserPage(int numPage)
{
    // align PAGE_SIZE
    if (num_free_user_pages > 0)
    {
        ptr_t ret = released_user_pages[--num_free_user_pages];
        return ret;
    }
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

void freeKernelPage(ptr_t addr)
{
    // assert that addr must be page aligned
    if (LOWBIT(addr) < PAGE_SIZE)
        panic("freeKernelPage: addr must be page aligned");
    released_kernel_pages[num_free_kernel_pages++] = addr;
}

void freeUserPage(ptr_t addr)
{
    if (LOWBIT(addr) < PAGE_SIZE)
        panic("freeKernelPage: addr must be page aligned");
    released_user_pages[num_free_user_pages++] = addr;
}
