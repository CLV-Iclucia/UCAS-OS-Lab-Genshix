#include <os/irq.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <pgtable.h>

void vmprint(PTE* pagetable) {
  static int dep = 0;
  if (!dep) printk("page table %lx\n", pagetable);
  int bound = dep == 0 ? 64 : 512;
  for (int i = 0; i < bound; i++) {
    PTE pte = pagetable[i];
    if ((pte & PTE_V) && ((pte & (PTE_R | PTE_W | PTE_X)) == 0)) {
      // this PTE points to a lower-level page table.
      uint64_t child = ADDR(get_pa(pte));
      for (int j = 1; j <= dep; j++) printk(".. ");
      printk("..%d: pte %lx pa %lx\n", i, pte, child);
      dep++;
      vmprint(KPTR(PTE, pa2kva(PA(child))));
      dep--;
    } else if (pte & PTE_V) {
      for (int j = 1; j <= dep; j++) printk(".. ");
      printk("..%d: pte %lx pa %lx\n", i, pte, PTE2PA(pte));
    }
  }
}

void handle_segfault(pcb_t* p, tcb_t* t, uva_t fault_uva) {
  spin_lock_acquire(&p->lock);
  kill_pcb(p);
  spin_lock_release(&p->lock);
  // printk(
  //     "Invalid uva access: %lx\nProcess %d(%s) has been killed.",
  //     ADDR(fault_uva), p->pid, p->name);
  spin_lock_acquire(&t->lock);
  sched(t);
}

static PTE* get_pte(PTE* pgdir, uva_t uva) {
  for (int level = 2; level > 0; level--) {
    uint32_t vpn = VPN(ADDR(uva), level);
    if (pgdir[vpn] & PTE_V) {
      pa_t pa = get_pa(pgdir[vpn]);
      pgdir = KPTR(PTE, pa2kva(pa));
    } else
      return NULL;
  }
  uint32_t vpn = VPN(PGROUNDDOWN(ADDR(uva)), 0);
  return &pgdir[vpn];
}

static bool try_copy_on_write(pcb_t* p, tcb_t* t, uva_t fault_uva) {
  static spin_lock_t cow_lock = {-1, UNLOCKED};
  bool success = false;
  PTE* ppte = get_pte(p->pgdir, fault_uva);
  if (ppte == NULL) return false;
  if (*ppte & PTE_C) {
    spin_lock_acquire(&cow_lock);
    // because we record the ustack, this should be handled specially
    // if we copy on write a stack, the t->ustack should be changed as well
    bool reset_stack = is_within_stack(fault_uva, t);
    pa_t pa = get_pa(*ppte);
    spin_lock_acquire(&ref_cnt_lock);
    bool needs_copy = try_free_cow_page(pa);
    if (needs_copy) {
      kva_t nkva = kmalloc();
      memcpy(KPTR(void, nkva), KPTR(void, pa2kva(pa)), PAGE_SIZE);
      // printk("n = %d", *(int*)(ADDR(pa2kva(pa)) + 0xfc0));
      uint8_t idx = page_idx(nkva);
      pa_t npa = kva2pa(nkva);
      set_pfn(ppte, ADDR(npa) >> NORMAL_PAGE_SHIFT);
      ref_cnt[idx]++;
      if (reset_stack)
        t->ustack = nkva; 
    }
    if (reset_stack)
      set_attribute(ppte, PTE_U | PTE_R | PTE_W | PTE_V);
    else
      set_attribute(ppte, PTE_U | PTE_R | PTE_X | PTE_W | PTE_V);
    spin_lock_release(&ref_cnt_lock);
    success = true;
    spin_lock_release(&cow_lock);
  }
  return success;
}

void handle_pgfault(regs_context_t* regs, uint64_t stval, uint64_t scause) {
  uva_t fault_uva = UVA(stval);
  tcb_t* t = mythread();
  pcb_t* p = myproc();
  if (t->trapframe->sepc == 0 && stval == 0) {
    t->trapframe->a0() = 0;
    do_thread_exit();
  }
  if (is_valid_uva(fault_uva, p)) {
    if (scause == EXCC_STORE_PAGE_FAULT && try_copy_on_write(p, t, fault_uva))
      return;
    // it is a valid uva, but not loaded
    // load on demand
    uint64_t offset = ADDR(fault_uva) - USER_ENTRYPOINT_UVA;
    uint64_t load_offset = PGROUNDDOWN(offset);
    pa_t pa = PGROUNDDOWN(stval) - USER_ENTRYPOINT_UVA < p->filesz
                  ? load_task_by_name(p->name, load_offset)
                  : pmalloc();
    assert(ADDR(pa) != 0);
    spin_lock_acquire(&p->lock);
    uvmmap(UVA(PGROUNDDOWN(stval)), pa, p->pgdir,
           PTE_R | PTE_W | PTE_X | PTE_U);
    spin_lock_release(&p->lock);
  } else
    handle_segfault(p, t, fault_uva);
}