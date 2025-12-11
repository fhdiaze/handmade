// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

// Game services

struct Game_OffScreenBuffer {
	void *memory;
	long width;
	long height;
	long pitch; // size of a row in bytes
};

/*
* Render
*/
void game_update_and_render(struct Game_OffScreenBuffer *buffer, long blue_offset,
                            long green_offset);

// Platform services

#endif // GAME_H
