// clang-format Language: C

#ifndef WIN_HANDMADE_H
#define WIN_HANDMADE_H

#include "game.h"
#include <windows.h>

typedef struct Win_WindowDimensions {
	long width;
	long height;
} Win_WindowDimensions;

typedef struct Win_OffScreenBuffer {
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes
	unsigned bytes_per_pixel;
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

typedef struct Win_GameCode {
	HMODULE game_dll;
	FILETIME dll_write_time;
	game_update_and_render_func *update_and_render;
	game_sound_create_samples_func *sound_create_samples;

	bool is_valid;
} Win_GameCode;

typedef struct Win_State {
	HANDLE recording_handle;
	HANDLE playback_handle;
	unsigned input_recording_index;
	unsigned input_playing_index;
} Win_State;

#endif // WIN_HANDMADE_H
