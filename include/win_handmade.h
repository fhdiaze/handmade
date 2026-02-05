// clang-format Language: C

#ifndef WIN_HANDMADE_H
#define WIN_HANDMADE_H

#include "game.h"
#include <windows.h>

#define WIN_STATE_MAX_FILE_PATH MAX_PATH
#define WIN_REPLAY_MAX_SLOTS 4
#define WIN_REPLAY_NO_SLOT UINT8_MAX

#define LODWORD(l) ((unsigned long)(((size_t)(l)) & 0xFFFFFFFF))
#define HIDWORD(l) ((unsigned long)((((size_t)(l)) >> (sizeof(unsigned) * CHAR_BIT)) & 0xFFFFFFFF))

typedef struct Win_WindowDimensions {
	long width;
	long height;
} Win_WindowDimensions;

typedef struct Win_Bitmap {
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes
	unsigned bytes_per_pixel;
	void *memory;
	BITMAPINFO info;
} Win_Bitmap;

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

	/**
	 * @brief could be null, check before call it
	 */
	game_bitmap_update_and_render_func *update_and_render;

	/**
	 * @brief could be null, check before call it
	 */
	game_sound_create_samples_func *sound_create_samples;

	bool is_valid;
} Win_GameCode;

typedef struct Win_ReplaySlot {
	HANDLE file_handle;
	HANDLE file_map;
	void *memory;
	char filepath[WIN_STATE_MAX_FILE_PATH];
} Win_ReplaySlot;

typedef enum Win_ReplayStatus : uint8_t {
	WIN_REPLAY_NORMAL,
	WIN_REPLAY_RECORD,
	WIN_REPLAY_RECORDED,
	WIN_REPLAY_PLAYBACK,
} Win_ReplayStatus;

typedef struct Win_State {
	size_t gamemem_size;
	void *gamemem;

	Win_ReplaySlot replay_slots[WIN_REPLAY_MAX_SLOTS];
	uint8_t replay_slot_index;
	Win_ReplayStatus replay_status;

	char *exe_path_last_slash;
	char exe_path[WIN_STATE_MAX_FILE_PATH];
} Win_State;

#endif // WIN_HANDMADE_H
