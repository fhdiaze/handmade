// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <assert.h>
#include <stdint.h>

#include "handmade_platform.h"
#include "handmade_tile.h"

#define GAME_MAX_MOUSE_BUTTONS 5
#define GAME_MAX_CONTROLLERS 5
#define GAME_MAX_CONTROLLER_BUTTONS 12

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct Game_Bitmap {
	void *top_left_px;
	unsigned width_px;    // pixels
	unsigned height_px;   // pixels
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
	uint8_t ended_down;
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
	uint8_t is_analog;
	uint8_t is_connected;
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

typedef struct Game_World {
	Tile_Map *map;
} Game_World;

typedef struct Game_HeroBitmaps {
	int32_t align_x_px;
	int32_t align_y_px;

	Plat_LoadedBitmap head;
	Plat_LoadedBitmap cape;
	Plat_LoadedBitmap torso;
} Game_HeroBitmaps;

typedef struct Game_State {
	Plat_Arena arena;
	Game_World *world;
	Tile_Position camera_position;
	Tile_Position hero_position;

	Plat_LoadedBitmap backdrop;

	uint8_t hero_facing_direction;
	Game_HeroBitmaps hero_bitmaps[4];
} Game_State;

// Utilities

static inline Game_ControllerInput *game_input_get_controller(Game_Input *input,
                                                              size_t controller_index)
{
	assert(controller_index < GAME_MAX_CONTROLLERS);

	return &input->controllers[controller_index];
}

// Game services

/**
 * @brief Updates the game status and renders it
 */
#define GAME_BITMAP_UPDATE_AND_RENDER(name)                                                  \
	void name(Game_Bitmap *bitmap, Plat_ThreadContext *thread, Plat_Memory *Plat_Memory, \
	          Game_Input *input)
typedef GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render_func);

#define GAME_SOUND_CREATE_SAMPLES(name) \
	void name(Game_SoundBuffer *soundbuff, Plat_ThreadContext *thread, Plat_Memory *memory)
typedef GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples_func);

#endif // GAME_H
