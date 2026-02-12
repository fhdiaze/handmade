/*
* Game api implementation
*/

#include "game.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

typedef struct Game_Tilemap {
	unsigned count_x;
	unsigned count_y;

	float tile_width;
	float tile_height;

	float upper_left_x;
	float upper_left_y;

	uint32_t *tiles;
} Game_Tilemap;

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
 * @param min_x
 * @param min_y
 * @param max_x
 * @param max_y
 * @param red
 * @param green
 * @param blue
 */
static void game_bitmap_render_rectangle(Game_Bitmap *bitmap, float min_x, float min_y, float max_x,
                                         float max_y, float red, float green, float blue)
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

	uint32_t red_bits = (uint32_t)tools_float_round_to_int(red * 255.0F);
	uint32_t green_bits = (uint32_t)tools_float_round_to_int(green * 255.0F);
	uint32_t blue_bits = (uint32_t)tools_float_round_to_int(blue * 255.0F);
	uint32_t color = red_bits << 16UL | green_bits << 8UL | blue_bits;

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

static bool game_tilemap_is_point_empty(Game_Tilemap *tilemap, float test_x, float test_y)
{
	bool is_empty = false;

	unsigned player_tile_x =
		(unsigned)((test_x - (float)tilemap->upper_left_x) / (float)tilemap->tile_width);
	unsigned player_tile_y =
		(unsigned)((test_y - (float)tilemap->upper_left_y) / (float)tilemap->tile_width);

	if (player_tile_x >= 0 && player_tile_x < tilemap->count_x && player_tile_y >= 0 &&
	    player_tile_y < tilemap->count_y) {
		unsigned tile_map_value =
			tilemap->tiles[player_tile_y * tilemap->count_x + player_tile_x];
		is_empty = tile_map_value != 0;
	}

	return is_empty;
}

GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	uint32_t tiles[TILE_MAP_COUNT_Y*TILE_MAP_COUNT_X] = {
		1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
		1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
		0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
		1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
		1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
	};
	Game_Tilemap tilemap = {
		.count_x = TILE_MAP_COUNT_X,
		.count_y = TILE_MAP_COUNT_Y,
		.upper_left_x = -30.0F,
		.upper_left_y = -30.0F,
		.tile_width = 60.0F,
		.tile_height = 60.0F,
		.tiles = tiles,
	};

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_state->player_x = 150.0F;
		game_state->player_y = 150.0F;
		game_memory->is_initialized = true;
	}

	for (size_t controller_idx = 0; controller_idx < GAME_MAX_CONTROLLERS; ++controller_idx) {
		Game_ControllerInput *controller = game_input_get_controller(input, controller_idx);

		if (!controller->is_connected) {
			continue;
		}

		if (controller->is_analog) {
		} else {
			float pixels_per_sec_player_x = 0.0F;
			float pixels_per_sec_player_y = 0.0F;

			if (controller->moveup.ended_down) {
				pixels_per_sec_player_y = -1.0F;
			}

			if (controller->movedown.ended_down) {
				pixels_per_sec_player_y = 1.0F;
			}

			if (controller->moveleft.ended_down) {
				pixels_per_sec_player_x = -1.0F;
			}

			if (controller->moveright.ended_down) {
				pixels_per_sec_player_x = 1.0F;
			}

			pixels_per_sec_player_x *= 64.0F;
			pixels_per_sec_player_y *= 64.0F;

			float new_player_x = game_state->player_x +
			                     input->secs_time_delta * pixels_per_sec_player_x;
			float new_player_y = game_state->player_y +
			                     input->secs_time_delta * pixels_per_sec_player_y;

			unsigned player_tile_x =
				(unsigned)((new_player_x - upper_left_x) / tile_width);
			unsigned player_tile_y =
				(unsigned)((new_player_y - upper_left_y) / tile_width);

			bool is_valid = false;
			if (player_tile_x >= 0 && player_tile_x < TILE_MAP_COUNT_X &&
			    player_tile_y >= 0 && player_tile_y < TILE_MAP_COUNT_Y) {
				unsigned tile_map_value = tile_map[player_tile_y][player_tile_x];
				is_valid = tile_map_value == 0;
			}

			if (is_valid) {
				game_state->player_x = new_player_x;
				game_state->player_y = new_player_y;
			}
		}

		game_bitmap_render_rectangle(bitmap, 0.0F, 0.0F, (float)bitmap->width,
		                             (float)bitmap->height, 1.0F, 0.0F, 1.0F);

		for (unsigned row = 0; row < 9; ++row) {
			for (unsigned col = 0; col < 17; ++col) {
				uint32_t tile_id = tile_map[row][col];
				float gray = tile_id == 1 ? 1.0F : 0.5F;

				float min_x = upper_left_x + (float)col * tile_width;
				float min_y = upper_left_y + (float)row * tile_height;
				float max_x = min_x + tile_width;
				float max_y = min_y + tile_height;

				game_bitmap_render_rectangle(bitmap, min_x, min_y, max_x, max_y,
				                             gray, gray, gray);
			}
		}

		float player_red = 1.0F;
		float player_green = 1.0F;
		float player_blue = 0.0F;
		float player_width = 0.75F * tile_width;
		float player_height = tile_height;
		float player_left = game_state->player_x - 0.5F * player_width;
		float player_top = game_state->player_y - player_height;
		game_bitmap_render_rectangle(bitmap, player_left, player_top,
		                             player_left + player_width, player_top + player_height,
		                             player_red, player_green, player_blue);
	}
}

GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples)
{
	assert(sizeof(Game_State) <= memory->permamem_size);

	Game_State *game_state = memory->permamem;
	game_sound_output(game_state, soundbuff, 400);
}
