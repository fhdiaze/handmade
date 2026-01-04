/*
* Windows platform code
*/

#undef TIX_LOG_LEVEL
#define TIX_LOG_LEVEL TIX_LOG_LEVEL_ERROR

#include "win_handmade.h"
#include "game.c"
#include "tix_log.h"
#include <dsound.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>

// globals
static bool is_global_running = false;
static Win_OffScreenBuffer global_back_buffer;
static LPDIRECTSOUNDBUFFER secbuffer;
static int64_t global_perf_count_frequency;

// macros

#define X_INPUT_GET_STATE(name)                                   \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, \
	                  [[__maybe_unused__]] XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name)                                   \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, \
	                  [[__maybe_unused__]] XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DSOUND_CREATE(name) \
	HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(direct_sound_create);

// services

Plat_ReadFileResult plat_debug_readfile(const char *const filename)
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

void plat_debug_freefile(void *memory)
{
	if (memory) {
		VirtualFree(memory, 0, MEM_RELEASE);
	}
}

bool plat_debug_writefile(const char *const filename, size_t memorysize, void *memory)
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
		XInputGetState = (x_input_get_state *)GetProcAddress(xinput_lib, "XInputGetState");
		if (!XInputGetState) {
			XInputGetState = XInputGetStateStub;
		}
		XInputSetState = (x_input_set_state *)GetProcAddress(xinput_lib, "XInputSetState");
		if (!XInputSetState) {
			XInputSetState = XInputSetStateStub;
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
		OutputDebugStringA("Error loading dsound.dll");
	}

	direct_sound_create *dsound_create =
		(direct_sound_create *)GetProcAddress(dsound_lib, "DirectSoundCreate");
	if (!dsound_create) {
		// TODO(fredy): diagnostic
		OutputDebugStringA("Error getting DirectSoundCreate function");
	}

	LPDIRECTSOUND direct_sound;
	if (FAILED(dsound_create(nullptr, &direct_sound, nullptr))) {
		// TODO(fredy): diagnostic
		OutputDebugStringA("Error creating the handler for direct sound");
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
		OutputDebugStringA("Error setting the cooperative level for direct sound");
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
		OutputDebugStringA("Error creating the primary buffer (handle for the sound card)");
	}

	if (FAILED(IDirectSoundBuffer_SetFormat(primbuffer, &waveformat))) {
		// TODO(fredy): diagnostic
		OutputDebugStringA("Error setting the format for the primary buffer");
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
		OutputDebugStringA("Error creating the secondary buffer");
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

static void win_input_process_keyboard_msg(Game_ButtonState *newstate, bool is_down)
{
	assert(newstate->ended_down != is_down);
	newstate->ended_down = is_down;
	++newstate->half_transition_count;
}

/**
 * @brief Normalize stick value to [-1.0, 1.0]
 *
 * @param value
 * @param dead_zone
 * @return float
 */
static float win_input_process_stick_value(short value, short dead_zone)
{
	if (value < -dead_zone) {
		return (float)value / 32768.0f;
	}
	if (value > dead_zone) {
		return (float)value / 32767.0f;
	}

	return 0.0f;
}

static void win_input_process_digital_button(DWORD xinput_button_state, Game_ButtonState *oldstate,
                                             DWORD buttonbit, Game_ButtonState *newstate)
{
	newstate->ended_down = (xinput_button_state & buttonbit) == buttonbit;
	newstate->half_transition_count = oldstate->ended_down != newstate->ended_down;
}

static void win_process_messages(Game_ControllerInput *keyboard_controller)
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
					win_input_process_keyboard_msg(&keyboard_controller->moveup,
					                               is_down);
				} else if (vk_code == 'A') {
					win_input_process_keyboard_msg(
						&keyboard_controller->moveleft, is_down);
				} else if (vk_code == 'S') {
					win_input_process_keyboard_msg(
						&keyboard_controller->movedown, is_down);
				} else if (vk_code == 'D') {
					win_input_process_keyboard_msg(
						&keyboard_controller->moveright, is_down);
				} else if (vk_code == 'Q') {
					win_input_process_keyboard_msg(
						&keyboard_controller->left_shoulder, is_down);
				} else if (vk_code == 'E') {
					win_input_process_keyboard_msg(
						&keyboard_controller->right_shoulder, is_down);
				} else if (vk_code == VK_UP) {
					win_input_process_keyboard_msg(
						&keyboard_controller->actionup, is_down);
				} else if (vk_code == VK_LEFT) {
					win_input_process_keyboard_msg(
						&keyboard_controller->actionleft, is_down);
				} else if (vk_code == VK_DOWN) {
					win_input_process_keyboard_msg(
						&keyboard_controller->actiondown, is_down);
				} else if (vk_code == VK_RIGHT) {
					win_input_process_keyboard_msg(
						&keyboard_controller->actionright, is_down);
				} else if (vk_code == VK_ESCAPE) {
					win_input_process_keyboard_msg(&keyboard_controller->start,
					                               is_down);
				} else if (vk_code == VK_SPACE) {
					win_input_process_keyboard_msg(&keyboard_controller->back,
					                               is_down);
				}
			}
			bool alt_key_was_down = (msg.lParam & (1 << 29)) != 0;
			if ((vk_code == VK_F4) && alt_key_was_down) {
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
static void win_resize_dib_section(Win_OffScreenBuffer *buffer, long win_width, long win_height)
{
	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = win_width;
	buffer->height = win_height;
	buffer->bytes_per_pixel = 4;
	buffer->bitmap_info.bmiHeader.biSize = sizeof(buffer->bitmap_info.bmiHeader);
	buffer->bitmap_info.bmiHeader.biWidth = buffer->width;
	// NOTE(fredy): top-down layout (opposite to bottom-up).
	// The first three bytes on the bitmap are for the top-left pixel
	buffer->bitmap_info.bmiHeader.biHeight = -buffer->height;
	buffer->bitmap_info.bmiHeader.biPlanes = 1;
	buffer->bitmap_info.bmiHeader.biBitCount = 32;
	buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;

	long bitmap_memory_size = buffer->width * buffer->height * buffer->bytes_per_pixel;

	buffer->memory = VirtualAlloc(nullptr, (size_t)bitmap_memory_size, MEM_RESERVE | MEM_COMMIT,
	                              PAGE_READWRITE);
	buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

static void win_buffer_display_in_window(Win_OffScreenBuffer *buffer, HDC dchandle, long win_width,
                                         long win_height)
{
	StretchDIBits(dchandle, 0, 0, win_width, win_height, 0, 0, buffer->width, buffer->height,
	              buffer->memory, &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK win_main_window_proc([[__maybe_unused__]] HWND winhandle,
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
		win_buffer_display_in_window(&global_back_buffer, dchandle, windim.width,
		                             windim.height);
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
 * @param screen_buffer
 * @param x pixel index
 * @param top pixel index
 * @param bottom pixel index
 */
static void win_debug_draw_vertical(Win_OffScreenBuffer *screen_buffer, size_t x, size_t top,
                                    size_t bottom, uint32_t color)
{
	uint8_t *pixel_start = (uint8_t *)screen_buffer->memory +
	                       x * (size_t)screen_buffer->bytes_per_pixel +
	                       top * (size_t)screen_buffer->pitch;
	uint32_t *pixel;
	for (size_t y = top; y < bottom; ++y) {
		pixel = (uint32_t *)pixel_start;
		*pixel = color;
		pixel_start += screen_buffer->pitch;
	}
}

static inline void win_debug_draw_sound_buffer_mark(Win_OffScreenBuffer *screen_buffer,
                                                    Win_SoundOutput *soundout,
                                                    float pixels_per_byte, size_t padx, size_t top,
                                                    size_t bottom, unsigned long value,
                                                    uint32_t color)
{
	assert(value < soundout->buffsize);
	size_t x = padx + (size_t)(pixels_per_byte * (float)value);
	win_debug_draw_vertical(screen_buffer, x, top, bottom, color);
}

/**
 * @brief debug code for sound
 *
 * @param screen_buffer
 * @param last_play_cursors_size
 * @param last_play_cursors sound play cursor positions inside the sound buffer
 * @param soundout
 * @param target_secs_per_frame
 */
static void win_debug_sync_display(Win_OffScreenBuffer *screen_buffer,
                                   size_t last_cursors_marks_size,
                                   Win_DebugTimeMark *last_cursors_marks, Win_SoundOutput *soundout,
                                   float target_secs_per_frame)
{
	size_t padx = 16;
	size_t pady = 16;

	size_t top = pady;
	size_t bottom = (size_t)screen_buffer->height - pady;

	size_t painting_width = (size_t)screen_buffer->width - 2 * padx;
	float pixels_per_byte = (float)painting_width / (float)soundout->buffsize;

	for (size_t i = 0; i < last_cursors_marks_size; ++i) {
		Win_DebugTimeMark current_mark = last_cursors_marks[i];
		win_debug_draw_sound_buffer_mark(screen_buffer, soundout, pixels_per_byte, padx,
		                                 top, bottom, current_mark.play_cursor, 0xFFFFFFFF);
		win_debug_draw_sound_buffer_mark(screen_buffer, soundout, pixels_per_byte, padx,
		                                 top, bottom, current_mark.write_cursor,
		                                 0xFFFF0000);
	}
}

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hinstance,
                     [[__maybe_unused__]] HINSTANCE hprevinstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine, [[__maybe_unused__]] int nCmdShow)
{
	LARGE_INTEGER perf_count_frequency_result;
	QueryPerformanceFrequency(&perf_count_frequency_result);
	global_perf_count_frequency = perf_count_frequency_result.QuadPart;

	// sets the scheduler granularity to 1ms, so that our Sleep() can be more granular
	unsigned desire_scheduler_ms = 1;
	bool sleep_granular = timeBeginPeriod(desire_scheduler_ms) == TIMERR_NOERROR;

	win_xinput_load();

	WNDCLASSA winclass = {
		.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc = win_main_window_proc,
		.hInstance = hinstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	win_resize_dib_section(&global_back_buffer, 1200, 700);

	constexpr unsigned frames_of_audio_latency = 3;
	constexpr unsigned monitorhz = 60;
	constexpr unsigned gamehz = monitorhz / 2;
	float target_secs_per_frame = 1.0f / (float)gamehz;

	if (!RegisterClassA(&winclass)) {
		tix_loge("error registering the window class");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(0, winclass.lpszClassName, "Handmade Hero",
	                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
	                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
	                                 nullptr, hinstance, nullptr);
	if (!winhandle) {
		tix_loge("error creating the window");
		return EXIT_FAILURE;
	}

	HDC dchandle = GetDC(winhandle);

	Win_SoundOutput soundout = {
		.samples_per_sec = 48000,
		.bytes_per_sample = sizeof(uint16_t) * 2,
	};
	soundout.buffsize = soundout.samples_per_sec * soundout.bytes_per_sample;
	soundout.latency_sample_count = frames_of_audio_latency * soundout.samples_per_sec / gamehz;

	win_sound_init(winhandle, soundout.samples_per_sec, soundout.buffsize);
	win_sound_clear_buffer(&soundout);

	if (FAILED(IDirectSoundBuffer_Play(secbuffer, 0, 0, DSBPLAY_LOOPING))) {
		tix_loge("Error playing dsound secondary buffer");
	}

	is_global_running = true;

#if 0
	// NOTE(fredy): debug the play/write cursor update frequency
	// on the handmade hero machine it was 480 samples
	while (is_global_running) {
		unsigned long play_cursor;
		unsigned long write_cursor;
		IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor, &write_cursor);

		char text_buffer[256];
		sprintf(text_buffer, "PC: %lu, WC: %lu", play_cursor, write_cursor);
		OutputDebugStringA(text_buffer);
	}
#endif // Debug play/write cursor update latency

	int16_t *samples = (int16_t *)VirtualAlloc(nullptr, soundout.buffsize,
	                                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Game_Memory game_memory = {};
	game_memory.permsize = MB_TO_BYTE(64);
	game_memory.transize = GB_TO_BYTE(1);
	size_t total_size = game_memory.permsize + game_memory.transize;
	game_memory.permstorage =
		VirtualAlloc(BASE_ADDRESS, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	game_memory.transtorage = (uint8_t *)game_memory.permstorage + game_memory.permsize;

	if (!samples || !game_memory.permstorage || !game_memory.transtorage) {
		return EXIT_FAILURE;
	}

	Game_SoundBuffer soundbuff = {
		.samples_per_sec = soundout.samples_per_sec,
		.samples = samples,
	};

	Game_Input inputs[2] = {};
	Game_Input *new_input = &inputs[0];
	Game_Input *old_input = &inputs[1];

	LARGE_INTEGER last_counter = win_clock_get_wall();

	constexpr size_t debug_last_cursor_marks_size = gamehz / 2;
	size_t debug_last_cursor_mark_index = 0;
	Win_DebugTimeMark debug_last_cursor_marks[debug_last_cursor_marks_size] = {};

	unsigned long last_play_cursor = 0;
	bool is_sound_valid = false;

	size_t last_cycle_count = __rdtsc();
	while (is_global_running) {
		Game_ControllerInput *old_keyboard_controller =
			game_input_get_controller(old_input, 0);
		Game_ControllerInput *new_keyboard_controller =
			game_input_get_controller(new_input, 0);
		Game_ControllerInput zero_controller = {};
		*new_keyboard_controller = zero_controller;
		new_keyboard_controller->connected = true;

		for (size_t i = 0; i < GAME_MAX_BUTTONS; ++i) {
			new_keyboard_controller->buttons[i].ended_down =
				old_keyboard_controller->buttons[i].ended_down;
		}

		win_process_messages(new_keyboard_controller);

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
				new_controller->connected = false;
				continue;
			}

			// Plugged in
			new_controller->connected = true;
			XINPUT_GAMEPAD *pad = &state.Gamepad;

			new_controller->analog = true;
			new_controller->stick_avg_x = win_input_process_stick_value(
				pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			new_controller->stick_avg_y = win_input_process_stick_value(
				pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

			if (new_controller->stick_avg_x != 0.0f ||
			    new_controller->stick_avg_y != 0.0f) {
				new_controller->analog = true;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
				new_controller->stick_avg_y = 1.0f;
				new_controller->analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
				new_controller->stick_avg_y = -1.0f;
				new_controller->analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
				new_controller->stick_avg_x = -1.0f;
				new_controller->analog = false;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
				new_controller->stick_avg_x = 1.0f;
				new_controller->analog = false;
			}

			float threshold = 0.5f;
			win_input_process_digital_button(
				new_controller->stick_avg_x < -threshold ? 1 : 0,
				&old_controller->moveleft, 1, &new_controller->moveleft);
			win_input_process_digital_button(
				new_controller->stick_avg_x > threshold ? 1 : 0,
				&old_controller->moveright, 1, &new_controller->moveright);
			win_input_process_digital_button(
				new_controller->stick_avg_y < -threshold ? 1 : 0,
				&old_controller->movedown, 1, &new_controller->movedown);
			win_input_process_digital_button(
				new_controller->stick_avg_y > threshold ? 1 : 0,
				&old_controller->moveup, 1, &new_controller->moveup);

			win_input_process_digital_button(pad->wButtons, &old_controller->actiondown,
			                                 XINPUT_GAMEPAD_A,
			                                 &new_controller->actiondown);
			win_input_process_digital_button(pad->wButtons,
			                                 &old_controller->actionright,
			                                 XINPUT_GAMEPAD_B,
			                                 &new_controller->actionright);
			win_input_process_digital_button(pad->wButtons, &old_controller->actionleft,
			                                 XINPUT_GAMEPAD_X,
			                                 &new_controller->actionleft);
			win_input_process_digital_button(pad->wButtons, &old_controller->actionup,
			                                 XINPUT_GAMEPAD_Y,
			                                 &new_controller->actionup);

			win_input_process_digital_button(pad->wButtons,
			                                 &old_controller->left_shoulder,
			                                 XINPUT_GAMEPAD_LEFT_SHOULDER,
			                                 &new_controller->left_shoulder);
			win_input_process_digital_button(pad->wButtons,
			                                 &old_controller->right_shoulder,
			                                 XINPUT_GAMEPAD_RIGHT_SHOULDER,
			                                 &new_controller->right_shoulder);

			win_input_process_digital_button(pad->wButtons, &old_controller->start,
			                                 XINPUT_GAMEPAD_START,
			                                 &new_controller->start);
			win_input_process_digital_button(pad->wButtons, &old_controller->back,
			                                 XINPUT_GAMEPAD_BACK,
			                                 &new_controller->back);
		}
		size_t byte_to_lock = 0;
		size_t bytes_to_write = 0;
		size_t target_cursor = 0;
		if (is_sound_valid) {
			// NOTE(fredy): Compute how much sound to write and where
			byte_to_lock = (soundout.running_sample_index * soundout.bytes_per_sample) %
			               soundout.buffsize;
			target_cursor =
				(last_play_cursor + (unsigned long)(soundout.latency_sample_count *
			                                            soundout.bytes_per_sample)) %
				soundout.buffsize;

			if (byte_to_lock > target_cursor) {
				bytes_to_write = soundout.buffsize - byte_to_lock;
				bytes_to_write += target_cursor;
			} else {
				bytes_to_write = target_cursor - byte_to_lock;
			}
		}

		soundbuff.sample_count = bytes_to_write / soundout.bytes_per_sample;

		Game_OffScreenBuffer screenbuff = {
			.memory = global_back_buffer.memory,
			.width = global_back_buffer.width,
			.height = global_back_buffer.height,
			.pitch = global_back_buffer.pitch,
		};
		game_update_and_render(&game_memory, new_input, &screenbuff, &soundbuff);

		if (is_sound_valid) {
#ifdef DEBUG
			{
				unsigned long play_cursor;
				unsigned long write_cursor;
				IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
				                                      &write_cursor);
				if (byte_to_lock <= write_cursor) {
					tix_loge(
						"LPC: %lu, BTL: %zu, TC: %zu, BTW: %zu - PC: %lu, WC: %lu",
						last_play_cursor, byte_to_lock, target_cursor,
						bytes_to_write, play_cursor, write_cursor);
				}
			}
#endif
			win_sound_fill_buffer(&soundout, byte_to_lock, bytes_to_write, &soundbuff);
		}

		LARGE_INTEGER work_counter = win_clock_get_wall();
		float work_secs_elapsed = win_clock_elapsed_secs(last_counter, work_counter);

		float secs_elapsed_for_frame = work_secs_elapsed;
		if (secs_elapsed_for_frame < target_secs_per_frame) {
			if (sleep_granular) {
				unsigned long sleep_ms =
					(unsigned long)(1000.0f * (target_secs_per_frame -
				                                   secs_elapsed_for_frame));
				if (sleep_ms > 0) {
					Sleep(sleep_ms);
				}
			}

			while (secs_elapsed_for_frame < target_secs_per_frame) {
				secs_elapsed_for_frame =
					win_clock_elapsed_secs(last_counter, win_clock_get_wall());
			}
		} else {
			// missed frame rate!
		}

		LARGE_INTEGER end_counter = win_clock_get_wall();
		float ms_per_frame = 1000.0f * win_clock_elapsed_secs(last_counter, end_counter);
		last_counter = end_counter;

		Win_WindowDimensions windim = win_window_get_dimensions(winhandle);

#ifdef DEBUG
		win_debug_sync_display(&global_back_buffer, debug_last_cursor_marks_size,
		                       debug_last_cursor_marks, &soundout, target_secs_per_frame);
#endif

		win_buffer_display_in_window(&global_back_buffer, dchandle, windim.width,
		                             windim.height);

		unsigned long play_cursor;
		unsigned long write_cursor;
		if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
		                                                    &write_cursor))) {
			last_play_cursor = play_cursor;
			if (!is_sound_valid) {
				soundout.running_sample_index =
					write_cursor / soundout.bytes_per_sample;
				is_sound_valid = true;
			}
		} else {
			is_sound_valid = false;
			tix_loge("Error getting current position of dsound secondary buffer");
		}

#ifdef DEBUG
		{
			if (is_sound_valid) {
				assert(debug_last_cursor_mark_index < debug_last_cursor_marks_size);
				Win_DebugTimeMark *mark =
					&debug_last_cursor_marks[debug_last_cursor_mark_index];
				mark->play_cursor = play_cursor;
				mark->write_cursor = write_cursor;
				debug_last_cursor_mark_index = (debug_last_cursor_mark_index + 1) %
				                               debug_last_cursor_marks_size;
			}
		}
#endif

		Game_Input *temp = new_input;
		new_input = old_input;
		old_input = temp;

		uint64_t end_cycle_count = __rdtsc();
		uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
		last_cycle_count = end_cycle_count;

		float fps = (float)1000 / (float)ms_per_frame;
		float mega_cycles_per_frame = (float)cycles_elapsed / 1000000.0f;

		tix_logi("%fms/f, %ff/s, %fmc/f", (double)ms_per_frame, (double)fps,
		         (double)mega_cycles_per_frame);
	}

	return EXIT_SUCCESS;
}
