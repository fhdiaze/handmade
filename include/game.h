// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <assert.h>
#include <stdint.h>

#define KB_TO_BYTES(_pr_v) (_pr_v * 1024)
#define MB_TO_BYTES(_pr_v) (KB_TO_BYTES(_pr_v) * 1024)
#define GB_TO_BYTES(_pr_v) (MB_TO_BYTES(_pr_v) * 1024)
#define TB_TO_BYTES(_pr_v) (GB_TO_BYTES(_pr_v) * 1024)

#define TOOLS_MIN(_pr_a, _pr_b) ((_pr_a) < (_pr_b) ? (_pr_a) : (_pr_b))
#define TOOLS_MAX(_pr_a, _pr_b) ((_pr_a) > (_pr_b) ? (_pr_a) : (_pr_b))

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
#define RING_BETWEEN(start, end, test)                               \
	((end) >= (start) ? ((test) >= (start) && (test) <= (end)) : \
	                    ((test) >= (start) || (test) <= (end)))

#ifdef DEBUG
#define BASE_ADDRESS ((void *)TB_TO_BYTES(2))
#else
#define BASE_ADDRESS (nullptr)
#endif // DEBUG

static constexpr float PIE = 3.14159265359F;
static constexpr unsigned GAME_MAX_MOUSE_BUTTONS = 5;
static constexpr unsigned GAME_MAX_CONTROLLERS = 5;
static constexpr unsigned GAME_MAX_CONTROLLER_BUTTONS = 12;

/**
 * @brief (0,0) is on the top left corner
 *
 */
typedef struct Game_Bitmap {
	void *memory;
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes

	unsigned bytes_per_pixel;
} Game_Bitmap;

typedef struct Game_SoundBuffer {
	unsigned samples_per_sec;
	unsigned sample_count;
	int16_t *samples;
} Game_SoundBuffer;

typedef struct Game_ButtonState {
	// half transistion count per frame
	unsigned half_transition_count;
	bool ended_down;
} Game_ButtonState;

typedef struct Game_ControllerInput {
	float stick_avg_x;
	float stick_avg_y;

	union {
		Game_ButtonState buttons[GAME_MAX_CONTROLLER_BUTTONS];
		struct {
			Game_ButtonState moveup;
			Game_ButtonState movedown;
			Game_ButtonState moveleft;
			Game_ButtonState moveright;

			Game_ButtonState actionup;
			Game_ButtonState actiondown;
			Game_ButtonState actionleft;
			Game_ButtonState actionright;

			Game_ButtonState left_shoulder;
			Game_ButtonState right_shoulder;

			Game_ButtonState start;
			Game_ButtonState back;
		};
	};

	// TODO(fredy): bools in structs are suspicious
	bool is_analog;
	bool is_connected;
} Game_ControllerInput;

typedef struct Game_Input {
	unsigned mouse_x;
	unsigned mouse_y;
	unsigned mouse_z; // mouse wheel

	float secs_time_delta;

	union {
		Game_ButtonState mouse_buttons[GAME_MAX_MOUSE_BUTTONS];

		struct {
			Game_ButtonState mouse_main;
			Game_ButtonState mouse_middle;
			Game_ButtonState mouse_secondary;
			Game_ButtonState mouse_back;
			Game_ButtonState mouse_forward;
		};
	};

	Game_ControllerInput controllers[GAME_MAX_CONTROLLERS];
} Game_Input;

typedef struct Game_Tilemap {
	uint32_t *tiles;
} Game_Tilemap;

typedef struct Game_World {
	// meters
	float tile_side_mts;
	// pixels
	unsigned tile_side_pxs;

	unsigned tilemaps_count_x;
	unsigned tilemaps_count_y;

	unsigned tiles_count_x;
	unsigned tiles_count_y;

	// Offset of the tile map relative to the screen
	int screen_offset_x;
	int screen_offset_y;

	Game_Tilemap *tilemaps;
} Game_World;

// Philly chicken avocado
// rainbow roll
// Prawn katsu and yuzu mayonnaise, no spicy please

typedef struct Game_RawPosition {
	unsigned tilemap_x;
	unsigned tilemap_y;

	// X coordinate on the window
	float win_rel_x;
	// Y coordinate on the window
	float win_rel_y;
} Game_RawPosition;

typedef struct Game_CanonicalPosition {
#if 1
	unsigned tilemap_x;
	unsigned tilemap_y;
#endif

	unsigned tile_x;
	unsigned tile_y;

	// X relative to the upper left corner of the tile
	float tile_rel_x;
	// Y relative to the upper left corner of the tile
	float tile_rel_y;
} Game_CanonicalPosition;

typedef struct Game_State {
	Game_CanonicalPosition playerpos;
} Game_State;

typedef struct Game_Thread {
	unsigned placeholder;
} Game_Thread;

// Utilities

static inline Game_ControllerInput *game_input_get_controller(Game_Input *input,
                                                              size_t controller_index)
{
	assert(controller_index < GAME_MAX_CONTROLLERS);
	return &input->controllers[controller_index];
}

// Platform services

#ifdef DEBUG

typedef struct Plat_ReadFileResult {
	size_t size;
	void *memory;
} Plat_ReadFileResult;

#define PLAT_DEBUG_READFILE(name) \
	Plat_ReadFileResult name(Game_Thread *thread, const char *const filename)
typedef PLAT_DEBUG_READFILE(plat_debug_readfile_func);

#define PLAT_DEBUG_FREEFILE(name) void name(Game_Thread *thread, void *memory)
typedef PLAT_DEBUG_FREEFILE(plat_debug_freefile_func);

#define PLAT_DEBUG_WRITEFILE(name) \
	bool name(Game_Thread *thread, const char *const filename, size_t memorysize, void *memory)
typedef PLAT_DEBUG_WRITEFILE(plat_debug_writefile_func);

#endif // DEBUG

// Game services

typedef struct Game_Memory {
	size_t permamem_size; // permanent storage in bytes
	void *permamem; // This should be zero initialized

	size_t transmem_size; // transient storage in bytes
	void *transmem; // This should be zero initialized

	plat_debug_freefile_func *plat_debug_free_file;
	plat_debug_readfile_func *plat_debug_read_file;
	plat_debug_writefile_func *plat_debug_write_file;

	bool is_initialized;
} Game_Memory;

/**
 * @brief Updates the game status and renders it
 */
#define GAME_BITMAP_UPDATE_AND_RENDER(name)                                         \
	void name(Game_Thread *thread, Game_Memory *game_memory, Game_Input *input, \
	          Game_Bitmap *bitmap)
typedef GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render_func);

#define GAME_SOUND_CREATE_SAMPLES(name) \
	void name(Game_Thread *thread, Game_Memory *memory, Game_SoundBuffer *soundbuff)
typedef GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples_func);

#endif // GAME_H
