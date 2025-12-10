// clang-format Language: C

/*
* Defines the platform specific Api
*/

#ifndef PLATFORM_H
#define PLATFORM_H

struct Plat_Window;

/*
* Platform load file
*/
void plat_file_load(void);

/*
* Platform open window
*/
struct Plat_Window *plat_window_open(void);

#endif // PLATFORM_H
