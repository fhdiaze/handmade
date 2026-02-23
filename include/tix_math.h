// clang-format Language: C
#ifndef TIX_MATH_H
#define TIX_MATH_H

#include <assert.h>
#include <math.h>
#include <stdint.h>

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
	int result = (int)(value + 0.5F);

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

#endif // TIX_MATH_H
