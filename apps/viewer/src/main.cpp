#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "viewer_internal.hpp"

#if defined(_WIN32)
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return elf3d::viewer::run_viewer_entry(__argc, __argv);
}
#else
int main(int argument_count, char** arguments) {
    return elf3d::viewer::run_viewer_entry(argument_count, arguments);
}
#endif
