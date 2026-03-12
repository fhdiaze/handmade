// clang-format Language: C
#ifndef TILE_H
#define TILE_H

#include <stdint.h>

typedef struct Game_ChunkPosition {
	uint32_t left_lower_tile_x;
	uint32_t left_lower_tile_y;

	/**
	 * @brief X tile relative to the lower left tile of a chunk
	 */
	uint32_t rel_tile_x;

	/**
	 * @brief Y tile relative to the lower left tile of a chunk
	 */
	uint32_t rel_tile_y;
} Game_ChunkPosition;

typedef struct Game_TileChunk {
	uint32_t *tiles;
} Game_TileChunk;

typedef struct Game_Tilemap {
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

	Game_TileChunk *tilechunks;

} Game_Tilemap;

// TODO(Fredy.Diaz): rename this as Tilemap Position
typedef struct Game_TilemapPosition {
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
} Game_TilemapPosition;

#endif // TILE_H
