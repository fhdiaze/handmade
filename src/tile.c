#include <assert.h>

#include "game.h"
#include "tile.h"
#include "tix_math.h"

static inline bool tile_map_correct_coord(Tile_Map *map, uint32_t *tile, float *tile_rel)
{
	int tile_offset = tix_math_float_round_to_int(*tile_rel / map->tile_side_mts);

	// World is toroidal
	*tile = (unsigned)((int)*tile + tile_offset);

	*tile_rel -= (float)(tile_offset)*map->tile_side_mts;

	assert(*tile_rel <= map->tile_radius_mts);
	assert(*tile_rel >= -map->tile_radius_mts);

	return true;
}

static inline Tile_ChunkPosition tile_map_get_chunk_pos(Tile_Map *map, uint32_t tile_x,
                                                        uint32_t tile_y)
{
	Tile_ChunkPosition result;

	result.chunk_x = tile_x >> map->chunk_shift_bits;
	result.chunk_y = tile_y >> map->chunk_shift_bits;
	result.tile_x = tile_x & map->chunk_mask;
	result.tile_y = tile_y & map->chunk_mask;

	return result;
}

static Tile_Chunk *tile_map_get_chunk(Tile_Map *map, uint32_t chunk_x, uint32_t chunk_y)
{
	Tile_Chunk *result = nullptr;

	if (chunk_x < map->side_tcs && chunk_y < map->side_tcs) {
		result = &map->tilechunks[chunk_y * map->side_tcs + chunk_x];
	}

	return result;
}

static inline uint32_t tile_map_get_tile_value(Tile_Map *map, uint32_t tile_x, uint32_t tile_y)
{
	uint32_t tile_value = 0;

	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(map, tile_x, tile_y);
	Tile_Chunk *chunk = tile_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y);

	if (!chunk) {
		return tile_value;
	}

	assert(cpos.tile_x < map->chunk_side_tls);
	assert(cpos.tile_y < map->chunk_side_tls);

	tile_value = chunk->tiles[cpos.tile_y * map->chunk_side_tls + cpos.tile_x];

	return tile_value;
}

bool tile_map_correct_position(Tile_Map *map, Tile_Position *pos)
{
	bool was_success = tile_map_correct_coord(map, &pos->tile_x, &pos->tile_rel_x_mts);
	if (!was_success) {
		return was_success;
	}

	was_success = tile_map_correct_coord(map, &pos->tile_y, &pos->tile_rel_y_mts);

	return was_success;
}

bool tile_map_is_point_walkable(Tile_Map *map, Tile_Position pos)
{
	uint32_t tile_value = tile_map_get_tile_value(map, pos.tile_x, pos.tile_y);
	bool is_walkable = tile_value == TILE_TYPE_WALKABLE;

	return is_walkable;
}

void tile_map_set_tile_value(Tile_Map *map, Game_Arena *arena, uint32_t tile_x, uint32_t tile_y,
                             uint32_t tile_value)
{
	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(map, tile_x, tile_y);
	Tile_Chunk *tilechunk = tile_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y);

	assert(tilechunk);

	assert(cpos.tile_x < map->chunk_side_tls);
	assert(cpos.tile_y < map->chunk_side_tls);

	tilechunk->tiles[cpos.tile_y * map->chunk_side_tls + cpos.tile_x] = tile_value;
}
