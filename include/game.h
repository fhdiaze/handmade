// clang-format Language: C

#ifndef GAME_H
#define GAME_H

#include <assert.h>
#include <stdint.h>

#include "lib.h"

#define GAME_DLL_NAME "game.dll"

// =============================================================================
// Memory
// =============================================================================

typedef struct Arena {
	size_t capacity_byte;
	unsigned char *base_address;
	size_t used_byte;
} Arena;

void arena_init(Arena *restrict arena, const size_t size, unsigned char *const restrict base)
{
	arena->capacity_byte = size;
	arena->base_address = base;
	arena->used_byte = 0;
}

void *arena_push_size(Arena *arena, size_t size)
{
	assert(arena->used_byte + size <= arena->capacity_byte);

	void *result = arena->base_address + arena->used_byte;
	arena->used_byte += size;

	return result;
}

void *arena_push_array(Arena *arena, size_t count, size_t size)
{
	void *result = arena_push_size(arena, count * size);

	return result;
}

// =============================================================================
// Tile Map
// =============================================================================

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

#define MAP_GET_TILE_TYPE_BY_POS(map, pos) map_get_tile_type(map, (pos).tile_x, (pos).tile_y, (pos).tile_z)
#define MAP_IS_POSITION_WALKABLE(map, pos) map_is_tile_walkable(map, (pos).tile_x, (pos).tile_y, (pos).tile_z)

#define MAP_ARE_SAME_TILE(one_position, other_position)                                                          \
	((one_position).tile_x == (other_position).tile_x && (one_position).tile_y == (other_position).tile_y && \
	 (one_position).tile_z == (other_position).tile_z)

typedef enum TileType : uint32_t {
	TILE_TYPE_NONE,
	TILE_TYPE_EMPTY,
	TILE_TYPE_WALL,
	TILE_TYPE_STAIRS_UP,
	TILE_TYPE_STAIRS_DOWN,
} TileType;

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

typedef struct TileChunk {
	uint32_t *tiles;
} TileChunk;

/**
 * @brief Origin of the map is bottom-left corner of the screen
 */
typedef struct Map {
	/**
	 * @brief Chunks are laid out in memory with z as the outermost dimension, then y, then x
	 */
	TileChunk *chunks;
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
	 * @brief Offset vector relative to the center of the tile in meters
	 */
	Vtwo tile_offset_m;
} Position;

typedef struct PositionDelta {
	// Delta on x and y axis in meters
	Vtwo delta_xy_m;

	float delta_z_m;
} PositionDelta;

/**
 * @brief Normalizes a single axis coordinate so the tile offset stays within [-TILE_RADIUS_M, TILE_RADIUS_M].
 *
 * Converts any whole-tile excess in @p tile_offset_f into discrete tile steps and adds them to @p tile.
 * The world is toroidal, so tile indices wrap around without bounds checking.
 *
 * @param tile          Pointer to the tile index on one axis. Updated in place by the number of whole tiles
 *                      spanned by the offset.
 * @param tile_offset_f Pointer to the sub-tile offset in meters from the center of the tile. Reduced in place
 *                      to the remainder after whole tiles are extracted.
 * @return Always 1 (success).
 */
static inline uint32_t map_normalize_coord(uint32_t *tile, float *tile_offset_f)
{
	int tile_offset = float_round_to_int(*tile_offset_f / TILE_SIDE_M);

	// World is toroidal
	*tile = (unsigned)((int)*tile + tile_offset);

	*tile_offset_f -= (float)(tile_offset)*TILE_SIDE_M;

	assert(*tile_offset_f <= TILE_RADIUS_M);
	assert(*tile_offset_f >= -TILE_RADIUS_M);

	return 1U;
}

static inline ChunkPosition map_get_chunk_pos(uint32_t tile_x, uint32_t tile_y, uint32_t tile_z)
{
	ChunkPosition result;

	result.chunk_x = tile_x >> CHUNK_SHIFT_BIT;
	result.chunk_y = tile_y >> CHUNK_SHIFT_BIT;
	result.chunk_z = tile_z;
	result.tile_x = tile_x & CHUNK_MASK;
	result.tile_y = tile_y & CHUNK_MASK;

	return result;
}

static inline TileChunk *map_get_chunk(Map *map, uint32_t chunk_x, uint32_t chunk_y, uint32_t chunk_z)
{
	TileChunk *result = nullptr;

	if (chunk_x < MAP_SIDE_X_CHK && chunk_y < MAP_SIDE_Y_CHK && chunk_z < MAP_SIDE_Z_CHK) {
		result = &map->chunks[chunk_z * MAP_SIZE_XY_CHK + chunk_y * MAP_SIDE_X_CHK + chunk_x];
	}

	return result;
}

/**
 * @brief Gets the tile type id
 *
 * @param map
 * @param tile_x
 * @param tile_y
 * @param tile_z
 * @return uint32_t
 */
