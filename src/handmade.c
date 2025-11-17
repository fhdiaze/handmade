#include "log.h"
#include <limits.h>
#include <stdint.h>
#include <windows.h>

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
	long bytes_per_pixel;
};

static boolean global_running = false;
static struct Win_OffScreenBuffer global_back_buffer;

static struct Win_WindowDimensions win_window_get_dimensions(HWND winhandle)
{
	struct Win_WindowDimensions result;
	RECT client_rec;
	GetClientRect(winhandle, &client_rec);
	result.width = client_rec.right - client_rec.left;
	result.height = client_rec.bottom - client_rec.top;

	return result;
}

static void render_weird_gradient(struct Win_OffScreenBuffer buffer,
                                  long blue_offset, long green_offset)
{
	uint8_t *row = (uint8_t *)buffer.memory;
	for (long y = 0; y < buffer.height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (long x = 0; x < buffer.width; ++x) {
			// Little endian in memory  B G R X -> because of the endianess
			// little endian on a register: 0xXXRRGGBB
			uint8_t blue = (uint8_t)(x + blue_offset);
			uint8_t green = (uint8_t)(y + green_offset);
			*pixel = (uint32_t)(green << CHAR_BIT) | blue;
			++pixel;
		}
		row += buffer.pitch;
	}
}

/*
 * dib: device independent bitmap
 */
static void win_resize_dib_section(struct Win_OffScreenBuffer *buffer,
                                   long win_width, long win_height)
{
	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = win_width;
	buffer->height = win_height;
	buffer->bytes_per_pixel = 4;

	buffer->bitmap_info.bmiHeader.biSize =
		sizeof(buffer->bitmap_info.bmiHeader);
	buffer->bitmap_info.bmiHeader.biWidth = buffer->width;
	// NOTE(fredy): top-down layout (opposite to bottom-up).
	// The first three bytes on the bitmap are for the top-left pixel
	buffer->bitmap_info.bmiHeader.biHeight = -buffer->height;
	buffer->bitmap_info.bmiHeader.biPlanes = 1;
	buffer->bitmap_info.bmiHeader.biBitCount = 32;
	buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;

	long bitmap_memory_size =
		buffer->width * buffer->height * buffer->bytes_per_pixel;

	buffer->memory = VirtualAlloc(nullptr, (size_t)bitmap_memory_size,
	                              MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

static void win_buffer_display_in_window(struct Win_OffScreenBuffer buffer,
                                         HDC device_context, long win_width,
                                         long win_height)
{
	StretchDIBits(device_context, 0, 0, win_width, win_height, 0, 0,
	              buffer.width, buffer.height, buffer.memory,
	              &buffer.bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK win_main_window_callback([[__maybe_unused__]] HWND winhandle,
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
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC dchandle = BeginPaint(winhandle, &paint);
		struct Win_WindowDimensions windim =
			win_window_get_dimensions(winhandle);
		win_buffer_display_in_window(global_back_buffer, dchandle,
		                             windim.width, windim.height);
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
                     [[__maybe_unused__]] LPSTR lpCmdLine,
                     [[__maybe_unused__]] int nCmdShow)
{
	WNDCLASS window_class = {
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = win_main_window_callback,
		.hInstance = hinstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	win_resize_dib_section(&global_back_buffer, 1200, 700);

	ATOM main_window_atom = RegisterClassA(&window_class);
	if (!main_window_atom) {
		// TODO(fredy): Logging
		logi("error");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(0, MAKEINTATOM(main_window_atom),
	                                 "Handmade Hero",
	                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
	                                 CW_USEDEFAULT, CW_USEDEFAULT,
	                                 CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
	                                 nullptr, hinstance, nullptr);
	if (!winhandle) {
		// TODO(fredy): Logging
		logi("error");
		return EXIT_FAILURE;
	}

	MSG msg;
	global_running = true;
	int x_offset = 0;
	int y_offset = 0;
	while (global_running) {
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				global_running = false;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		render_weird_gradient(global_back_buffer, x_offset, y_offset);
		HDC dchandle = GetDC(winhandle);
		struct Win_WindowDimensions windim =
			win_window_get_dimensions(winhandle);
		win_buffer_display_in_window(global_back_buffer, dchandle,
		                             windim.width, windim.height);
		ReleaseDC(winhandle, dchandle);

		++x_offset;
		y_offset += 2;
	}

	return EXIT_SUCCESS;
}
