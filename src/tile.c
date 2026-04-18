#include <assert.h>

#include "game.h"
#include "tile.h"
#include "tix_math.h"

static inline uint8_t tile_map_correct_coord(uint32_t *tile, float *tile_rel)
{
	int tile_offset = tix_math_float_round_to_int(*tile_rel / TILE_SIDE_M);

	// World is toroidal
	*tile = (unsigned)((int)*tile + tile_offset);

	*tile_rel -= (float)(tile_offset)*TILE_SIDE_M;

	assert(*tile_rel <= TILE_RADIUS_M);
	assert(*tile_rel >= -TILE_RADIUS_M);

	return 1U;
}

static inline Tile_ChunkPosition tile_map_get_chunk_pos(uint32_t tile_x, uint32_t tile_y,
                                                        uint32_t tile_z)
{
	Tile_ChunkPosition result;

	result.chunk_x = tile_x >> CHUNK_SHIFT_BIT;
	result.chunk_y = tile_y >> CHUNK_SHIFT_BIT;
	result.chunk_z = tile_z;
	result.tile_x = tile_x & CHUNK_MASK;
	result.tile_y = tile_y & CHUNK_MASK;

	return result;
}

static inline Tile_Chunk *tile_map_get_chunk(Tile_Map *map, uint32_t chunk_x, uint32_t chunk_y,
                                             uint32_t chunk_z)
{
	Tile_Chunk *result = nullptr;

	if (chunk_x < MAP_SIDE_X_CHK && chunk_y < MAP_SIDE_Y_CHK && chunk_z < MAP_SIDE_Z_CHK) {
		result =
			&map->chunks[chunk_z * MAP_SIZE_XY_CHK + chunk_y * MAP_SIDE_X_CHK + chunk_x];
	}

	return result;
}

static inline uint32_t tile_map_get_tile_value(Tile_Map *map, uint32_t tile_x, uint32_t tile_y,
                                               uint32_t tile_z)
{
	uint32_t tile_value = 0;

	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(tile_x, tile_y, tile_z);
	Tile_Chunk *chunk = tile_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	if (!chunk || !chunk->tiles) {
		return tile_value;
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tile_value = chunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x];

	return tile_value;
}

static uint8_t tile_map_correct_position(Tile_Position *pos)
{
	uint8_t was_success = tile_map_correct_coord(&pos->tile_x, &pos->offset_x_m);
	if (!was_success) {
		return was_success;
	}

	was_success = tile_map_correct_coord(&pos->tile_y, &pos->offset_y_m);

	return was_success;
}

static uint8_t tile_map_is_point_walkable(Tile_Map *map, Tile_Position pos)
{
	uint32_t tile_value = tile_map_get_tile_value(map, pos.tile_x, pos.tile_y, pos.tile_z);
	uint8_t is_walkable = tile_value == TILE_TYPE_EMPTY || tile_value == TILE_TYPE_STAIRS_UP ||
	                      tile_value == TILE_TYPE_STAIRS_DOWN;

	return is_walkable;
}

static void tile_map_set_tile_value(Tile_Map *map, Game_Arena *arena, uint32_t tile_x,
                                    uint32_t tile_y, uint32_t tile_z, uint32_t tile_value)
{
	Tile_ChunkPosition cpos = tile_map_get_chunk_pos(tile_x, tile_y, tile_z);
	Tile_Chunk *tilechunk = tile_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	assert(tilechunk);

	if (!tilechunk->tiles) {
		tilechunk->tiles =
			game_arena_push_array(arena, (size_t)CHUNK_SIZE_TL, sizeof(uint32_t));
		for (uint32_t tile_idx = 0; tile_idx < CHUNK_SIZE_TL; ++tile_idx) {
			tilechunk->tiles[tile_idx] = TILE_TYPE_EMPTY;
		}
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tilechunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x] = tile_value;
}
