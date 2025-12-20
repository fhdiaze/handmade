/*
* Game api implementation
*/

#include "game.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>

/**
 * @brief
 *
 * @param buffer
 * @param samples
 */
static void game_sound_output(Game_SoundBuffer *buffer, size_t tonehz)
{
	static float tsine;
	float tone_volume = 3000;
	size_t wave_period = buffer->samples_per_sec / tonehz;
	int16_t *sample_out = buffer->samples;
	float sample_value = 0;
	for (size_t i = 0; i < buffer->sample_count; ++i) {
		float_t sine_value = sinf(tsine);
		sample_value = sine_value * tone_volume;
		*sample_out = (int16_t)sample_value; // channel one
		++sample_out;
		*sample_out = (int16_t)sample_value; // channel two
		++sample_out;

		tsine += 2.0f * Pi32 / (float_t)wave_period;
	}
}

static void game_render_weird_gradient(Game_OffScreenBuffer *buffer, long blue_offset,
                                       long green_offset)
{
	uint8_t *row = (uint8_t *)buffer->memory;
	for (long y = 0; y < buffer->height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (long x = 0; x < buffer->width; ++x) {
			// Little endian in memory  B G R X -> because of the endianess
			// little endian on a register: 0xXXRRGGBB
			uint8_t blue = (uint8_t)(x + blue_offset);
			uint8_t green = (uint8_t)(y + green_offset);
			*pixel = (uint32_t)(green << CHAR_BIT) | blue;
			++pixel;
		}
		row += buffer->pitch;
	}
}

void game_update_and_render(Game_Input *input, Game_OffScreenBuffer *screenbuff,
                            Game_SoundBuffer *soundbuff)
{
	static long blue_offset = 0;
	static long green_offset = 0;
	static size_t tonehz = 256;

	Game_ControllerInput *izero = &input->controllers[0];
	if (izero->analog) {
		tonehz = 256 + (size_t)(128.0f * (float)izero->endy);
		blue_offset += (long)(4.0f * (float)izero->endx);
	} else {
	}

	if (izero->down.ended_down) {
		green_offset += 1;
	}

	game_sound_output(soundbuff, tonehz);
	game_render_weird_gradient(screenbuff, blue_offset, green_offset);
}
