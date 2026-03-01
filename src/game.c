/*
* Game api implementation
*/

#include "game.h"

#include "tix_math.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define TILES_COUNT_X 17
#define TILES_COUNT_Y 9
#define TILE_SIDE_PXS 60
#define TILE_SIDE_MTS 1

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

	unsigned min_x_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(min_x), 0);
	unsigned min_y_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(min_y), 0);
	unsigned max_x_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(max_x), 0);
	unsigned max_y_px = (unsigned)tix_math_int_max(tix_math_float_round_to_int(max_y), 0);

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

static inline uint32_t game_tilemap_get_tile_value(Game_Tilemap *tilemap, Game_World *world,
                                                   unsigned tile_x, unsigned tile_y)
{
	assert(tilemap);

	assert(tile_x >= 0 && tile_x < world->tiles_count_x && tile_y >= 0 &&
	       tile_y < world->tiles_count_y);

	uint32_t tile_value = tilemap->tiles[tile_y * world->tiles_count_x + tile_x];

	return tile_value;
}

static inline bool game_tilemap_is_point_empty(Game_Tilemap *tilemap, Game_World *world,
                                               unsigned tile_x, unsigned tile_y)
{
	assert(tilemap);

	bool is_empty = false;

	if (tile_x >= 0 && tile_x < world->tiles_count_x && tile_y >= 0 &&
	    tile_y < world->tiles_count_y) {
		uint32_t tilemap_value =
			game_tilemap_get_tile_value(tilemap, world, tile_x, tile_y);
		is_empty = tilemap_value == 0;
	}

	return is_empty;
}

static Game_Tilemap *game_world_get_tilemap(Game_World *world, unsigned tilemap_x,
                                            unsigned tilemap_y)
{
	if (tilemap_x >= world->tilemaps_count_x || tilemap_y >= world->tilemaps_count_y) {
		return nullptr;
	}

	Game_Tilemap *tilemap = &world->tilemaps[tilemap_y * world->tilemaps_count_x + tilemap_x];

	return tilemap;
}

static inline bool game_world_correct_coord(Game_World *world, unsigned tiles_count,
                                            unsigned *tilemap, unsigned *tile, float *tile_rel)
{
	int tiles_delta = (int)tix_math_float_floor(*tile_rel / (float)world->tile_side_pxs);
	int actual_tile = (int)*tile + tiles_delta;
	int tilemaps_delta = (int)tix_math_float_floor((float)actual_tile / (float)tiles_count);

	int new_tilemap = (int)*tilemap + tilemaps_delta;

	if (new_tilemap < 0 || new_tilemap > (int)world->tilemaps_count_x) {
		return false;
	}

	*tilemap = (unsigned)new_tilemap;

	int new_tile = actual_tile - tilemaps_delta * (int)tiles_count;

	assert(new_tile >= 0 && new_tile < (int)tiles_count);

	*tile = (unsigned)(new_tile);
	*tile_rel -= (float)(tiles_delta * (int)world->tile_side_pxs);

	assert(*tile_rel >= 0.0F && *tile_rel < (float)world->tile_side_pxs);

	return true;
}

static bool game_world_correct_position(Game_World *world, Game_CanonicalPosition *pos)
{
	bool was_success = game_world_correct_coord(world, world->tiles_count_x, &pos->tilemap_x,
	                                            &pos->tile_x, &pos->tile_rel_x);
	if (!was_success) {
		return was_success;
	}

	was_success = game_world_correct_coord(world, world->tiles_count_y, &pos->tilemap_y,
	                                       &pos->tile_y, &pos->tile_rel_y);

	return was_success;
}

static bool game_world_is_point_empty(Game_World *world, Game_CanonicalPosition pos)
{
	bool is_empty = false;

	Game_Tilemap *tilemap = game_world_get_tilemap(world, pos.tilemap_x, pos.tilemap_y);
	if (!tilemap) {
		return is_empty;
	}

	is_empty = game_tilemap_is_point_empty(tilemap, world, pos.tile_x, pos.tile_y);

	return is_empty;
}

