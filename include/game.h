// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

static constexpr float Pi32 = 3.14159265359f;

// Game services

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

/**
 * @brief Updates the game status and renders it
 *
 * @param screenbuff
 * @param soundbuff
 * @param blue_offset
 * @param green_offset
 */
void game_update_and_render(Game_OffScreenBuffer *screenbuff, Game_SoundBuffer *soundbuff,
                            long blue_offset, long green_offset, size_t tonehz);

/**
 * @brief Creates the sound samples
 *
 * @param buffer       Pointer to the sound buffer to fill
 * @param tonehz
 */
void game_sound_output(Game_SoundBuffer *buffer, size_t tonehz);

// Platform services

#endif // GAME_H
