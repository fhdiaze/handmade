// clang-format Language: C

#ifndef WIN_HANDMADE_H
#define WIN_HANDMADE_H

#include <windows.h>

typedef struct Win_WindowDimensions {
	long width;
	long height;
} Win_WindowDimensions;

typedef struct Win_OffScreenBuffer {
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes
	unsigned pixel_size_bytes;
	void *memory;
	BITMAPINFO bitmap_info;
} Win_OffScreenBuffer;

typedef struct Win_SoundOutput {
	size_t running_sample_index;
	unsigned samples_per_sec;
	unsigned bytes_per_sample; // Size of the sample in bytes
	unsigned buffsize;
	unsigned safety_bytes;
} Win_SoundOutput;

typedef struct Win_DebugTimeMark {
	unsigned long output_play_cursor;
	unsigned long output_write_cursor;

	unsigned long flip_play_cursor;
	unsigned long flip_write_cursor;

	unsigned output_location;
	unsigned output_byte_count;

	unsigned frame_flip_byte;
} Win_DebugTimeMark;

#endif // WIN_HANDMADE_H
