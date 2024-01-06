/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <os/lock.h>
#include <type.h>
#include <pgtable.h>
#include <assert.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K

#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_START_KVA 0xffffffc052000000
#define FREEMEM_START_PA  0x52000000
#define FREEMEM_END_KVA   0xffffffc060000000

#define STACK_TOP_UVA 0xf00010000
#define USER_ENTRYPOINT_UVA 0x100000

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define PGROUNDUP(sz)   ROUND(sz, PAGE_SIZE)
#define PGROUNDDOWN(a)  ROUNDDOWN(a, PAGE_SIZE)

typedef struct pcb pcb_t;
extern spin_lock_t ref_cnt_lock;
extern uint8_t ref_cnt[];
void init_memory();

// TODO [P4-task1] */
extern kva_t kmalloc(); // the only difference between kmalloc and pmalloc is the return type
static inline pa_t pmalloc() {
  return kva2pa(kmalloc());
}
extern void kfree(kva_t kva);
static inline void pfree(pa_t pa) {
  kfree(pa2kva(pa));
}
static inline uint8_t page_idx(kva_t kva) {
  uint64_t offset = ADDR(kva) - FREEMEM_START_KVA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  return idx;
}
extern void share_pgtable(kva_t dest_pgdir_kva, kva_t src_pgdir_kva);
extern kva_t alloc_page_helper(uint64_t va, PTE* pgdir);

void free_physical_page(pa_t pa);
// all the functions ended with "unmap" only clear the mapping in the page table
// the page table is not freed
void kvm_unmap_early();
void kvm_remap(kva_t kva, pa_t pa);
kva_t uvmcreate();

void uvmmap(uva_t uva, pa_t pa, PTE* pgdir, uint64_t attr);
pa_t uvmwalk(uva_t uva, PTE* pgdir, bool alloc);

// unmap a page starting at uva, and optionally free the physical page
void uvmunmap(PTE* pgdir, uva_t uva);
// run through the page table and free all the physical pages
void uvmfree(pcb_t* p);
void uvmclear(PTE* p, int level);
void kvmmap(kva_t kva, pa_t pa, PTE* pgdir, uint64_t attr);
// free a virtual page starting at va
void vmfree(PTE* pgtbl, uint64_t va);


// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
void do_shm_get(void);
void do_shm_dt(void);

void copy_pgdir(PTE* dest, PTE* src, uva_t from, uva_t to, int level);
int copy_page(PTE* dest, PTE* src, uva_t uva);

void inc_ref_cnt(kva_t kva);
bool try_free_cow_page(pa_t pa);
#endif /* MM_H */
