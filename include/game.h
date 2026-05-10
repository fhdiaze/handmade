// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <assert.h>
#include <stdint.h>

#include "lib.h"
#include "platform.h"

#define GAME_DLL_NAME "game.dll"

#define TILE_RADIUS_PX 30
#define TILE_RADIUS_M 0.7F

#define TILE_SIDE_PX (TILE_RADIUS_PX * 2UL)

/**
 * @brief Side of a tile in meters
 */
#define TILE_SIDE_M (TILE_RADIUS_M * 2.0F)

#define TILE_PIXELS_PER_METER ((float)TILE_RADIUS_PX / TILE_RADIUS_M)
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

#define MAP_GET_TILE_VALUE_BY_POS(map, pos) \
	game_map_get_tile_value(map, pos.tile_x, pos.tile_y, pos.tile_z)

#define MAP_ARE_SAME_TILE(one_position, other_position)  \
	(one_position.tile_x == other_position.tile_x && \
	 one_position.tile_y == other_position.tile_y && \
	 one_position.tile_z == other_position.tile_z)

#define MAX_MOUSE_BUTTONS 5
#define MAX_CONTROLLERS 5
#define MAX_CONTROLLER_BUTTONS 12

typedef struct ChunkPosition {
	/**
	 * @brief Increases towards the right of the screen
	 */
	uint32_t chunk_x;

	/**
	 * @brief Increases towards the top screen
	 */
	uint32_t chunk_y;

	/**
	 * @brief Increases towards the player or user
	 */
	uint32_t chunk_z;

	/**
	 * @brief X tile relative to the chunk
	 */
	uint32_t tile_x;

	/**
	 * @brief Y tile relative to the chunk
	 */
	uint32_t tile_y;
} ChunkPosition;

typedef struct TilesChunk {
	uint32_t *tiles;
} TilesChunk;

/**
 * @brief Origin of the map is bottom-left corner of the screen
 */
typedef struct Map {
	/**
	 * @brief Chunks are laid out in memory with z as the outermost dimension, then y, then x
	 */
	TilesChunk *chunks;
} Map;

typedef struct Position {
	/**
	 * @brief Increases towards the right of the screen
	 */
	uint32_t tile_x;

	/**
	 * @brief Increases towards the top screen
	 */
	uint32_t tile_y;

	/**
	 * @brief Increases towards the player or user
	 */
	uint32_t tile_z;

	/**
	 * @brief X relative to the center of the tile in meters
	 */
	float offset_x_m;

	/**
	 * @brief Y relative to the center of the tile in meters
	 */
	float offset_y_m;
} Position;

typedef struct PositionDelta {
	// Delta on x axis in meters
	float delta_x_m;
	float delta_y_m;
	float delta_z_m;
} PositionDelta;

typedef enum TileType : uint32_t {
	TILE_TYPE_NONE = 0,
	TILE_TYPE_EMPTY = 1,
	TILE_TYPE_WALL = 2,
	TILE_TYPE_STAIRS_UP = 3,
	TILE_TYPE_STAIRS_DOWN = 4,
} TileType;

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct GameBitmap {
	void *top_left_px;

	// width in pixels
	unsigned width_px;

	// Height in pixels
	unsigned height_px;

	// Size of a row in bytes
	unsigned pitch_bytes;
	unsigned bytes_per_pixel;
} GameBitmap;

typedef struct GameSoundBuffer {
	unsigned samples_per_sec;
	unsigned sample_count;
	int16_t *samples;
} GameSoundBuffer;

typedef struct ButtonState {
	// half transition count per frame
	unsigned half_transition_count;
	uint8_t ended_down;
} ButtonState;

typedef struct ControllerState {
	float stick_avg_x;
	float stick_avg_y;

	union {
		ButtonState buttons[MAX_CONTROLLER_BUTTONS];
		struct {
			ButtonState moveup;
			ButtonState movedown;
			ButtonState moveleft;
			ButtonState moveright;

			ButtonState actionup;
			ButtonState actiondown;
			ButtonState actionleft;
			ButtonState actionright;

			ButtonState left_shoulder;
			ButtonState right_shoulder;

			ButtonState start;
			ButtonState back;
		};
	};

	// TODO(fredy): bools in structs are suspicious
	uint8_t is_analog;
	uint8_t is_connected;
} ControllerState;

typedef struct GameInput {
	unsigned mouse_x;
	unsigned mouse_y;
	unsigned mouse_z; // mouse wheel

	float secs_time_delta;

	union {
		ButtonState mouse_buttons[MAX_MOUSE_BUTTONS];

		struct {
			ButtonState mouse_main;
			ButtonState mouse_middle;
			ButtonState mouse_secondary;
			ButtonState mouse_back;
			ButtonState mouse_forward;
		};
	};

	ControllerState controllers[MAX_CONTROLLERS];
} GameInput;

typedef struct World {
	Map *map;
} World;

typedef struct HeroBitmaps {
	// Top-left corner is the origin
	int32_t align_x_px;

	// Top-left corner is the origin
	int32_t align_y_px;

	LoadedBitmap head;
	LoadedBitmap cape;
	LoadedBitmap torso;
} HeroBitmaps;

