// clang-format Language: C

#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

// The idea is to have just a couple of files where you have logic that depends on the compiler/platform

#include <stdint.h>

#include "handmade_platform.h"

typedef struct Intrs_BitScanResult {
	unsigned long index;
	uint8_t was_found;
} Intrs_BitScanResult;

/**
 * @brief Finds the index of the first non-zero bit if there is one.
 *
 * @param value
 * @param index
 * @return uint8_t Non zero value if a non-zero value was found, 0 otherwise
 */
Intrs_BitScanResult intrs_bit_find_least_significant_set_bit(uint32_t value)
{
	Intrs_BitScanResult result = {};

#if COMPILER_MSVC
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

#endif // HANDMADE_INTRINSICS_H
