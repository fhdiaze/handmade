// clang-format Language: C

#ifndef WIN_HANDMADE_H
#define WIN_HANDMADE_H

#include <windows.h>

struct Win_WindowDimensions {
	long width;
	long height;
};

struct Win_OffScreenBuffer {
	BITMAPINFO bitmap_info;
	void *memory;
	long width;
	long height;
	long pitch; // size of a row in bytes
};

struct Win_SoundOutput {
	size_t samples_per_sec;
	size_t running_sample_index;
	/** Size of the sample in bytes */
	size_t sample_size;
	size_t buffsize;
	float tsine;
	size_t latency_sample_count;
};

#endif // WIN_HANDMADE_H