typedef struct GameState {
	Arena arena;
	World *world;
	Position camera_position;
	Position hero_position;

	LoadedBitmap backdrop;

	uint8_t hero_facing_direction;
	HeroBitmaps hero_bitmaps[4];
} GameState;

// Utilities

static inline ControllerState *game_input_get_controller(GameInput *input, size_t controller_index)
{
	assert(controller_index < MAX_CONTROLLERS);

	return &input->controllers[controller_index];
}

// Game services

static inline uint8_t game_map_correct_coord(uint32_t *tile, float *tile_rel)
{
	int tile_offset = lib_float_round_to_int(*tile_rel / TILE_SIDE_M);

	// World is toroidal
	*tile = (unsigned)((int)*tile + tile_offset);

	*tile_rel -= (float)(tile_offset)*TILE_SIDE_M;

	assert(*tile_rel <= TILE_RADIUS_M);
	assert(*tile_rel >= -TILE_RADIUS_M);

	return 1U;
}

static inline ChunkPosition game_map_get_chunk_pos(uint32_t tile_x, uint32_t tile_y,
                                                   uint32_t tile_z)
{
	ChunkPosition result;

	result.chunk_x = tile_x >> CHUNK_SHIFT_BIT;
	result.chunk_y = tile_y >> CHUNK_SHIFT_BIT;
	result.chunk_z = tile_z;
	result.tile_x = tile_x & CHUNK_MASK;
	result.tile_y = tile_y & CHUNK_MASK;

	return result;
}

static inline TilesChunk *game_map_get_chunk(Map *map, uint32_t chunk_x, uint32_t chunk_y,
                                             uint32_t chunk_z)
{
	TilesChunk *result = nullptr;

	if (chunk_x < MAP_SIDE_X_CHK && chunk_y < MAP_SIDE_Y_CHK && chunk_z < MAP_SIDE_Z_CHK) {
		result =
			&map->chunks[chunk_z * MAP_SIZE_XY_CHK + chunk_y * MAP_SIDE_X_CHK + chunk_x];
	}

	return result;
}

static inline uint32_t game_map_get_tile_value(Map *map, uint32_t tile_x, uint32_t tile_y,
                                               uint32_t tile_z)
{
	uint32_t tile_value = 0;

	ChunkPosition cpos = game_map_get_chunk_pos(tile_x, tile_y, tile_z);
	TilesChunk *chunk = game_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	if (!chunk || !chunk->tiles) {
		return tile_value;
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tile_value = chunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x];

	return tile_value;
}

static uint8_t game_map_correct_position(Position *pos)
{
	uint8_t was_success = game_map_correct_coord(&pos->tile_x, &pos->offset_x_m);
	if (!was_success) {
		return was_success;
	}

	was_success = game_map_correct_coord(&pos->tile_y, &pos->offset_y_m);

	return was_success;
}

static uint8_t game_map_is_point_walkable(Map *map, Position pos)
{
	uint32_t tile_value = game_map_get_tile_value(map, pos.tile_x, pos.tile_y, pos.tile_z);
	uint8_t is_walkable = tile_value == TILE_TYPE_EMPTY || tile_value == TILE_TYPE_STAIRS_UP ||
	                      tile_value == TILE_TYPE_STAIRS_DOWN;

	return is_walkable;
}

static void game_map_set_tile_value(Map *map, Arena *arena, uint32_t tile_x, uint32_t tile_y,
                                    uint32_t tile_z, uint32_t tile_value)
{
	ChunkPosition cpos = game_map_get_chunk_pos(tile_x, tile_y, tile_z);
	TilesChunk *tilechunk = game_map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	assert(tilechunk);

	if (!tilechunk->tiles) {
		tilechunk->tiles = arena_push_array(arena, (size_t)CHUNK_SIZE_TL, sizeof(uint32_t));
		for (uint32_t tile_idx = 0; tile_idx < CHUNK_SIZE_TL; ++tile_idx) {
			tilechunk->tiles[tile_idx] = TILE_TYPE_EMPTY;
		}
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tilechunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x] = tile_value;
}

static PositionDelta game_map_substract_positions(Position *start_position, Position *end_position)
{
	PositionDelta result = {};

	float delta_tile_x = (float)end_position->tile_x - (float)start_position->tile_x;
	float delta_tile_y = (float)end_position->tile_y - (float)start_position->tile_y;
	float delta_tile_z = (float)end_position->tile_z - (float)start_position->tile_z;

	result.delta_x_m =
		delta_tile_x * TILE_SIDE_M + end_position->offset_x_m - start_position->offset_x_m;
	result.delta_y_m =
		delta_tile_y * TILE_SIDE_M + end_position->offset_y_m - start_position->offset_y_m;
	result.delta_z_m = delta_tile_z * TILE_SIDE_M;

	return result;
}

/**
 * @brief Updates the game status and renders it
 */
#define GAME_UPDATE_AND_RENDER(name)                                                 \
	void name(GameBitmap *bitmap, ThreadContext *thread, GameMemory *GameMemory, \
	          GameInput *input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define SOUND_CREATE_SAMPLES(name) \
	void name(GameSoundBuffer *soundbuff, ThreadContext *thread, GameMemory *memory)
typedef SOUND_CREATE_SAMPLES(sound_create_samples_func);

#endif // GAME_H
