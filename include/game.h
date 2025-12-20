// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

static constexpr float Pi32 = 3.14159265359f;
static constexpr unsigned game_max_controllers = 4;

// Game services

/*
* Boolean without the overhead of normalization. Take into account that any value greater than 0 is true
*/
typedef uint8_t bool8;

typedef struct Game_OffScreenBuffer {
	void *memory;
	long width;
	long height;
	long pitch; // size of a row in bytes
} Game_OffScreenBuffer;

typedef struct Game_SoundBuffer {
	size_t samples_per_sec;
	size_t sample_count;
	int16_t *samples;
} Game_SoundBuffer;

typedef struct Game_ButtonState {
	int half_transition_count;
	bool8 ended_down;
} Game_ButtonState;

typedef struct Game_ControllerInput {
	bool8 analog;

	float startx;
	float starty;

	float minx;
	float miny;

	float maxx;
	float maxy;

	float endx;
	float endy;

	union {
		Game_ButtonState buttons[6];
		struct {
			Game_ButtonState up;
			Game_ButtonState down;
			Game_ButtonState left;
			Game_ButtonState right;
			Game_ButtonState left_shoulder;
			Game_ButtonState right_shoulder;
		};
	};
} Game_ControllerInput;

typedef struct Game_Input {
	Game_ControllerInput controllers[game_max_controllers];
} Game_Input;

/**
 * @brief Updates the game status and renders it
 *
 * @param screenbuff
 * @param soundbuff
 */
void game_update_and_render(Game_Input *input, Game_OffScreenBuffer *screenbuff,
                            Game_SoundBuffer *soundbuff);

// Platform services

#endif // GAME_H
