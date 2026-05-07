// clang-format Language: C

#ifndef HM_WIN_H
#define HM_WIN_H

#include <windows.h>

#include "hm_game.h"

#define HM_WIN__MAX_FILE_PATH MAX_PATH
#define HM_WIN__REPLAY_MAX_SLOTS 4
#define HM_WIN__REPLAY_NO_SLOT UINT8_MAX

#define LODWORD(l) ((unsigned long)(((size_t)(l)) & 0xFFFFFFFF))
#define HIDWORD(l) ((unsigned long)((((size_t)(l)) >> (sizeof(unsigned) * CHAR_BIT)) & 0xFFFFFFFF))

typedef struct HmWin_WindowDimensions {
	long width;
	long height;
} HmWin_WindowDimensions;

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct HmWin_Bitmap {
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes
	unsigned bytes_per_pixel;
	void *top_left_px;
	BITMAPINFO info;
} Win_Bitmap;

typedef struct HmWin_SoundOutput {
	size_t running_sample_index;
	unsigned samples_per_sec;
	unsigned bytes_per_sample; // Size of the sample in bytes
	unsigned buffsize;
	unsigned safety_bytes;
} Win_SoundOutput;

typedef struct HmWin_DebugTimeMark {
	unsigned long output_play_cursor;
	unsigned long output_write_cursor;

	unsigned long flip_play_cursor;
	unsigned long flip_write_cursor;

	unsigned output_location;
	unsigned output_byte_count;

	unsigned frame_flip_byte;
} Win_DebugTimeMark;

typedef struct HmWin_GameCode {
	HMODULE game_dll;

	/**
	 * @brief could be null, check before call it
	 */
	game_bitmap_update_and_render_func *update_and_render;

	/**
	 * @brief could be null, check before call it
	 */
	game_sound_create_samples_func *sound_create_samples;

	FILETIME dll_write_time;

	uint8_t is_valid;
} Win_GameCode;

typedef struct HmWin_ReplaySlot {
	HANDLE file_handle;
	HANDLE file_map;
	void *memory;
	char filepath[HM_WIN__MAX_FILE_PATH];
} Win_ReplaySlot;

typedef enum HmWin_ReplayStatus : uint8_t {
	WIN_REPLAY_NORMAL,
	WIN_REPLAY_RECORD,
	WIN_REPLAY_RECORDED,
	WIN_REPLAY_PLAYBACK,
} Win_ReplayStatus;

typedef struct HmWin_State {
	size_t gamemem_size;
	void *gamemem;

	Win_ReplaySlot replay_slots[HM_WIN__REPLAY_MAX_SLOTS];
	HANDLE replay_file_handle;

	char *exe_path_last_slash;
	char exe_path[HM_WIN__MAX_FILE_PATH];

	uint8_t replay_slot_index;
	Win_ReplayStatus replay_status;
} Win_State;

#endif // HM_WIN_H
