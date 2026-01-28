/*
* Windows platform code
*/

#undef TIX_LOG_LEVEL
#define TIX_LOG_LEVEL TIX_LOG_LEVEL_DEBUG

#include "win_handmade.h"

#include "tix_log.h"
#include <dsound.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>

// globals
static bool is_global_running = false;
static bool is_global_pause = false;
static Win_Bitmap global_bitmap;
static LPDIRECTSOUNDBUFFER secbuffer;
static int64_t global_perf_count_frequency;

// macros

#define XINPUT_GET_STATE(name)                                    \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, \
	                  [[__maybe_unused__]] XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(xinput_get_state_func);
XINPUT_GET_STATE(xinput_get_state_stub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_get_state_func *xinput_get_state = xinput_get_state_stub;
#define XInputGetState xinput_get_state

#define XINPUT_SET_STATE(name)                                    \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, \
	                  [[__maybe_unused__]] XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE(xinput_set_state_func);
XINPUT_SET_STATE(xinput_set_state_stub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_set_state_func *xinput_set_state = xinput_set_state_stub;
#define XInputSetState xinput_set_state

#define DSOUND_CREATE(name) \
	HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(direct_sound_create_func);

// util

static void tix_string_concat(const size_t one_count, const char *const restrict one,
                              const size_t other_count, const char *const restrict other,
                              const size_t destsize, char *const restrict dest)
{
	for (unsigned i = 0; i < one_count; ++i) {
		dest[i] = one[i];
	}

	for (unsigned i = 0; i < other_count; ++i) {
		dest[one_count + i] = other[i];
	}

	dest[one_count + other_count] = '\0';
}

// services

PLAT_DEBUG_FREEFILE(plat_debug_freefile)
{
	if (memory) {
		VirtualFree(memory, 0, MEM_RELEASE);
	}
}

PLAT_DEBUG_READFILE(plat_debug_readfile)
{
	Plat_ReadFileResult result = {};
	HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                            0, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return result;
	}

	LARGE_INTEGER filesize_struct;
	if (!GetFileSizeEx(handle, &filesize_struct)) {
		goto error_cleanup;
	}

	uint32_t filesize = lltoul(filesize_struct.QuadPart);
	result.memory = VirtualAlloc(nullptr, filesize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!result.memory) {
		goto error_cleanup;
	}

	DWORD bytesread;
	if (!ReadFile(handle, result.memory, filesize, &bytesread, nullptr) ||
	    bytesread != filesize) {
		goto error_cleanup;
	}

	result.size = filesize;

	CloseHandle(handle);

	return result;

error_cleanup:
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
	}

	plat_debug_freefile(result.memory);

	result.memory = nullptr;
	result.size = 0;

	return result;
}

PLAT_DEBUG_WRITEFILE(plat_debug_writefile)
{
	bool result = false;
	HANDLE handle = CreateFileA(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return result;
	}

	DWORD byteswritten;
	if (!WriteFile(handle, memory, (DWORD)memorysize, &byteswritten, nullptr)) {
		goto error_cleanup;
	}

	CloseHandle(handle);

	return byteswritten == memorysize;

error_cleanup:
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
	}

	return false;
}

static bool win_file_get_exe_path(Win_State *winstate)
{
	unsigned long exe_path_length =
		GetModuleFileNameA(nullptr, winstate->exe_path, WIN_STATE_MAX_FILE_PATH);
	if (exe_path_length == 0 || exe_path_length == WIN_STATE_MAX_FILE_PATH) {
		TIX_LOGE("unable to get the executable path");
		return false;
	}

	winstate->exe_path_last_slash = winstate->exe_path + exe_path_length - 1;
	for (; winstate->exe_path_last_slash > winstate->exe_path;
	     --winstate->exe_path_last_slash) {
		if (*winstate->exe_path_last_slash == '\\') {
			break;
		}
	}

	return true;
}

static void win_file_build_path(Win_State *winstate, const char *const filename,
                                const unsigned dest_count, char *dest)
{
	tix_string_concat((size_t)(winstate->exe_path_last_slash - winstate->exe_path + 1),
	                  winstate->exe_path, strlen(filename), filename, dest_count, dest);
}

static void win_file_build_input_path(Win_State *winstate, unsigned slot_index, unsigned dest_count,
                                      char *dest)
{
	assert(slot_index == 1);
	win_file_build_path(winstate, "loopedit.hmi", dest_count, dest);
}

static inline bool win_file_get_last_write_time(const char *const filename, FILETIME *result)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
		TIX_LOGE("unable to check the timestamp of the dll");
		return false;
	}

	*result = data.ftLastWriteTime;

	return true;
}

static bool win_code_load_game(const char *const gamedll_path, const char *const tmpdll_path,
                               Win_GameCode *game_code)
{
	FILETIME dll_last_write_time = {};
	if (!win_file_get_last_write_time(gamedll_path, &dll_last_write_time)) {
		return false;
	}

	if (!CopyFileA(gamedll_path, tmpdll_path, false)) {
		DWORD error = GetLastError();
		TIX_LOGE("unable to copy the dll: '%s', error: %lu", gamedll_path, error);
		return false;
	}

	game_code->game_dll = LoadLibraryA(tmpdll_path);
	game_code->dll_write_time = dll_last_write_time;

	if (game_code->game_dll) {
		game_code->update_and_render = (game_bitmap_update_and_render_func *)GetProcAddress(
			game_code->game_dll, "game_bitmap_update_and_render");
		game_code->sound_create_samples = (game_sound_create_samples_func *)GetProcAddress(
			game_code->game_dll, "game_sound_create_samples");

		game_code->is_valid = game_code->sound_create_samples &&
		                      game_code->update_and_render;
	}

	if (!game_code->is_valid) {
		game_code->sound_create_samples = nullptr;
		game_code->update_and_render = nullptr;
	}

	return game_code->is_valid;
}

