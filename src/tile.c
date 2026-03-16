#include <assert.h>

#include "game.h"
#include "tile.h"
#include "tix_math.h"

static inline bool tile_map_correct_coord(Tile_Map *tilemap, uint32_t *tile, float *tile_rel)
{
	int tile_offset = tix_math_float_round_to_int(*tile_rel / tilemap->tile_side_mts);

	// World is toroidal
	*tile = (unsigned)((int)*tile + tile_offset);

	assert(*tile < tilemap->chunk_side_tls);

	*tile_rel -= (float)(tile_offset)*tilemap->tile_side_mts;

	assert(*tile_rel <= tilemap->tile_radius_mts);
	assert(*tile_rel >= -tilemap->tile_radius_mts);

	return true;
}

static inline Tile_ChunkPosition tile_map_get_chunk_pos(Tile_Map *tilemap, uint32_t tile_x,
                                                        uint32_t tile_y)
{
	Tile_ChunkPosition result;

	result.left_lower_tile_x = tile_x >> tilemap->chunk_shift_bits;
	result.left_lower_tile_y = tile_y >> tilemap->chunk_shift_bits;
	result.rel_tile_x = tile_x & tilemap->chunk_mask;
	result.rel_tile_y = tile_y & tilemap->chunk_mask;

	return result;
}

static Tile_Chunk *tile_map_get_tilechunk(Tile_Map *tilemap, uint32_t tilechunk_x,
                                          uint32_t tilechunk_y)
{
	Tile_Chunk *result = nullptr;

	if (tilechunk_x < tilemap->side_tcs && tilechunk_y < tilemap->side_tcs) {
		result = &tilemap->tilechunks[tilechunk_y * tilemap->side_tcs + tilechunk_x];
	}

	return result;
}

static inline uint32_t tile_map_get_tile_value(Tile_Map *tilemap, uint32_t tile_x, uint32_t tile_y)
{
	uint32_t tile_value = 0;

	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(tilemap, tile_x, tile_y);
	Tile_Chunk *tilechunk =
		tile_map_get_tilechunk(tilemap, cpos.left_lower_tile_x, cpos.left_lower_tile_y);

	if (!tilechunk) {
		return tile_value;
	}

	assert(tile_x < tilemap->chunk_side_tls);
	assert(tile_y < tilemap->chunk_side_tls);

	tile_value = tilechunk->tiles[tile_y * tilemap->chunk_side_tls + tile_x];

	return tile_value;
}

bool tile_map_correct_position(Tile_Map *tilemap, Tile_MapPosition *pos)
{
	bool was_success = tile_map_correct_coord(tilemap, &pos->tile_x, &pos->tile_rel_x_mts);
	if (!was_success) {
		return was_success;
	}

	was_success = tile_map_correct_coord(tilemap, &pos->tile_y, &pos->tile_rel_y_mts);

	return was_success;
}

bool tile_map_is_point_empty(Tile_Map *tilemap, Tile_MapPosition pos)
{
	uint32_t tile_value = tile_map_get_tile_value(tilemap, pos.tile_x, pos.tile_y);
	bool is_empty = tile_value == 0;

	return is_empty;
}

void tile_map_set_tile_value(Tile_Map *tilemap, Game_Arena *arena, uint32_t tile_x, uint32_t tile_y,
                             uint32_t tile_value)
{
	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(tilemap, tile_x, tile_y);
	Tile_Chunk *tilechunk =
		tile_map_get_tilechunk(tilemap, cpos.left_lower_tile_x, cpos.left_lower_tile_y);

	if (!tilechunk) {
		return;
	}

	assert(tile_x < tilemap->chunk_side_tls);
	assert(tile_y < tilemap->chunk_side_tls);

	tilechunk->tiles[tile_y * tilemap->chunk_side_tls + tile_x] = tile_value;
}
