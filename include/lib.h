// clang-format Language: C

#ifndef LIB_H
#define LIB_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h> // IWYU pragma: keep
#include <time.h>

#ifndef LIB_COMPILER_MSVC
#define LIB_COMPILER_MSVC 0
#endif

#ifndef LIB_COMPILER_LLVM
#define LIB_COMPILER_LLVM 0
#endif

#if !LIB_COMPILER_MSVC && !LIB_COMPILER_LLVM
#ifdef _MSC_VER
#undef LIB_COMPILER_MSVC
#define LIB_COMPILER_MSVC 1
#else
#undef LIB_COMPILER_LLVM
#define LIB_COMPILER_LLVM 1
#endif
#endif

#if LIB_COMPILER_MSVC
#include <intrin.h>
#endif

// Constants
static constexpr float PIE = 3.14159265359F;
#define LIB_LOG_TSTAMP_BUF_SIZE 32
#define LIB_LOG_LEVEL_ALL 0UL
#define LIB_LOG_LEVEL_TRACE 1UL
#define LIB_LOG_LEVEL_DEBUG 2UL
#define LIB_LOG_LEVEL_INFO 3UL
#define LIB_LOG_LEVEL_WARN 4UL
#define LIB_LOG_LEVEL_ERROR 5UL
#define LIB_LOG_LEVEL_FATAL 6UL
#define LIB_LOG_LEVEL_OFF 7UL

#define LIB_KB_TO_BYTES(_pr_v) ((_pr_v) * 1024)
#define MB_TO_BYTES(_pr_v) (LIB_KB_TO_BYTES(_pr_v) * 1024)
#define GB_TO_BYTES(_pr_v) (MB_TO_BYTES(_pr_v) * 1024)
#define TB_TO_BYTES(_pr_v) (GB_TO_BYTES(_pr_v) * 1024)

#define LIB_MATH_MIN(_pr_a, _pr_b) ((_pr_a) < (_pr_b) ? (_pr_a) : (_pr_b))
#define LIB_MATH_MAX(_pr_a, _pr_b) ((_pr_a) > (_pr_b) ? (_pr_a) : (_pr_b))

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
#define RING_BETWEEN(start, end, test) \
	((end) >= (start) ? ((test) >= (start) && (test) <= (end)) : ((test) >= (start) || (test) <= (end)))

// Defines what is the minimum priority of a message to be logged.
// Anything with higher priority is going to be logged.
#ifndef LIB_LOG_LEVEL
#define LIB_LOG_LEVEL LIB_LOG_LEVEL_ALL
#endif // LIB_LOG_LEVEL

#define STRINGIFY(n) #n
#define STRGY(n) STRINGIFY(n)

#if DEBUG
#define LIB_LOG_WRITE(fmt, ...)                                              \
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
#define LIB_LOG_WRITE(fmt, ...) printf(fmt __VA_OPT__(, ) __VA_ARGS__)
#endif

#define LIB_LOG_MSG(log_level, fmt, file_name, func_name, line_number, ...)                                   \
	do {                                                                                                  \
		char _pr_tstamp_str[LIB_LOG_TSTAMP_BUF_SIZE];                                                 \
		struct timespec _pr_ts;                                                                       \
		struct tm _pr_tm;                                                                             \
                                                                                                              \
		if (!timespec_get(&_pr_ts, TIME_UTC)) {                                                       \
			break;                                                                                \
		}                                                                                             \
		if (gmtime_s(&_pr_tm, &_pr_ts.tv_sec)) {                                                      \
			break;                                                                                \
		}                                                                                             \
		if (strftime(_pr_tstamp_str, LIB_LOG_TSTAMP_BUF_SIZE, "%FT%T", &_pr_tm) == 0) {               \
			break;                                                                                \
		}                                                                                             \
                                                                                                              \
		LIB_LOG_WRITE("%c[%s.%09ldZ] %s:%s:%s: " fmt "\n", log_level, _pr_tstamp_str, _pr_ts.tv_nsec, \
		              file_name, func_name, STRGY(line_number) __VA_OPT__(, ) __VA_ARGS__);           \
	} while (false)

