#include <windows.h>

int CALLBACK WinMain([[__maybe_unused__]] HINSTANCE hInstance,
                     [[__maybe_unused__]] HINSTANCE hPrevInstance,
                     [[__maybe_unused__]] LPSTR lpCmdLine,
                     [[__maybe_unused__]] int nCmdShow)
{
	MessageBox(nullptr, "This is handmade hero.", "Handmade hero",
	           MB_OK | MB_ICONINFORMATION);
	return EXIT_SUCCESS;
}
