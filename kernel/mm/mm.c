#include <assert.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>
#include <pgtable.h>

#define PAGE_NUM (FREEMEM_END_KVA - FREEMEM_START_KVA) / PAGE_SIZE

PTE* kpgdir = (PTE*)PGDIR_KVA;
spin_lock_t kpgdir_lock;
typedef struct physical_page {
  kva_t next, prev;
} physical_page_t;

static physical_page_t freemem_list;
static spin_lock_t freemem_lock;
void init_memory() {
  // link all physical pages between FREEMEM_START_KVA and FREEMEM_END_KVA
  // into a list, and set freemem_list to the head of the list
  freemem_list.next = KVA(FREEMEM_START_KVA);
  freemem_list.prev = KVA(FREEMEM_END_KVA);
  for (uint64_t kva = FREEMEM_START_KVA; kva < FREEMEM_END_KVA;
       kva += PAGE_SIZE) {
    physical_page_t* page = (physical_page_t*)kva;
    if (kva == FREEMEM_END_KVA - PAGE_SIZE)
      page->next = KVA((uint64_t)(&freemem_list));
    else
      page->next = KVA(kva + PAGE_SIZE);
    if (kva != FREEMEM_START_KVA)
      page->prev = KVA(kva - PAGE_SIZE);
    else
      page->prev = KVA((uint64_t)(&freemem_list));
  }
  spin_lock_init(&freemem_lock);
}

static uint8_t ref_cnt[PAGE_NUM];
spin_lock_t ref_cnt_lock;
static uint8_t get_ref_cnt(kva_t pa) {
  spin_lock_acquire(&ref_cnt_lock);
  uint64_t offset = ADDR(pa) - FREEMEM_START_PA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  uint8_t cnt = ref_cnt[idx];
  spin_lock_release(&ref_cnt_lock);
  return cnt;
}

void inc_ref_cnt(kva_t kva) {
  spin_lock_acquire(&ref_cnt_lock);
  uint64_t offset = ADDR(kva) - FREEMEM_START_KVA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  ref_cnt[idx]++;
  spin_lock_release(&ref_cnt_lock);
}

static void dec_ref_cnt(kva_t kva) {
  spin_lock_acquire(&ref_cnt_lock);
  uint64_t offset = ADDR(kva) - FREEMEM_START_KVA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  assert(idx >= 0 && idx < PAGE_NUM);
  assert(ref_cnt[idx] > 0);
  ref_cnt[idx]--;
  spin_lock_release(&ref_cnt_lock);
}

static uint8_t dec_and_get_ref_cnt(kva_t kva) {
  spin_lock_acquire(&ref_cnt_lock);
  uint64_t offset = ADDR(kva) - FREEMEM_START_KVA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  assert(idx >= 0 && idx < PAGE_NUM);
  assert(ref_cnt[idx] > 0);
  ref_cnt[idx]--;
  int cnt = ref_cnt[idx];
  spin_lock_release(&ref_cnt_lock);
  return cnt;
}

void kfree(kva_t kva) {
  spin_lock_acquire(&freemem_lock);
  physical_page_t* page = KPTR(physical_page_t, kva);
  page->next = freemem_list.next;
  page->prev = KVA((uint64_t)(&freemem_list));
  KPTR(physical_page_t, freemem_list.next)->prev = kva;
  freemem_list.next = kva;
  spin_lock_release(&freemem_lock);
}

kva_t kmalloc() {
  spin_lock_acquire(&freemem_lock);
  if (KPTR(physical_page_t, freemem_list.next) == &freemem_list) return KVA(-1);
  kva_t kva = freemem_list.next;
  freemem_list.next = KPTR(physical_page_t, kva)->next;
  KPTR(physical_page_t, freemem_list.next)->prev = KVA((&freemem_list));
  memset(KPTR(void, kva), 5, PAGE_SIZE);
  spin_lock_release(&freemem_lock);
  return kva;
}