static inline TileType map_get_tile_type(Map *map, uint32_t tile_x, uint32_t tile_y, uint32_t tile_z)
{
	TileType tile_type = TILE_TYPE_NONE;

	ChunkPosition cpos = map_get_chunk_pos(tile_x, tile_y, tile_z);
	TileChunk *chunk = map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	if (!chunk || !chunk->tiles) {
		return tile_type;
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tile_type = chunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x];

	return tile_type;
}

/**
 * @brief Normalizes a full 2D map position so both axis offsets stay within [-TILE_RADIUS_M, TILE_RADIUS_M].
 *
 * Calls map_normalize_coord on the x and y axes in sequence. Any whole-tile excess in the tile offsets is
 * folded back into the discrete tile indices. The z axis and tile_z are left unchanged.
 *
 * @param pos Position to normalize in place.
 * @return 1 on success, 0 if normalization of either axis fails.
 */
static uint32_t map_normalize_position(Position *pos)
{
	uint32_t was_success = map_normalize_coord(&pos->tile_x, &pos->tile_offset_m.x);
	if (!was_success) {
		return was_success;
	}

	was_success = map_normalize_coord(&pos->tile_y, &pos->tile_offset_m.y);

	return was_success;
}

static inline uint32_t tile_is_walkable(TileType tile_type)
{
	uint32_t is_walkable = tile_type == TILE_TYPE_EMPTY || tile_type == TILE_TYPE_STAIRS_UP ||
	                       tile_type == TILE_TYPE_STAIRS_DOWN;

	return is_walkable;
}

static uint32_t map_is_tile_walkable(Map *map, uint32_t tile_x, uint32_t tile_y, uint32_t tile_z)
{
	TileType tile_type = map_get_tile_type(map, tile_x, tile_y, tile_z);
	uint32_t is_walkable = tile_is_walkable(tile_type);

	return is_walkable;
}

static void map_set_tile_value(Map *map, Arena *arena, uint32_t tile_x, uint32_t tile_y, uint32_t tile_z,
                               TileType tile_type)
{
	ChunkPosition cpos = map_get_chunk_pos(tile_x, tile_y, tile_z);
	TileChunk *tilechunk = map_get_chunk(map, cpos.chunk_x, cpos.chunk_y, cpos.chunk_z);

	assert(tilechunk);

	if (!tilechunk->tiles) {
		tilechunk->tiles = arena_push_array(arena, (size_t)CHUNK_SIZE_TL, sizeof(uint32_t));
		for (uint32_t tile_idx = 0; tile_idx < CHUNK_SIZE_TL; ++tile_idx) {
			tilechunk->tiles[tile_idx] = TILE_TYPE_EMPTY;
		}
	}

	assert(cpos.tile_x < CHUNK_SIDE_TL);
	assert(cpos.tile_y < CHUNK_SIDE_TL);

	tilechunk->tiles[cpos.tile_y * CHUNK_SIDE_TL + cpos.tile_x] = tile_type;
}

/**
 * @brief Calculates end_position - start_position
 *
 * @param start_position
 * @param end_position
 * @return PositionDelta
 */
static PositionDelta position_substract(Position *start_position, Position *end_position)
{
	PositionDelta result = {};

	Vtwo delta_xy_tile = {
		.x = (float)end_position->tile_x - (float)start_position->tile_x,
		.y = (float)end_position->tile_y - (float)start_position->tile_y,
	};
	float delta_tile_z = (float)end_position->tile_z - (float)start_position->tile_z;

	Vtwo delta_xy_m = vtwo_scale(delta_xy_tile, TILE_SIDE_M);

	Vtwo delta_tile_offset_m = vtwo_sub(end_position->tile_offset_m, start_position->tile_offset_m);
	delta_xy_m = vtwo_add(delta_xy_m, delta_tile_offset_m);

	result.delta_xy_m = delta_xy_m;
	result.delta_z_m = delta_tile_z * TILE_SIDE_M;

	return result;
}

// =============================================================================
// Input
// =============================================================================

#define MAX_MOUSE_BUTTONS 5
#define MAX_CONTROLLERS 5
#define MAX_CONTROLLER_BUTTONS 12

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

	float time_delta_sec;

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

static inline ControllerState *input_get_controller(GameInput *input, size_t controller_index)
{
	assert(controller_index < MAX_CONTROLLERS);

	return &input->controllers[controller_index];
}

// =============================================================================
// Rendering
// =============================================================================

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct GameOffscreenBuffer {
	void *top_left_px;

	// width in pixels
	unsigned width_px;

	// Height in pixels
	unsigned height_px;

	// Size of a row in bytes
	unsigned pitch_bytes;
	unsigned bytes_per_pixel;
} GameOffscreenBuffer;

