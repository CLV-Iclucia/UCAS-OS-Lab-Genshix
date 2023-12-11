#include <os/kernel.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/task.h>
#include <type.h>

#include <pgtable.h>

extern uint32_t tasknum;
extern uint32_t name_region_offset;
extern char getc_img(uint32_t offset);
extern void init_panic(const char*);

// load a page starting at offset
pa_t load_task_img(int taskid, uint64_t offset) {
  assert(NORMAL_PAGE_ALIGNED(offset));
  if (taskid < 1 || taskid > tasknum) init_panic("taskid out of range!\n\r");
  uint64_t task_start = tasks[taskid - 1].offset;
  uint64_t task_end =
      taskid == tasknum ? name_region_offset : tasks[taskid].offset;
  uint64_t load_start = task_start + offset;
  uint64_t load_end =
      load_start + offset >= task_end ? task_end : load_start + PAGE_SIZE;
  kva_t load_kva = kmalloc();
  uint64_t kva = ADDR(load_kva), p = load_start;
  uint32_t load_len = 0;
  for (; p < load_end; kva++, p++, load_len++) 
    *(char*)kva = getc_img(p);
  for (; load_len < PAGE_SIZE; kva++, load_len++) 
    *(char*)kva = 0;
  return kva2pa(load_kva);
}

static int strcmp_img(const char* name, uint32_t img_name) {
  char* pa = name;
  uint32_t p = img_name;
  while (*pa != '\0' && getc_img(p) != '\0') {
    if (*pa != getc_img(p)) return -1;
    pa++;
    p++;
  }
  if (*pa != '\0' || getc_img(p) != '\0')
    return -1;
  else
    return 0;
}

pa_t load_task_by_name(const char* name, uint64_t offset) {
  for (int i = 0; i < tasknum; i++) {
    if (strcmp_img(name, tasks[i].name_offset) == 0)
      return load_task_img(i + 1, offset);
  }
  return PA(0);
}

uint32_t get_task_filesz(const char* name) {
  for (int i = 0; i < tasknum; i++) {
    if (strcmp_img(name, tasks[i].name_offset) == 0) {
      uint64_t task_start = tasks[i].offset;
      uint64_t task_end = i == tasknum - 1 ? name_region_offset : tasks[i + 1].offset;
      assert(task_end > task_start);
      return task_end - task_start;
    }
  }
  return 0;
}

uint32_t get_task_memsz(const char* name) {
  for (int i = 0; i < tasknum; i++) {
    if (strcmp_img(name, tasks[i].name_offset) == 0)
      return tasks[i].memsz;
  }
  return 0;
}