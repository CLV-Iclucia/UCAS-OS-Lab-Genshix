#ifndef DEBUGS_H
#define DEBUGS_H

#define LOG_PROC_ENABLED false
#define LOG_LOCK_ENABLED false
#define LOG_INTR_ENABLED false
#define LOG_SYSCALL_ENABLED false
#define LOG_MEM_ENABLED false

#define log(OPT, fmt, ...) \
do {\
    if (LOG_##OPT##_ENABLED) \
        printl("["#OPT"]--------------------------\n" fmt "\n", ##__VA_ARGS__); \
} while(0)

#define log_line(OPT, fmt, ...) \
do {\
    if (LOG_##OPT##_ENABLED) \
        printl("["#OPT"] "fmt "\n", ##__VA_ARGS__); \
} while(0)

#define start_log_block(OPT) \
do {\
    if (LOG_##OPT##_ENABLED) \
        printl("["#OPT " LOG BLOCK] {\n"); \
} while(0)

#define end_log_block(OPT) \
do {\
    if (LOG_##OPT##_ENABLED) \
        printl("}\n"); \
} while(0)

#define log_block(OPT, statement) \
do {\
    if (LOG_##OPT##_ENABLED) \
    {\
        start_log_block(OPT);\
        statement;\
        end_log_block(OPT);\
    }\
} while(0)

#define panic(fmt, ...) \
do {\
    printk("[PANIC] " fmt "\n", ##__VA_ARGS__);\
    while(1);\
} while(0)
#endif

void kernel_self_check();