#define LIB_LOG_MSG_NOOP(...) ((void)0)

// Logs a trace message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_TRACE
// Usage: LIB_LOGD("Log trace: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_TRACE
#define LIB_LOGT(fmt, ...) LIB_LOG_MSG('T', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGT(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // logt

// Logs a debug message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_DEBUG.
// Usage: LIB_LOGD("log debug: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_DEBUG
#define LIB_LOGD(fmt, ...) LIB_LOG_MSG('D', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGD(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // LIB_LOGD

// Logs an information message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_INFO
// Usage: LIB_LOGI("Log info: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_INFO
#define LIB_LOGI(fmt, ...) LIB_LOG_MSG('I', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGI(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // LIB_LOGI

// Logs a warning message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_WARN
// Usage: LIB_LOGW("Log warn: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_WARN
#define LIB_LOGW(fmt, ...) LIB_LOG_MSG('W', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGW(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // LIB_LOGW

// Logs an error message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_ERROR
// Usage: LIB_LOGE("Log error: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_ERROR
#define LIB_LOGE(fmt, ...) LIB_LOG_MSG('E', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGE(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // LIB_LOGE

// Logs a fatal message if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_FATAL
// Usage: LIB_LOGF("Log fatal: x=%d", x);
#if LIB_LOG_LEVEL <= LIB_LOG_LEVEL_FATAL
#define LIB_LOGF(fmt, ...) LIB_LOG_MSG('F', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LIB_LOGF(fmt, ...) LIB_LOG_MSG_NOOP()
#endif // LIB_LOGF

static void string_concat(const size_t one_count, const char *const restrict one, const size_t other_count,
                          const char *const restrict other, const size_t destsize, char *const restrict dest)
{
	for (unsigned i = 0; i < one_count; ++i) {
		dest[i] = one[i];
	}

	for (unsigned i = 0; i < other_count; ++i) {
		dest[one_count + i] = other[i];
	}

	dest[one_count + other_count] = '\0';
}

/**
 * @brief truncates a int64_t into a uint32_t.
 *
 * @param value
 * @return uint32_t
 */
inline uint32_t i64_to_u32(int64_t value)
{
	assert(value < INT32_MAX && value >= 0);

	return (uint32_t)value;
}

inline int int_min(int a, int b)
{
	return a < b ? a : b;
}

inline int int_max(int a, int b)
{
	return a > b ? a : b;
}

/**
 * @brief Rounds a float to the nearest biggest int: 0.5 -> 1
 *
 * @param value
 * @return int
 */
inline int float_round_to_int(float value)
{
	int result = (int)roundf(value);

	return result;
}

/**
 * @brief Rounds a float to the nearest biggest unsigned int: 0.5 -> 1
 *
 * @param value
 * @return int
 */
inline uint32_t float_round_to_uint(float value)
{
	assert(value >= 0.0F);

	uint32_t result = (uint32_t)(value + 0.5F);

	return result;
}

inline float float_floor(float value)
{
	return floorf(value);
}

inline float float_square(float value)
{
	return value * value;
}

inline unsigned int_abs(int value)
{
	return (unsigned)(value < 0 ? -value : value);
}

typedef struct BitScanResult {
	unsigned long index;
	uint8_t was_found;
} BitScanResult;

/**
 * @brief Finds the index of the first non-zero bit if there is one.
 *
 * @param value
 * @param index
 * @return uint8_t Non zero value if a non-zero value was found, 0 otherwise
 */
BitScanResult uint_find_least_significant_set_bit(uint32_t value)
{
	BitScanResult result = {};

#if LIB_COMPILER_MSVC
	result.was_found = _BitScanForward(&result.index, value);
#else
	for (uint8_t test = 0; test < 32; ++test) {
		if (value & (1U << test)) {
			result.was_found = 1U;
			result.index = test;

			break;
		}
	}
#endif

	return result;
}

/**
 * @brief Vector in plane
 */
typedef union Vtwo {
	struct {
		float x;
		float y;
	};
	float e[2];
} Vtwo;

