/*
* Windows platform code
*/

#undef LIB_LOG_LEVEL
#define LIB_LOG_LEVEL LIB_LOG_LEVEL_DEBUG

#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>

#include "app.h"
#include "lib.h"

#define TMP_APP_DLL_FILENAME_FMT "handame_app_tmp_%lu.dll"

#define XINPUT_GET_STATE(name) \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, [[__maybe_unused__]] XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(xinput_get_state_func);
XINPUT_GET_STATE(xinput_get_state_stub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_get_state_func *xinput_get_state = xinput_get_state_stub;
#define XInputGetState xinput_get_state

#define XINPUT_SET_STATE(name) \
	DWORD WINAPI name([[__maybe_unused__]] DWORD dwUserIndex, [[__maybe_unused__]] XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE(xinput_set_state_func);
XINPUT_SET_STATE(xinput_set_state_stub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static xinput_set_state_func *xinput_set_state = xinput_set_state_stub;
#define XInputSetState xinput_set_state

#define DSOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(direct_sound_create_func);

#define MAX_FILE_PATH MAX_PATH
#define REPLAY_MAX_SLOTS 4
#define REPLAY_NO_SLOT UINT8_MAX

#define LODWORD(l) ((unsigned long)(((size_t)(l)) & 0xFFFFFFFF))
#define HIDWORD(l) ((unsigned long)((((size_t)(l)) >> (sizeof(unsigned) * CHAR_BIT)) & 0xFFFFFFFF))

typedef struct WindowDimensions {
	long width;
	long height;
} WindowDimensions;

/**
 * @brief (0,0) is on the top left corner.
 * The byte order in a register (little endian) is AA RR GG BB
 */
typedef struct WinOffscreenBuffer {
	unsigned width_px;
	unsigned height_px;
	unsigned pitch_bytes; // size of a row in bytes
	unsigned bytes_per_pixel;
	void *top_left_px;
	BITMAPINFO info;
} WinOffscreenBuffer;

typedef struct WinSoundOutput {
	size_t running_sample_index;
	unsigned samples_per_sec;
	unsigned bytes_per_sample; // Size of the sample in bytes
	unsigned buffsize_bytes;
	unsigned safety_bytes;
} WinSoundOutput;

typedef struct DebugTimeMark {
	unsigned long output_play_cursor;
	unsigned long output_write_cursor;

	unsigned long flip_play_cursor;
	unsigned long flip_write_cursor;

	unsigned output_location;
	unsigned output_byte_count;

	unsigned frame_flip_byte;
} DebugTimeMark;

typedef struct EngineCode {
	HMODULE app_dll;

	/**
	 * @brief could be null, check before call it
	 */
	game_update_and_render_func *update_and_render;

	/**
	 * @brief could be null, check before call it
	 */
	sound_create_samples_func *sound_create_samples;

	FILETIME dll_write_time;

	uint8_t is_valid;
} EngineCode;

typedef struct ReplaySlot {
	HANDLE file_handle;
	HANDLE file_map;
	void *memory;
	char filepath[MAX_FILE_PATH];
} ReplaySlot;

typedef enum ReplayStatus : uint8_t {
	WIN_REPLAY_NORMAL,
	WIN_REPLAY_RECORD,
	WIN_REPLAY_RECORDED,
	WIN_REPLAY_PLAYBACK,
} ReplayStatus;

typedef struct WinState {
	size_t memory_size_bytes;
	void *memory_base_address;

	ReplaySlot replay_slots[REPLAY_MAX_SLOTS];
	HANDLE replay_file_handle;

	char *exe_path_last_slash;
	char exe_path[MAX_FILE_PATH];

	uint8_t replay_slot_index;
	ReplayStatus replay_status;
} WinState;

// globals
static uint32_t g_is_running = 0U;
static uint32_t g_is_pause = 0U;
static WinOffscreenBuffer g_win_back_buffer;
static LPDIRECTSOUNDBUFFER g_secbuffer;
static int64_t g_perf_count_frequency;
static uint32_t g_show_cursor_debug;
static WINDOWPLACEMENT g_window_position = {
	.length = sizeof(g_window_position),
};

static void window_toggle_fullscreen(HWND win_handle)
{
	uint32_t window_style = (uint32_t)GetWindowLong(win_handle, GWL_STYLE);
	if (window_style & WS_OVERLAPPEDWINDOW) {
		MONITORINFO monitor_info = {
			.cbSize = sizeof(monitor_info),
		};
		if (GetWindowPlacement(win_handle, &g_window_position) &&
		    GetMonitorInfo(MonitorFromWindow(win_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info)) {
			SetWindowLong(win_handle, GWL_STYLE, (int32_t)(window_style & ~(uint32_t)WS_OVERLAPPEDWINDOW));
			SetWindowPos(win_handle, HWND_TOP, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
			             monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
			             monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
			             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLong(win_handle, GWL_STYLE, (int32_t)(window_style | WS_OVERLAPPEDWINDOW));
		SetWindowPlacement(win_handle, &g_window_position);
		SetWindowPos(win_handle, nullptr, 0, 0, 0, 0,
		             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

FILE_FREE_DEBUG(file_free_debug)
{
	if (base_address) {
		VirtualFree(base_address, 0, MEM_RELEASE);
	}
}

FILE_READ_DEBUG(file_read_debug)
{
	ReadFileResult result = {};

	HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (handle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER filesize_struct;
		if (GetFileSizeEx(handle, &filesize_struct)) {
			uint32_t file_size_byte = i64_to_u32(filesize_struct.QuadPart);
			result.base_address =
				VirtualAlloc(nullptr, file_size_byte, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (result.base_address) {
				DWORD read_size_byte = 0;
				if (ReadFile(handle, result.base_address, file_size_byte, &read_size_byte, nullptr) ||
				    read_size_byte == file_size_byte) {
					result.size_byte = file_size_byte;
				} else {
					LOG_ERROR("failed to read the file: %s", path);

					file_free_debug(thread, result.base_address);

					result.base_address = nullptr;
					result.size_byte = 0;
				}
			} else {
				LOG_ERROR("failed to allocate memory for the content of file: %s", path);
			}
		} else {
			LOG_ERROR("failed to get the size of the file: %s", path);
		}

		CloseHandle(handle);
	} else {
		LOG_ERROR("failed to open the file: %s", path);
	}

	return result;
}

FILE_WRITE_DEBUG(file_write_debug)
{
	uint8_t result = 0U;

	HANDLE handle = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (handle != INVALID_HANDLE_VALUE) {
		DWORD byteswritten = 0;
		if (WriteFile(handle, base_address, (DWORD)memory_size_byte, &byteswritten, nullptr)) {
			result = byteswritten == memory_size_byte;
		} else {
			LOG_ERROR("failed to write to the file: %s", path);
		}

		CloseHandle(handle);
	} else {
		LOG_ERROR("failed to open the file: %s", path);
	}

	return result;
}

static uint8_t file_get_exe_path(WinState *winstate)
{
	unsigned long exe_path_length = GetModuleFileNameA(nullptr, winstate->exe_path, MAX_FILE_PATH);
	if (exe_path_length == 0 || exe_path_length == MAX_FILE_PATH) {
		LOG_ERROR("unable to get the executable path");
		return 0U;
	}

	winstate->exe_path_last_slash = winstate->exe_path + exe_path_length - 1;
	for (; winstate->exe_path_last_slash > winstate->exe_path; --winstate->exe_path_last_slash) {
		if (*winstate->exe_path_last_slash == '\\') {
			break;
		}
	}

	return 1U;
}

static void file_build_path(WinState *winstate, const char *const filename, const unsigned dest_count, char *dest)
{
	string_concat((size_t)(winstate->exe_path_last_slash - winstate->exe_path + 1), winstate->exe_path,
	              strlen(filename), filename, dest_count, dest);
}

/**
 * @brief
 *
 * @param winstate
 * @param slot_index
 * @param dest_count
 * @param dest
 * @return 1 on success, 0 if the building of the path failed.
 */
static uint32_t file_build_input_path(WinState *winstate, unsigned slot_index, unsigned dest_count, char *dest)
{
	uint32_t was_success = 0U;

	assert(slot_index < REPLAY_MAX_SLOTS);

	char filename[64];
	if (sprintf(filename, "loopedit_%d.hmi", slot_index) > 0) {
		was_success = 1U;
		file_build_path(winstate, filename, dest_count, dest);
	}

	return was_success;
}

/**
 * @brief
 *
 * @param file_path
 * @param result
 * @return The result code, 0 if error
 */
static inline uint32_t file_get_last_write_time(const char *const file_path, FILETIME *result)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(file_path, GetFileExInfoStandard, &data)) {
		LOG_ERROR("unable to check the timestamp of the file: %s", file_path);
		return 0U;
	}

	*result = data.ftLastWriteTime;

	return 1U;
}

/**
 * @brief
 *
 * @param enginedll_path
 * @param tmpdll_path
 * @param game_code
 * @return The result code, 0 if error
 */
static uint32_t code_load_app(EngineCode *game_code, const char *const enginedll_path, const char *const tmpdll_path,
                              const char *const enginedll_lock_path)
{
	uint32_t result_code = 0U;

	FILETIME dll_lock_last_write_time = {};
	if (file_get_last_write_time(enginedll_lock_path, &dll_lock_last_write_time)) {
		LOG_ERROR("unable to read the dll, waiting for pdb file");
		return result_code;
	}

	FILETIME dll_last_write_time = {};
	if (!file_get_last_write_time(enginedll_path, &dll_last_write_time)) {
		return result_code;
	}

	if (!CopyFileA(enginedll_path, tmpdll_path, 0U)) {
		DWORD error = GetLastError();
		LOG_ERROR("unable to copy the dll: '%s', error: %lu", enginedll_path, error);
		return result_code;
	}

	game_code->app_dll = LoadLibraryA(tmpdll_path);
	game_code->dll_write_time = dll_last_write_time;

	if (game_code->app_dll) {
		game_code->update_and_render =
			(game_update_and_render_func *)GetProcAddress(game_code->app_dll, "game_update_and_render");
		game_code->sound_create_samples =
			(sound_create_samples_func *)GetProcAddress(game_code->app_dll, "sound_create_samples");

		game_code->is_valid = game_code->sound_create_samples && game_code->update_and_render;
	}

	if (!game_code->is_valid) {
		game_code->sound_create_samples = nullptr;
		game_code->update_and_render = nullptr;

		LOG_ERROR("unable to load the dll: '%s'", enginedll_path);
	}

	assert(game_code->is_valid);

	return game_code->is_valid;
}

static uint8_t code_unload_game(EngineCode *game_code)
{
	if (game_code->app_dll) {
		if (!FreeLibrary(game_code->app_dll)) {
			return 0U;
		}
		game_code->app_dll = nullptr;
	}

	game_code->is_valid = 0U;
	game_code->sound_create_samples = nullptr;
	game_code->update_and_render = nullptr;

	return 1U;
}

static void xinput_load(void)
{
	HMODULE xinput_lib = LoadLibraryA("xinput1_4.dll");

	if (!xinput_lib) {
		xinput_lib = LoadLibraryA("xinput9_1_0.dll");
	}

	if (!xinput_lib) {
		xinput_lib = LoadLibraryA("xinput1_3.dll");
	}

	if (xinput_lib) {
		XInputGetState = (xinput_get_state_func *)GetProcAddress(xinput_lib, "XInputGetState");
		if (!XInputGetState) {
			XInputGetState = xinput_get_state_stub;
		}
		XInputSetState = (xinput_set_state_func *)GetProcAddress(xinput_lib, "XInputSetState");
		if (!XInputSetState) {
			XInputSetState = xinput_set_state_stub;
		}
	}
}

/*
* TODO(fredy): it looks like direct sound is deprecated
*/
static void sound_init(HWND winhandle, size_t samples_per_sec, size_t buffersize)
{
	HMODULE dsound_lib = LoadLibraryA("dsound.dll");
	if (!dsound_lib) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error loading dsound.dll");
		return;
	}

	direct_sound_create_func *dsound_create =
		(direct_sound_create_func *)GetProcAddress(dsound_lib, "DirectSoundCreate");
	if (!dsound_create) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error getting DirectSoundCreate function");
		return;
	}

	LPDIRECTSOUND direct_sound = nullptr;
	if (FAILED(dsound_create(nullptr, &direct_sound, nullptr))) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error creating the handler for direct sound");
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
		LOG_ERROR("Error setting the cooperative level for direct sound");
	}

	// NOTE(fredy): create the primary buffer
	DSBUFFERDESC primbufferdesc = {
		.dwSize = sizeof(primbufferdesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER,
	};
	LPDIRECTSOUNDBUFFER primbuffer = nullptr;
	if (FAILED(IDirectSound_CreateSoundBuffer(direct_sound, &primbufferdesc, &primbuffer, nullptr))) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error creating the primary buffer (handle for the sound card)");
	}

	if (FAILED(IDirectSoundBuffer_SetFormat(primbuffer, &waveformat))) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error setting the format for the primary buffer");
	}

	// NOTE(fredy): create the secondary buffer
	DSBUFFERDESC secbufferdesc = {
		.dwSize = sizeof(secbufferdesc),
		.dwBufferBytes = (DWORD)buffersize,
		.lpwfxFormat = &waveformat,
	};
	if (FAILED(IDirectSound_CreateSoundBuffer(direct_sound, &secbufferdesc, &g_secbuffer, nullptr))) {
		// TODO(fredy): diagnostic
		LOG_ERROR("Error creating the secondary buffer");
	}
}

static void sound_clear_buffer(WinSoundOutput *sound_output)
{
	void *region_one = nullptr;
	DWORD region_one_size = 0;
	void *region_two = nullptr;
	DWORD region_two_size = 0;
	if (FAILED(IDirectSoundBuffer_Lock(g_secbuffer, 0, sound_output->buffsize_bytes, &region_one, &region_one_size,
	                                   &region_two, &region_two_size, 0))) {
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

	if (FAILED(IDirectSoundBuffer_Unlock(g_secbuffer, region_one, region_one_size, region_two, region_two_size))) {
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
static void sound_fill_buffer(WinSoundOutput *soundout, size_t byte_to_lock, size_t bytes_to_write,
                              GameSoundBuffer *soundbuff)
{
	void *region_one;
	unsigned long region_one_size;
	void *region_two;
	unsigned long region_two_size;

	if (FAILED(IDirectSoundBuffer_Lock(g_secbuffer, byte_to_lock, bytes_to_write, &region_one, &region_one_size,
	                                   &region_two, &region_two_size, 0))) {
		OutputDebugStringA("Error locking dsound secondary buffer");
	}

	size_t region_sample_count = region_one_size / soundout->bytes_per_sample;
	int16_t *sample_out = (int16_t *)region_one;
	int16_t *sample_in = soundbuff->samples_base_address;
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

	if (FAILED(IDirectSoundBuffer_Unlock(g_secbuffer, region_one, region_one_size, region_two, region_two_size))) {
		OutputDebugStringA("Error unlocking dsound secondary buffer");
	}
}

static void keyboard_process_message(ButtonState *newstate, uint32_t is_down)
{
	if (newstate->ended_down != is_down) {
		newstate->ended_down = (uint8_t)is_down;
		++newstate->half_transition_count;
	}
}

/**
 * @brief Normalize xinput stick value to [-1.0, 1.0]
 *
 * @param value
 * @param dead_zone
 * @return float
 */
static float xinput_process_stick_value(short value, short dead_zone)
{
	if (value < -dead_zone) {
		return (float)value / 32768.0F;
	}
	if (value > dead_zone) {
		return (float)value / 32767.0F;
	}

	return 0.0F;
}

/**
 * @brief Processes a button from an xinput controller
 *
 * @param xinput_button_state
 * @param oldstate
 * @param buttonbit
 * @param newstate
 */
static void xinput_process_button(DWORD xinput_button_state, ButtonState *oldstate, DWORD buttonbit,
                                  ButtonState *newstate)
{
	newstate->ended_down = (xinput_button_state & buttonbit) == buttonbit;
	newstate->half_transition_count = oldstate->ended_down != newstate->ended_down;
}

static void input_begin_recording(WinState *winstate)
{
	ReplaySlot *replay_slot = &winstate->replay_slots[winstate->replay_slot_index];

	assert(replay_slot->memory);

	winstate->replay_file_handle = replay_slot->file_handle;

	assert(winstate->replay_file_handle);

	LARGE_INTEGER filepos;
	filepos.QuadPart = (long long)winstate->memory_size_bytes;
	SetFilePointerEx(winstate->replay_file_handle, filepos, nullptr, FILE_BEGIN);

	CopyMemory(replay_slot->memory, winstate->memory_base_address, winstate->memory_size_bytes);

	winstate->replay_status = WIN_REPLAY_RECORD;
}

static void input_end_recording(WinState *winstate)
{
	assert(winstate->replay_file_handle);
	winstate->replay_status = WIN_REPLAY_RECORDED;
}

static void input_begin_playback(WinState *winstate)
{
	ReplaySlot *replay_slot = &winstate->replay_slots[winstate->replay_slot_index];

	assert(replay_slot->memory);

	winstate->replay_file_handle = replay_slot->file_handle;

	assert(winstate->replay_file_handle);

	LARGE_INTEGER filepos;
	filepos.QuadPart = (long long)winstate->memory_size_bytes;
	SetFilePointerEx(winstate->replay_file_handle, filepos, nullptr, FILE_BEGIN);

	CopyMemory(winstate->memory_base_address, replay_slot->memory, winstate->memory_size_bytes);

	winstate->replay_status = WIN_REPLAY_PLAYBACK;
}

static void input_end_playback(WinState *winstate)
{
	assert(winstate->replay_file_handle);
	winstate->replay_status = WIN_REPLAY_NORMAL;
}

static void input_record(WinState *winstate, GameInput *input)
{
	assert(winstate->replay_file_handle);
	unsigned long bytes_written = 0;
	WriteFile(winstate->replay_file_handle, input, sizeof(*input), &bytes_written, nullptr);
}

static void input_playback(WinState *winstate, GameInput *input)
{
	assert(winstate->replay_file_handle);

	unsigned long bytes_read = 0;

	assert(ReadFile(winstate->replay_file_handle, input, sizeof(*input), &bytes_read, nullptr));

	if (!bytes_read) {
		input_end_playback(winstate);
		input_begin_playback(winstate);

		ReadFile(winstate->replay_file_handle, input, sizeof(*input), &bytes_read, nullptr);
	}

	assert(bytes_read == sizeof(*input));
}

static void window_pump_messages(WinState *winstate, ControllerState *keyboard_controller)
{
	MSG msg;
	while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
		case WM_QUIT: {
			g_is_running = 0U;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			size_t vk_code = (size_t)msg.wParam;
			size_t key_stroke_info = (size_t)msg.lParam;
			uint32_t was_down = (key_stroke_info & (1U << 30U)) != 0;
			uint32_t is_down = (key_stroke_info & (1UL << 31UL)) == 0;

			if (was_down != is_down) {
				if (vk_code == 'K') {
					keyboard_process_message(&keyboard_controller->moveup, is_down);
				} else if (vk_code == 'H') {
					keyboard_process_message(&keyboard_controller->moveleft, is_down);
				} else if (vk_code == 'J') {
					keyboard_process_message(&keyboard_controller->movedown, is_down);
				} else if (vk_code == 'L') {
					keyboard_process_message(&keyboard_controller->moveright, is_down);
				} else if (vk_code == 'Q') {
					keyboard_process_message(&keyboard_controller->left_shoulder, is_down);
				} else if (vk_code == 'E') {
					keyboard_process_message(&keyboard_controller->right_shoulder, is_down);
				} else if (vk_code == VK_CONTROL) {
					keyboard_process_message(&keyboard_controller->actionup, is_down);
				} else if (vk_code == VK_LEFT) {
					keyboard_process_message(&keyboard_controller->actionleft, is_down);
				} else if (vk_code == VK_DOWN) {
					keyboard_process_message(&keyboard_controller->actiondown, is_down);
				} else if (vk_code == VK_RIGHT) {
					keyboard_process_message(&keyboard_controller->actionright, is_down);
				} else if (vk_code == VK_ESCAPE) {
					keyboard_process_message(&keyboard_controller->back, is_down);
				} else if (vk_code == VK_SPACE) {
					keyboard_process_message(&keyboard_controller->start, is_down);
				}
#if DEBUG
				else if (vk_code == 'P') {
					if (is_down) {
						g_is_pause = !g_is_pause;
					}
				} else if (vk_code == 'L') {
					if (is_down) {
						if (winstate->replay_status == WIN_REPLAY_NORMAL) {
							winstate->replay_slot_index = 0;
							input_begin_recording(winstate);
						} else if (winstate->replay_status == WIN_REPLAY_RECORD) {
							input_end_recording(winstate);
						} else if (winstate->replay_status == WIN_REPLAY_RECORDED) {
							input_begin_playback(winstate);
						} else { // it was in playback mode
							input_end_playback(winstate);
						}
					}
#endif
				}

				if (is_down) {
					uint32_t was_alt_key_down = key_stroke_info & (1U << 29U);
					if ((vk_code == VK_F4) && was_alt_key_down) {
						g_is_running = 0;
					}

					if (vk_code == VK_F11 && was_alt_key_down) {
						if (msg.hwnd) {
							window_toggle_fullscreen(msg.hwnd);
						}
					}
				}
			}
			break;
		default: {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} break;
		}
		}
	}
}

static WindowDimensions window_get_dimensions(HWND winhandle)
{
	WindowDimensions result;
	RECT client_rec;

	GetClientRect(winhandle, &client_rec);

	result.width = client_rec.right - client_rec.left;
	result.height = client_rec.bottom - client_rec.top;

	return result;
}

/*
 * dib: device independent bitmap
 */
static void offscreen_resize_section(WinOffscreenBuffer *back_buffer, unsigned win_width, unsigned win_height)
{
	if (back_buffer->top_left_px) {
		VirtualFree(back_buffer->top_left_px, 0, MEM_RELEASE);
	}

	back_buffer->width_px = win_width;
	back_buffer->height_px = win_height;
	back_buffer->bytes_per_pixel = 4;
	back_buffer->info.bmiHeader.biSize = sizeof(back_buffer->info.bmiHeader);
	back_buffer->info.bmiHeader.biWidth = (long)back_buffer->width_px;
	// NOTE(fredy): top-down layout (opposite to bottom-up).
	// The first three bytes on the bitmap are for the top-left pixel
	back_buffer->info.bmiHeader.biHeight = -(long)back_buffer->height_px;
	back_buffer->info.bmiHeader.biPlanes = 1;
	back_buffer->info.bmiHeader.biBitCount = 32;
	back_buffer->info.bmiHeader.biCompression = BI_RGB;

	size_t bitmap_memory_size = (size_t)(back_buffer->width_px) * (size_t)(back_buffer->height_px) *
	                            (size_t)(back_buffer->bytes_per_pixel);

	back_buffer->top_left_px = VirtualAlloc(nullptr, bitmap_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	back_buffer->pitch_bytes = back_buffer->width_px * back_buffer->bytes_per_pixel;
}

static void window_display_offscreen_buffer(HDC device_context, WinOffscreenBuffer *back_buffer, long win_width,
                                            long win_height)
{
	if (win_width >= 2 * (int)back_buffer->width_px && win_height >= 2 * (int)back_buffer->height_px) {
		StretchDIBits(device_context, 0, 0, win_width, win_height, 0, 0, (int)back_buffer->width_px,
		              (int)back_buffer->height_px, back_buffer->top_left_px, &back_buffer->info, DIB_RGB_COLORS,
		              SRCCOPY);
	} else {
		int offset_x = 10;
		int offset_y = 10;
		PatBlt(device_context, 0, 0, win_width, offset_y, BLACKNESS);
		PatBlt(device_context, offset_x + (int)back_buffer->width_px, 0,
		       win_width - offset_x - (int)back_buffer->width_px, win_height, BLACKNESS);
		PatBlt(device_context, 0, offset_y + (int)back_buffer->height_px, win_width,
		       win_height - offset_y - (int)back_buffer->height_px, BLACKNESS);
		PatBlt(device_context, 0, 0, offset_x, win_height, BLACKNESS);
		StretchDIBits(device_context, offset_x, offset_y, (int)back_buffer->width_px,
		              (int)back_buffer->height_px, 0, 0, (int)back_buffer->width_px,
		              (int)back_buffer->height_px, back_buffer->top_left_px, &back_buffer->info, DIB_RGB_COLORS,
		              SRCCOPY);
	}
}

static LRESULT CALLBACK window_procedure(HWND window, [[__maybe_unused__]] UINT msg, [[__maybe_unused__]] WPARAM wparam,
                                         [[__maybe_unused__]] LPARAM lparam)
{
	LRESULT result = 0;

	switch (msg) {
	case WM_CLOSE: {
		g_is_running = 0U;
	} break;
	case WM_SETCURSOR: {
		if (g_show_cursor_debug) {
			result = DefWindowProcA(window, msg, wparam, lparam);
		} else {
			SetCursor(nullptr);
		}
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_DESTROY: {
		g_is_running = 0U;
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		assert(false && "We must be processing the keyboard in other place");
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC dchandle = BeginPaint(window, &paint);
		WindowDimensions win_dim = window_get_dimensions(window);
		window_display_offscreen_buffer(dchandle, &g_win_back_buffer, win_dim.width, win_dim.height);
		EndPaint(window, &paint);
	} break;
	default: {
		OutputDebugStringA("default\n");
		result = DefWindowProcA(window, msg, wparam, lparam);
	} break;
	}

	return result;
}

static inline LARGE_INTEGER clock_get_wall(void)
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	return counter;
}

static inline float clock_elapsed_secs(LARGE_INTEGER start, LARGE_INTEGER end)
{
	return (float)(end.QuadPart - start.QuadPart) / (float)g_perf_count_frequency;
}

/**
 * @brief
 *
 * @param bitmap_buffer
 * @param x pixel index
 * @param top pixel index
 * @param bottom pixel index
 */
static void bitmap_draw_vertical_debug(WinOffscreenBuffer *bitmap, unsigned x, unsigned top, unsigned bottom,
                                       unsigned color)
{
	assert(x >= 0 && x < bitmap->width_px);
	assert(bottom >= 0 && bottom < bitmap->height_px);
	assert(top >= 0 && top < bitmap->height_px);
	assert(top <= bottom);

	unsigned char *pixel_start = (unsigned char *)bitmap->top_left_px + x * (size_t)bitmap->bytes_per_pixel +
	                             top * (size_t)bitmap->pitch_bytes;
	uint32_t *pixel = nullptr;
	for (size_t y = top; y < bottom; ++y) {
		pixel = (uint32_t *)pixel_start;
		*pixel = color;
		pixel_start += bitmap->pitch_bytes;
	}
}

static inline void offscreen_draw_sound_mark_debug(WinOffscreenBuffer *back_buffer, WinSoundOutput *soundout,
                                                   float pixels_per_byte, unsigned pad_x, unsigned top, unsigned bottom,
                                                   unsigned value, uint32_t color)
{
	assert(value < soundout->buffsize_bytes);
	unsigned x = pad_x + (unsigned)(pixels_per_byte * (float)value);
	bitmap_draw_vertical_debug(back_buffer, x, top, bottom, color);
}

/**
 * @brief debug code for sound
 *
 * @param bitmap_buffer
 * @param last_play_cursors_size
 * @param last_play_cursors sound play cursor positions inside the sound buffer
 * @param soundout
 */
static void offscreen_draw_sound_sync_debug(WinOffscreenBuffer *back_buffer, unsigned last_cursors_marks_size,
                                            DebugTimeMark *last_cursors_marks, unsigned current_mark_index,
                                            WinSoundOutput *winsound)
{
	unsigned pad_x = 16;
	unsigned pad_y = 16;

	unsigned line_height = 64;

	unsigned painting_width = back_buffer->width_px - 2 * pad_x;
	float pixels_per_byte = (float)painting_width / (float)winsound->buffsize_bytes;

	for (size_t i = 0; i < last_cursors_marks_size; ++i) {
		DebugTimeMark current_mark = last_cursors_marks[i];
		unsigned long play_color = 0xFFFFFFFF;
		unsigned long write_color = 0xFFFF0000;
		unsigned long frame_flip_byte_color = 0xFFFFFF00;
		unsigned long play_window_color = 0xFFFF00FF;

		unsigned top = pad_y;
		unsigned bottom = pad_y + line_height;
		unsigned first_top = 0;
		if (i == current_mark_index) {
			top += pad_y + line_height;
			bottom += pad_y + line_height;

			first_top = top;

			offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
			                                current_mark.output_play_cursor, play_color);
			offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
			                                current_mark.output_write_cursor, write_color);

			top += pad_y + line_height;
			bottom += pad_y + line_height;

			offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
			                                current_mark.output_location, play_color);
			offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
			                                RING_ADD(winsound->buffsize_bytes, current_mark.output_location,
			                                         current_mark.output_byte_count),
			                                write_color);

			top += pad_y + line_height;
			bottom += pad_y + line_height;

			offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, first_top,
			                                bottom, current_mark.frame_flip_byte, frame_flip_byte_color);
		}

		offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
		                                current_mark.flip_play_cursor, play_color);

		offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
		                                RING_SUB(winsound->buffsize_bytes, current_mark.flip_play_cursor,
		                                         480 * winsound->bytes_per_sample),
		                                play_window_color);
		offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
		                                RING_ADD(winsound->buffsize_bytes, current_mark.flip_play_cursor,
		                                         480 * winsound->bytes_per_sample),
		                                play_window_color);

		offscreen_draw_sound_mark_debug(back_buffer, winsound, pixels_per_byte, pad_x, top, bottom,
		                                current_mark.flip_write_cursor, write_color);
	}
}