/**
 * @brief (0,0) is on the bottom left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct LoadedBitmap {
	/**
	 * @brief Width in pixels.
	 */
	uint32_t width_px;

	/**
	 * @brief Height in pixels.
	 */
	uint32_t height_px;

	uint32_t *bottom_left_px;
} LoadedBitmap;

typedef struct HeroBitmaps {
	// Top-left corner is the origin
	int32_t align_x_px;

	// Top-left corner is the origin
	int32_t align_y_px;

	LoadedBitmap head;
	LoadedBitmap cape;
	LoadedBitmap torso;
} HeroBitmaps;

/**
 * @brief The byte order at increasing memory addresses is (BB GB RR AA).
 * The byte order in a register (little endian) is (AA RR GG BB).
 * The pixels order is bottom-up.
 */
#pragma pack(push, 1)
typedef struct BitmapHeader {
	uint16_t file_type;
	uint32_t file_size;

	uint16_t reserved_one;
	uint16_t reserved_two;

	uint32_t offset;

	/**
	 * @brief Size of this header in bytes
	 */
	uint32_t header_size_byte;

	/**
	 * @brief Width in pixels.
	 * Negative height: Invalid. Kept for historical reasons.
	 */
	int32_t width_px;

	/**
	 * @brief Height in pixels.
	 * Positive height: the bitmap is stored bottom-up (rows stored from bottom to top, the traditional BMP layout).
	 * Negative height: the bitmap is stored top-down (rows stored from top to bottom).
	 */
	int32_t height_px;
	uint16_t planes;

	uint16_t bits_per_pixel;

	uint32_t compression;

	/**
	 * @brief Size of the bitmap in bytes
	 */
	uint32_t bitmap_size_byte;

	int32_t horz_resolution;
	int32_t vert_resolution;

	uint32_t colors_used;
	uint32_t colors_important;

	uint32_t red_mask;
	uint32_t green_mask;
	uint32_t blue_mask;
} BitmapHeader;
#pragma pack(pop)

// =============================================================================
// Audio
// =============================================================================

typedef struct GameSoundBuffer {
	unsigned samples_per_sec;
	unsigned sample_count;
	int16_t *samples;
} GameSoundBuffer;

// =============================================================================
// Game State
// =============================================================================

typedef struct World {
	Map *map;
} World;

typedef enum HeroFacingDirection : uint8_t {
	HERO_FACING_RIGHT,
	HERO_FACING_UP,
	HERO_FACING_LEFT,
	HERO_FACING_DOWN,
} HeroFacingDirection;

typedef struct GameState {
	Arena arena;
	World *world;

	Position camera_position;
	Position hero_position;
	Vtwo hero_velocity;

	LoadedBitmap backdrop;

	uint8_t hero_facing_direction;
	HeroBitmaps hero_bitmaps[4];
} GameState;

// =============================================================================
// Platform and Game API Shared
// =============================================================================

typedef struct ThreadContext {
	unsigned placeholder;
} ThreadContext;

// =============================================================================
// Platform API
// =============================================================================

#if DEBUG

#define MEMORY_BASE_ADDRESS ((void *)TB_TO_BYTES(2))

typedef struct ReadFileResult {
	size_t size_byte;
	void *base_address;
} ReadFileResult;

#define FILE_READ_DEBUG(name) ReadFileResult name(const char *const filename, ThreadContext *thread)
#define FILE_FREE_DEBUG(name) void name(void *memory, ThreadContext *thread)
#define FILE_WRITE_DEBUG(name) \
	uint8_t name(const char *const filename, size_t memorysize, void *memory, ThreadContext *thread)

typedef FILE_READ_DEBUG(file_read_debug_func);

typedef FILE_FREE_DEBUG(file_free_debug_func);

typedef FILE_WRITE_DEBUG(file_write_debug_func);

#else
#define MEMORY_BASE_ADDRESS (nullptr)
#endif // DEBUG

// =============================================================================
// Game API
// =============================================================================

typedef struct Storage {
	size_t permanent_storage_size_byte; // permanent storage in bytes
	void *permanent_storage;            // This should be zero initialized

	size_t transient_storage_size_byte; // transient storage in bytes
	void *transient_storage;            // This should be zero initialized

	file_free_debug_func *plat_file_free_debug;
	file_read_debug_func *plat_file_read_debug;
	file_write_debug_func *file_write_debug;

	uint8_t is_initialized;
} Storage;

/**
 * @brief Updates the game status and renders it
 */
#define GAME_UPDATE_AND_RENDER(name) \
	void name(GameOffscreenBuffer *back_buffer, ThreadContext *thread, Storage *Storage, GameInput *input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

#define SOUND_CREATE_SAMPLES(name) void name(GameSoundBuffer *soundbuff, ThreadContext *thread, Storage *memory)
typedef SOUND_CREATE_SAMPLES(sound_create_samples_func);

#endif // GAME_H
