// clang-format Language: C

#ifndef LIB_H
#define LIB_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h> // IWYU pragma: keep
#include <string.h>
#include <time.h>

// =============================================================================
// Compiler detection
// =============================================================================

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

// =============================================================================
// Memory
// =============================================================================

#define KB_TO_BYTES(_pr_v) ((_pr_v) * 1024)
#define MB_TO_BYTES(_pr_v) (KB_TO_BYTES(_pr_v) * 1024)
#define GB_TO_BYTES(_pr_v) (MB_TO_BYTES(_pr_v) * 1024)
#define TB_TO_BYTES(_pr_v) (GB_TO_BYTES(_pr_v) * 1024)

#define ARENA_PUSH_ARRAY(arena, type, count) (type *)arena_push((arena), sizeof(type) * (count))
#define ARENA_PUSH_STRUCT(arena, type) (type *)arena_push((arena), sizeof(type))

#define ARENA_PUSH_ARRAY_ZERO(arena, type, count) (type *)arena_push_zero((arena), sizeof(type) * (count))
#define ARENA_PUSH_STRUCT_ZERO(arena, type) (type *)arena_push_zero((arena), sizeof(type))

typedef struct Arena {
	size_t capacity_bytes;
	unsigned char *base_address;
	size_t used_bytes;
} Arena;

void arena_init(Arena *restrict arena, const size_t size, unsigned char *const restrict base)
{
	arena->capacity_bytes = size;
	arena->base_address = base;
	arena->used_bytes = 0;
}

void *arena_push(Arena *arena, size_t size)
{
	assert(arena->used_bytes + size <= arena->capacity_bytes);

	void *result = arena->base_address + arena->used_bytes;
	arena->used_bytes += size;

	return result;
}

void *arena_push_zero(Arena *arena, size_t size)
{
	void *result = arena_push(arena, size);

	memset(result, 0, size);

	return nullptr;
}

// =============================================================================
// Math
// =============================================================================

#define PIE 3.14159265359F
#define NUMBER_MIN(_pr_a, _pr_b) ((_pr_a) < (_pr_b) ? (_pr_a) : (_pr_b))
#define NUMBER_MAX(_pr_a, _pr_b) ((_pr_a) > (_pr_b) ? (_pr_a) : (_pr_b))

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

inline int32_t float_floor_to_int(float value)
{
	int32_t result = (int32_t)floorf(value);

	return result;
}

inline uint32_t float_ceil_to_uint(float value)
{
	assert(value >= 0.0F);

	uint32_t result = (uint32_t)ceilf(value);

	return result;
}

inline int32_t float_ceil_to_int(float value)
{
	int32_t result = (int32_t)ceilf(value);

	return result;
}

inline float float_square(float value)
{
	return value * value;
}

inline unsigned int_abs(int value)
{
	// trick to avoid branches: (x ^ (x >> 31)) - (x >> 31)
	unsigned result = value < 0 ? -(unsigned)value : (unsigned)value;

	return result;
}

// =============================================================================
// Ring buffer
// =============================================================================

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
#define RING_IS_BETWEEN(start, end, test) \
	((end) >= (start) ? ((test) >= (start) && (test) <= (end)) : ((test) >= (start) || (test) <= (end)))

// =============================================================================
// String
// =============================================================================

#define STRINGIFY(n) #n
#define XSTRINGIFY(n) STRINGIFY(n)

