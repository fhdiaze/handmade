/*
* Windows platform code
*/

#include "win_handmade.h"
#include "game.c"
#include <dsound.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>

static bool global_running = false;
static struct Win_OffScreenBuffer global_back_buffer;
static LPDIRECTSOUNDBUFFER secbuffer;

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

Plat_ReadFileResult plat_debug_readfile(char *filename)
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

bool plat_debug_writefile(char *filename, size_t memorysize, void *memory)
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

static void win_sound_clear_buffer(struct Win_SoundOutput *sound_output)
{
	void *region_one;
	DWORD region_one_size;
	void *region_two;
	DWORD region_two_size;
	if (FAILED(IDirectSoundBuffer_Lock(secbuffer, 0, sound_output->buffsize, &region_one,
	                                   &region_one_size, &region_two, &region_two_size, 0))) {
		OutputDebugStringA("Error locking dsound secondary buffer");
	}

	size_t bytes_count = region_one_size;
	int8_t *byte_out = (int8_t *)region_one;
	for (size_t i = 0; i < bytes_count; ++i) {
		*byte_out = 0;
		++byte_out;
	}

	byte_out = (int8_t *)region_two;
	bytes_count = region_two_size;
	for (size_t i = 0; i < bytes_count; ++i) {
		*byte_out = 0;
		++byte_out;
	}

	if (FAILED(IDirectSoundBuffer_Unlock(secbuffer, region_one, region_one_size, region_two,
	                                     region_two_size))) {
		OutputDebugStringA("Error unlocking dsound secondary buffer");
	}
}

static void win_sound_fill_buffer(struct Win_SoundOutput *soundout, size_t byte_to_lock,
                                  size_t bytes_to_write, Game_SoundBuffer *soundbuff)
{
	void *region_one;
	DWORD region_one_size;
	void *region_two;
	DWORD region_two_size;

	if (FAILED(IDirectSoundBuffer_Lock(secbuffer, byte_to_lock, bytes_to_write, &region_one,
	                                   &region_one_size, &region_two, &region_two_size, 0))) {
		OutputDebugStringA("Error locking dsound secondary buffer");
	}

	DWORD region_sample_count = region_one_size / soundout->sample_size;
	int16_t *sample_out = (int16_t *)region_one;
	int16_t *sample_in = soundbuff->samples;
	for (DWORD i = 0; i < region_sample_count; ++i) {
		*sample_out = *sample_in; // channel one
		++sample_out;
		++sample_in;
		*sample_out = *sample_in; // channel two
		++sample_out;
		++sample_in;

		++(soundout->running_sample_index);
	}

	sample_out = (int16_t *)region_two;
	region_sample_count = region_two_size / soundout->sample_size;
	for (DWORD i = 0; i < region_sample_count; ++i) {
		*sample_out = *sample_in; // channel one
		++sample_out;
		++sample_in;
		*sample_out = *sample_in; // channel two
		++sample_out;
		++sample_in;

		++(soundout->running_sample_index);
	}

	if (FAILED(IDirectSoundBuffer_Unlock(secbuffer, region_one, region_one_size, region_two,
	                                     region_two_size))) {
		OutputDebugStringA("Error unlocking dsound secondary buffer");
	}
}

static void win_input_process_keyboard_msg(Game_ButtonState *newstate, bool is_down)
{
	newstate->ended_down = is_down;
	++newstate->half_transition_count;
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
			global_running = false;
		} break;
		case WM_SYSKEYDOWN: {
		} break;
		case WM_SYSKEYUP: {
			DWORD vk_code = (DWORD)msg.wParam;
			bool was_down = ((msg.lParam & (1 << 30)) != 0);
			bool is_down = ((msg.lParam & (1 << 31)) == 0);

			if (was_down != is_down) {
				if (vk_code == 'W') {
				} else if (vk_code == 'A') {
				} else if (vk_code == 'S') {
				} else if (vk_code == 'D') {
				} else if (vk_code == 'Q') {
					win_input_process_keyboard_msg(
						&keyboard_controller->left_shoulder, is_down);
				} else if (vk_code == 'E') {
					win_input_process_keyboard_msg(
						&keyboard_controller->right_shoulder, is_down);
				} else if (vk_code == VK_UP) {
					win_input_process_keyboard_msg(&keyboard_controller->up,
					                               is_down);
				} else if (vk_code == VK_LEFT) {
					win_input_process_keyboard_msg(&keyboard_controller->left,
					                               is_down);
				} else if (vk_code == VK_DOWN) {
					win_input_process_keyboard_msg(&keyboard_controller->down,
					                               is_down);
				} else if (vk_code == VK_RIGHT) {
					win_input_process_keyboard_msg(&keyboard_controller->right,
					                               is_down);
				} else if (vk_code == VK_ESCAPE) {
					global_running = false;
				} else if (vk_code == VK_SPACE) {
				}
			}
			bool alt_key_was_down = (msg.lParam & (1 << 29)) != 0;
			if ((vk_code == VK_F4) && alt_key_was_down) {
				global_running = false;
			}
		} break;
		case WM_KEYDOWN: {
		} break;
		case WM_KEYUP: {
		} break;
		default: {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} break;
		}
	}
}

