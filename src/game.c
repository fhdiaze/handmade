/*
* Game api implementation
*/

#include "game.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "tix_math.h"

#include "tile.c"

#define TILES_COUNT_X 256
#define TILES_COUNT_Y 256

#define TILE_RADIUS_PXS 30
#define TILE_RADIUS_MTS 0.7F

#define TILE_SIDE_PXS TILE_RADIUS_PXS * 2
#define TILE_SIDE_MTS TILE_RADIUS_MTS * 2

#define PXS_PER_MTR (float)TILE_RADIUS_PXS / TILE_RADIUS_MTS

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
static void game_bitmap_render_rectangle(Game_Bitmap *bitmap, float min_x_pxs, float min_y_pxs,
                                         float max_x_pxs, float max_y_pxs, float red, float green,
                                         float blue)
{
	assert(min_x_pxs <= max_x_pxs && min_y_pxs <= max_y_pxs);

	unsigned min_x_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(min_x_pxs), 0);
	unsigned min_y_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(min_y_pxs), 0);
	unsigned max_x_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(max_x_pxs), 0);
	unsigned max_y_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(max_y_pxs), 0);

	min_x_px = TOOLS_MIN(min_x_px, bitmap->width);
	min_y_px = TOOLS_MIN(min_y_px, bitmap->height);
	max_x_px = TOOLS_MIN(max_x_px, bitmap->width);
	max_y_px = TOOLS_MIN(max_y_px, bitmap->height);

	uint32_t red_bits = (uint32_t)tix_math_float_round_to_int(red * 255.0F);
	uint32_t green_bits = (uint32_t)tix_math_float_round_to_int(green * 255.0F);
	uint32_t blue_bits = (uint32_t)tix_math_float_round_to_int(blue * 255.0F);
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

GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_state->playerpos.tile_x = 3;
		game_state->playerpos.tile_y = 3;
		game_state->playerpos.tile_rel_x_mts = 0.0F;
		game_state->playerpos.tile_rel_y_mts = 0.0F;

		game_state->world = ;
		Game_World *world = game_state->world;

		Game_TileChunk tilechunk = {
			.tiles = (uint32_t *)tiles,
		};

		Game_Tilemap tilemap = {
			.chunk_shift_bits = 8,
			.chunk_mask = 0xFF,
			.chunk_side_tls = 256,
			.tile_radius_pxs = TILE_RADIUS_PXS,
			.tile_radius_mts = TILE_RADIUS_MTS,
			.tile_side_pxs = TILE_SIDE_PXS,
			.tile_side_mts = TILE_SIDE_MTS,
			.pxs_per_mtr = PXS_PER_MTR,
			.side_tcs = 1,
			.tilechunks = &tilechunk,
		};

		uint32_t tiles_per_width = 17;
		uint32_t tiles_per_height = 9;
		for (uint32_t screen_y = 0; screen_y < 32; ++screen_y) {
			for (uint32_t screen_x = 0; screen_x < 17; ++screen_x) {
				for (uint32_t chunk_tile_y = 0; chunk_tile_y < tiles_per_height;
				     ++chunk_tile_y) {
					for (uint32_t chunk_tile_x = 0;
					     chunk_tile_x < tiles_per_width; ++chunk_tile_x) {
						uint32_t tile_x =
							screen_x * tiles_per_width + chunk_tile_x;
						uint32_t tile_y =
							screen_y * tiles_per_height + chunk_tile_y;
						game_tilemap_set_tile_value(world->tilemap,
						                            chunk_tile_x,
						                            chunk_tile_y, 0);
					}
				}
			}
		}

		game_memory->is_initialized = true;
	}

	float player_height_mts = 1.4F;
	float player_width_mts = 0.75F * player_height_mts;

	for (size_t controller_idx = 0; controller_idx < GAME_MAX_CONTROLLERS; ++controller_idx) {
		Game_ControllerInput *controller = game_input_get_controller(input, controller_idx);

		if (!controller->is_connected) {
			continue;
		}

		if (controller->is_analog) {
		} else {
			float mts_per_sec_player_x = 0.0F;
			float mts_per_sec_player_y = 0.0F;

			if (controller->moveup.ended_down) {
				mts_per_sec_player_y = 1.0F;
			}

			if (controller->movedown.ended_down) {
				mts_per_sec_player_y = -1.0F;
			}

			if (controller->moveleft.ended_down) {
				mts_per_sec_player_x = -1.0F;
			}

			if (controller->moveright.ended_down) {
				mts_per_sec_player_x = 1.0F;
			}

			float player_speed = 2.0F;
			if (controller->actionup.ended_down) {
				player_speed = 10.0F;
			}

			mts_per_sec_player_x *= player_speed;
			mts_per_sec_player_y *= player_speed;

			float new_player_x = game_state->playerpos.tile_rel_x_mts +
			                     input->secs_time_delta * mts_per_sec_player_x;
			float new_player_y = game_state->playerpos.tile_rel_y_mts +
			                     input->secs_time_delta * mts_per_sec_player_y;

			Game_TilemapPosition new_player_pos = game_state->playerpos;
			new_player_pos.tile_rel_x_mts = new_player_x;
			new_player_pos.tile_rel_y_mts = new_player_y;

			if (!game_tilemap_correct_position(&tilemap, &new_player_pos)) {
				continue;
			}

			Game_TilemapPosition left_bottom_pos = new_player_pos;
			left_bottom_pos.tile_rel_x_mts -= player_width_mts * 0.5F;
			if (!game_tilemap_correct_position(&tilemap, &left_bottom_pos)) {
				continue;
			}

			Game_TilemapPosition right_bottom_pos = new_player_pos;
			right_bottom_pos.tile_rel_x_mts += player_width_mts * 0.5F;
			if (!game_tilemap_correct_position(&tilemap, &right_bottom_pos)) {
				continue;
			}

			if (game_tilemap_is_point_empty(&tilemap, new_player_pos) &&
			    game_tilemap_is_point_empty(&tilemap, left_bottom_pos) &&
			    game_tilemap_is_point_empty(&tilemap, right_bottom_pos)) {
				game_state->playerpos = new_player_pos;
			} else {
				continue;
			}
		}

		game_bitmap_render_rectangle(bitmap, 0.0F, 0.0F, (float)bitmap->width,
		                             (float)bitmap->height, 1.0F, 0.0F, 1.0F);

		float center_x = (float)bitmap->width * 0.5F;
		float center_y = (float)bitmap->height * 0.5F;
		for (int32_t tile_row_offset = -10; tile_row_offset < 10; ++tile_row_offset) {
			for (int32_t tile_col_offset = -20; tile_col_offset < 20;
			     ++tile_col_offset) {
				uint32_t col = (uint32_t)(tile_col_offset +
				                          (int32_t)game_state->playerpos.tile_x);
				uint32_t row = (uint32_t)(tile_row_offset +
				                          (int32_t)game_state->playerpos.tile_y);

				uint32_t tile_type_id =
					game_tilemap_get_tile_value(&tilemap, col, row);
				float gray = tile_type_id == 1 ? 1.0F : 0.5F;

				float min_x_pxs =
					center_x - (float)tilemap.tile_radius_pxs +
					(float)tile_col_offset * (float)tilemap.tile_side_pxs -
					game_state->playerpos.tile_rel_x_mts * tilemap.pxs_per_mtr;
				float min_y_pxs =
					center_y - (float)tilemap.tile_side_pxs -
					(float)tile_row_offset * (float)tilemap.tile_side_pxs +
					game_state->playerpos.tile_rel_y_mts * tilemap.pxs_per_mtr +
					(float)tilemap.tile_radius_pxs;
				float max_x_pxs = min_x_pxs + (float)tilemap.tile_side_pxs;
				float max_y_pxs = min_y_pxs + (float)tilemap.tile_side_pxs;

				if (game_state->playerpos.tile_y == row &&
				    game_state->playerpos.tile_x == col) {
					gray = 0.0F;
				}
				game_bitmap_render_rectangle(bitmap, min_x_pxs, min_y_pxs,
				                             max_x_pxs, max_y_pxs, gray, gray,
				                             gray);
			}
		}

		float player_red = 1.0F;
		float player_green = 1.0F;
		float player_blue = 0.0F;
		float player_min_x_pxs = center_x - 0.5F * player_width_mts * tilemap.pxs_per_mtr;
		float player_min_y_pxs = center_y - player_height_mts * tilemap.pxs_per_mtr;
		float player_max_x_pxs = player_min_x_pxs + player_width_mts * tilemap.pxs_per_mtr;
		float player_max_y_pxs = player_min_y_pxs + player_height_mts * tilemap.pxs_per_mtr;
		game_bitmap_render_rectangle(bitmap, player_min_x_pxs, player_min_y_pxs,
		                             player_max_x_pxs, player_max_y_pxs, player_red,
		                             player_green, player_blue);
	}
}

GAME_SOUND_CREATE_SAMPLES(game_sound_create_samples)
{
	assert(sizeof(Game_State) <= memory->permamem_size);

	Game_State *game_state = memory->permamem;
	game_sound_output(game_state, soundbuff, 400);
}
