#include "log.h"
#include <windows.h>

LRESULT CALLBACK main_window_callback([[__maybe_unused__]] HWND hwnd,
                                      [[__maybe_unused__]] UINT msg,
                                      [[__maybe_unused__]] WPARAM wparam,
                                      [[__maybe_unused__]] LPARAM lparam)
{
	LRESULT result = 0;

	switch (msg) {
	case WM_SIZE: {
		OutputDebugStringA("WM_SIZE\n");
	} break;
	case WM_DESTROY: {
		OutputDebugStringA("WM_DESTROY\n");
		PostQuitMessage(0);
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC hdc = BeginPaint(hwnd, &paint);
		LONG x = paint.rcPaint.left;
		LONG y = paint.rcPaint.top;
		LONG width = paint.rcPaint.right - paint.rcPaint.left;
		LONG height = paint.rcPaint.bottom - paint.rcPaint.top;
		static DWORD operation = WHITENESS;
		if (operation == WHITENESS) {
			operation = BLACKNESS;
		} else {
			operation = WHITENESS;
		}
		PatBlt(hdc, x, y, width, height, operation);
		EndPaint(hwnd, &paint);
	} break;
	default: {
		OutputDebugStringA("default\n");
		result = DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	}

	return result;
}

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hInstance,
                     [[__maybe_unused__]] HINSTANCE hPrevInstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine,
                     [[__maybe_unused__]] int nCmdShow)
{
	WNDCLASS window_class = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = main_window_callback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};
	MessageBoxA(nullptr, "This is handmade hero.", "Handmade hero",
	            MB_OK | MB_ICONINFORMATION);

	ATOM main_window_atom = RegisterClassA(&window_class);
	if (!main_window_atom) {
		// TODO(fredy): Logging
		logi("error");
		return EXIT_FAILURE;
	}

	HWND winhandle = CreateWindowExA(
		0, MAKEINTATOM(main_window_atom), "Handmade Hero",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
	if (!winhandle) {
		// TODO(fredy): Logging
		logi("error");
		return EXIT_FAILURE;
	}

	MSG msg;
	while (true) {
		BOOL msgres = GetMessageA(&msg, nullptr, 0, 0);
		if (msgres <= 0) {
			break;
		}

		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return EXIT_SUCCESS;
}