GAME_BITMAP_UPDATE_AND_RENDER(game_bitmap_update_and_render)
{
	assert(sizeof(Game_State) <= game_memory->permamem_size);

	uint32_t tiles00[TILES_COUNT_Y][TILES_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
		{ 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
	};
	uint32_t tiles01[TILES_COUNT_Y][TILES_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	};
	uint32_t tiles10[TILES_COUNT_Y][TILES_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
	};
	uint32_t tiles11[TILES_COUNT_Y][TILES_COUNT_X] = {
		{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	};
	Game_Tilemap tilemaps[2][2] = {};
	tilemaps[0][0].tiles = (uint32_t *)tiles00;
	tilemaps[0][1].tiles = (uint32_t *)tiles10;
	tilemaps[1][0].tiles = (uint32_t *)tiles01;
	tilemaps[1][1].tiles = (uint32_t *)tiles11;

	Game_World world = {
		.tile_side_mts = TILE_SIDE_MTS,
		.tile_side_pxs = TILE_SIDE_PXS,
		.tilemaps_count_x = 2,
		.tilemaps_count_y = 2,
		.tiles_count_x = TILES_COUNT_X,
		.tiles_count_y = TILES_COUNT_Y,
		.screen_offset_x = -TILE_SIDE_PXS / 2,
		.screen_offset_y = 0,
		.tilemaps = (Game_Tilemap *)tilemaps,
	};

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_state->playerpos.tilemap_x = 0;
		game_state->playerpos.tilemap_y = 0;
		game_state->playerpos.tile_x = 3;
		game_state->playerpos.tile_y = 3;
		game_state->playerpos.tile_rel_x = 5.0F;
		game_state->playerpos.tile_rel_y = 5.0F;

		game_memory->is_initialized = true;
	}

	float player_width = 0.75F * (float)world.tile_side_pxs;
	float player_height = (float)world.tile_side_pxs;

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

			float new_player_x = game_state->playerpos.tile_rel_x +
			                     input->secs_time_delta * pixels_per_sec_player_x;
			float new_player_y = game_state->playerpos.tile_rel_y +
			                     input->secs_time_delta * pixels_per_sec_player_y;

			Game_CanonicalPosition new_player_pos = game_state->playerpos;
			new_player_pos.tile_rel_x = new_player_x;
			new_player_pos.tile_rel_y = new_player_y;

			if (new_player_x < 0) {
				int a = 0;
			}

			if (!game_world_correct_position(&world, &new_player_pos)) {
				continue;
			}

			Game_CanonicalPosition left_bottom_pos = new_player_pos;
			left_bottom_pos.tile_rel_x =
				new_player_pos.tile_rel_x - player_width * 0.5F;
			if (!game_world_correct_position(&world, &left_bottom_pos)) {
				continue;
			}

			Game_CanonicalPosition right_bottom_pos = new_player_pos;
			right_bottom_pos.tile_rel_x =
				new_player_pos.tile_rel_x + player_width * 0.5F;
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

		Game_Tilemap *tilemap = game_world_get_tilemap(
			&world, game_state->playerpos.tilemap_x, game_state->playerpos.tilemap_y);
		for (unsigned row = 0; row < world.tiles_count_y; ++row) {
			for (unsigned col = 0; col < world.tiles_count_x; ++col) {
				uint32_t tile_id =
					game_tilemap_get_tile_value(tilemap, &world, col, row);
				float gray = tile_id == 1 ? 1.0F : 0.5F;

				float min_x = (float)world.screen_offset_x +
				              (float)col * (float)world.tile_side_pxs;
				float min_y = (float)world.screen_offset_y +
				              (float)row * (float)world.tile_side_pxs;
				float max_x = min_x + (float)world.tile_side_pxs;
				float max_y = min_y + (float)world.tile_side_pxs;

				game_bitmap_render_rectangle(bitmap, min_x, min_y, max_x, max_y,
				                             gray, gray, gray);
			}
		}

		float player_red = 1.0F;
		float player_green = 1.0F;
		float player_blue = 0.0F;
		float player_x = (float)(game_state->playerpos.tile_x * world.tile_side_pxs) +
		                 game_state->playerpos.tile_rel_x + (float)world.screen_offset_x;
		float player_y = (float)(game_state->playerpos.tile_y * world.tile_side_pxs) +
		                 game_state->playerpos.tile_rel_y + (float)world.screen_offset_y;
		float player_left = player_x - 0.5F * player_width;
		float player_top = player_y - player_height;
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
