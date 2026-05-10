// clang-format Language: C

#ifndef PLATFORM_H
#define PLATFORM_H

#include <assert.h>
#include <stdint.h>

#if DEBUG
#define PLAT_BASE_ADDRESS ((void *)TB_TO_BYTES(2))
#else
#define PLAT_BASE_ADDRESS (nullptr)
#endif // DEBUG

/**
 * @brief The byte order at increasing memory addresses is (BB GB RR AA).
 * The byte order in a register (little endian) is (AA RR GG BB).
 * The pixels order is bottom-up.
 */
#pragma pack(push, 1)
typedef struct HmBitmapHeader {
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
} HmBitmapHeader;
#pragma pack(pop)

/**
 * @brief (0,0) is on the bottom left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct HmLoadedBitmap {
	/**
	 * @brief Width in pixels.
	 */
	uint32_t width_px;

	/**
	 * @brief Height in pixels.
	 */
	uint32_t height_px;

	uint32_t *bottom_left_px;
} HmLoadedBitmap;

typedef struct HmThreadContext {
	unsigned placeholder;
} HmThreadContext;

#if DEBUG

typedef struct HmReadFileResult {
	size_t size_byte;
	void *base_address;
} HmReadFileResult;

#define PLAT_FILE_READ_DEBUG(name) \
	HmReadFileResult name(const char *const filename, HmThreadContext *thread)
typedef PLAT_FILE_READ_DEBUG(plat_file_read_debug_func);

#define PLAT_FILE_FREE_DEBUG(name) void name(void *memory, HmThreadContext *thread)
typedef PLAT_FILE_FREE_DEBUG(plat_file_free_debug_func);

#define PLAT_FILE_WRITE_DEBUG(name)                                               \
	uint8_t name(const char *const filename, size_t memorysize, void *memory, \
	             HmThreadContext *thread)
typedef PLAT_FILE_WRITE_DEBUG(plat_file_write_debug_func);

#endif // DEBUG

typedef struct HmMemory {
	size_t permanent_storage_size_byte; // permanent storage in bytes
	void *permanent_storage;            // This should be zero initialized

	size_t transient_storage_size_byte; // transient storage in bytes
	void *transient_storage;            // This should be zero initialized

	plat_file_free_debug_func *plat_file_free_debug;
	plat_file_read_debug_func *plat_file_read_debug;
	plat_file_write_debug_func *plat_file_write_debug;

	uint8_t is_initialized;
} HmMemory;

typedef struct Plat_Arena {
	size_t capacity_byte;
	unsigned char *base_address;
	size_t used_byte;
} Plat_Arena;

void plat_arena_init(Plat_Arena *restrict arena, const size_t size,
                     unsigned char *const restrict base)
{
	arena->capacity_byte = size;
	arena->base_address = base;
	arena->used_byte = 0;
}

void *plat_arena_push_size(Plat_Arena *arena, size_t size)
{
	assert(arena->used_byte + size <= arena->capacity_byte);

	void *result = arena->base_address + arena->used_byte;
	arena->used_byte += size;

	return result;
}

void *plat_arena_push_array(Plat_Arena *arena, size_t count, size_t size)
{
	void *result = plat_arena_push_size(arena, count * size);

	return result;
}

#endif // PLATFORM_H
