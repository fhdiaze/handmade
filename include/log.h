// clang-format Language: C

#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#define LOG_LEVEL_ALL 0UL
#define LOG_LEVEL_TRACE 1UL
#define LOG_LEVEL_DEBUG 2UL
#define LOG_LEVEL_INFO 3UL
#define LOG_LEVEL_WARN 4UL
#define LOG_LEVEL_ERROR 5UL
#define LOG_LEVEL_FATAL 6UL
#define LOG_LEVEL_OFF 7UL

// Defines what is the minimum priority of a message to be logged.
// Anything with higher priority is going to be logged.
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ALL
#endif

#define LOG_MSG(log_level_pr, log_level_str, fmt, file_name, line_number, ...) \
        do {                                                                   \
                if (LOG_LEVEL <= log_level_pr)                                 \
                        printf("[" log_level_str "] %s:%lu : " fmt, file_name, \
                               line_number, __VA_ARGS__);                      \
        } while (0)

// Logs a trace message if LOG_LEVEL <= LOG_LEVEL_TRACE
#define logt(fmt, ...)                                                   \
        LOG_MSG(LOG_LEVEL_TRACE, "TRACE", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

// Logs a debug message if LOG_LEVEL <= LOG_LEVEL_DEBUG.
// Usage: logd("Debug info: x=%d\n", x);
#define logd(fmt, ...)                                                   \
        LOG_MSG(LOG_LEVEL_DEBUG, "DEBUG", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

// Logs an information message if LOG_LEVEL <= LOG_LEVEL_INFO
#define logi(fmt, ...)                                                 \
        LOG_MSG(LOG_LEVEL_INFO, "INFO", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

// Logs a warning message if LOG_LEVEL <= LOG_LEVEL_WARN
#define logw(fmt, ...)                                                 \
        LOG_MSG(LOG_LEVEL_WARN, "WARN", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

// Logs an error message if LOG_LEVEL <= LOG_LEVEL_ERROR
#define loge(fmt, ...)                                                   \
        LOG_MSG(LOG_LEVEL_ERROR, "ERROR", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

// Logs a fatal message if LOG_LEVEL <= LOG_LEVEL_FATAL
#define logf(fmt, ...)                                                   \
        LOG_MSG(LOG_LEVEL_FATAL, "FATAL", fmt, __FILE__, __LINE__ + 0UL, \
                __VA_ARGS__)

#endif
