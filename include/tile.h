#ifndef TILE_H
#define TILE_H

#include <stdint.h>

#define TILE_RADIUS_PX 30
#define TILE_RADIUS_M 0.7F

#define TILE_SIDE_PX (TILE_RADIUS_PX * 2UL)
/**
 * @brief Side of a tile in meters
 */
#define TILE_SIDE_M (TILE_RADIUS_M * 2.0F)
#define PIXELS_PER_METER ((float)TILE_RADIUS_PX / TILE_RADIUS_M)
#define CHUNK_SHIFT_BIT 4UL

/**
 * @brief Size of a chunk in tiles
 */
#define CHUNK_SIDE_TL (1UL << CHUNK_SHIFT_BIT)
#define CHUNK_MASK (CHUNK_SIDE_TL - 1)
#define CHUNK_SIZE_TL (CHUNK_SIDE_TL * CHUNK_SIDE_TL)

/**
* @brief Map side in tile chunks
*/
#define MAP_SIDE_X_CHK 128
#define MAP_SIDE_Y_CHK 128
#define MAP_SIDE_Z_CHK 2
#define MAP_SIZE_XY_CHK (MAP_SIDE_Y_CHK * MAP_SIDE_X_CHK)
#define MAP_SIZE_CHK (MAP_SIZE_XY_CHK * MAP_SIDE_Z_CHK)

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
	Tile_Chunk *chunks;
} Tile_Map;

typedef struct Tile_Position {
	uint32_t tile_x;
	uint32_t tile_y;
	uint32_t tile_z;

	/**
	 * @brief X relative to the center of the tile in meters
	 */
	float offset_x_m;

	/**
	 * @brief Y relative to the center of the tile in meters
	 *
	 */
	float offset_y_m;
} Tile_Position;

typedef enum Tile_Type : uint32_t {
	TILE_TYPE_NONE = 0,
	TILE_TYPE_EMPTY = 1,
	TILE_TYPE_WALL = 2,
	TILE_TYPE_STAIRS_UP = 3,
	TILE_TYPE_STAIRS_DOWN = 4,
} Tile_Type;

#endif // TILE_H
