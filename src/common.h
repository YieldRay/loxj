#ifndef loxj_common_h
#define loxj_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// condition to enable trace

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

#define NAN_BOXING // 需要确保 CPU 支持

#define LOXJ_OPTIONS_NATIVE
#define LOXJ_OPTIONS_ESCAPE
#define LOXJ_OPTIONS_SLEEP              // 启用 sleep 函数（非 C 标准库的）
#define LOXJ_OPTIONS_INIT "constructor" // 类构造器函数名，默认为 init
#define LOXJ_OPTIONS_INIT_LENGTH 11     // 上面字符串的长度
#define LOXJ_OPTIMIZE_HASH

#undef DEBUG_TRACE_EXECUTION
#undef DEBUG_PRINT_CODE
#undef DEBUG_STRESS_GC
#undef DEBUG_LOG_GC

#endif