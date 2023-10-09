#ifndef DEBUGS_H
#define DEBUGS_H

// #define LOG_PROC_ENABLED
#define LOG_LOCK_ENABLED

#define LOG(OPT, fmt, ...) LOG_##OPT(fmt)

#ifndef LOG_PROC_ENABLED
#define LOG_PROC(fmt, ...)
#else
#define LOG_LOCK(fmt, ...)
#endif

