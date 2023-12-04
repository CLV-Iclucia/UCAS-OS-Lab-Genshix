#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <pgtable.h>

pa_t load_task_img(int taskid, uint64_t offset);
pa_t load_task_by_name(const char* name, uint64_t offset);
uint32_t get_task_filesz(const char* name);
uint32_t get_task_memsz(const char* name);
#endif