static struct Win_WindowDimensions win_window_get_dimensions(HWND winhandle)
{
	struct Win_WindowDimensions result;
	RECT client_rec;
	GetClientRect(winhandle, &client_rec);
	result.width = client_rec.right - client_rec.left;
	result.height = client_rec.bottom - client_rec.top;

	return result;
}

/*
 * dib: device independent bitmap
 */
static void win_resize_dib_section(struct Win_OffScreenBuffer *buffer, long win_width,
                                   long win_height)
{
	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = win_width;
	buffer->height = win_height;

	long bytes_per_pixel = 4;

	buffer->bitmap_info.bmiHeader.biSize = sizeof(buffer->bitmap_info.bmiHeader);
	buffer->bitmap_info.bmiHeader.biWidth = buffer->width;
	// NOTE(fredy): top-down layout (opposite to bottom-up).
	// The first three bytes on the bitmap are for the top-left pixel
	buffer->bitmap_info.bmiHeader.biHeight = -buffer->height;
	buffer->bitmap_info.bmiHeader.biPlanes = 1;
	buffer->bitmap_info.bmiHeader.biBitCount = 32;
	buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;

	long bitmap_memory_size = buffer->width * buffer->height * bytes_per_pixel;

	buffer->memory = VirtualAlloc(nullptr, (size_t)bitmap_memory_size, MEM_RESERVE | MEM_COMMIT,
	                              PAGE_READWRITE);
	buffer->pitch = buffer->width * bytes_per_pixel;
}

