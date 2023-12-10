#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void) {
  __asm__ __volatile__("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr) {
  __asm__ __volatile__("sfence.vma %0" : : "r"(addr) : "memory");
}

static inline void local_flush_icache_all(void) {
  asm volatile("fence.i" ::: "memory");
}

static inline void set_satp(unsigned mode, unsigned asid, unsigned long ppn) {
  unsigned long __v =
      (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) |
                      ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
  __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR
#define PGDIR_KVA 0xffffffc051000000lu
static inline void switch_kpgdir() {
  set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
  local_flush_tlb_all();
}



/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define PTE_V (1 << 0)
#define PTE_R (1 << 1)   /* Readable */
#define PTE_W (1 << 2)  /* Writable */
#define PTE_X (1 << 3)   /* Executable */
#define PTE_U (1 << 4)   /* User */
#define PTE_G (1 << 5) /* Global */
#define PTE_A                                        \
  (1 << 6)                   /* Set by hardware on any access \
                              */
#define PTE_D (1 << 7) /* Set by hardware on any write */
#define PTE_S (1 << 8)  /* Reserved for software-shared */
#define PTE_C (1 << 9) /* Copy on write bit */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)
#define PTE_MASK ((1lu << 54) - 1)


#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)
#define NORMAL_PAGE_ALIGNED(addr) (((addr) & (NORMAL_PAGE_SIZE - 1)) == 0)
#define LARGE_PAGE_ALIGNED(addr) (((addr) & (LARGE_PAGE_SIZE - 1)) == 0)
#define KVA_START 0xffffffc050000000lu
#define KVA_VPN_2 VPN(KVA_START, 2)

typedef uint64_t PTE;

typedef struct kva {
  uint64_t addr;
} kva_t;

typedef struct pa {
  uint64_t addr;
} pa_t;

typedef struct uva {
  uint64_t addr;
} uva_t;

#define KVA(addr) ((kva_t){(uint64_t)(addr)})
#define KPTR(type, kva) ((type *)((kva).addr))
#define PA(addr) ((pa_t){(uint64_t)(addr)})
#define PPTR(type, pa) ((type *)((pa).addr))
#define UVA(addr) ((uva_t){(uint64_t)(addr)})
#define UPTR(type, uva) ((type *)((uva).addr))
#define ADDR(address) ((address).addr)
#define VPN(address, level) ((address >> (NORMAL_PAGE_SHIFT + (9 * level))) & ((1 << PPN_BITS) - 1))
#define PTE2PA(pte) (((pte >> _PAGE_PFN_SHIFT) & PTE_MASK) << NORMAL_PAGE_SHIFT)

static inline void switch_pgdir(int pid, pa_t pgdir_pa) {
  set_satp(SATP_MODE_SV39, pid, ADDR(pgdir_pa) >> NORMAL_PAGE_SHIFT);
  local_flush_tlb_all();
}

extern PTE* kpgdir;

static inline void my_assert(int cond) {
  if (!cond) {
    while (1)
      ;
  }
}

extern kva_t kmalloc();


/* Translation between physical addr and kernel virtual addr */
static inline pa_t kva2pa(kva_t kva) {
  my_assert(kva.addr >= 0xffffffc000000000);
  return PA(kva.addr - 0xffffffc000000000);
}

static inline kva_t pa2kva(pa_t pa) {
  my_assert(pa.addr <= 0x3fffffffff);
  return KVA(pa.addr + 0xffffffc000000000);
}

static inline int is_large_page(PTE entry) {
  return (entry & PTE_V) && ((entry & (PTE_R | PTE_W | PTE_X)));
}

#define is_kva(addr) ((((uint64_t)(addr)) & 0xffffffc000000000))

static inline int valid(PTE entry) { return (entry & PTE_V); }

/* get physical page addr from PTE 'entry' */
static inline pa_t get_pa(PTE entry) {
  if (!valid(entry)) return PA(0);
  return PA(((entry >> _PAGE_PFN_SHIFT) & PTE_MASK) << NORMAL_PAGE_SHIFT);
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry) { return (entry >> _PAGE_PFN_SHIFT); }
static inline void set_pfn(PTE *entry, uint64_t pfn) {
  *entry |= (pfn << _PAGE_PFN_SHIFT);
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask) {
  return entry & mask;
}
static inline void set_attribute(PTE *entry, uint64_t bits) { *entry |= bits; }

extern void memset(void *dest, uint8_t val, uint32_t len);
static inline void clear_pgdir(pa_t pgdir_addr) {
  my_assert((ADDR(pgdir_addr) & (NORMAL_PAGE_SIZE - 1)) == 0);
  memset(PPTR(void, pgdir_addr), 0, NORMAL_PAGE_SIZE);
}

static inline PTE* new_pgdir() {
  PTE* pgdir = KPTR(PTE, kmalloc());
  if (pgdir == NULL) return NULL;
  memset(pgdir, 0, NORMAL_PAGE_SIZE);
  return pgdir;
}

#endif  // PGTABLE_H