/**
 * @brief Flips a vector in the x axis (inverts its x coordinate)
 *
 * @param a The vector to flip
 * @return Vtwo The flipped vector (-a.x, a.y)
 *
 * @example
 * Vtwo v = {3.0f, 4.0f};
 * Vtwo flipped = vtwo_flip_x(v);  // {-3.0f, 4.0f}
 */
inline Vtwo vtwo_flip_x(Vtwo a)
{
	Vtwo result = {
		.x = a.x,
		.y = -a.y,
	};

	return result;
}

/**
 * @brief Flips a vector in the y axis (inverts its y coordinate)
 *
 * @param a The vector to flip
 * @return Vtwo The flipped vector (a.x, -a.y)
 *
 * @example
 * Vtwo v = {3.0f, 4.0f};
 * Vtwo flipped = vtwo_flip_y(v);  // {3.0f, -4.0f}
 */
inline Vtwo vtwo_flip_y(Vtwo a)
{
	Vtwo result = {
		.x = a.x,
		.y = -a.y,
	};

	return result;
}

/**
 * @brief Negates a vector (inverts its direction)
 *
 * @param a The vector to negate
 * @return Vtwo The negated vector (-a.x, -a.y)
 *
 * @example
 * Vtwo v = {3.0f, 4.0f};
 * Vtwo neg = vtwo_inv(v);  // {-3.0f, -4.0f}
 */
inline Vtwo vtwo_neg(Vtwo a)
{
	Vtwo result = {
		.x = -a.x,
		.y = -a.y,
	};

	return result;
}

/**
 * @brief Adds two vectors
 *
 * @param a The first vector
 * @param b The second vector
 * @return Vtwo The sum of a and b (a.x + b.x, a.y + b.y)
 *
 * @example
 * Vtwo a = {1.0f, 2.0f};
 * Vtwo b = {3.0f, 4.0f};
 * Vtwo sum = vtwo_add(a, b);  // {4.0f, 6.0f}
 */
inline Vtwo vtwo_add(Vtwo a, Vtwo b)
{
	Vtwo result = {
		.x = a.x + b.x,
		.y = a.y + b.y,
	};

	return result;
}

/**
 * @brief Subtracts vector b from vector b (calculates a - b)
 *
 * @param a The vector to subtract from
 * @param b The vector to subtract from a
 * @return Vtwo The difference (a.x - b.x, a.y - b.y)
 *
 * @example
 * Vtwo a = {1.0f, 2.0f};
 * Vtwo b = {5.0f, 7.0f};
 * Vtwo diff = vtwo_sub(a, b);  // {-4.0f, -5.0f}
 */
inline Vtwo vtwo_sub(Vtwo a, Vtwo b)
{
	Vtwo result = {
		.x = a.x - b.x,
		.y = a.y - b.y,
	};

	return result;
}

/**
 * @brief Scales x axis
 *
 * @param a The vector
 * @param s The scalar
 * @return Vtwo The scaled vector (a.x * s, a.y)
 */
inline Vtwo vtwo_scale_x(Vtwo a, float s)
{
	Vtwo result = {
		.x = a.x * s,
		.y = a.y,
	};

	return result;
}

/**
 * @brief Scalar multiplication
 *
 * @param a The vector
 * @param s The scalar
 * @return Vtwo The scaled vector (a.x * s, a.y * s)
 */
inline Vtwo vtwo_scale(Vtwo a, float s)
{
	Vtwo result = {
		.x = a.x * s,
		.y = a.y * s,
	};

	return result;
}

/**
 * @brief Scalar addition
 *
 * @param a The vector
 * @param s The scalar
 * @return Vtwo The translated vector (a.x * s, a.y * s)
 */
inline Vtwo vtwo_add_scalar(Vtwo a, float s)
{
	Vtwo result = {
		.x = a.x + s,
		.y = a.y + s,
	};

	return result;
}

/**
 * @brief Calculates the dot product between two bidimensional vectors
 *
 * @param a One vector
 * @param b Other vector
 * @return float The dot product a . b
 */
inline float vtwo_dot(Vtwo a, Vtwo b)
{
	float result = a.x * b.x + a.y * b.y;

	return result;
}

#endif // LIB_H