static bool win_code_unload_game(Win_GameCode *game_code)
{
	if (game_code->game_dll) {
		if (!FreeLibrary(game_code->game_dll)) {
			return false;
		}
		game_code->game_dll = nullptr;
	}

	game_code->is_valid = false;
	game_code->sound_create_samples = nullptr;
	game_code->update_and_render = nullptr;

	return true;
}

static void win_xinput_load(void)
{
	HMODULE xinput_lib = LoadLibraryA("xinput1_4.dll");

	if (!xinput_lib) {
		xinput_lib = LoadLibraryA("xinput9_1_0.dll");
	}

	if (!xinput_lib) {
		xinput_lib = LoadLibraryA("xinput1_3.dll");
	}

	if (xinput_lib) {
		XInputGetState =
			(xinput_get_state_func *)GetProcAddress(xinput_lib, "XInputGetState");
		if (!XInputGetState) {
			XInputGetState = xinput_get_state_stub;
		}
		XInputSetState =
			(xinput_set_state_func *)GetProcAddress(xinput_lib, "XInputSetState");
		if (!XInputSetState) {
			XInputSetState = xinput_set_state_stub;
		}
	}
}

/*
* TODO(fredy): it looks like direct sound is deprecated
*/
static void win_sound_init(HWND winhandle, size_t samples_per_sec, size_t buffersize)
{
	HMODULE dsound_lib = LoadLibraryA("dsound.dll");
	if (!dsound_lib) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error loading dsound.dll");
		return;
	}

	direct_sound_create_func *dsound_create =
		(direct_sound_create_func *)GetProcAddress(dsound_lib, "DirectSoundCreate");
	if (!dsound_create) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error getting DirectSoundCreate function");
		return;
	}

	LPDIRECTSOUND direct_sound;
	if (FAILED(dsound_create(nullptr, &direct_sound, nullptr))) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error creating the handler for direct sound");
		return;
	}

	// NOTE(fredy): create the buffers
	WAVEFORMATEX waveformat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = (DWORD)samples_per_sec,
		.wBitsPerSample = 16,
		.cbSize = 0,
	};
	waveformat.nBlockAlign = waveformat.nChannels * waveformat.wBitsPerSample / CHAR_BIT;
	waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;

	if (FAILED(IDirectSound_SetCooperativeLevel(direct_sound, winhandle, DSSCL_PRIORITY))) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error setting the cooperative level for direct sound");
	}

	// NOTE(fredy): create the primary buffer
	DSBUFFERDESC primbufferdesc = {
		.dwSize = sizeof(primbufferdesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER,
	};
	LPDIRECTSOUNDBUFFER primbuffer;
	if (FAILED(IDirectSound_CreateSoundBuffer(direct_sound, &primbufferdesc, &primbuffer,
	                                          nullptr))) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error creating the primary buffer (handle for the sound card)");
	}

	if (FAILED(IDirectSoundBuffer_SetFormat(primbuffer, &waveformat))) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error setting the format for the primary buffer");
	}

	// NOTE(fredy): create the secondary buffer
	DSBUFFERDESC secbufferdesc = {
		.dwSize = sizeof(secbufferdesc),
		.dwBufferBytes = (DWORD)buffersize,
		.lpwfxFormat = &waveformat,
	};
	if (FAILED(IDirectSound_CreateSoundBuffer(direct_sound, &secbufferdesc, &secbuffer,
	                                          nullptr))) {
		// TODO(fredy): diagnostic
		TIX_LOGE("Error creating the secondary buffer");
	}
}

static void win_sound_clear_buffer(Win_SoundOutput *sound_output)
{
	void *region_one;
	DWORD region_one_size;
	void *region_two;
	DWORD region_two_size;
	if (FAILED(IDirectSoundBuffer_Lock(secbuffer, 0, sound_output->buffsize, &region_one,
	                                   &region_one_size, &region_two, &region_two_size, 0))) {
		OutputDebugStringA("Error locking dsound secondary buffer");
		return;
	}

	size_t bytes_count = region_one_size;
	int8_t *byte_out = (int8_t *)region_one;
	for (size_t i = 0; i < bytes_count; ++i) {
		*byte_out = 0;
		++byte_out;
	}

	bytes_count = region_two_size;
	byte_out = (int8_t *)region_two;
	for (size_t i = 0; i < bytes_count; ++i) {
		*byte_out = 0;
		++byte_out;
	}

	if (FAILED(IDirectSoundBuffer_Unlock(secbuffer, region_one, region_one_size, region_two,
	                                     region_two_size))) {
		OutputDebugStringA("Error unlocking dsound secondary buffer");
	}
}

/**
 * @brief copy sound from game buffer to win buffer
 *
 * @param soundout windows sound output config
 * @param byte_to_lock
 * @param bytes_to_write
 * @param soundbuff game sound buffer
 */
