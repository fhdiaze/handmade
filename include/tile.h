// clang-format Language: C
#ifndef TILE_H
#define TILE_H

#include <stdint.h>

#define TILE_RADIUS_PXS 30
#define TILE_RADIUS_MTS 0.7F

#define TILE_SIDE_PXS (TILE_RADIUS_PXS * 2UL)
#define TILE_SIDE_MTS (TILE_RADIUS_MTS * 2.0F)
#define PXS_PER_MTR (float)TILE_RADIUS_PXS / TILE_RADIUS_MTS

#define CHUNK_SHIFT_BITS 4UL
#define CHUNK_SIDE_TLS (1UL << CHUNK_SHIFT_BITS)
#define CHUNK_MASK CHUNK_SIDE_TLS - 1
#define CHUNK_TILES_COUNT (CHUNK_SIDE_TLS * CHUNK_SIDE_TLS)

/**
* @brief Map side in tilechunks
*/
#define MAP_CHUNKS_COUNT_X 128
#define MAP_CHUNKS_COUNT_Y 128
#define MAP_CHUNKS_COUNT_Z 2
#define MAP_CHUNKS_COUNT_XY (MAP_CHUNKS_COUNT_Y * MAP_CHUNKS_COUNT_X)
#define MAP_CHUNKS_COUNT (MAP_CHUNKS_COUNT_XY * MAP_CHUNKS_COUNT_Z)

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
	float tile_rel_x_mts;

	/**
	 * @brief Y relative to the center of the tile
	 *
	 */
	float tile_rel_y_mts;
} Tile_Position;

typedef enum Tile_Type : uint32_t {
	TILE_TYPE_EMPTY = 0,
	TILE_TYPE_WALKABLE = 1,
	TILE_TYPE_WALL = 2,
	TILE_TYPE_STAIRS_UP = 3,
	TILE_TYPE_STAIRS_DOWN = 4,
} Tile_Type;

#endif // TILE_H
