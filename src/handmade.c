#include <basetsd.h>
#include <dsound.h>
#include <libloaderapi.h>
#include <limits.h>
#include <math.h>
#include <playsoundapi.h>
#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <winerror.h>
#include <xinput.h>

#define Pi32 3.14159265359f
typedef int64_t bool64;

struct Win_WindowDimensions {
	long width;
	long height;
};

struct Win_OffScreenBuffer {
	BITMAPINFO bitmap_info;
	void *memory;
	long width;
	long height;
	long pitch; // size of a row in bytes
};

static boolean global_running = false;
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

static void win_xinput_load(void)
{
	HMODULE xinput_lib = LoadLibraryA("xinput1_4.dll");
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

struct Win_SoundOutput {
	size_t samples_per_sec;
	size_t tone_hz;
	float_t tone_volume;
	size_t running_sample_index;
	size_t wave_period;
	size_t bytes_per_sample;
	size_t buffer_size;
};

static void win_sound_fill_buffer(struct Win_SoundOutput *sound_output, size_t byte_to_lock,
                                  size_t bytes_to_write)
{
	void *region_one;
	DWORD region_one_size;
	void *region_two;
	DWORD region_two_size;

	if (FAILED(IDirectSoundBuffer_Lock(secbuffer, byte_to_lock, bytes_to_write, &region_one,
	                                   &region_one_size, &region_two, &region_two_size, 0))) {
		// TODO(fredy): diagnostic
		OutputDebugStringA("Error locking dsound secondary buffer");
	}

	DWORD region_sample_count = region_one_size / sound_output->bytes_per_sample;
	int16_t *sample_out = (int16_t *)region_one;
	float_t sample_value = 0;
	for (DWORD i = 0; i < region_sample_count; ++i) {
		float_t t = 2.0f * Pi32 * (float_t)(sound_output->running_sample_index) /
		            (float_t)sound_output->wave_period;
		float_t sine_value = sinf(t);
		sample_value = sine_value * sound_output->tone_volume;
		*sample_out = (int16_t)sample_value; // channel one
		++sample_out;
		*sample_out = (int16_t)sample_value; // channel two
		++sample_out;

		++(sound_output->running_sample_index);
	}

	sample_out = (int16_t *)region_two;
	region_sample_count = region_two_size / sound_output->bytes_per_sample;
	for (DWORD i = 0; i < region_sample_count; ++i) {
		float_t t = 2.0f * Pi32 * (float_t)(sound_output->running_sample_index) /
		            (float_t)sound_output->wave_period;
		float_t sine_value = sinf(t);
		sample_value = sine_value * sound_output->tone_volume;
		*sample_out = (int16_t)sample_value; // channel one
		++sample_out;
		*sample_out = (int16_t)sample_value; // channel two
		++sample_out;

		++(sound_output->running_sample_index);
	}

	if (FAILED(IDirectSoundBuffer_Unlock(secbuffer, region_one, region_one_size, region_two,
	                                     region_two_size))) {
		// TODO(fredy): diagnostic
		OutputDebugStringA("Error unlocking dsound secondary buffer");
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

static void render_weird_gradient(struct Win_OffScreenBuffer *buffer, long blue_offset,
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
	case WM_SIZE: {
		OutputDebugStringA("WM_SIZE\n");
	} break;
	case WM_CLOSE: {
		OutputDebugStringA("WM_CLOSE\n");
		global_running = false;
	} break;
	case WM_DESTROY: {
		OutputDebugStringA("WM_DESTROY\n");
		global_running = false;
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		DWORD vk_code = (DWORD)wparam;
		boolean was_down = ((lparam & (1 << 30)) != 0);
		boolean is_down = ((lparam & (1 << 31)) == 0);

		if (was_down != is_down) {
			if (vk_code == 'W') {
			} else if (vk_code == 'A') {
			} else if (vk_code == 'S') {
			} else if (vk_code == 'D') {
			} else if (vk_code == 'Q') {
			} else if (vk_code == 'E') {
			} else if (vk_code == VK_UP) {
			} else if (vk_code == VK_LEFT) {
			} else if (vk_code == VK_DOWN) {
			} else if (vk_code == VK_RIGHT) {
			} else if (vk_code == VK_ESCAPE) {
			} else if (vk_code == VK_SPACE) {
			}
		}
		bool64 alt_key_was_down = (lparam & (1 << 29));
		if ((vk_code == VK_F4) && alt_key_was_down) {
			global_running = false;
		}
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
		result = DefWindowProc(winhandle, msg, wparam, lparam);
	} break;
	}

	return result;
}

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hinstance,
                     [[__maybe_unused__]] HINSTANCE hprevinstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine, [[__maybe_unused__]] int nCmdShow)
{
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
		// TODO(fredy): Logging
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

	// NOTE(fredy): render test
	int x_offset = 0;
	int y_offset = 0;

	// NOTE(fredy): sound test
	struct Win_SoundOutput sound_output = {
		.samples_per_sec = 48000,
		.tone_hz = 256,
		.running_sample_index = 0,
		.bytes_per_sample = sizeof(uint16_t) * 2,
		.tone_volume = 3000,
	};
	sound_output.wave_period = sound_output.samples_per_sec / sound_output.tone_hz;
	sound_output.buffer_size = sound_output.samples_per_sec * sound_output.bytes_per_sample;

	win_sound_init(winhandle, sound_output.samples_per_sec, sound_output.buffer_size);

	bool is_sound_playing = false;
	global_running = true;
	while (global_running) {
		MSG msg;
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				global_running = false;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		for (WORD i = 0; i < XUSER_MAX_COUNT; ++i) {
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
			[[__maybe_unused__]] WORD start = (pad->wButtons & XINPUT_GAMEPAD_START);
			[[__maybe_unused__]] WORD back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
			[[__maybe_unused__]] WORD left_shoulder =
				(pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			[[__maybe_unused__]] WORD right_shoulder =
				(pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			[[__maybe_unused__]] WORD a_button = (pad->wButtons & XINPUT_GAMEPAD_A);
			[[__maybe_unused__]] WORD b_button = (pad->wButtons & XINPUT_GAMEPAD_B);
			[[__maybe_unused__]] WORD x_button = (pad->wButtons & XINPUT_GAMEPAD_X);
			[[__maybe_unused__]] WORD y_button = (pad->wButtons & XINPUT_GAMEPAD_Y);

			SHORT stick_x = pad->sThumbLX;
			SHORT stick_y = pad->sThumbLY;

			x_offset += stick_x >> 12;
			y_offset += stick_y >> 12;
		}

		render_weird_gradient(&global_back_buffer, x_offset, y_offset);

		++x_offset;
		y_offset += 2;

		DWORD play_cursor;
		DWORD write_cursor;
		if (FAILED(IDirectSoundBuffer_GetCurrentPosition(secbuffer, &play_cursor,
		                                                 &write_cursor))) {
			// TODO(fredy): diagnostic
			OutputDebugStringA(
				"Error getting current position of dsound secondary buffer");
		}

		size_t byte_to_lock =
			(sound_output.running_sample_index * sound_output.bytes_per_sample) %
			sound_output.buffer_size;
		size_t bytes_to_write;
		if (byte_to_lock == play_cursor) {
			bytes_to_write = sound_output.buffer_size;
		} else if (byte_to_lock > play_cursor) {
			bytes_to_write = sound_output.buffer_size - byte_to_lock;
			bytes_to_write += play_cursor;
		} else {
			bytes_to_write = play_cursor - byte_to_lock;
		}

		win_sound_fill_buffer(&sound_output, byte_to_lock, bytes_to_write);

		if (!is_sound_playing) {
			if (FAILED(IDirectSoundBuffer_Play(secbuffer, 0, 0, DSBPLAY_LOOPING))) {
				OutputDebugStringA("Error playing dsound secondary buffer");
			}
			is_sound_playing = true;
		}

		struct Win_WindowDimensions windim = win_window_get_dimensions(winhandle);
		win_buffer_display_in_window(&global_back_buffer, dchandle, windim.width,
		                             windim.height);
	}

	return EXIT_SUCCESS;
}
