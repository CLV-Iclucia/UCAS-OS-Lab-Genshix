#include <assert.h>
#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>
// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

static void* ioremap_normal(unsigned long phys_addr, unsigned long size) {
  assert(NORMAL_PAGE_ALIGNED(phys_addr));
  assert(NORMAL_PAGE_ALIGNED(size));
  for (uint32_t addr = phys_addr, kaddr = io_base; addr < phys_addr + size;
       addr += NORMAL_PAGE_SIZE, kaddr += NORMAL_PAGE_SIZE) {
    assert(is_kva(kaddr));
    kvmmap(KVA(kaddr), PA(addr), kpgdir, PTE_X | PTE_W | PTE_R);
  }
  uintptr_t ret_kaddr = io_base;
  io_base += size;
  return (void*)ret_kaddr;
}

static void* ioremap_large(unsigned long phys_addr, unsigned long size) {
  return NULL;
}

void* ioremap(unsigned long phys_addr, unsigned long size) {
  // TODO: [p5-task1] map one specific physical region to virtual address
  if (size < LARGE_PAGE_SIZE)
    return ioremap_normal(phys_addr, size);
  else
    return ioremap_large(phys_addr, size);
}

void iounmap(void* io_addr) {
  // TODO: [p5-task1] a very naive iounmap() is OK
  // maybe no one would call this function?
}
