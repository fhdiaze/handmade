// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define KB_TO_BYTE(_pr_v) (_pr_v * 1024ull)
#define MB_TO_BYTE(_pr_v) (KB_TO_BYTE(_pr_v) * 1024ull)
#define GB_TO_BYTE(_pr_v) (MB_TO_BYTE(_pr_v) * 1024ull)
#define TB_TO_BYTE(_pr_v) (GB_TO_BYTE(_pr_v) * 1024ull)

#ifdef DEBUG
#define BASE_ADDRESS ((void *)TB_TO_BYTE(2))
#else
#define BASE_ADDRESS (nullptr)
#endif // DEBUG

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

typedef struct Game_State {
	long blue_offset;
	long green_offset;
	size_t tonehz;
} Game_State;

typedef struct Game_Memory {
	bool initialized;

	size_t permsize;
	void *permstorage; // This should be zero initialized

	size_t transize;
	void *transtorage; // This should be zero initialized
} Game_Memory;

/**
 * @brief Updates the game status and renders it
 *
 * @param screenbuff
 * @param soundbuff
 */
void game_update_and_render(Game_Memory *memory, Game_Input *input,
                            Game_OffScreenBuffer *screenbuff, Game_SoundBuffer *soundbuff);

// Platform services

#endif // GAME_H
