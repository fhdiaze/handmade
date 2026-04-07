#ifndef TILE_H
#define TILE_H

#include <stdint.h>

#define TILE_RADIUS_PXS 30
#define TILE_RADIUS_MTS 0.7F

#define TILE_SIDE_PXS (TILE_RADIUS_PXS * 2UL)
#define TILE_SIDE_MTS (TILE_RADIUS_MTS * 2.0F)
#define PXS_PER_MTR ((float)TILE_RADIUS_PXS / TILE_RADIUS_MTS)

#define CHUNK_SHIFT_BITS 4UL
#define CHUNK_SIDE_TLS (1UL << CHUNK_SHIFT_BITS)
#define CHUNK_MASK (CHUNK_SIDE_TLS - 1)
#define CHUNK_TILES_COUNT (CHUNK_SIDE_TLS * CHUNK_SIDE_TLS)

/**
* @brief Map side in tilechunks
*/
#define MAP_CHUNKS_COUNT_X 128
#define MAP_CHUNKS_COUNT_Y 128
#define MAP_CHUNKS_COUNT_Z 2
#define MAP_CHUNKS_COUNT_XY (MAP_CHUNKS_COUNT_Y * MAP_CHUNKS_COUNT_X)
#define MAP_CHUNKS_COUNT (MAP_CHUNKS_COUNT_XY * MAP_CHUNKS_COUNT_Z)

#define TILE_MAP_GET_TILE_VALUE_BY_POS(map, pos) \
	tile_map_get_tile_value(map, pos.tile_x, pos.tile_y, pos.tile_z)

#define TILE_MAP_ARE_SAME_TILE(one, other) \
	(one.tile_x == other.tile_x && one.tile_y == other.tile_y && one.tile_z == other.tile_z)

typedef struct Tile_ChunkPosition {
	uint32_t chunk_x;
	uint32_t chunk_y;
	uint32_t chunk_z;

	/**
	 * @brief X tile relative to the chunk
	 */
	uint32_t tile_x;

	/**
	 * @brief Y tile relative to the chunk
	 */
	uint32_t tile_y;
} Tile_ChunkPosition;

typedef struct Tile_Chunk {
	uint32_t *tiles;
} Tile_Chunk;

typedef struct Tile_Map {
	Tile_Chunk *tilechunks;
} Tile_Map;

typedef struct Tile_Position {
	uint32_t tile_x;
	uint32_t tile_y;
	uint32_t tile_z;

	/**
	 * @brief X relative to the center of the tile
	 */
	float offset_x_mts;

	/**
	 * @brief Y relative to the center of the tile
	 *
	 */
	float offset_y_mts;
} Tile_Position;

typedef enum Tile_Type : uint32_t {
	TILE_TYPE_NONE = 0,
	TILE_TYPE_WALL = 1,
	TILE_TYPE_EMPTY = 2,
	TILE_TYPE_STAIRS_UP = 3,
	TILE_TYPE_STAIRS_DOWN = 4,
} Tile_Type;

#endif // TILE_H
