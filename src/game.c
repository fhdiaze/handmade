/*
* Game api implementation
*/

#include "game.h"

#include "tix_math.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define TILES_COUNT_X 256
#define TILES_COUNT_Y 256
#define TILE_SIDE_PXS 60
#define TILE_SIDE_MTS 1.4F
#define PXS_PER_MTR (float)TILE_SIDE_PXS / TILE_SIDE_MTS

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

static inline Game_ChunkPosition game_world_get_chunk_pos(Game_World *world, uint32_t tile_x,
                                                          uint32_t tile_y)
{
	Game_ChunkPosition result;

	result.left_lower_tile_x = tile_x >> world->chunk_shift_bits;
	result.left_lower_tile_y = tile_y >> world->chunk_shift_bits;
	result.rel_tile_x = tile_x & world->chunk_mask;
	result.rel_tile_y = tile_y & world->chunk_mask;

	return result;
}

static inline uint32_t game_tilechunk_get_tile_value(Game_TileChunk *tilechunk, Game_World *world,
                                                     uint32_t tile_x, uint32_t tile_y)
{
	assert(tilechunk);
	assert(tile_x < world->chunk_side_tls);
	assert(tile_y < world->chunk_side_tls);

	uint32_t tile_value = tilechunk->tiles[tile_y * world->chunk_side_tls + tile_x];

	return tile_value;
}

static inline bool game_tilechunk_is_tile_empty(Game_TileChunk *tilechunk, Game_World *world,
                                                uint32_t tile_x, uint32_t tile_y)
{
	assert(tilechunk);

	bool is_empty = false;

	assert(tile_x < world->chunk_side_tls);
	assert(tile_y < world->chunk_side_tls);

	uint32_t tilemap_value = game_tilechunk_get_tile_value(tilechunk, world, tile_x, tile_y);
	is_empty = tilemap_value == 0;

	return is_empty;
}

static Game_TileChunk *game_world_get_tilechunk(Game_World *world, uint32_t tilechunk_x,
                                                uint32_t tilechunk_y)
{
	Game_TileChunk *result = nullptr;

	if (tilechunk_x < world->side_tcs && tilechunk_y < world->side_tcs) {
		result = &world->tilechunks[tilechunk_y * world->side_tcs + tilechunk_x];
	}

	return result;
}

static inline bool game_world_correct_coord(Game_World *world, unsigned *tile, float *tile_rel)
{
	int tiles_deviation = (int)tix_math_float_floor(*tile_rel / world->tile_side_mts);
	// World is toroidal
	*tile = (unsigned)((int)*tile + tiles_deviation);

	assert(*tile < world->chunk_side_tls);

	*tile_rel -= (float)(tiles_deviation)*world->tile_side_mts;

	assert(*tile_rel >= 0.0F);
	assert(*tile_rel < world->tile_side_mts);

	return true;
}

static bool game_world_correct_position(Game_World *world, Game_WorldPosition *pos)
{
	bool was_success = game_world_correct_coord(world, &pos->tile_x, &pos->tile_rel_x_mts);
	if (!was_success) {
		return was_success;
	}

	was_success = game_world_correct_coord(world, &pos->tile_y, &pos->tile_rel_y_mts);

	return was_success;
}

static bool game_world_is_point_empty(Game_World *world, Game_WorldPosition wpos)
{
	bool is_empty = false;

	Game_ChunkPosition cpos = game_world_get_chunk_pos(world, wpos.tile_x, wpos.tile_y);
	Game_TileChunk *tilechunk =
		game_world_get_tilechunk(world, cpos.left_lower_tile_x, cpos.left_lower_tile_y);
	if (!tilechunk) {
		return is_empty;
	}

	is_empty = game_tilechunk_is_tile_empty(tilechunk, world, wpos.tile_x, wpos.tile_y);

	return is_empty;
}

GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	uint32_t tiles[TILES_COUNT_Y][TILES_COUNT_X] = {
		{
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		},
		{
			1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
		},
		{
			1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		},
		{
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		},
	};

	Game_TileChunk tilechunks[2][2] = {};
	tilechunks[0][0].tiles = (uint32_t *)tiles00;
	tilechunks[0][1].tiles = (uint32_t *)tiles10;
	tilechunks[1][0].tiles = (uint32_t *)tiles01;
	tilechunks[1][1].tiles = (uint32_t *)tiles11;

	float camera_zero_x_pxs = -TILE_SIDE_PXS / 2;
	float camera_zero_y_pxs = TILES_COUNT_Y * TILE_SIDE_PXS;

	Game_World world = {
		.chunk_mask = 0xFF,
		.chunk_shift_bits = 8,
		.chunk_side_tls = 256,
		.pxs_per_mtr = PXS_PER_MTR,
		.tile_side_mts = TILE_SIDE_MTS,
		.tile_side_pxs = TILE_SIDE_PXS,
		.tilechunks = (Game_TileChunk *)tilechunks,
	};

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_state->playerpos.tile_x = 3;
		game_state->playerpos.tile_y = 3;
		game_state->playerpos.tile_rel_x_mts = 0.5F;
		game_state->playerpos.tile_rel_y_mts = 0.5F;

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

			mts_per_sec_player_x *= 2.0F;
			mts_per_sec_player_y *= 2.0F;

			float new_player_x = game_state->playerpos.tile_rel_x_mts +
			                     input->secs_time_delta * mts_per_sec_player_x;
			float new_player_y = game_state->playerpos.tile_rel_y_mts +
			                     input->secs_time_delta * mts_per_sec_player_y;

			Game_WorldPosition new_player_pos = game_state->playerpos;
			new_player_pos.tile_rel_x_mts = new_player_x;
			new_player_pos.tile_rel_y_mts = new_player_y;

			if (!game_world_correct_position(&world, &new_player_pos)) {
				continue;
			}

			Game_WorldPosition left_bottom_pos = new_player_pos;
			left_bottom_pos.tile_rel_x_mts -= player_width_mts * 0.5F;
			if (!game_world_correct_position(&world, &left_bottom_pos)) {
				continue;
			}

			Game_WorldPosition right_bottom_pos = new_player_pos;
			right_bottom_pos.tile_rel_x_mts += player_width_mts * 0.5F;
			if (!game_world_correct_position(&world, &right_bottom_pos)) {
				continue;
			}

			if (game_world_is_point_empty(&world, new_player_pos) &&
			    game_world_is_point_empty(&world, left_bottom_pos) &&
			    game_world_is_point_empty(&world, right_bottom_pos)) {
				game_state->playerpos = new_player_pos;
			} else {
				continue;
			}
		}

		game_bitmap_render_rectangle(bitmap, 0.0F, 0.0F, (float)bitmap->width,
		                             (float)bitmap->height, 1.0F, 0.0F, 1.0F);

		Game_TileChunk *tilechunk = game_world_get_tilechunk(
			&world, game_state->playerpos.tile_x, game_state->playerpos.tile_y);
		for (unsigned row = 0; row < world.chunk_side_tls; ++row) {
			for (unsigned col = 0; col < world.chunk_side_tls; ++col) {
				uint32_t tile_id =
					game_tilechunk_get_tile_value(tilechunk, &world, col, row);
				float gray = tile_id == 1 ? 1.0F : 0.5F;

				float min_x_pxs =
					camera_zero_x_pxs + (float)col * (float)world.tile_side_pxs;
				float max_y_pxs =
					camera_zero_y_pxs - (float)row * (float)world.tile_side_pxs;
				float min_y_pxs = max_y_pxs - (float)world.tile_side_pxs;
				float max_x_pxs = min_x_pxs + (float)world.tile_side_pxs;

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
		float player_x_pxs =
			camera_zero_x_pxs +
			(float)game_state->playerpos.tile_x * (float)world.tile_side_pxs +
			game_state->playerpos.tile_rel_x_mts * world.pxs_per_mtr;
		float player_y_pxs =
			camera_zero_y_pxs -
			(float)game_state->playerpos.tile_y * (float)world.tile_side_pxs -
			game_state->playerpos.tile_rel_y_mts * world.pxs_per_mtr;
		float player_min_x_pxs = player_x_pxs - 0.5F * player_width_mts * world.pxs_per_mtr;
		float player_min_y_pxs = player_y_pxs - player_height_mts * world.pxs_per_mtr;
		float player_max_x_pxs = player_min_x_pxs + player_width_mts * world.pxs_per_mtr;
		float player_max_y_pxs = player_min_y_pxs + player_height_mts * world.pxs_per_mtr;
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
