/*
* Game api implementation
*/

#include "game.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

/**
 * @brief
 *
 * @param buffer
 * @param samples
 */
static void game_sound_output(Game_State *game_state, Game_SoundBuffer *buffer)
{
	float tone_volume = 3000;
	size_t wave_period = buffer->samples_per_sec / game_state->tonehz;
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

		game_state->tsine += 2.0f * PIE / (float_t)wave_period;
		if (game_state->tsine > 2.0f * PIE) {
			game_state->tsine -= 2.0f * PIE;
		}
	}
}

static void game_render_weird_gradient(Game_OffScreenBuffer *buffer, unsigned blue_offset,
                                       unsigned green_offset)
{
	uint8_t *row = (uint8_t *)buffer->memory;
	for (size_t y = 0; y < buffer->height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (size_t x = 0; x < buffer->width; ++x) {
			// Little endian in memory  B G R X -> because of the endianess
			// little endian on a register: 0xXXRRGGBB
			uint8_t blue = (uint8_t)(x + blue_offset);
			uint8_t green = (uint8_t)(y + green_offset);
			*pixel = (uint32_t)(green << CHAR_BIT * 2) | blue;
			++pixel;
		}
		row += buffer->pitch_bytes;
	}
}

static void game_render_player(Game_OffScreenBuffer *screen, unsigned player_x, unsigned player_y)
{
	assert(player_x <= screen->width - 10 && player_y <= screen->height - 10);

	uint32_t color = 0xFFFFFFFF;
	uint8_t *pixel_index = (uint8_t *)screen->memory +
	                       player_x * (size_t)screen->bytes_per_pixel +
	                       player_y * (size_t)screen->pitch_bytes;
	uint32_t *pixel;
	for (unsigned y = player_y; y < player_y + 10; ++y) {
		for (unsigned x = player_x; x < player_x + 10; ++x) {
			pixel = (uint32_t *)pixel_index;
			*pixel = color;
			pixel_index += screen->bytes_per_pixel;
		}

		pixel_index += screen->pitch_bytes - 10 * screen->bytes_per_pixel;
	}
}

GAME_SCREEN_UPDATE_AND_RENDER(game_screen_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		const char *const filename = __FILE__;
		Plat_ReadFileResult read = game_memory->plat_debug_read_file(filename);
		if (read.memory) {
			game_memory->plat_debug_write_file("test.out", read.size, read.memory);
			game_memory->plat_debug_free_file(read.memory);
			read.size = 0;
		}

		game_state->tonehz = 512;
		game_state->blue_offset = 0;
		game_state->green_offset = 0;
		game_state->tsine = 0.0f;

		game_state->player_x = 100;
		game_state->player_y = 100;

		game_state->tjump = 0.0f;

		game_memory->is_initialized = true;
	}

	for (size_t i = 0; i < GAME_MAX_CONTROLLERS; ++i) {
		Game_ControllerInput *controller = game_input_get_controller(input, i);

		if (!controller->is_connected) {
			continue;
		}

		if (controller->is_analog) {
			game_state->blue_offset += (unsigned)(4.0f * controller->stick_avg_x);
			game_state->tonehz = 512 + (unsigned)(128.0f * controller->stick_avg_y);
		} else {
			if (controller->moveleft.ended_down) {
				game_state->blue_offset -= 1;
			}

			if (controller->moveright.ended_down) {
				game_state->blue_offset += 1;
			}
		}

		if (controller->actiondown.ended_down) {
			game_state->green_offset += 1;
		}

		if (controller->actionup.ended_down) {
			game_state->green_offset -= 1;
		}

		game_state->player_x += (unsigned)(4.0f * controller->stick_avg_x);
		game_state->player_y -= (unsigned)(4.0f * controller->stick_avg_y);

		if (game_state->tjump > 0) {
			int delta = (int)(4.0f * controller->stick_avg_y +
			                  10.0f * sinf(2.0f * PIE * game_state->tjump));
			game_state->player_y =
				delta < 0 ? RING_SUB(screenbuff->height - 10, game_state->player_y,
			                             (unsigned)(-delta)) :
					    RING_ADD(screenbuff->height - 10, game_state->player_y,
			                             (unsigned)delta);
		}

		if (controller->actiondown.ended_down) {
			game_state->tjump = 1.0f;
		}
		game_state->tjump -= 0.013f;

		game_state->player_x %= screenbuff->width - 10;
		game_state->player_y %= screenbuff->height - 10;
	}

	game_render_weird_gradient(screenbuff, game_state->blue_offset, game_state->green_offset);
	game_render_player(screenbuff, game_state->player_x, game_state->player_y);
}

GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples)
{
	assert(sizeof(Game_State) <= memory->permamem_size);

	Game_State *game_state = memory->permamem;
	game_sound_output(game_state, soundbuff);
}
