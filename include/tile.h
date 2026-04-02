// clang-format Language: C
#ifndef TILE_H
#define TILE_H

#include <stdint.h>

typedef struct Tile_ChunkPosition {
	uint32_t chunk_x;
	uint32_t chunk_y;

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
	uint16_t chunk_shift_bits;
	uint32_t chunk_mask;

	/**
	 * @brief Chunk side in tiles
	 */
	uint16_t chunk_side_tls;

	uint32_t tile_radius_pxs;
	float tile_radius_mts;

	uint32_t tile_side_pxs;
	float tile_side_mts;

	float pxs_per_mtr;

	/**
	 * @brief Side of the world in tilechunks
	 */
	uint32_t side_tcs;

	Tile_Chunk *tilechunks;

} Tile_Map;

typedef struct Tile_Position {
	uint32_t tile_x;
	uint32_t tile_y;

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
} Tile_Type;

#endif // TILE_H