pa_t uvmwalk(uva_t uva, PTE* pgdir, bool alloc) {
  assert(is_kva(pgdir));
  uint64_t offset = ADDR(uva) & 0xfff;
  pa_t pa;
  for (int level = 2; level >= 0; level--) {
    uint32_t vpn = VPN(ADDR(uva), level);
    if (pgdir[vpn] & PTE_V) {
      pa = get_pa(pgdir[vpn]);
      pgdir = KPTR(PTE, pa2kva(pa));
    } else if (alloc) {
      kva_t kva = kmalloc();
      pa = kva2pa(kva);
      set_pfn(&pgdir[vpn], ADDR(pa) >> NORMAL_PAGE_SHIFT);
      set_attribute(&pgdir[vpn], PTE_R | PTE_W | PTE_X | PTE_U);
      pgdir = KPTR(PTE, pa2kva(pa));
    } else
      return PA(0);
  }
  return PA(ADDR(pa) | offset);
}
// unmap the large virtual page starting with kva
void kvm_unmap_early() {
  for (uint64_t kva = 0x50000000lu; kva < 0x51000000lu; kva += 0x200000lu) {
    assert(LARGE_PAGE_ALIGNED(kva));
    uint32_t vpn2 = VPN(kva, 2);
    uint32_t vpn1 = VPN(kva, 1);
    PTE pte1 = kpgdir[vpn2];
    pa_t pa = get_pa(pte1);  // first level page table
    PTE* pgdir = KPTR(PTE, pa2kva(pa));
    assert(pgdir[vpn1] & PTE_V);
    pgdir[vpn1] ^= PTE_V;
  }
  for (uint64_t kva = 0x50000000lu; kva < 0x51000000lu; kva += 0x200000lu) {
    uint32_t vpn2 = VPN(kva, 2);
    kpgdir[vpn2] = 0;
  }
}

static void set_pgdir(kva_t pgdir_kva) {
  memset(KPTR(void, pgdir_kva), 0, PAGE_SIZE);
}

// map a normal virtual page starting with va to physical page starting with pa
// va must be page aligned, and pa must be allocated
void uvmmap(uva_t uva, pa_t mapped_pa, PTE* pgdir, uint64_t attr) {
  assert(attr & PTE_U);
  assert(NORMAL_PAGE_ALIGNED(ADDR(uva)));
  for (int level = 2; level >= 0; level--) {
    assert(is_kva(pgdir));
    uint32_t vpn = VPN(ADDR(uva), level);
    if (pgdir[vpn] & PTE_V) {
      pa_t pa = get_pa(pgdir[vpn]);
      pgdir = KPTR(PTE, pa2kva(pa));
    } else {
      if (level == 0) {
        set_pfn(&pgdir[vpn], ADDR(mapped_pa) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn], PTE_V | attr);
        break;
      }
      PTE* new_pgdir_kptr = new_pgdir();
      pa_t new_pgdir_pa = kva2pa(KVA(new_pgdir_kptr));
      set_pfn(&pgdir[vpn], ADDR(new_pgdir_pa) >> NORMAL_PAGE_SHIFT);
      set_attribute(&pgdir[vpn], PTE_V);
      pgdir = new_pgdir_kptr;
    }
  }
  inc_ref_cnt(pa2kva(mapped_pa));
}

void uvmunmap(PTE* pgdir, uva_t uva) {
  assert(is_kva(pgdir));
  uint32_t vpn;
  for (int level = 2; level > 0; level--) {
    vpn = VPN(ADDR(uva), level);
    if (pgdir[vpn] & PTE_V) {
      pa_t pa = get_pa(pgdir[vpn]);
      pgdir = KPTR(PTE, pa2kva(pa));
    }
  }
  vpn = VPN(ADDR(uva), 0);
  free_physical_page(get_pa(pgdir[vpn]));
  pgdir[vpn] = 0;
}

void free_physical_page(pa_t pa) {
  kva_t kva = pa2kva(pa);
  if (dec_and_get_ref_cnt(kva) == 0) kfree(kva);
}

bool try_free_cow_page(pa_t pa) {
  spin_lock_acquire(&ref_cnt_lock);
  uint64_t offset = ADDR(pa) - FREEMEM_START_PA;
  assert(offset % PAGE_SIZE == 0);
  uint64_t idx = offset / PAGE_SIZE;
  assert(idx >= 0 && idx < PAGE_NUM);
  uint8_t cnt = ref_cnt[idx];
  assert(cnt > 0);
  if (cnt == 1) {
    spin_lock_release(&ref_cnt_lock);
    return false;
  }
  ref_cnt[idx]--;
  spin_lock_release(&ref_cnt_lock);
  return true;  
}

