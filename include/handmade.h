// clang-format Language: C

#ifndef HANDMADE_H
#define HANDMADE_H

#include <assert.h>
#include <stdint.h>

#include "lib.h"

// =============================================================================
// Common
// =============================================================================

typedef struct ThreadContext {
	unsigned placeholder;
} ThreadContext;

// =============================================================================
// Platform API
// =============================================================================

#if DEBUG

#define MEMORY_BASE_ADDRESS ((void *)TB_TO_BYTES(2))

typedef struct ReadFileResult {
	size_t size_bytes;
	void *base_address;
} ReadFileResult;

#define FILE_READ_DEBUG(name) ReadFileResult name(const char *const filename, ThreadContext *thread)
#define FILE_FREE_DEBUG(name) void name(void *memory, ThreadContext *thread)
#define FILE_WRITE_DEBUG(name) \
	uint8_t name(const char *const filename, size_t memorysize, void *memory, ThreadContext *thread)

typedef FILE_READ_DEBUG(file_read_debug_func);

typedef FILE_FREE_DEBUG(file_free_debug_func);

typedef FILE_WRITE_DEBUG(file_write_debug_func);

#else // DEBUG

#define MEMORY_BASE_ADDRESS (nullptr)

#endif // DEBUG

// =============================================================================
// Game API
// =============================================================================

#define HANDMADE_DLL_NAME "handmade.dll"

#define MAX_MOUSE_BUTTONS 5
#define MAX_CONTROLLERS 5
#define MAX_CONTROLLER_BUTTONS 12

typedef struct ButtonState {
	// Half transition count per frame
	unsigned half_transition_count;
	uint8_t ended_down;
} ButtonState;

typedef struct ControllerState {
	float stick_avg_x;
	float stick_avg_y;

	union {
		ButtonState buttons[MAX_CONTROLLER_BUTTONS];
		struct {
			ButtonState moveup;
			ButtonState movedown;
			ButtonState moveleft;
			ButtonState moveright;

			ButtonState actionup;
			ButtonState actiondown;
			ButtonState actionleft;
			ButtonState actionright;

			ButtonState left_shoulder;
			ButtonState right_shoulder;

			ButtonState start;
			ButtonState back;
		};
	};

	// TODO(fredy): bools in structs are suspicious
	uint8_t is_analog;
	uint8_t is_connected;
} ControllerState;

typedef struct GameInput {
	unsigned mouse_x;
	unsigned mouse_y;
	unsigned mouse_z; // mouse wheel

	float time_delta_sec;

	union {
		ButtonState mouse_buttons[MAX_MOUSE_BUTTONS];

		struct {
			ButtonState mouse_main;
			ButtonState mouse_middle;
			ButtonState mouse_secondary;
			ButtonState mouse_back;
			ButtonState mouse_forward;
		};
	};

	ControllerState controllers[MAX_CONTROLLERS];
} GameInput;

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct GameOffscreenBuffer {
	void *top_left_px;

	// width in pixels
	unsigned width_px;

	// Height in pixels
	unsigned height_px;

	// Size of a row in bytes
	unsigned pitch_bytes;
	unsigned bytes_per_pixel;
} GameOffscreenBuffer;

typedef struct GameSoundBuffer {
	unsigned samples_per_sec;
	unsigned sample_count;
	int16_t *samples;
} GameSoundBuffer;

typedef struct Storage {
	size_t permanent_storage_size_bytes; // permanent storage in bytes
	void *permanent_storage;            // This should be zero initialized

	size_t transient_storage_size_bytes; // transient storage in bytes
	void *transient_storage;            // This should be zero initialized

	file_free_debug_func *plat_file_free_debug;
	file_read_debug_func *plat_file_read_debug;
	file_write_debug_func *file_write_debug;

	uint8_t is_initialized;
} Storage;

static inline ControllerState *input_get_controller(GameInput *input, size_t controller_index)
{
	assert(controller_index < MAX_CONTROLLERS);

	return &input->controllers[controller_index];
}

/**
 * @brief Updates the game status and renders it
 */
#define GAME_UPDATE_AND_RENDER(name) \
	void name(GameOffscreenBuffer *back_buffer, ThreadContext *thread, Storage *storage, GameInput *input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define SOUND_CREATE_SAMPLES(name) void name(GameSoundBuffer *soundbuff, ThreadContext *thread, Storage *memory)
typedef SOUND_CREATE_SAMPLES(sound_create_samples_func);

#endif // HANDMADE_H
