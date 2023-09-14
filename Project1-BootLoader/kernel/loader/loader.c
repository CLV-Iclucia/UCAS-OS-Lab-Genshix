#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

extern uint32_t tasknum;
extern uint32_t meta_offset;
extern char getc_img(uint32_t offset);
extern void panic(const char *);
uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    if (taskid < 1 || taskid > tasknum) {
        panic("taskid out of range!\n\r");
    }
    uint32_t entry_point = 0x52000000 + (taskid - 1) * 0x10000;
    uint32_t task_start = tasks[taskid - 1].offset;
    uint32_t task_end = taskid == tasknum ? meta_offset : tasks[taskid].offset;
    uint32_t pa = entry_point, p = task_start;
    for (; p < task_end; p++, pa++)
        *(char*)pa = getc_img(p);
    return entry_point;
}

static int strcmp_img(const char* name, uint32_t img_name) {
    char* pa = name;
    uint32_t p = img_name;
    while (*pa != '\0' && getc_img(p) != '\0') {
        if (*pa != getc_img(p)) return -1;
        pa++;
        p++;
    }
    if (*pa != '\0' || getc_img(p) != '\0') return -1;
    else return 0;
}

uint64_t load_task_by_name(const char* name) {
    for (int i = 0; i < tasknum; i++) {
        if (strcmp_img(name, tasks[i].name_offset) == 0)
            return load_task_img(i + 1);
    }
    return 0;
}