static void win_sound_fill_buffer(Win_SoundOutput *soundout, size_t byte_to_lock,
                                  size_t bytes_to_write, Game_SoundBuffer *soundbuff)
{
	void *region_one;
	unsigned long region_one_size;
	void *region_two;
	unsigned long region_two_size;

	if (FAILED(IDirectSoundBuffer_Lock(secbuffer, byte_to_lock, bytes_to_write, &region_one,
	                                   &region_one_size, &region_two, &region_two_size, 0))) {
		OutputDebugStringA("Error locking dsound secondary buffer");
	}

	size_t region_sample_count = region_one_size / soundout->bytes_per_sample;
	int16_t *sample_out = (int16_t *)region_one;
	int16_t *sample_in = soundbuff->samples;
	for (size_t i = 0; i < region_sample_count; ++i) {
		*sample_out = *sample_in; // channel one
		++sample_out;
		++sample_in;
		*sample_out = *sample_in; // channel two
		++sample_out;
		++sample_in;

		++soundout->running_sample_index;
	}

	sample_out = (int16_t *)region_two;
	region_sample_count = region_two_size / soundout->bytes_per_sample;
	for (size_t i = 0; i < region_sample_count; ++i) {
		*sample_out = *sample_in; // channel one
		++sample_out;
		++sample_in;
		*sample_out = *sample_in; // channel two
		++sample_out;
		++sample_in;

		++soundout->running_sample_index;
	}

	if (FAILED(IDirectSoundBuffer_Unlock(secbuffer, region_one, region_one_size, region_two,
	                                     region_two_size))) {
		OutputDebugStringA("Error unlocking dsound secondary buffer");
	}
}

static void win_keyboard_process_message(Game_ButtonState *newstate, bool is_down)
{
	assert(newstate->ended_down != is_down);
	newstate->ended_down = is_down;
	++newstate->half_transition_count;
}

/**
 * @brief Normalize xinput stick value to [-1.0, 1.0]
 *
 * @param value
 * @param dead_zone
 * @return float
 */
static float win_xinput_process_stick_value(short value, short dead_zone)
{
	if (value < -dead_zone) {
		return (float)value / 32768.0f;
	}
	if (value > dead_zone) {
		return (float)value / 32767.0f;
	}

	return 0.0f;
}

/**
 * @brief Processes a button from an xinput controller
 *
 * @param xinput_button_state
 * @param oldstate
 * @param buttonbit
 * @param newstate
 */
static void win_xinput_process_button(DWORD xinput_button_state, Game_ButtonState *oldstate,
                                      DWORD buttonbit, Game_ButtonState *newstate)
{
	newstate->ended_down = (xinput_button_state & buttonbit) == buttonbit;
	newstate->half_transition_count = oldstate->ended_down != newstate->ended_down;
}

static void win_input_begin_recording(Win_State *winstate, unsigned input_recording_index)
{
	char filepath[WIN_STATE_MAX_FILE_PATH];
	win_file_build_input_path(winstate, input_recording_index, sizeof(filepath), filepath);

	winstate->input_recording_index = input_recording_index;
	winstate->recording_handle =
		CreateFileA(filepath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);

	assert(winstate->gamemem_size <= UINT32_MAX);

	unsigned long bytes_written;
	WriteFile(winstate->recording_handle, winstate->gamemem, (uint32_t)winstate->gamemem_size,
	          &bytes_written, nullptr);
}

static void win_input_end_recording(Win_State *winstate)
{
	CloseHandle(winstate->recording_handle);
	winstate->input_recording_index = 0;
}

static void win_input_begin_playback(Win_State *winstate, unsigned input_playback_index)
{
	char filepath[WIN_STATE_MAX_FILE_PATH];
	win_file_build_input_path(winstate, input_playback_index, sizeof(filepath), filepath);

	winstate->input_playing_index = input_playback_index;
	winstate->playback_handle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, nullptr,
	                                        OPEN_EXISTING, 0, nullptr);

	assert(winstate->gamemem_size <= UINT32_MAX);

	unsigned long bytes_read;
	ReadFile(winstate->playback_handle, winstate->gamemem, (uint32_t)winstate->gamemem_size,
	         &bytes_read, nullptr);
}

static void win_input_end_playback(Win_State *winstate)
{
	CloseHandle(winstate->playback_handle);
	winstate->input_playing_index = 0;
}

static void win_input_record(Win_State *winstate, Game_Input *input)
{
	unsigned long bytes_written;
	WriteFile(winstate->recording_handle, input, sizeof(*input), &bytes_written, nullptr);
}

static void win_input_playback(Win_State *winstate, Game_Input *input)
{
	unsigned long bytes_read;
	if (!ReadFile(winstate->playback_handle, input, sizeof(*input), &bytes_read, nullptr)) {
		// Failed to read
		return;
	}

	if (!bytes_read) {
		unsigned playing_index = winstate->input_playing_index;

		win_input_end_playback(winstate);
		win_input_begin_playback(winstate, playing_index);

		ReadFile(winstate->playback_handle, input, sizeof(*input), &bytes_read, nullptr);
	}
}

