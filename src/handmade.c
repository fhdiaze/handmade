#include "log.h"
#include <limits.h>
#include <stdint.h>
#include <windows.h>
#include <winuser.h>

static boolean running = false;
static BITMAPINFO bitmap_info;
static void *bitmap_memory;
static long bitmap_width;
static long bitmap_height;
static long bytes_per_pixel = 4;

static void render_weird_gradient(long x_offset, long y_offset)
{
	uint8_t *row = (uint8_t *)bitmap_memory;
	long pitch = bitmap_width * bytes_per_pixel;
	for (long y = 0; y < bitmap_height; ++y) {
		uint32_t *pixel = (uint32_t *)row;
		for (long x = 0; x < bitmap_width; ++x) {
			// Little endian in memory  B G R X -> because of the endianess
			// little endian on a register: 0xXXRRGGBB
			uint8_t blue = (uint8_t) (x + x_offset);
			uint8_t green = (uint8_t) (y + y_offset);
			*pixel = (uint32_t) (green << CHAR_BIT) | blue;
			++pixel;
		}
		row += pitch;
	}
}

/*
 * dib: device independent bitmap
 */
static void resize_dib_section(long width, long height)
{
	if (bitmap_memory) {
		VirtualFree(bitmap_memory, 0, MEM_RELEASE);
	}

	bitmap_width = width;
	bitmap_height = height;

	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = bitmap_width;
	bitmap_info.bmiHeader.biHeight = -bitmap_height; // top-down layout
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	long bitmap_memory_size =
		bitmap_width * bitmap_height * bytes_per_pixel;

	bitmap_memory = VirtualAlloc(nullptr, (size_t)bitmap_memory_size,
	                             MEM_COMMIT, PAGE_READWRITE);
}

static void update_window(HDC device_context, RECT *client_rect)
{
	long win_width = client_rect->right - client_rect->left;
	long win_height = client_rect->bottom - client_rect->top;
	StretchDIBits(device_context, 0, 0, win_width, win_height, 0, 0,
	              bitmap_width, bitmap_height, bitmap_memory, &bitmap_info,
	              DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK main_window_callback([[__maybe_unused__]] HWND winhandle,
                                      [[__maybe_unused__]] UINT msg,
                                      [[__maybe_unused__]] WPARAM wparam,
                                      [[__maybe_unused__]] LPARAM lparam)
{
	LRESULT result = 0;

	switch (msg) {
	case WM_SIZE: {
		RECT client_rec;
		GetClientRect(winhandle, &client_rec);
		long width = client_rec.right - client_rec.left;
		long height = client_rec.bottom - client_rec.top;
		resize_dib_section(width, height);
		OutputDebugStringA("WM_SIZE\n");
	} break;
	case WM_CLOSE: {
		OutputDebugStringA("WM_CLOSE\n");
		running = false;
	} break;
	case WM_DESTROY: {
		OutputDebugStringA("WM_DESTROY\n");
		running = false;
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC device_context = BeginPaint(winhandle, &paint);
		RECT client_rec;
		GetClientRect(winhandle, &client_rec);
		update_window(device_context, &client_rec);
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
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = main_window_callback,
		.hInstance = hinstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

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
	running = true;
	int x_offset = 0;
	int y_offset = 0;
	while (running) {
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		render_weird_gradient(x_offset, y_offset);
		HDC dchandle = GetDC(winhandle);
		RECT client_rec;
		GetClientRect(winhandle, &client_rec);
		update_window(dchandle, &client_rec);
		ReleaseDC(winhandle, dchandle);

		++x_offset;
	}

	return EXIT_SUCCESS;
}