void string_concat(const size_t one_count, const char *const restrict one, const size_t other_count,
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

// =============================================================================
// Logging
// =============================================================================

#define LOG_TSTAMP_BUF_SIZE 32
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
#endif // LOG_LEVEL

#if DEBUG
#define IMPL_LOG_WRITE(fmt, ...)                                                     \
	do {                                                                         \
		FILE *_pr_log_file = fopen("log.txt", "a+");                         \
		if (_pr_log_file != nullptr) {                                       \
			(void)fprintf(_pr_log_file, fmt __VA_OPT__(, ) __VA_ARGS__); \
			(void)fclose(_pr_log_file);                                  \
		}                                                                    \
	} while (false)
#else
#define IMPL_LOG_WRITE(fmt, ...) printf(fmt __VA_OPT__(, ) __VA_ARGS__)
#endif

#define IMPL_LOG_MSG(log_level, fmt, file_name, func_name, line_number, ...)                                   \
	do {                                                                                                   \
		char _pr_tstamp_str[LOG_TSTAMP_BUF_SIZE];                                                      \
		struct timespec _pr_ts;                                                                        \
		struct tm _pr_tm;                                                                              \
                                                                                                               \
		if (!timespec_get(&_pr_ts, TIME_UTC)) {                                                        \
			break;                                                                                 \
		}                                                                                              \
		if (gmtime_s(&_pr_tm, &_pr_ts.tv_sec)) {                                                       \
			break;                                                                                 \
		}                                                                                              \
		if (strftime(_pr_tstamp_str, LOG_TSTAMP_BUF_SIZE, "%FT%T", &_pr_tm) == 0) {                    \
			break;                                                                                 \
		}                                                                                              \
                                                                                                               \
		IMPL_LOG_WRITE("%c[%s.%09ldZ] %s:%s:%s: " fmt "\n", log_level, _pr_tstamp_str, _pr_ts.tv_nsec, \
		               file_name, func_name, XSTRINGIFY(line_number) __VA_OPT__(, ) __VA_ARGS__);      \
	} while (false)

#define IMPL_LOG_MSG_NOOP(...) ((void)0)

// Logs a trace message if LOG_LEVEL <= LOG_LEVEL_TRACE
// Usage: LOG_TRACE("Log trace: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE(fmt, ...) IMPL_LOG_MSG('T', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_TRACE

// Logs a debug message if LOG_LEVEL <= LOG_LEVEL_DEBUG.
// Usage: LOG_DEBUG("log debug: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) IMPL_LOG_MSG('D', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_DEBUG

// Logs an information message if LOG_LEVEL <= LOG_LEVEL_INFO
// Usage: LOG_INFO("Log info: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) IMPL_LOG_MSG('I', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_INFO

// Logs a warning message if LOG_LEVEL <= LOG_LEVEL_WARN
// Usage: LOG_WARN("Log warn: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...) IMPL_LOG_MSG('W', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_WARN(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_WARN

// Logs an error message if LOG_LEVEL <= LOG_LEVEL_ERROR
// Usage: LOG_ERROR("Log error: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) IMPL_LOG_MSG('E', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_ERROR

// Logs a fatal message if LOG_LEVEL <= LOG_LEVEL_FATAL
// Usage: LOG_FATAL("Log fatal: x=%d", x);
#if LOG_LEVEL <= LOG_LEVEL_FATAL
#define LOG_FATAL(fmt, ...) IMPL_LOG_MSG('F', fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG_FATAL(fmt, ...) IMPL_LOG_MSG_NOOP()
#endif // LOG_FATAL

// =============================================================================
// Bit operations
// =============================================================================

typedef struct CtzResult {
	uint32_t count;
	uint8_t was_found;
} CtzResult;

/**
 * @brief Count trailing zeroes - returns the index of the first least-significant non-zero bit if there is one.
 *
 * @param value
 * @param index
 * @return BitScanResult .index = number of trailing zeroes, .was_found = 1 if a non-zero bit was found,
 * otherwise .was_found = 0
 */
CtzResult uint_ctz(uint32_t value)
{
	CtzResult result = {};

#if LIB_COMPILER_MSVC
	unsigned long ctz_tmp = 0;
	result.was_found = _BitScanForward(&ctz_tmp, value);
	result.count = ctz_tmp;
#elif LIB_COMPILER_LLVM
	if (value != 0U) {
		result.was_found = 1U;
		result.index = (unsigned long)__builtin_ctz(value);
	}
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
 * @brief Rotates the bits of @p value to the left by @p shift positions
 *
 * @param value The 32-bit value to rotate
 * @param shift The number of bit positions to rotate left (0-31)
 * @return uint32_t @p value with its bits rotated left by @p shift
 *
 * @example
 * uint32_t r = uint_rotl(0x00000001U, 1);  // 0x00000002
 * uint32_t r = uint_rotl(0x80000000U, 1);  // 0x00000001
 */
uint32_t uint_rotl(uint32_t value, int32_t shift)
{
	uint32_t result = _rotl(value, shift);

	return result;
}

// =============================================================================
// Vector math
// =============================================================================

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
 * @brief Subtracts vector b from vector a (calculates a - b)
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

/**
 * @brief Calculates the squared norm (squared length) of a vector
 *
 * Avoids a square root; use when only comparing magnitudes.
 *
 * @param a A vector
 * @return float ||a||^2 = a.x^2 + a.y^2
 */
inline float vtwo_norm_sq(Vtwo a)
{
	float norm_sq = vtwo_dot(a, a);

	return norm_sq;
}

inline float vtwo_norm(Vtwo a)
{
	float norm_sq = vtwo_norm_sq(a);
	float result = sqrtf(norm_sq);

	return result;
}

inline Vtwo vtwo_normalize(Vtwo a)
{
	float norm = vtwo_norm(a);
	Vtwo result = vtwo_scale(a, 1.0F / norm);

	return result;
}

#endif // LIB_H
