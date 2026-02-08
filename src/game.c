/*
* Game api implementation
*/

#include "game.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief
 *
 * @param buffer
 * @param samples
 */
static void game_sound_output(Game_State *game_state, Game_SoundBuffer *buffer, unsigned tonehz)
{
	float tone_volume = 3000;
	size_t wave_period = buffer->samples_per_sec / tonehz;
	int16_t *sample_out = buffer->samples;
	float sample_value = 0;
	for (size_t i = 0; i < buffer->sample_count; ++i) {
#if 0
		float sine_value = sinf(game_state->tsine);
		sample_value = sine_value * tone_volume;
#else
		sample_value = 0;
#endif
		*sample_out = (int16_t)sample_value; // channel one
		++sample_out;
		*sample_out = (int16_t)sample_value; // channel two
		++sample_out;

#if 0
		game_state->tsine += 2.0F * PIE / (float_t)wave_period;
		if (game_state->tsine > 2.0F * PIE) {
			game_state->tsine -= 2.0f * PIE;
		}
#endif
	}
}

/**
 * @brief maxx and maxy not included
 *
 * @param bitmap
 * @param minx
 * @param miny
 * @param maxx
 * @param maxy
 */
static void game_bitmap_render_rectangle(Game_Bitmap *bitmap, float min_x, float min_y, float max_x,
                                         float max_y, unsigned color)
{
	assert(min_x <= max_x && min_y <= max_y);

	unsigned min_x_px = (unsigned)tools_int_max(tools_float_round_to_int(min_x), 0);
	unsigned min_y_px = (unsigned)tools_int_max(tools_float_round_to_int(min_y), 0);
	unsigned max_x_px = (unsigned)tools_int_max(tools_float_round_to_int(max_x), 0);
	unsigned max_y_px = (unsigned)tools_int_max(tools_float_round_to_int(max_y), 0);

	min_x_px = TOOLS_MIN(min_x_px, bitmap->width);
	min_y_px = TOOLS_MIN(min_y_px, bitmap->height);
	max_x_px = TOOLS_MIN(max_x_px, bitmap->width);
	max_y_px = TOOLS_MIN(max_y_px, bitmap->height);

	uint8_t *pixel_ptr = (uint8_t *)bitmap->memory +
	                     (size_t)(min_x_px * bitmap->bytes_per_pixel) +
	                     (size_t)(min_y_px * bitmap->pitch_bytes);
	uint32_t *pixel = nullptr;
	for (unsigned y = min_y_px; y < max_y_px; ++y) {
		for (unsigned x = min_x_px; x < max_x_px; ++x) {
			pixel = (uint32_t *)pixel_ptr;
			*pixel = color;
			pixel_ptr += bitmap->bytes_per_pixel;
		}

		pixel_ptr += bitmap->pitch_bytes - (max_x_px - min_x_px) * bitmap->bytes_per_pixel;
	}
}

GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_memory->is_initialized = true;
	}

	for (size_t controller_idx = 0; controller_idx < GAME_MAX_CONTROLLERS; ++controller_idx) {
		Game_ControllerInput *controller = game_input_get_controller(input, controller_idx);

		if (!controller->is_connected) {
			continue;
		}

		if (controller->is_analog) {
		} else {
		}

		game_bitmap_render_rectangle(bitmap, 0.0F, 0.0F, (float)bitmap->width,
		                             (float)bitmap->height, 0x00FF00FF);
		game_bitmap_render_rectangle(bitmap, 10.0F, 40.0F, 40.0F, 41.0F, 0x0000FFFF);
	}
}

GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples)
{
	assert(sizeof(Game_State) <= memory->permamem_size);

	Game_State *game_state = memory->permamem;
	game_sound_output(game_state, soundbuff, 400);
}
