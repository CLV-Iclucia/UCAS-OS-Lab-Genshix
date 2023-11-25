#ifndef ASSERT_H
#define ASSERT_H

#include <screen.h>
#include <printk.h>

static inline void _panic(const char* file_name,int lineno, const char* func_name)
{
    screen_clear();
    printk("Assertion failed at %s in %s:%d\n\r",
           func_name,file_name,lineno);
    for(;;);
}

static inline void _panic_msg(const char* file_name,int lineno, const char* func_name, const char* msg)
{
    printk("Assertion failed at %s in %s:%d\n\r",
           func_name,file_name,lineno);
    printk("Message: %s\n\r",msg);
    for(;;);
}

#define assert(cond)                                 \
    {                                                \
        if (!(cond)) {                               \
            _panic(__FILE__, __LINE__,__FUNCTION__); \
        }                                            \
    }

// an assert with a message
#define assert_msg(cond,msg)                         \
    {                                                \
        if (!(cond)) {                               \
            _panic_msg(__FILE__, __LINE__,__FUNCTION__,msg); \
        }                                            \
    }

#endif /* ASSERT_H */
