// clang-format Language: C

#ifndef TIX_LIB_H
#define TIX_LIB_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h> // IWYU pragma: keep
#include <time.h>

// Constants
static constexpr float PIE = 3.14159265359F;
#define TIX_LOG_TSTAMP_BUF_SIZE 32
#define TIX_LOG_LEVEL_ALL 0UL
#define TIX_LOG_LEVEL_TRACE 1UL
#define TIX_LOG_LEVEL_DEBUG 2UL
#define TIX_LOG_LEVEL_INFO 3UL
#define TIX_LOG_LEVEL_WARN 4UL
#define TIX_LOG_LEVEL_ERROR 5UL
#define TIX_LOG_LEVEL_FATAL 6UL
#define TIX_LOG_LEVEL_OFF 7UL

#define KB_TO_BYTES(_pr_v) ((_pr_v) * 1024)
#define MB_TO_BYTES(_pr_v) (KB_TO_BYTES(_pr_v) * 1024)
#define GB_TO_BYTES(_pr_v) (MB_TO_BYTES(_pr_v) * 1024)
#define TB_TO_BYTES(_pr_v) (GB_TO_BYTES(_pr_v) * 1024)

#define TOOLS_MIN(_pr_a, _pr_b) ((_pr_a) < (_pr_b) ? (_pr_a) : (_pr_b))
#define TOOLS_MAX(_pr_a, _pr_b) ((_pr_a) > (_pr_b) ? (_pr_a) : (_pr_b))

/**
 * @brief Calculates the distance between two indexes in a ring buffer
 */
#define RING_DIFF(size, start, end) ((end) >= (start) ? (end) - (start) : (size) - (start) + (end))

/**
 * @brief Calculates the complement to the ring size for a value
 */
#define RING_COMPLEMENT(size, value) ((size) - (value))

/**
 * @brief Adds a positive offset to a index in a ring buffer
 */
#define RING_ADD(size, index, offset) (((index) + (offset)) % (size))

/**
 * @brief Subtracts a positive offset to a index in a ring buffer
 */
#define RING_SUB(size, index, offset) (((index) + RING_COMPLEMENT((size), (offset))) % (size))

/**
 * @brief Checks if an index is between start and end indexes in a ring buffer
 */
#define RING_BETWEEN(start, end, test)                               \
	((end) >= (start) ? ((test) >= (start) && (test) <= (end)) : \
	                    ((test) >= (start) || (test) <= (end)))

// Defines what is the minimum priority of a message to be logged.
// Anything with higher priority is going to be logged.
#ifndef TIX_LOG_LEVEL
#define TIX_LOG_LEVEL TIX_LOG_LEVEL_ALL
#endif // TIX_LOG_LEVEL

#define STRINGIFY(n) #n
#define STRGY(n) STRINGIFY(n)

#ifdef DEBUG
#define TIX_LOG_WRITE(fmt, ...)                                              \
	do {                                                                 \
		FILE *_pr_log_file = fopen("log.txt", "a+");                 \
		if (_pr_log_file == nullptr) {                               \
			break;                                               \
		}                                                            \
                                                                             \
		(void)fprintf(_pr_log_file, fmt __VA_OPT__(, ) __VA_ARGS__); \
		(void)fclose(_pr_log_file);                                  \
	} while (false)
#else
#define TIX_LOG_WRITE(fmt, ...) printf(fmt __VA_OPT__(, ) __VA_ARGS__)
#endif

#define TIX_LOG_MSG(log_level, fmt, file_name, func_name, line_number, ...)                     \
	do {                                                                                    \
		char _pr_tstamp_str[TIX_LOG_TSTAMP_BUF_SIZE];                                   \
		struct timespec _pr_ts;                                                         \
		struct tm _pr_tm;                                                               \
                                                                                                \
		if (!timespec_get(&_pr_ts, TIME_UTC)) {                                         \
			break;                                                                  \
		}                                                                               \
		if (gmtime_s(&_pr_tm, &_pr_ts.tv_sec)) {                                        \
			break;                                                                  \
		}                                                                               \
		if (strftime(_pr_tstamp_str, TIX_LOG_TSTAMP_BUF_SIZE, "%FT%T", &_pr_tm) == 0) { \
			break;                                                                  \
		}                                                                               \
                                                                                                \
		TIX_LOG_WRITE("%c[%s.%09ldZ] %s:%s:%s: " fmt "\n", log_level, _pr_tstamp_str,   \
		              _pr_ts.tv_nsec, file_name, func_name,                             \
		              STRGY(line_number) __VA_OPT__(, ) __VA_ARGS__);                   \
	} while (false)

#define TIX_LOG_MSG_NOOP(...) ((void)0)

// Logs a trace message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_TRACE
// Usage: TIX_LOGD("Log trace: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_TRACE
#define TIX_LOGT(fmt, ...) TIX_LOG_MSG('T', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGT(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // logt

// Logs a debug message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_DEBUG.
// Usage: TIX_LOGD("log debug: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_DEBUG
#define TIX_LOGD(fmt, ...) TIX_LOG_MSG('D', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGD(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // TIX_LOGD

// Logs an information message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_INFO
// Usage: TIX_LOGI("Log info: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_INFO
#define TIX_LOGI(fmt, ...) TIX_LOG_MSG('I', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGI(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // TIX_LOGI

// Logs a warning message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_WARN
// Usage: TIX_LOGW("Log warn: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_WARN
#define TIX_LOGW(fmt, ...) TIX_LOG_MSG('W', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGW(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // TIX_LOGW

// Logs an error message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_ERROR
// Usage: TIX_LOGE("Log error: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_ERROR
#define TIX_LOGE(fmt, ...) TIX_LOG_MSG('E', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGE(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // TIX_LOGE

// Logs a fatal message if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_FATAL
// Usage: TIX_LOGF("Log fatal: x=%d", x);
#if TIX_LOG_LEVEL <= TIX_LOG_LEVEL_FATAL
#define TIX_LOGF(fmt, ...) TIX_LOG_MSG('F', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define TIX_LOGF(fmt, ...) TIX_LOG_MSG_NOOP()
#endif // TIX_LOGF

/**
 * @brief truncates a int64_t into a uint32_t.
 *
 * @param value
 * @return uint32_t
 */
inline uint32_t tix_math_ll_to_ul(int64_t value)
{
	assert(value < INT32_MAX && value >= 0);

	return (uint32_t)value;
}

inline int tix_math_int_min(int a, int b)
{
	return a < b ? a : b;
}

inline int tix_math_int_max(int a, int b)
{
	return a > b ? a : b;
}

/**
 * @brief Rounds a float to the nearest biggest int: 0.5 -> 1
 *
 * @param value
 * @return int
 */
inline int tix_math_float_round_to_int(float value)
{
	int result = (int)roundf(value);

	return result;
}

inline float tix_math_float_floor(float value)
{
	return floorf(value);
}

inline unsigned tix_math_int_abs(int value)
{
	return (unsigned)(value < 0 ? -value : value);
}

#endif // TIX_LIB_H