static void win_window_pump_messages(Win_State *winstate, Game_ControllerInput *keyboard_controller)
{
	MSG msg;
	while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
		case WM_QUIT: {
			is_global_running = false;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			DWORD vk_code = (DWORD)msg.wParam;
			bool was_down = (msg.lParam & (1 << 30)) != 0;
			bool is_down = (msg.lParam & (1 << 31)) == 0;

			if (was_down != is_down) {
				if (vk_code == 'W') {
					win_keyboard_process_message(&keyboard_controller->moveup,
					                             is_down);
				} else if (vk_code == 'A') {
					win_keyboard_process_message(&keyboard_controller->moveleft,
					                             is_down);
				} else if (vk_code == 'S') {
					win_keyboard_process_message(&keyboard_controller->movedown,
					                             is_down);
				} else if (vk_code == 'D') {
					win_keyboard_process_message(
						&keyboard_controller->moveright, is_down);
				} else if (vk_code == 'Q') {
					win_keyboard_process_message(
						&keyboard_controller->left_shoulder, is_down);
				} else if (vk_code == 'E') {
					win_keyboard_process_message(
						&keyboard_controller->right_shoulder, is_down);
				} else if (vk_code == VK_UP) {
					win_keyboard_process_message(&keyboard_controller->actionup,
					                             is_down);
				} else if (vk_code == VK_LEFT) {
					win_keyboard_process_message(
						&keyboard_controller->actionleft, is_down);
				} else if (vk_code == VK_DOWN) {
					win_keyboard_process_message(
						&keyboard_controller->actiondown, is_down);
				} else if (vk_code == VK_RIGHT) {
					win_keyboard_process_message(
						&keyboard_controller->actionright, is_down);
				} else if (vk_code == VK_ESCAPE) {
					win_keyboard_process_message(&keyboard_controller->start,
					                             is_down);
				} else if (vk_code == VK_SPACE) {
					win_keyboard_process_message(&keyboard_controller->back,
					                             is_down);
				}
#if DEBUG
				else if (vk_code == 'P') {
					if (is_down) {
						is_global_pause = !is_global_pause;
					}
				} else if (vk_code == 'L') {
					if (is_down) {
						if (winstate->input_playing_index) {
							win_input_end_playback(winstate);
						} else if (!winstate->input_recording_index) {
							win_input_begin_recording(winstate, 1);
						} else { // it was in recording mode
							win_input_end_recording(winstate);
							win_input_begin_playback(winstate, 1);
						}
					}
				}
#endif
			}
			bool was_alt_key_down = (msg.lParam & (1 << 29)) != 0;
			if ((vk_code == VK_F4) && was_alt_key_down) {
				is_global_running = false;
			}
		} break;
		default: {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} break;
		}
	}
}

static Win_WindowDimensions win_window_get_dimensions(HWND winhandle)
{
	Win_WindowDimensions result;
	RECT client_rec;
	GetClientRect(winhandle, &client_rec);
	result.width = client_rec.right - client_rec.left;
	result.height = client_rec.bottom - client_rec.top;

	return result;
}

/*
 * dib: device independent bitmap
 */
static void win_bitmap_resize_section(Win_Bitmap *buffer, unsigned win_width, unsigned win_height)
{
	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = win_width;
	buffer->height = win_height;
	buffer->bytes_per_pixel = 4;
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = (long)buffer->width;
	// NOTE(fredy): top-down layout (opposite to bottom-up).
	// The first three bytes on the bitmap are for the top-left pixel
	buffer->info.bmiHeader.biHeight = -(long)buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	long bitmap_memory_size = (long)(buffer->width * buffer->height * buffer->bytes_per_pixel);

	buffer->memory = VirtualAlloc(nullptr, (size_t)bitmap_memory_size, MEM_RESERVE | MEM_COMMIT,
	                              PAGE_READWRITE);
	buffer->pitch_bytes = buffer->width * buffer->bytes_per_pixel;
}

