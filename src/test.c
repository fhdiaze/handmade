#include "log.h"
#include <stdio.h>
#include <windows.h>

void foo(void)
{
        char foo[] = "This is the first thing we have actually printed\n";
        OutputDebugStringA(foo);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
        logi("%p\n", (void *)hInstance);
        printf("%p\n", (void *)hPrevInstance);
        printf("%p\n", (void *)lpCmdLine);
        printf("%d\n", nCmdShow);
        foo();
}
