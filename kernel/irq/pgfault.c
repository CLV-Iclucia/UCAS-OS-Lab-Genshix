#include <os/irq.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>

void vmprint(PTE *pagetable) {
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
  printk("Invalid uva access: %lx\nProcess %s has been killed. The fault instruction is %lx.",
          ADDR(fault_uva), p->name, t->trapframe->sepc);
  spin_lock_acquire(&t->lock);
  sched(t);
}

void handle_pgfault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
  uva_t fault_uva = UVA(stval);
  tcb_t *t = mythread();
  pcb_t *p = myproc();
  if (is_valid_uva(fault_uva, p)) {
    // it is a valid uva, but not loaded
    // load on demand
    uint64_t offset = ADDR(fault_uva) - USER_ENTRYPOINT_UVA;
    uint64_t load_offset = PGROUNDDOWN(offset);
    pa_t pa = stval - USER_ENTRYPOINT_UVA < p->filesz
                  ? load_task_by_name(p->name, load_offset)
                  : pmalloc();
    assert(ADDR(pa) != 0);
    if (stval - USER_ENTRYPOINT_UVA >= p->filesz)
      memset(KPTR(void, pa2kva(pa)), 0, PAGE_SIZE);
    spin_lock_acquire(&p->lock);
    uvmmap(UVA(PGROUNDDOWN(stval)), pa, p->pgdir,
           PTE_R | PTE_W | PTE_X | PTE_U);
  //  vmprint(p->pgdir);
    spin_lock_release(&p->lock);
  } else
    handle_segfault(p, t, fault_uva);
}