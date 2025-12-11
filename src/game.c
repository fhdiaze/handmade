/*
* Game api implementation
*/

#include "game.h"
#include <limits.h>
#include <stdint.h>

static void game_render_weird_gradient(struct Game_OffScreenBuffer *buffer, long blue_offset,
                                       long green_offset)
{
	uint8_t *row = (uint8_t *)buffer->memory;
	for (long y = 0; y < buffer->height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (long x = 0; x < buffer->width; ++x) {
			// Little endian in memory  B G R X -> because of the endianess
			// little endian on a register: 0xXXRRGGBB
			uint8_t blue = (uint8_t)(x + blue_offset);
			uint8_t green = (uint8_t)(y + green_offset);
			*pixel = (uint32_t)(green << CHAR_BIT) | blue;
			++pixel;
		}
		row += buffer->pitch;
	}
}

void game_update_and_render(struct Game_OffScreenBuffer *buffer, long blue_offset,
                            long green_offset)
{
	game_render_weird_gradient(buffer, blue_offset, green_offset);
}