static void win_window_display_bitmap(HDC dchandle, Win_Bitmap *bitmap, long win_width,
                                      long win_height)
{
	StretchDIBits(dchandle, 0, 0, (int)bitmap->width, (int)bitmap->height, 0, 0,
	              (int)bitmap->width, (int)bitmap->height, bitmap->memory, &bitmap->info,
	              DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK win_window_handle_callback([[__maybe_unused__]] HWND winhandle,
                                                   [[__maybe_unused__]] UINT msg,
                                                   [[__maybe_unused__]] WPARAM wparam,
                                                   [[__maybe_unused__]] LPARAM lparam)
{
	LRESULT result = 0;

	switch (msg) {
	case WM_CLOSE: {
		is_global_running = false;
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_DESTROY: {
		is_global_running = false;
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		assert(false && "We must be processing the keyboard in other place");
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC dchandle = BeginPaint(winhandle, &paint);
		Win_WindowDimensions windim = win_window_get_dimensions(winhandle);
		win_window_display_bitmap(dchandle, &global_bitmap, windim.width, windim.height);
		EndPaint(winhandle, &paint);
	} break;
	default: {
		OutputDebugStringA("default\n");
		result = DefWindowProcA(winhandle, msg, wparam, lparam);
	} break;
	}

	return result;
}

static inline LARGE_INTEGER win_clock_get_wall(void)
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	return counter;
}

static inline float win_clock_elapsed_secs(LARGE_INTEGER start, LARGE_INTEGER end)
{
	return (float)(end.QuadPart - start.QuadPart) / (float)global_perf_count_frequency;
}

/**
 * @brief
 *
 * @param bitmap_buffer
 * @param x pixel index
 * @param top pixel index
 * @param bottom pixel index
 */
static void win_bitmap_draw_vertical_debug(Win_Bitmap *bitmap, unsigned x, unsigned top,
                                           unsigned bottom, unsigned color)
{
	assert(x >= 0 && x < bitmap->width);
	assert(bottom >= 0 && bottom < bitmap->height);
	assert(top >= 0 && top < bitmap->height);
	assert(top <= bottom);

	uint8_t *pixel_start = (uint8_t *)bitmap->memory + x * (size_t)bitmap->bytes_per_pixel +
	                       top * (size_t)bitmap->pitch_bytes;
	uint32_t *pixel;
	for (size_t y = top; y < bottom; ++y) {
		pixel = (uint32_t *)pixel_start;
		*pixel = color;
		pixel_start += bitmap->pitch_bytes;
	}
}

static inline void win_bitmap_draw_sound_buffer_mark_debug(Win_Bitmap *bitmap_buffer,
                                                           Win_SoundOutput *soundout,
                                                           float pixels_per_byte, unsigned padx,
                                                           unsigned top, unsigned bottom,
                                                           unsigned value, uint32_t color)
{
	assert(value < soundout->buffsize);
	unsigned x = padx + (unsigned)(pixels_per_byte * (float)value);
	win_bitmap_draw_vertical_debug(bitmap_buffer, x, top, bottom, color);
}

/**
 * @brief debug code for sound
 *
 * @param bitmap_buffer
 * @param last_play_cursors_size
 * @param last_play_cursors sound play cursor positions inside the sound buffer
 * @param soundout
 * @param target_secs_per_frame
 */
static void win_bitmap_draw_sound_sync_debug(Win_Bitmap *bitmap_buffer,
                                             unsigned last_cursors_marks_size,
                                             Win_DebugTimeMark *last_cursors_marks,
                                             unsigned current_mark_index, Win_SoundOutput *winsound)
{
	unsigned padx = 16;
	unsigned pady = 16;

	unsigned line_height = 64;

	unsigned painting_width = (unsigned)bitmap_buffer->width - 2 * padx;
	float pixels_per_byte = (float)painting_width / (float)winsound->buffsize;

	for (size_t i = 0; i < last_cursors_marks_size; ++i) {
		Win_DebugTimeMark current_mark = last_cursors_marks[i];
		unsigned long play_color = 0xFFFFFFFF;
		unsigned long write_color = 0xFFFF0000;
		unsigned long frame_flip_byte_color = 0xFFFFFF00;
		unsigned long play_window_color = 0xFFFF00FF;

		unsigned top = pady;
		unsigned bottom = pady + line_height;
		unsigned first_top = 0;
		if (i == current_mark_index) {
			top += pady + line_height;
			bottom += pady + line_height;

			first_top = top;

			win_bitmap_draw_sound_buffer_mark_debug(bitmap_buffer, winsound,
			                                        pixels_per_byte, padx, top, bottom,
			                                        current_mark.output_play_cursor,
			                                        play_color);
			win_bitmap_draw_sound_buffer_mark_debug(bitmap_buffer, winsound,
			                                        pixels_per_byte, padx, top, bottom,
			                                        current_mark.output_write_cursor,
			                                        write_color);

			top += pady + line_height;
			bottom += pady + line_height;

			win_bitmap_draw_sound_buffer_mark_debug(bitmap_buffer, winsound,
			                                        pixels_per_byte, padx, top, bottom,
			                                        current_mark.output_location,
			                                        play_color);
			win_bitmap_draw_sound_buffer_mark_debug(
				bitmap_buffer, winsound, pixels_per_byte, padx, top, bottom,
				RING_ADD(winsound->buffsize, current_mark.output_location,
			                 current_mark.output_byte_count),
				write_color);

			top += pady + line_height;
			bottom += pady + line_height;

			win_bitmap_draw_sound_buffer_mark_debug(
				bitmap_buffer, winsound, pixels_per_byte, padx, first_top, bottom,
				current_mark.frame_flip_byte, frame_flip_byte_color);
		}

		win_bitmap_draw_sound_buffer_mark_debug(bitmap_buffer, winsound, pixels_per_byte,
		                                        padx, top, bottom,
		                                        current_mark.flip_play_cursor, play_color);

		win_bitmap_draw_sound_buffer_mark_debug(
			bitmap_buffer, winsound, pixels_per_byte, padx, top, bottom,
			RING_SUB(winsound->buffsize, current_mark.flip_play_cursor,
		                 480 * winsound->bytes_per_sample),
			play_window_color);
		win_bitmap_draw_sound_buffer_mark_debug(
			bitmap_buffer, winsound, pixels_per_byte, padx, top, bottom,
			RING_ADD(winsound->buffsize, current_mark.flip_play_cursor,
		                 480 * winsound->bytes_per_sample),
			play_window_color);

		win_bitmap_draw_sound_buffer_mark_debug(bitmap_buffer, winsound, pixels_per_byte,
		                                        padx, top, bottom,
		                                        current_mark.flip_write_cursor,
		                                        write_color);
	}
}

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hinstance,
                     [[__maybe_unused__]] HINSTANCE hprevinstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine, [[__maybe_unused__]] int nCmdShow)
{
	// NOTE(): never use MAX_PATH in user-facing code. it is dangerous.
	Win_State winstate = {
		.input_playing_index = 0,
		.input_recording_index = 0,
	};

	LARGE_INTEGER perf_count_frequency_result;
	QueryPerformanceFrequency(&perf_count_frequency_result);
	global_perf_count_frequency = perf_count_frequency_result.QuadPart;

	win_file_get_exe_path(&winstate);

	char tmpgamedll_filename[WIN_STATE_MAX_FILE_PATH];
	char gamedll_path[WIN_STATE_MAX_FILE_PATH];
	char tmpgamedll_path[WIN_STATE_MAX_FILE_PATH];

	win_file_build_path(&winstate, "game.dll", sizeof(gamedll_path), gamedll_path);

	sprintf(tmpgamedll_filename, "game_tmp_%lu.dll", GetCurrentTime());
	win_file_build_path(&winstate, tmpgamedll_filename, sizeof(tmpgamedll_path),
	                    tmpgamedll_path);

	// sets the scheduler granularity to 1ms, so that our Sleep() can be more granular
	unsigned desire_scheduler_ms = 1;
	bool is_granular_sleep = timeBeginPeriod(desire_scheduler_ms) == TIMERR_NOERROR;

	win_xinput_load();

	WNDCLASSA winclass = {
		.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc = win_window_handle_callback,
		.hInstance = hinstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	win_bitmap_resize_section(&global_bitmap, 1200, 700);

	constexpr unsigned monitorhz = 60;
	constexpr unsigned gamehz = monitorhz / 2;
	float target_secs_per_frame = 1.0f / (float)gamehz;

	if (!RegisterClassA(&winclass)) {
		TIX_LOGE("error registering the window class");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(0, winclass.lpszClassName, "Handmade Hero",
	                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
	                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
	                                 nullptr, hinstance, nullptr);
	if (!winhandle) {
		TIX_LOGE("error creating the window");
		return EXIT_FAILURE;
	}

	HDC dchandle = GetDC(winhandle);

	Win_SoundOutput winsound = {
		.samples_per_sec = 48000,
		.bytes_per_sample = sizeof(uint16_t) * 2,
	};
	winsound.buffsize = winsound.samples_per_sec * winsound.bytes_per_sample;
	winsound.safety_bytes = winsound.samples_per_sec * winsound.bytes_per_sample / gamehz / 3;

	win_sound_init(winhandle, winsound.samples_per_sec, winsound.buffsize);
	win_sound_clear_buffer(&winsound);

	if (FAILED(IDirectSoundBuffer_Play(secbuffer, 0, 0, DSBPLAY_LOOPING))) {
		TIX_LOGE("Error playing dsound secondary buffer");
		return EXIT_FAILURE;
	}

	is_global_running = true;

#if 0
	{
		// NOTE(fredy): debug the play/write cursor update frequency
		// on the handmade hero machine it was 480 samples
		unsigned long previous_play_cursor = 0;
		unsigned long play_cursor = 0;
		unsigned long write_cursor = 0;
		for (size_t i = 0; i < 50; ++i) {
			IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
			                                      &write_cursor);
			size_t cursor_delta_bytes =
				CIRCULAR_DIFF(play_cursor, write_cursor, soundout.buffsize);
			size_t play_delta_bytes =
				CIRCULAR_DIFF(previous_play_cursor, play_cursor, soundout.buffsize);
			TIX_LOGD(
				"PC: %lu, WC: %lu, CDT: %zu bytes (%zu samples), PDT: %zu bytes (%zu samples)",
				play_cursor, write_cursor, cursor_delta_bytes,
				cursor_delta_bytes / soundout.sample_size_bytes, play_delta_bytes,
				play_delta_bytes / soundout.sample_size_bytes);

			previous_play_cursor = play_cursor;
		}
	}
#endif // debug sound

	int16_t *samples = (int16_t *)VirtualAlloc(nullptr, winsound.buffsize,
	                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Game_Memory game_memory = {
		.plat_debug_free_file = plat_debug_freefile,
		.plat_debug_read_file = plat_debug_readfile,
		.plat_debug_write_file = plat_debug_writefile,
	};
	game_memory.permamem_size = MB_TO_BYTE(64);
	game_memory.transmem_size = GB_TO_BYTE(1);

	winstate.gamemem_size = game_memory.permamem_size + game_memory.transmem_size;
	winstate.gamemem = VirtualAlloc(BASE_ADDRESS, winstate.gamemem_size,
	                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	game_memory.permamem = winstate.gamemem;
	game_memory.transmem = (uint8_t *)game_memory.permamem + game_memory.permamem_size;

	if (!samples || !game_memory.permamem || !game_memory.transmem) {
		return EXIT_FAILURE;
	}

	Game_SoundBuffer game_soundbuff = {
		.samples_per_sec = winsound.samples_per_sec,
		.samples = samples,
	};

	Game_Bitmap bitmapbuff = {};

	Game_Input inputs[2] = {};
	Game_Input *new_input = &inputs[0];
	Game_Input *old_input = &inputs[1];

	LARGE_INTEGER last_counter = win_clock_get_wall();
	LARGE_INTEGER flip_wall_clock = win_clock_get_wall();

	constexpr size_t debug_last_cursor_marks_size = gamehz / 2;
	unsigned debug_last_cursor_mark_index = 0;
	Win_DebugTimeMark debug_last_cursor_marks[debug_last_cursor_marks_size] = {};

	size_t sound_latency_bytes = 0;
	float sound_latency_secs = 0.0f;
	bool is_sound_valid = false;

	Win_GameCode game_code = {};
	FILETIME gamedll_last_write_time = {};
	if (!win_code_load_game(gamedll_path, tmpgamedll_path, &game_code)) {
		return EXIT_FAILURE;
	}

	size_t last_cycle_count = __rdtsc();
	while (is_global_running) {
		if (win_file_get_last_write_time(gamedll_path, &gamedll_last_write_time) &&
		    CompareFileTime(&game_code.dll_write_time, &gamedll_last_write_time) != 0 &&
		    win_code_unload_game(&game_code)) {
			sprintf(tmpgamedll_filename, "game_tmp_%lu.dll", GetCurrentTime());
			win_file_build_path(&winstate, tmpgamedll_filename, sizeof(tmpgamedll_path),
			                    tmpgamedll_path);

			if (!win_code_load_game(gamedll_path, tmpgamedll_path, &game_code)) {
				return EXIT_FAILURE;
			}
		}

		/**
		* @brief Gather input
		*/
		Game_ControllerInput *old_keyboard_controller =
			game_input_get_controller(old_input, 0);
		Game_ControllerInput *new_keyboard_controller =
			game_input_get_controller(new_input, 0);
		Game_ControllerInput zero_controller = {};
		*new_keyboard_controller = zero_controller;
		new_keyboard_controller->is_connected = true;

		for (size_t i = 0; i < GAME_MAX_BUTTONS; ++i) {
			new_keyboard_controller->buttons[i].ended_down =
				old_keyboard_controller->buttons[i].ended_down;
		}

		win_window_pump_messages(&winstate, new_keyboard_controller);

		if (is_global_pause) {
			continue;
		}

		// +1 Taking into account keyboard controller
		unsigned short max_controller_count = XUSER_MAX_COUNT;
		if (max_controller_count > GAME_MAX_CONTROLLERS - 1) {
			max_controller_count = GAME_MAX_CONTROLLERS - 1;
		}

		for (unsigned long i = 0; i < max_controller_count; ++i) {
			unsigned long our_controller_index = i + 1;
			Game_ControllerInput *old_controller =
				game_input_get_controller(old_input, our_controller_index);
			Game_ControllerInput *new_controller =
				game_input_get_controller(new_input, our_controller_index);
			XINPUT_STATE state;
			if (XInputGetState(i, &state) != ERROR_SUCCESS) {
				new_controller->is_connected = false;
				continue;
			}

			// Plugged in
			new_controller->is_connected = true;
			new_controller->is_analog = old_controller->is_analog;

			XINPUT_GAMEPAD *pad = &state.Gamepad;

			new_controller->stick_avg_x = win_xinput_process_stick_value(
				pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			new_controller->stick_avg_y = win_xinput_process_stick_value(
				pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

			if (new_controller->stick_avg_x != 0.0f ||
			    new_controller->stick_avg_y != 0.0f) {
				new_controller->is_analog = true;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
				new_controller->stick_avg_y = 1.0f;
				new_controller->is_analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
				new_controller->stick_avg_y = -1.0f;
				new_controller->is_analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
				new_controller->stick_avg_x = -1.0f;
				new_controller->is_analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
				new_controller->stick_avg_x = 1.0f;
				new_controller->is_analog = false;
			}

			float threshold = 0.5f;
			win_xinput_process_button(new_controller->stick_avg_x < -threshold ? 1 : 0,
			                          &old_controller->moveleft, 1,
			                          &new_controller->moveleft);
			win_xinput_process_button(new_controller->stick_avg_x > threshold ? 1 : 0,
			                          &old_controller->moveright, 1,
			                          &new_controller->moveright);
			win_xinput_process_button(new_controller->stick_avg_y < -threshold ? 1 : 0,
			                          &old_controller->movedown, 1,
			                          &new_controller->movedown);
			win_xinput_process_button(new_controller->stick_avg_y > threshold ? 1 : 0,
			                          &old_controller->moveup, 1,
			                          &new_controller->moveup);

			win_xinput_process_button(pad->wButtons, &old_controller->actiondown,
			                          XINPUT_GAMEPAD_A, &new_controller->actiondown);
			win_xinput_process_button(pad->wButtons, &old_controller->actionright,
			                          XINPUT_GAMEPAD_B, &new_controller->actionright);
			win_xinput_process_button(pad->wButtons, &old_controller->actionleft,
			                          XINPUT_GAMEPAD_X, &new_controller->actionleft);
			win_xinput_process_button(pad->wButtons, &old_controller->actionup,
			                          XINPUT_GAMEPAD_Y, &new_controller->actionup);

			win_xinput_process_button(pad->wButtons, &old_controller->left_shoulder,
			                          XINPUT_GAMEPAD_LEFT_SHOULDER,
			                          &new_controller->left_shoulder);
			win_xinput_process_button(pad->wButtons, &old_controller->right_shoulder,
			                          XINPUT_GAMEPAD_RIGHT_SHOULDER,
			                          &new_controller->right_shoulder);

			win_xinput_process_button(pad->wButtons, &old_controller->start,
			                          XINPUT_GAMEPAD_START, &new_controller->start);
			win_xinput_process_button(pad->wButtons, &old_controller->back,
			                          XINPUT_GAMEPAD_BACK, &new_controller->back);
		}

		/**
		 * @brief Update and rendering
		 */

		bitmapbuff.memory = global_bitmap.memory;
		bitmapbuff.width = global_bitmap.width;
		bitmapbuff.height = global_bitmap.height;
		bitmapbuff.pitch_bytes = global_bitmap.pitch_bytes;
		bitmapbuff.bytes_per_pixel = global_bitmap.bytes_per_pixel;

		if (winstate.input_recording_index) {
			win_input_record(&winstate, new_input);
		}

		if (winstate.input_playing_index) {
			win_input_playback(&winstate, new_input);
		}
		if (game_code.update_and_render) {
			game_code.update_and_render(&game_memory, new_input, &bitmapbuff);
		}

		unsigned bytes_to_write = 0;

		unsigned long play_cursor;
		unsigned long write_cursor;
		if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
		                                                    &write_cursor))) {
			if (!is_sound_valid) {
				winsound.running_sample_index =
					write_cursor / winsound.bytes_per_sample;
				is_sound_valid = true;
			}

			// NOTE(fredy): Compute how much sound to write and where
			unsigned byte_to_lock =
				(winsound.running_sample_index * winsound.bytes_per_sample) %
				winsound.buffsize;

			float secs_from_flip =
				win_clock_elapsed_secs(flip_wall_clock, win_clock_get_wall());
			float secs_to_flip = target_secs_per_frame - secs_from_flip;
			unsigned bytes_per_sec =
				winsound.samples_per_sec * winsound.bytes_per_sample;
			unsigned bytes_to_flip = (unsigned)(secs_to_flip * (float)bytes_per_sec);
			unsigned bytes_per_frame =
				winsound.bytes_per_sample * winsound.samples_per_sec / gamehz;
			unsigned frame_flip_byte =
				RING_ADD(winsound.buffsize, play_cursor, bytes_to_flip);
			unsigned sound_flip_byte =
				RING_BETWEEN(play_cursor, frame_flip_byte, write_cursor) ?
					// Sound has low latency
					frame_flip_byte :
					// Sound has high latency
					write_cursor;

			TIX_LOGD("ET: %f secs, TTF: %f secs, BPF: %u, BTF: %u, FFB: %u, SFB: %u",
			         (double)secs_from_flip, (double)secs_to_flip, bytes_per_frame,
			         bytes_to_flip, frame_flip_byte, sound_flip_byte);
			unsigned target_cursor = RING_ADD(winsound.buffsize, sound_flip_byte,
			                                  bytes_per_frame + winsound.safety_bytes);

			bytes_to_write = RING_DIFF(winsound.buffsize, byte_to_lock, target_cursor);

			game_soundbuff.sample_count = bytes_to_write / winsound.bytes_per_sample;
			if (game_code.sound_create_samples) {
				game_code.sound_create_samples(&game_memory, &game_soundbuff);
			}
#if DEBUG
			Win_DebugTimeMark *mark =
				&debug_last_cursor_marks[debug_last_cursor_mark_index];

			mark->output_play_cursor = play_cursor;
			mark->output_write_cursor = write_cursor;
			mark->output_location = byte_to_lock;
			mark->output_byte_count = bytes_to_write;
			mark->frame_flip_byte = frame_flip_byte;

			sound_latency_bytes =
				RING_DIFF(winsound.buffsize, play_cursor, write_cursor);
			sound_latency_secs = (float)sound_latency_bytes /
			                     (float)winsound.bytes_per_sample /
			                     (float)winsound.samples_per_sec;

			TIX_LOGD(
				"Estimated - PC: %lu, WC: %lu, BTL: %u, TC: %u, BTW: %u - LAT: %zu (%f secs)",
				play_cursor, write_cursor, byte_to_lock, target_cursor,
				bytes_to_write, sound_latency_bytes, (double)sound_latency_secs);
#endif

			win_sound_fill_buffer(&winsound, byte_to_lock, bytes_to_write,
			                      &game_soundbuff);

		} else {
			is_sound_valid = false;
		}

		LARGE_INTEGER work_counter = win_clock_get_wall();
		float work_secs_elapsed = win_clock_elapsed_secs(last_counter, work_counter);

		float secs_elapsed_for_frame = work_secs_elapsed;
		if (secs_elapsed_for_frame < target_secs_per_frame) {
			if (is_granular_sleep) {
				unsigned long sleep_ms =
					(unsigned long)(1000.0f * (target_secs_per_frame -
				                                   secs_elapsed_for_frame));
				if (sleep_ms > 0) {
					Sleep(sleep_ms);
				}
			}

			float test_secs_elapsed_for_frame =
				win_clock_elapsed_secs(last_counter, win_clock_get_wall());
			if (test_secs_elapsed_for_frame > target_secs_per_frame) {
				TIX_LOGW("missed sleep: %f", (double)test_secs_elapsed_for_frame);
			}

			while (secs_elapsed_for_frame < target_secs_per_frame) {
				secs_elapsed_for_frame =
					win_clock_elapsed_secs(last_counter, win_clock_get_wall());
			}
		} else {
			// missed frame rate!
			TIX_LOGW("missed frame rate: %f", (double)secs_elapsed_for_frame);
		}

		LARGE_INTEGER end_counter = win_clock_get_wall();
		float ms_per_frame = 1000.0f * win_clock_elapsed_secs(last_counter, end_counter);
		last_counter = end_counter;

		Win_WindowDimensions windim = win_window_get_dimensions(winhandle);

#ifdef DEBUG
		win_bitmap_draw_sound_sync_debug(&global_bitmap, debug_last_cursor_marks_size,
		                                 debug_last_cursor_marks,
		                                 debug_last_cursor_mark_index - 1, &winsound);
#endif

		// Flip the frame
		win_window_display_bitmap(dchandle, &global_bitmap, windim.width, windim.height);

		flip_wall_clock = win_clock_get_wall();

#ifdef DEBUG
		{
			unsigned long debug_play_cursor;
			unsigned long debug_write_cursor;
			IDirectSoundBuffer_GetCurrentPosition(secbuffer, &debug_play_cursor,
			                                      &debug_write_cursor);
			if (is_sound_valid) {
				assert(debug_last_cursor_mark_index < debug_last_cursor_marks_size);
				Win_DebugTimeMark *mark =
					&debug_last_cursor_marks[debug_last_cursor_mark_index];
				mark->flip_play_cursor = debug_play_cursor;
				mark->flip_write_cursor = debug_write_cursor;
				TIX_LOGD("After flip - PC: %lu, WC: %lu, DPC: %lu, DWC: %lu",
				         play_cursor, write_cursor, debug_play_cursor,
				         debug_write_cursor);
			}
		}
#endif

		Game_Input *temp = new_input;
		new_input = old_input;
		old_input = temp;

		uint64_t end_cycle_count = __rdtsc();
		uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
		last_cycle_count = end_cycle_count;

		float fps = 1000.0f / ms_per_frame;
		float mega_cycles_per_frame = (float)cycles_elapsed / 1000000.0f;

		TIX_LOGI("%fms/f, %ff/s, %fmc/f", (double)ms_per_frame, (double)fps,
		         (double)mega_cycles_per_frame);

#if DEBUG
		{
			debug_last_cursor_mark_index = RING_ADD(debug_last_cursor_marks_size,
			                                        debug_last_cursor_mark_index, 1);
		}
#endif // debug
	}

	return EXIT_SUCCESS;
}
