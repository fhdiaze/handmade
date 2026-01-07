// clang-format Language: C

/*
* Game api definition
*/

#ifndef GAME_H
#define GAME_H

#include <assert.h>
#include <stdint.h>

#define KB_TO_BYTE(_pr_v) (_pr_v * 1024ull)
#define MB_TO_BYTE(_pr_v) (KB_TO_BYTE(_pr_v) * 1024ull)
#define GB_TO_BYTE(_pr_v) (MB_TO_BYTE(_pr_v) * 1024ull)
#define TB_TO_BYTE(_pr_v) (GB_TO_BYTE(_pr_v) * 1024ull)

/**
 * @brief Calculates the distance between two indexes in a ring buffer
 */
#define RING_DIFF(size, start, end) ((end) >= (start) ? (end) - (start) : (size) - (start) + (end))

/**
 * @brief Calculates the complement to the ring size for a value
 */
#define RING_COMPLEMENT(size, value) ((size) - (value))

/**
 * @brief Adds a positive offset to a index in a ring buffer
 */
#define RING_ADD(size, index, offset) (((index) + (offset)) % (size))

/**
 * @brief Subtracts a positive offset to a index in a ring buffer
 */
#define RING_SUB(size, index, offset) (((index) + RING_COMPLEMENT((size), (offset))) % (size))

/**
 * @brief Checks if an index is between start and end indexes in a ring buffer
 */
#define RING_BETWEEN(start, end, test)                               \
	((end) >= (start) ? ((test) >= (start) && (test) <= (end)) : \
	                    ((test) >= (start) || (test) <= (end)))

#ifdef DEBUG
#define BASE_ADDRESS ((void *)TB_TO_BYTE(2))
#else
#define BASE_ADDRESS (nullptr)
#endif // DEBUG

static constexpr float PIE = 3.14159265359f;
static constexpr unsigned GAME_MAX_CONTROLLERS = 5;
static constexpr unsigned GAME_MAX_BUTTONS = 12;

typedef struct Game_OffScreenBuffer {
	void *memory;
	unsigned width;
	unsigned height;
	unsigned pitch_bytes; // size of a row in bytes
} Game_OffScreenBuffer;

typedef struct Game_SoundBuffer {
	unsigned samples_per_sec;
	unsigned sample_count;
	int16_t *samples;
} Game_SoundBuffer;

typedef struct Game_ButtonState {
	// half transistion count per frame
	unsigned half_transition_count;
	bool ended_down;
} Game_ButtonState;

typedef struct Game_ControllerInput {
	float stick_avg_x;
	float stick_avg_y;

	union {
		Game_ButtonState buttons[GAME_MAX_BUTTONS];
		struct {
			Game_ButtonState moveup;
			Game_ButtonState movedown;
			Game_ButtonState moveleft;
			Game_ButtonState moveright;

			Game_ButtonState actionup;
			Game_ButtonState actiondown;
			Game_ButtonState actionleft;
			Game_ButtonState actionright;

			Game_ButtonState left_shoulder;
			Game_ButtonState right_shoulder;

			Game_ButtonState start;
			Game_ButtonState back;
		};
	};

	// TODO(fredy): bools in structs are suspicious
	bool is_analog;
	bool is_connected;
} Game_ControllerInput;

typedef struct Game_Input {
	Game_ControllerInput controllers[GAME_MAX_CONTROLLERS];
} Game_Input;

typedef struct Game_State {
	unsigned tonehz;
	unsigned blue_offset;
	unsigned green_offset;
} Game_State;

typedef struct Game_Memory {
	size_t permsize; // permanent storage in bytes
	void *permstorage; // This should be zero initialized

	size_t transize; // transient storage in bytes
	void *transtorage; // This should be zero initialized

	bool is_initialized;
} Game_Memory;

// Utilities

static inline Game_ControllerInput *game_input_get_controller(Game_Input *input,
                                                              size_t controller_index)
{
	assert(controller_index < GAME_MAX_CONTROLLERS);
	return &input->controllers[controller_index];
}

/**
 * @brief truncates a int64_t into a uint32_t.
 *
 * @param value
 * @return uint32_t
 */
static inline uint32_t lltoul(int64_t value)
{
	assert(value < INT32_MAX);
	assert(value >= 0);
	return (uint32_t)value;
}
// Game services

/**
 * @brief Updates the game status and renders it
 *
 * @param screenbuff
 * @param soundbuff
 */
void game_update_and_render(Game_Memory *memory, Game_Input *input,
                            Game_OffScreenBuffer *screenbuff);

void game_sound_create_samples(Game_Memory *memory, Game_SoundBuffer *soundbuff);

// Platform services

#ifdef DEBUG

typedef struct Plat_ReadFileResult {
	size_t size;
	void *memory;
} Plat_ReadFileResult;

Plat_ReadFileResult plat_debug_readfile(const char *const filename);
void plat_debug_freefile(void *memory);
bool plat_debug_writefile(const char *const filename, size_t memorysize, void *memory);

#endif // DEBUG

#endif // GAME_H
