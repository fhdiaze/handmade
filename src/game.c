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
	if (tilemap_x < 0 || tilemap_x >= world->tilemaps_count_x || tilemap_y < 0 ||
	    tilemap_y >= world->tilemaps_count_y) {
		return nullptr;
	}

	Game_Tilemap *tilemap = &world->tilemaps[tilemap_y * world->tilemaps_count_x + tilemap_x];

	return tilemap;
}

static Game_CanonicalPosition game_world_get_canonical_position(Game_World *world,
                                                                Game_RawPosition pos)
{
	Game_CanonicalPosition canpos = {
		.tilemap_x = pos.tilemap_x,
		.tilemap_y = pos.tilemap_y,
	};

	float x = pos.win_rel_x - (float)world->upper_left_x;
	float y = pos.win_rel_y - (float)world->upper_left_y;

	int tile_x = (int)tix_math_float_floor(x / (float)world->tile_width);
	int tile_y = (int)tix_math_float_floor(y / (float)world->tile_width);

	canpos.tile_rel_x = x - (float)(tile_x * (int)world->tile_width);
	canpos.tile_rel_y = y - (float)(tile_y * (int)world->tile_height);

	assert(canpos.tile_rel_x >= 0 && canpos.tile_rel_x < (float)world->tile_width);
	assert(canpos.tile_rel_y >= 0 && canpos.tile_rel_y < (float)world->tile_height);

	canpos.tile_x = tix_math_int_abs(tile_x);
	canpos.tile_y = (unsigned)abs(tile_y);

	if (tile_x < 0) {
		--canpos.tilemap_x;
		canpos.tile_x = world->tiles_count_x - canpos.tile_x;
	}

	if (tile_y < 0) {
		--canpos.tilemap_y;
		canpos.tile_y = world->tiles_count_y - canpos.tile_y;
	}

	if (tile_x >= (int)world->tiles_count_x) {
		++canpos.tilemap_x;
		canpos.tile_x = canpos.tile_x - world->tiles_count_x;
	}

	if (tile_y >= (int)world->tiles_count_y) {
		++canpos.tilemap_y;
		canpos.tile_y = canpos.tile_y - world->tiles_count_y;
	}

	return canpos;
}

static bool game_world_is_point_empty(Game_World *world, Game_RawPosition pos)
{
	bool is_empty = false;
	Game_CanonicalPosition canpos = game_world_get_canonical_position(world, pos);

	Game_Tilemap *tilemap = game_world_get_tilemap(world, canpos.tilemap_x, canpos.tilemap_y);
	if (!tilemap) {
		return is_empty;
	}

	return game_tilemap_is_point_empty(tilemap, world, canpos.tile_x, canpos.tile_y);
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
		.tilemaps_count_x = 2,
		.tilemaps_count_y = 2,
		.tiles_count_x = TILES_COUNT_X,
		.tiles_count_y = TILES_COUNT_Y,
		.upper_left_x = -30,
		.upper_left_y = 0,
		.tile_width = 60,
		.tile_height = 60,
		.tilemaps = (Game_Tilemap *)tilemaps,
	};

	Game_State *game_state = game_memory->permamem;
	if (!game_memory->is_initialized) {
		game_state->player_x = 150.0F;
		game_state->player_y = 150.0F;
		game_state->player_tilemap_x = 0;
		game_state->player_tilemap_y = 0;
		game_memory->is_initialized = true;
	}

	float player_width = 0.75F * (float)world.tile_width;
	float player_height = (float)world.tile_height;

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

			Game_RawPosition player_pos = {
				.tilemap_x = game_state->player_tilemap_x,
				.tilemap_y = game_state->player_tilemap_y,
				.win_rel_x = new_player_x,
				.win_rel_y = new_player_y,
			};

			Game_RawPosition left_bottom_pos = {
				.tilemap_x = game_state->player_tilemap_x,
				.tilemap_y = game_state->player_tilemap_y,
				.win_rel_x = new_player_x - player_width * 0.5F,
				.win_rel_y = new_player_y,
			};

			Game_RawPosition right_bottom_pos = {
				.tilemap_x = game_state->player_tilemap_x,
				.tilemap_y = game_state->player_tilemap_y,
				.win_rel_x = new_player_x + player_width * 0.5F,
				.win_rel_y = new_player_y,
			};

			if (game_world_is_point_empty(&world, player_pos) &&
			    game_world_is_point_empty(&world, left_bottom_pos) &&
			    game_world_is_point_empty(&world, right_bottom_pos)) {
				Game_CanonicalPosition canpos =
					game_world_get_canonical_position(&world, player_pos);

				game_state->player_tilemap_x = canpos.tilemap_x;
				game_state->player_tilemap_y = canpos.tilemap_y;
				game_state->player_x = (float)world.upper_left_x +
				                       (float)(world.tile_width * canpos.tile_x) +
				                       canpos.tile_rel_x;
				game_state->player_y = (float)world.upper_left_y +
				                       (float)(world.tile_height * canpos.tile_y) +
				                       canpos.tile_rel_y;
			}
		}

		game_bitmap_render_rectangle(bitmap, 0.0F, 0.0F, (float)bitmap->width,
		                             (float)bitmap->height, 1.0F, 0.0F, 1.0F);

		Game_Tilemap *tilemap = game_world_get_tilemap(&world, game_state->player_tilemap_x,
		                                               game_state->player_tilemap_y);
		for (unsigned row = 0; row < world.tiles_count_y; ++row) {
			for (unsigned col = 0; col < world.tiles_count_x; ++col) {
				uint32_t tile_id =
					game_tilemap_get_tile_value(tilemap, &world, col, row);
				float gray = tile_id == 1 ? 1.0F : 0.5F;

				float min_x = (float)world.upper_left_x +
				              (float)col * (float)world.tile_width;
				float min_y = (float)world.upper_left_y +
				              (float)row * (float)world.tile_height;
				float max_x = min_x + (float)world.tile_width;
				float max_y = min_y + (float)world.tile_height;

				game_bitmap_render_rectangle(bitmap, min_x, min_y, max_x, max_y,
				                             gray, gray, gray);
			}
		}

		float player_red = 1.0F;
		float player_green = 1.0F;
		float player_blue = 0.0F;
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