void uvmfree(pcb_t* p) {
  assert(is_kva(p->pgdir));
  for (uint64_t uva = USER_ENTRYPOINT_UVA; uva < USER_ENTRYPOINT_UVA + p->size;
       uva += PAGE_SIZE) {
    pa_t pa = uvmwalk(UVA(uva), p->pgdir, 0);
    if (ADDR(pa) != 0) free_physical_page(pa);
  }
}

// must be called after freeing all the physical pages
// free all levels of page table recursively
// note: ref cnt is not used for this, ref cnt is only for USER MAPPED PHYSICAL
// PAGES the main difference is that, the pages that need ref cnt are created in
// uvmmap other pages are created by calling kmalloc directly so when free these
// pages, we call kfree directly
void uvmclear(PTE* pgdir, int level) {
  assert(is_kva(pgdir));
  if (level == 0) return;
  uint32_t upper_bound = level == 2 ? KVA_VPN_2 : 512;
  for (int i = 0; i < upper_bound; i++) {
    PTE pte = pgdir[i];
    if ((pte & PTE_V) && !(pte & PTE_U)) {
      pa_t pa = get_pa(pte);
      PTE* pgtbl = KPTR(PTE, pa2kva(pa));
      uvmclear(pgtbl, level - 1);
      kfree(pa2kva(pa));
    }
  }
}

void kvmmap(kva_t kva, pa_t pa, PTE* pgdir, uint64_t attr) {
  assert(!(attr & PTE_U));
  assert(NORMAL_PAGE_ALIGNED(ADDR(kva)));
  for (int level = 2; level >= 0; level--) {
    assert(is_kva(pgdir));
    uint32_t vpn = VPN(ADDR(kva), level);
    if (pgdir[vpn] & PTE_V) {
      pa = get_pa(pgdir[vpn]);
      pgdir = KPTR(PTE, pa2kva(pa));
    } else {
      set_pfn(&pgdir[vpn], ADDR(pa) >> NORMAL_PAGE_SHIFT);
      set_attribute(&pgdir[vpn], level == 0 ? PTE_V | attr : PTE_V);
      kva_t new_pgdir_kva = kmalloc();
      pgdir = KPTR(PTE, new_pgdir_kva);
    }
  }
}