static void win_buffer_display_in_window(struct Win_OffScreenBuffer *buffer, HDC dchandle,
                                         long win_width, long win_height)
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
		global_running = false;
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_DESTROY: {
		global_running = false;
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		assert(false && "We are supposed to be processing the keyboard in other place");
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC dchandle = BeginPaint(winhandle, &paint);
		struct Win_WindowDimensions windim = win_window_get_dimensions(winhandle);
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

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hinstance,
                     [[__maybe_unused__]] HINSTANCE hprevinstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine, [[__maybe_unused__]] int nCmdShow)
{
	LARGE_INTEGER perf_count_frequency_result;
	QueryPerformanceFrequency(&perf_count_frequency_result);
	int64_t perf_count_frequency = perf_count_frequency_result.QuadPart;

	win_xinput_load();

	WNDCLASSA window_class = {
		.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc = win_main_window_proc,
		.hInstance = hinstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	win_resize_dib_section(&global_back_buffer, 1200, 700);

	ATOM main_window_atom = RegisterClassA(&window_class);
	if (!main_window_atom) {
		OutputDebugStringA("error");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(0, MAKEINTATOM(main_window_atom), "Handmade Hero",
	                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
	                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
	                                 nullptr, hinstance, nullptr);
	if (!winhandle) {
		// TODO(fredy): Logging
		OutputDebugStringA("error");
		return EXIT_FAILURE;
	}

	HDC dchandle = GetDC(winhandle);

	// NOTE(fredy): sound test
	struct Win_SoundOutput soundout = {
		.samples_per_sec = 48000,
		.sample_size = sizeof(uint16_t) * 2,
	};
	soundout.buffsize = soundout.samples_per_sec * soundout.sample_size;
	soundout.latency_sample_count = soundout.samples_per_sec / 15;

	win_sound_init(winhandle, soundout.samples_per_sec, soundout.buffsize);
	win_sound_clear_buffer(&soundout);

	if (FAILED(IDirectSoundBuffer_Play(secbuffer, 0, 0, DSBPLAY_LOOPING))) {
		OutputDebugStringA("Error playing dsound secondary buffer");
	}

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

	global_running = true;
	LARGE_INTEGER last_counter;
	QueryPerformanceCounter(&last_counter);
	uint64_t last_cycle_count = __rdtsc();
	Game_Input inputs[2] = {};
	Game_Input *new_input = &inputs[0];
	Game_Input *old_input = &inputs[1];
	while (global_running) {
		Game_ControllerInput *keyboard_controller = &new_input->controllers[0];
		Game_ControllerInput zero = {};
		*keyboard_controller = zero;

		win_process_messages(keyboard_controller);

		WORD max_controller_count = XUSER_MAX_COUNT;
		if (max_controller_count > game_max_controllers) {
			max_controller_count = game_max_controllers;
		}

		for (WORD i = 0; i < max_controller_count; ++i) {
			Game_ControllerInput *old_controller = &old_input->controllers[i];
			Game_ControllerInput *new_controller = &new_input->controllers[i];
			XINPUT_STATE state;
			if (XInputGetState(i, &state) != ERROR_SUCCESS) {
				continue;
			}

			// Plugged in
			XINPUT_GAMEPAD *pad = &state.Gamepad;

			[[__maybe_unused__]] WORD up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
			[[__maybe_unused__]] WORD down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			[[__maybe_unused__]] WORD left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			[[__maybe_unused__]] WORD right =
				(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

			new_controller->analog = true;
			new_controller->startx = old_controller->startx;
			new_controller->starty = old_controller->starty;

			float stickx_lim = pad->sThumbLX < 0 ? 32768.0f : 32767.0f;
			float sticky_lim = pad->sThumbLY < 0 ? 32768.0f : 32767.0f;
			[[__maybe_unused__]] float stickx = (float)pad->sThumbLX / stickx_lim;
			[[__maybe_unused__]] float sticky = (float)pad->sThumbLY / sticky_lim;

			new_controller->minx = new_controller->maxx = new_controller->endx = stickx;
			new_controller->miny = new_controller->maxy = new_controller->endy = sticky;

			win_input_process_digital_button(pad->wButtons, &old_controller->down,
			                                 XINPUT_GAMEPAD_A, &new_controller->down);
			win_input_process_digital_button(pad->wButtons, &old_controller->right,
			                                 XINPUT_GAMEPAD_B, &new_controller->right);
			win_input_process_digital_button(pad->wButtons, &old_controller->left,
			                                 XINPUT_GAMEPAD_X, &new_controller->left);
			win_input_process_digital_button(pad->wButtons, &old_controller->up,
			                                 XINPUT_GAMEPAD_Y, &new_controller->up);
			win_input_process_digital_button(pad->wButtons,
			                                 &old_controller->left_shoulder,
			                                 XINPUT_GAMEPAD_LEFT_SHOULDER,
			                                 &new_controller->left_shoulder);
			win_input_process_digital_button(pad->wButtons,
			                                 &old_controller->right_shoulder,
			                                 XINPUT_GAMEPAD_RIGHT_SHOULDER,
			                                 &new_controller->right_shoulder);

			//[[__maybe_unused__]] WORD start = (pad->wButtons & XINPUT_GAMEPAD_START);
			//[[__maybe_unused__]] WORD back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
		}

		DWORD play_cursor;
		DWORD write_cursor;
		unsigned is_sound_valid = true;
		if (FAILED(IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
		                                                 &write_cursor))) {
			is_sound_valid = false;
			OutputDebugStringA(
				"Error getting current position of dsound secondary buffer");
		}

		DWORD target_cursor = (play_cursor + (DWORD)(soundout.latency_sample_count *
		                                             soundout.sample_size)) %
		                      soundout.buffsize;
		size_t byte_to_lock =
			(soundout.running_sample_index * soundout.sample_size) % soundout.buffsize;
		size_t bytes_to_write;

		if (byte_to_lock > target_cursor) {
			bytes_to_write = soundout.buffsize - byte_to_lock;
			bytes_to_write += target_cursor;
		} else {
			bytes_to_write = target_cursor - byte_to_lock;
		}

		soundbuff.sample_count = bytes_to_write / soundout.sample_size;

		Game_OffScreenBuffer screenbuff = {
			.memory = global_back_buffer.memory,
			.width = global_back_buffer.width,
			.height = global_back_buffer.height,
			.pitch = global_back_buffer.pitch,
		};
		game_update_and_render(&game_memory, new_input, &screenbuff, &soundbuff);

		if (is_sound_valid) {
			win_sound_fill_buffer(&soundout, byte_to_lock, bytes_to_write, &soundbuff);
		}

		struct Win_WindowDimensions windim = win_window_get_dimensions(winhandle);
		win_buffer_display_in_window(&global_back_buffer, dchandle, windim.width,
		                             windim.height);

		uint64_t end_cycle_count = __rdtsc();

		LARGE_INTEGER end_counter;
		QueryPerformanceCounter(&end_counter);

		uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
		int64_t counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
		float_t ms_per_frame =
			1000.0f * (float_t)counter_elapsed / (float_t)perf_count_frequency;
		float_t fps = (float_t)ms_per_frame / (float_t)counter_elapsed;
		float_t mcpf = (float_t)cycles_elapsed / 1000000.0f;

		char perf_buffer[256];
		sprintf(perf_buffer, "%fms/f, %ff/s, %fmc/f", ms_per_frame, fps, mcpf);

		OutputDebugStringA(perf_buffer);

		last_counter = end_counter;
		last_cycle_count = end_cycle_count;

		Game_Input *temp = new_input;
		new_input = old_input;
		old_input = temp;
	}

	return EXIT_SUCCESS;
}