int CALLBACK WinMain(HINSTANCE hInstance, [[__maybe_unused__]] HINSTANCE hPrevInstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine, [[__maybe_unused__]] int nShowCmd)
{
	// NOTE(): never use MAX_PATH in user-facing code. it is dangerous.
	WinState win_state = {};

	LARGE_INTEGER perf_count_frequency_result;
	QueryPerformanceFrequency(&perf_count_frequency_result);
	g_perf_count_frequency = perf_count_frequency_result.QuadPart;

	file_get_exe_path(&win_state);

	char tmp_app_dll_filename[MAX_FILE_PATH];
	char app_dll_path[MAX_FILE_PATH];
	char tmp_app_dll_path[MAX_FILE_PATH];
	char app_dll_lock_path[MAX_FILE_PATH];

	file_build_path(&win_state, APP_DLL_NAME, sizeof(app_dll_path), app_dll_path);
	file_build_path(&win_state, "lock.tmp", sizeof(app_dll_lock_path), app_dll_lock_path);

	if (sprintf(tmp_app_dll_filename, TMP_APP_DLL_FILENAME_FMT, GetCurrentTime()) < 0) {
		return EXIT_FAILURE;
	}
	file_build_path(&win_state, tmp_app_dll_filename, sizeof(tmp_app_dll_path), tmp_app_dll_path);

	// sets the scheduler granularity to 1ms, so that our Sleep() can be more granular
	unsigned desire_scheduler_ms = 1;
	uint8_t is_granular_sleep = timeBeginPeriod(desire_scheduler_ms) == TIMERR_NOERROR;

	xinput_load();

#if DEBUG
	g_show_cursor_debug = 1U;
#endif

	WNDCLASSA win_class = {
		.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc = window_procedure,
		.hInstance = hInstance,
		.hCursor = LoadCursorA(nullptr, IDC_ARROW),
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	offscreen_resize_section(&g_win_back_buffer, 960, 540);

	if (!RegisterClassA(&win_class)) {
		LOG_ERROR("error registering the window class");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(0, win_class.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
	                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
	                                 hInstance, nullptr);
	if (!winhandle) {
		LOG_ERROR("error creating the window");
		return EXIT_FAILURE;
	}

	HDC dc_handle = GetDC(winhandle);

	WinSoundOutput win_sound = {
		.samples_per_sec = 48000,
		.bytes_per_sample = sizeof(uint16_t) * 2,
	};

	unsigned monitor_hz = 60;
	unsigned win_monitor_hz = (unsigned)GetDeviceCaps(dc_handle, VREFRESH);
	if (win_monitor_hz > 1) {
		monitor_hz = win_monitor_hz;
	}

	float game_hz = (float)monitor_hz / 2.0F;
	float target_frame_time_s = 1.0F / game_hz;
	unsigned bytes_per_sec = win_sound.samples_per_sec * win_sound.bytes_per_sample;
	unsigned bytes_per_frame = (unsigned)((float)bytes_per_sec * target_frame_time_s);

	win_sound.safety_bytes = bytes_per_frame / 3; // 1/3 of the samples per frame
	win_sound.buffsize_bytes = bytes_per_sec;     // 1 second of sound

	sound_init(winhandle, win_sound.samples_per_sec, win_sound.buffsize_bytes);
	sound_clear_buffer(&win_sound);

	if (FAILED(IDirectSoundBuffer_Play(g_secbuffer, 0, 0, DSBPLAY_LOOPING))) {
		LOG_ERROR("Error playing dsound secondary buffer");
		return EXIT_FAILURE;
	}

	g_is_running = 1U;

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
			LIB_LOGD(
				"PC: %lu, WC: %lu, CDT: %zu bytes (%zu samples), PDT: %zu bytes (%zu samples)",
				play_cursor, write_cursor, cursor_delta_bytes,
				cursor_delta_bytes / soundout.sample_size_bytes, play_delta_bytes,
				play_delta_bytes / soundout.sample_size_bytes);

			previous_play_cursor = play_cursor;
		}
	}
#endif // debug sound

	int16_t *samples =
		(int16_t *)VirtualAlloc(nullptr, win_sound.buffsize_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Storage Storage = {
		.plat_file_free_debug = file_free_debug,
		.plat_file_read_debug = file_read_debug,
		.file_write_debug = file_write_debug,
	};
	Storage.permanent_size_byte = MB_TO_BYTES(64ULL);
	Storage.transient_size_byte = GB_TO_BYTES(1ULL);

	win_state.memory_size_bytes = Storage.permanent_size_byte + Storage.transient_size_byte;
	win_state.memory_base_address = VirtualAlloc(MEMORY_BASE_ADDRESS, win_state.memory_size_bytes,
	                                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Storage.permanent_base_address = win_state.memory_base_address;
	Storage.transient_base_address = (unsigned char *)Storage.permanent_base_address + Storage.permanent_size_byte;

	for (uint8_t slot_index = 0; slot_index < REPLAY_MAX_SLOTS; ++slot_index) {
		ReplaySlot *replay_slot = &win_state.replay_slots[slot_index];

		if (!file_build_input_path(&win_state, slot_index, sizeof(replay_slot->filepath),
		                           replay_slot->filepath)) {
			return EXIT_FAILURE;
		}

		replay_slot->file_handle = CreateFileA(replay_slot->filepath, GENERIC_WRITE | GENERIC_READ, 0, nullptr,
		                                       CREATE_ALWAYS, 0, nullptr);
		replay_slot->file_map = CreateFileMapping(replay_slot->file_handle, nullptr, PAGE_READWRITE,
		                                          HIDWORD(win_state.memory_size_bytes),
		                                          LODWORD(win_state.memory_size_bytes), nullptr);
		replay_slot->memory =
			MapViewOfFile(replay_slot->file_map, FILE_MAP_ALL_ACCESS, 0, 0, win_state.memory_size_bytes);
	}

	if (!samples || !Storage.permanent_base_address || !Storage.transient_base_address) {
		return EXIT_FAILURE;
	}

	GameSoundBuffer game_soundbuff = {
		.samples_per_sec = win_sound.samples_per_sec,
		.samples_base_address = samples,
	};

	ThreadContext thread = {};
	GameOffscreenBuffer bitmap = {};

	GameInput inputs[2] = {};
	GameInput *new_input = &inputs[0];
	GameInput *old_input = &inputs[1];

	LARGE_INTEGER last_counter = clock_get_wall();
	LARGE_INTEGER flip_wall_clock = clock_get_wall();

	// constexpr unsigned debug_last_cursor_marks_size = 30;
	// unsigned debug_last_cursor_mark_index = 0;
	// DebugTimeMark debug_last_cursor_marks[debug_last_cursor_marks_size] = {};

	// size_t sound_latency_bytes = 0;
	// float sound_latency_s = 0.0F;
	uint8_t is_sound_valid = 0U;

	EngineCode game_code = {};
	FILETIME gamedll_last_write_time = {};
	if (!code_load_app(&game_code, app_dll_path, tmp_app_dll_path, app_dll_lock_path)) {
		return EXIT_FAILURE;
	}

	size_t last_cycle_count = __rdtsc();
	while (g_is_running) {
		new_input->time_delta_s = target_frame_time_s;

		if (file_get_last_write_time(app_dll_path, &gamedll_last_write_time) &&
		    CompareFileTime(&game_code.dll_write_time, &gamedll_last_write_time) != 0 &&
		    code_unload_game(&game_code)) {
			if (sprintf(tmp_app_dll_filename, TMP_APP_DLL_FILENAME_FMT, GetCurrentTime()) < 0) {
				return EXIT_FAILURE;
			}
			file_build_path(&win_state, tmp_app_dll_filename, sizeof(tmp_app_dll_path), tmp_app_dll_path);

			code_load_app(&game_code, app_dll_path, tmp_app_dll_path, app_dll_lock_path);
		}

		/**
		* @brief Gather input
		*/
		ControllerState *old_keyboard_controller = input_get_controller(old_input, 0);
		ControllerState *new_keyboard_controller = input_get_controller(new_input, 0);
		ControllerState zero_controller = {};
		*new_keyboard_controller = zero_controller;
		new_keyboard_controller->is_connected = 1U;

		for (size_t i = 0; i < MAX_CONTROLLER_BUTTONS; ++i) {
			new_keyboard_controller->buttons[i].ended_down = old_keyboard_controller->buttons[i].ended_down;
		}

		window_pump_messages(&win_state, new_keyboard_controller);

		if (g_is_pause) {
			continue;
		}

		POINT cursor_pos;
		GetCursorPos(&cursor_pos);
		ScreenToClient(winhandle, &cursor_pos);
		new_input->mouse_x = (unsigned)cursor_pos.x;
		new_input->mouse_y = (unsigned)cursor_pos.y;
		new_input->mouse_z = 0;
		keyboard_process_message(&new_input->mouse_back, GetAsyncKeyState(VK_XBUTTON1) < 0);
		keyboard_process_message(&new_input->mouse_forward, GetAsyncKeyState(VK_XBUTTON2) < 0);
		keyboard_process_message(&new_input->mouse_main, GetKeyState(VK_LBUTTON) < 0);
		keyboard_process_message(&new_input->mouse_middle, GetAsyncKeyState(VK_MBUTTON) < 0);
		keyboard_process_message(&new_input->mouse_secondary, GetKeyState(VK_RBUTTON) < 0);

		// +1 Taking into account keyboard controller
		unsigned short max_controller_count = XUSER_MAX_COUNT;
		if (max_controller_count > MAX_CONTROLLERS - 1) {
			max_controller_count = MAX_CONTROLLERS - 1;
		}

		for (unsigned long i = 0; i < max_controller_count; ++i) {
			unsigned long our_controller_index = i + 1;
			ControllerState *old_controller = input_get_controller(old_input, our_controller_index);
			ControllerState *new_controller = input_get_controller(new_input, our_controller_index);
			XINPUT_STATE state;
			if (XInputGetState(i, &state) != ERROR_SUCCESS) {
				new_controller->is_connected = 0U;
				continue;
			}

			// Plugged in
			new_controller->is_connected = 1U;
			new_controller->is_analog = old_controller->is_analog;

			XINPUT_GAMEPAD *pad = &state.Gamepad;

			new_controller->stick_avg_x =
				xinput_process_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			new_controller->stick_avg_y =
				xinput_process_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

			if (new_controller->stick_avg_x != 0.0F || new_controller->stick_avg_y != 0.0F) {
				new_controller->is_analog = 1U;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
				new_controller->stick_avg_y = 1.0F;
				new_controller->is_analog = 0U;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
				new_controller->stick_avg_y = -1.0F;
				new_controller->is_analog = 0U;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
				new_controller->stick_avg_x = -1.0F;
				new_controller->is_analog = 0U;
			}

			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
				new_controller->stick_avg_x = 1.0F;
				new_controller->is_analog = 0U;
			}

			float threshold = 0.5F;
			xinput_process_button(new_controller->stick_avg_x < -threshold ? 1 : 0,
			                      &old_controller->moveleft, 1, &new_controller->moveleft);
			xinput_process_button(new_controller->stick_avg_x > threshold ? 1 : 0,
			                      &old_controller->moveright, 1, &new_controller->moveright);
			xinput_process_button(new_controller->stick_avg_y < -threshold ? 1 : 0,
			                      &old_controller->movedown, 1, &new_controller->movedown);
			xinput_process_button(new_controller->stick_avg_y > threshold ? 1 : 0, &old_controller->moveup,
			                      1, &new_controller->moveup);

			xinput_process_button(pad->wButtons, &old_controller->actiondown, XINPUT_GAMEPAD_A,
			                      &new_controller->actiondown);
			xinput_process_button(pad->wButtons, &old_controller->actionright, XINPUT_GAMEPAD_B,
			                      &new_controller->actionright);
			xinput_process_button(pad->wButtons, &old_controller->actionleft, XINPUT_GAMEPAD_X,
			                      &new_controller->actionleft);
			xinput_process_button(pad->wButtons, &old_controller->actionup, XINPUT_GAMEPAD_Y,
			                      &new_controller->actionup);

			xinput_process_button(pad->wButtons, &old_controller->left_shoulder,
			                      XINPUT_GAMEPAD_LEFT_SHOULDER, &new_controller->left_shoulder);
			xinput_process_button(pad->wButtons, &old_controller->right_shoulder,
			                      XINPUT_GAMEPAD_RIGHT_SHOULDER, &new_controller->right_shoulder);

			xinput_process_button(pad->wButtons, &old_controller->start, XINPUT_GAMEPAD_START,
			                      &new_controller->start);
			xinput_process_button(pad->wButtons, &old_controller->back, XINPUT_GAMEPAD_BACK,
			                      &new_controller->back);
		}

		/**
		 * @brief Update and rendering
		 */

		bitmap.top_left_px = g_win_back_buffer.top_left_px;
		bitmap.width_px = g_win_back_buffer.width_px;
		bitmap.height_px = g_win_back_buffer.height_px;
		bitmap.pitch_bytes = g_win_back_buffer.pitch_bytes;
		bitmap.bytes_per_pixel = g_win_back_buffer.bytes_per_pixel;

		if (win_state.replay_status == WIN_REPLAY_RECORD) {
			input_record(&win_state, new_input);
		}

		if (win_state.replay_status == WIN_REPLAY_PLAYBACK) {
			input_playback(&win_state, new_input);
		}
		if (game_code.update_and_render) {
			game_code.update_and_render(&bitmap, &thread, &Storage, new_input);
		}

		unsigned bytes_to_write = 0;

		unsigned long play_cursor = 0;
		unsigned long write_cursor = 0;
		if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(g_secbuffer, &play_cursor, &write_cursor))) {
			if (!is_sound_valid) {
				win_sound.running_sample_index = write_cursor / win_sound.bytes_per_sample;
				is_sound_valid = 1U;
			}

			// NOTE(fredy): Compute how much sound to write and where
			unsigned byte_to_lock = (win_sound.running_sample_index * win_sound.bytes_per_sample) %
			                        win_sound.buffsize_bytes;

			float time_from_flip_s = clock_elapsed_secs(flip_wall_clock, clock_get_wall());
			float time_to_flip_s = target_frame_time_s - time_from_flip_s;
			unsigned bytes_to_flip = (unsigned)(time_to_flip_s * (float)bytes_per_sec);
			unsigned frame_flip_byte = RING_ADD(win_sound.buffsize_bytes, play_cursor, bytes_to_flip);
			unsigned sound_flip_byte = RING_IS_BETWEEN(play_cursor, frame_flip_byte, write_cursor) ?
			                                   // Sound has low latency
			                                   frame_flip_byte :
			                                   // Sound has high latency
			                                   write_cursor;

			// LIB_LOGD("ET: %f secs, TTF: %f secs, BPF: %u, BTF: %u, FFB: %u, SFB: %u",
			//          (double)secs_from_flip, (double)secs_to_flip, bytes_per_frame,
			//          bytes_to_flip, frame_flip_byte, sound_flip_byte);

			unsigned target_cursor = RING_ADD(win_sound.buffsize_bytes, sound_flip_byte,
			                                  bytes_per_frame + win_sound.safety_bytes);

			bytes_to_write = RING_DIFF(win_sound.buffsize_bytes, byte_to_lock, target_cursor);

			game_soundbuff.samples_count = bytes_to_write / win_sound.bytes_per_sample;
			if (game_code.sound_create_samples) {
				game_code.sound_create_samples(&game_soundbuff, &thread, &Storage);
			}
#if 0
			DebugTimeMark *mark =
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

			LIB_LOGD(
				"Estimated - PC: %lu, WC: %lu, BTL: %u, TC: %u, BTW: %u - LAT: %zu (%f secs)",
				play_cursor, write_cursor, byte_to_lock, target_cursor,
				bytes_to_write, sound_latency_bytes, (double)sound_latency_secs);
#endif

			sound_fill_buffer(&win_sound, byte_to_lock, bytes_to_write, &game_soundbuff);

		} else {
			is_sound_valid = 0U;
		}

		LARGE_INTEGER work_counter = clock_get_wall();
		float work_secs_elapsed = clock_elapsed_secs(last_counter, work_counter);

		float secs_elapsed_for_frame = work_secs_elapsed;
		if (secs_elapsed_for_frame < target_frame_time_s) {
			if (is_granular_sleep) {
				unsigned long sleep_ms =
					(unsigned long)(1000.0F * (target_frame_time_s - secs_elapsed_for_frame));
				if (sleep_ms > 0) {
					Sleep(sleep_ms);
				}
			}

			float test_secs_elapsed_for_frame = clock_elapsed_secs(last_counter, clock_get_wall());
			if (test_secs_elapsed_for_frame > target_frame_time_s) {
				LOG_WARN("missed sleep: %fs elapsed for frame", (double)test_secs_elapsed_for_frame);
			}

			while (secs_elapsed_for_frame < target_frame_time_s) {
				secs_elapsed_for_frame = clock_elapsed_secs(last_counter, clock_get_wall());
			}
		} else {
			// missed frame rate!
			LOG_WARN("missed frame rate: %fs elapsed for frame", (double)secs_elapsed_for_frame);
		}

		LARGE_INTEGER end_counter = clock_get_wall();
		float ms_per_frame = 1000.0F * clock_elapsed_secs(last_counter, end_counter);
		last_counter = end_counter;

		WindowDimensions windim = window_get_dimensions(winhandle);

#if 0
 		win_bitmap_draw_sound_sync_debug(&global_bitmap, debug_last_cursor_marks_size,
		                                 debug_last_cursor_marks,
		                                 debug_last_cursor_mark_index - 1, &winsound);
#endif

		// Flip the frame
		window_display_offscreen_buffer(dc_handle, &g_win_back_buffer, windim.width, windim.height);

		flip_wall_clock = clock_get_wall();

#if 0
		{
			unsigned long debug_play_cursor;
			unsigned long debug_write_cursor;
			IDirectSoundBuffer_GetCurrentPosition(secbuffer, &debug_play_cursor,
			                                      &debug_write_cursor);
			if (is_sound_valid) {
				assert(debug_last_cursor_mark_index < debug_last_cursor_marks_size);
				DebugTimeMark *mark =
					&debug_last_cursor_marks[debug_last_cursor_mark_index];
				mark->flip_play_cursor = debug_play_cursor;
				mark->flip_write_cursor = debug_write_cursor;
				LIB_LOGD("After flip - PC: %lu, WC: %lu, DPC: %lu, DWC: %lu",
				         play_cursor, write_cursor, debug_play_cursor,
				         debug_write_cursor);
			}
		}
#endif

		GameInput *temp = new_input;
		new_input = old_input;
		old_input = temp;

#if 1
		uint64_t end_cycle_count = __rdtsc();
		uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
		last_cycle_count = end_cycle_count;

		float fps = 1000.0F / ms_per_frame;
		float mega_cycles_per_frame = (float)cycles_elapsed / 1000000.0F;

		LOG_INFO("%fms/f, %ff/s, %fmc/f", (double)ms_per_frame, (double)fps, (double)mega_cycles_per_frame);
#endif

#if 0
		{
			debug_last_cursor_mark_index = RING_ADD(debug_last_cursor_marks_size,
			                                        debug_last_cursor_mark_index, 1);
		}
#endif // debug
	}

	return EXIT_SUCCESS;
}