/* this is used for mapping kernel virtual address into user page table */
// must be called with the lock protecting the dest page tables held
void share_pgtable(kva_t dest_pgdir_kva, kva_t src_pgdir_kva) {
  PTE* dest_pgdir = KPTR(PTE, dest_pgdir_kva);
  PTE* src_pgdir = KPTR(PTE, src_pgdir_kva);
  for (int i = 0; i < 512; i++) {
    if (src_pgdir[i] & PTE_V) {
      assert(!(dest_pgdir[i] & PTE_V));
      dest_pgdir[i] = src_pgdir[i];
    }
  }
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
kva_t alloc_page_helper(uint64_t va, PTE* pgdir) {
  // kva_t kva = kmalloc();
  // assert(NORMAL_PAGE_ALIGNED(ADDR(kva)));
  // pa_t pa = kva2pa(kva);

  // for (int level = 2; level >= 0; level--) {
  //   uint32_t vpn = VPN(va, level);
  //   PTE* pgtbl = KPTR(PTE, pa2kva(pa));
  //   if (pgdir[vpn] & PTE_V) {
  //     pa = get_pa(pgdir[vpn]);
  //     pgdir = KPTR(PTE, pa2kva(pa));
  //   } else {
  //     set_pfn(&pgdir[vpn], ADDR(pa) >> NORMAL_PAGE_SHIFT);
  //     set_attribute(&pgdir[vpn], PTE_V | PTE_R | PTE_W | PTE_X);
  //     pa = kva2pa(kva);
  //     pgdir = pgtbl;
  //   }
  // }
  // return kva;
}

void do_shm_get(void) {
  tcb_t* t = mythread();
  pcb_t* p = myproc();
  int key;
  if (argint(0, &key) < 0) {
    return;
  }
  //  t->trapframe->a0() = shm_page_get(key);
}

void copy_pgdir(PTE* npgdir, PTE* pgdir, uva_t from, uva_t to, int level) {
  assert(is_kva(npgdir) && is_kva(pgdir));
  int lower_bound = level == 2 ? VPN(ADDR(from), 2) : 0;
  int upper_bound = level == 2 ? VPN(ADDR(to), 2) + 1 : 512;
  for (int i = lower_bound; i < upper_bound; i++) {
    PTE pte = pgdir[i];
    if (!(pte & PTE_V)) continue;
    PTE npte = npgdir[i];
    assert(!(npte & PTE_V));
    pa_t pa = get_pa(pte);
    PTE* pgtbl = KPTR(PTE, pa2kva(pa));
    if (level) {
      PTE* new_pgtbl = new_pgdir();
      pa_t new_pgtbl_pa = kva2pa(KVA(new_pgtbl));
      set_pfn(&npgdir[i], ADDR(new_pgtbl_pa) >> NORMAL_PAGE_SHIFT);
      set_attribute(&npgdir[i], PTE_V);
      copy_pgdir(new_pgtbl, pgtbl, from, to, level - 1);
    } else {
      set_pfn(&npgdir[i], ADDR(pa) >> NORMAL_PAGE_SHIFT);
      if (pte & PTE_S) // do not copy shared pages
        continue;
      set_attribute(&npgdir[i], PTE_V | PTE_U | PTE_R | PTE_X | PTE_C);
      set_attribute(&pgdir[i], PTE_V | PTE_U | PTE_R | PTE_X | PTE_C);
      inc_ref_cnt(pa2kva(pa));
    }
  }
}

int copy_page(PTE* npgdir, PTE* pgdir, uva_t uva) {
  for (int level = 2; level >= 0; level--) {
    uint32_t vpn = VPN(ADDR(uva), level);
    PTE pte = pgdir[vpn];
    if (!(pte & PTE_V)) return -1;
    pa_t pa = get_pa(pte);
    PTE* pgtbl = KPTR(PTE, pa2kva(pa));
    if (level) {
      PTE* new_pgtbl = new_pgdir();
      pa_t new_pgtbl_pa = kva2pa(KVA(new_pgtbl));
      set_pfn(&npgdir[vpn], ADDR(new_pgtbl_pa) >> NORMAL_PAGE_SHIFT);
      set_attribute(&npgdir[vpn], PTE_V);
      npgdir = new_pgtbl;
      pgdir = pgtbl;
    } else {
      set_pfn(&npgdir[vpn], ADDR(pa) >> NORMAL_PAGE_SHIFT);
      if (pgdir[vpn] & PTE_S) // do not copy shared pages
        continue;
      set_attribute(&npgdir[vpn], PTE_V | PTE_R | PTE_U | PTE_X | PTE_C);
      set_attribute(&pgdir[vpn], PTE_V | PTE_R | PTE_U | PTE_X | PTE_C);
      inc_ref_cnt(pa2kva(pa));
    }
  }
  return 0;
}

#define SHARED_PAGE_NUM 32


static int shared_mapping_pool[SHARED_PAGE_NUM];
static int shared_mapping_pool_head = 0;
static int shared_mapping_pool_tail = SHARED_PAGE_NUM - 1;
typedef struct shared_page_mapping {
  int key;
  pa_t pa;
} shared_page_mapping_t;
static shared_page_mapping_t shared_page_mappings[SHARED_PAGE_NUM];
spin_lock_t shared_page_lock;
void init_shared_pages() {
  for (int i = 0; i < SHARED_PAGE_NUM; i++)
    shared_mapping_pool[i] = i;
  spin_lock_init(&shared_page_lock);
}

uint64_t get_shared_page(int key) {
  spin_lock_acquire(&shared_page_lock);
  for (int i = 0; i < SHARED_PAGE_NUM; i++) {
    if (shared_page_mappings[i].key == key) {
      spin_lock_release(&shared_page_lock);
      return ADDR(shared_page_mappings[i].pa);
    }
  }
  spin_lock_release(&shared_page_lock);
  return 0;
}

void do_shm_dt(void) {
  uint64_t addr = argraw(0);
  shm_page_dt(addr);
}

void shm_page_dt(uintptr_t addr) {
  
}