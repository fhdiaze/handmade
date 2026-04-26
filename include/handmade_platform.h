// clang-format Language: C

#ifndef HANDMADE_PLATFORM_H
#define HANDMADE_PLATFORM_H

#include <stdint.h>

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#ifdef _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif

#ifdef DEBUG
#define BASE_ADDRESS ((void *)TB_TO_BYTES(2))
#else
#define BASE_ADDRESS (nullptr)
#endif // DEBUG

/**
 * @brief The byte order at increasing memory addresses is (BB GB RR AA).
 * The byte order in a register (little endian) is (AA RR GG BB).
 * The pixels order is bottom-up.
 */
#pragma pack(push, 1)
typedef struct Plat_BitmapHeader {
	uint16_t file_type;
	uint32_t file_size;

	uint16_t reserved_one;
	uint16_t reserved_two;

	uint32_t offset;

	/**
	 * @brief Size of this header in bytes
	 */
	uint32_t header_size_byte;

	int32_t width_px;
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
} Plat_BitmapHeader;
#pragma pack(pop)

/**
 * @brief (0,0) is on the bottom left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct Plat_LoadedBitmap {
	// in pixels
	int32_t width_px;
	// in pixels
	int32_t height_px;

	uint32_t *bottom_left_px;
} Plat_LoadedBitmap;

typedef struct Plat_ThreadContext {
	unsigned placeholder;
} Plat_ThreadContext;

#ifdef DEBUG

typedef struct Plat_ReadFileResult {
	size_t size_byte;
	void *base_address;
} Plat_ReadFileResult;

#define PLAT_FILE_READ_DEBUG(name) \
	Plat_ReadFileResult name(const char *const filename, Plat_ThreadContext *thread)
typedef PLAT_FILE_READ_DEBUG(plat_file_read_debug_func);

#define PLAT_FILE_FREE_DEBUG(name) void name(void *memory, Plat_ThreadContext *thread)
typedef PLAT_FILE_FREE_DEBUG(plat_file_free_debug_func);

#define PLAT_FILE_WRITE_DEBUG(name)                                               \
	uint8_t name(const char *const filename, size_t memorysize, void *memory, \
	             Plat_ThreadContext *thread)
typedef PLAT_FILE_WRITE_DEBUG(plat_file_write_debug_func);

#endif // DEBUG

typedef struct Plat_Memory {
	size_t permanent_storage_size_byte; // permanent storage in bytes
	void *permanent_storage;            // This should be zero initialized

	size_t transient_storage_size_byte; // transient storage in bytes
	void *transient_storage;            // This should be zero initialized

	plat_file_free_debug_func *plat_file_free_debug;
	plat_file_read_debug_func *plat_file_read_debug;
	plat_file_write_debug_func *plat_file_write_debug;

	uint8_t is_initialized;
} Plat_Memory;

typedef struct Plat_Arena {
	size_t capacity_byte;
	unsigned char *base_address;
	size_t used_byte;
} Plat_Arena;

void plat_arena_init(Plat_Arena *arena, size_t size, unsigned char *base);

void *plat_arena_push_size(Plat_Arena *arena, size_t size);

void *plat_arena_push_array(Plat_Arena *arena, size_t count, size_t size);

#endif // HANDMADE_PLATFORM_H
