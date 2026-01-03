// clang-format Language: C

#ifndef WIN_HANDMADE_H
#define WIN_HANDMADE_H

#include <windows.h>

typedef struct Win_WindowDimensions {
	long width;
	long height;
} Win_WindowDimensions;

typedef struct Win_OffScreenBuffer {
	long width;
	long height;
	long pitch; // size of a row in bytes
	long bytes_per_pixel;
	void *memory;
	BITMAPINFO bitmap_info;
} Win_OffScreenBuffer;

typedef struct Win_SoundOutput {
	size_t samples_per_sec;
	size_t running_sample_index;
	size_t bytes_per_sample; // Size of the sample in bytes
	size_t buffsize;
	size_t latency_sample_count;
	float tsine;
} Win_SoundOutput;

typedef struct Win_DebugTimeMark {
	unsigned long play_cursor;
	unsigned long write_cursor;
} Win_DebugTimeMark;

#endif // WIN_HANDMADE_H
