#include <assert.h>

#include "handmade_platform.h"

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
