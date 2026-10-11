// ============================================================================
// src/platform/posix/window_posix.cpp -- the Win32 remainder of Platform::Window, over SDL
// ============================================================================
// The POSIX twin of src/platform/win/window_win.cpp (Phase D.4 POSIX). Everything else
// in platform_window.h is SDL's already and portable as it stands (src/platform/sdl/:
// the window, the pump, the geometry, the message box, and since SDL-C the cursor
// (cursor_sdl.cpp) and ClipCursor (input_sdl.cpp)). This file covers only what
// window_win.cpp still does with Win32 calls on the SDL-created HWND:
//
//   IsVisible     IsWindowVisible             -> !(SDL_GetWindowFlags & SDL_WINDOW_HIDDEN)
//   ShowDefault   ShowWindow(SW_SHOWDEFAULT)  -> SDL_ShowWindow (a POSIX launch carries no
//                                                STARTUPINFO show state to honour)
//
// Under dxvk-native the native handle IS the SDL_Window* (window_sdl.cpp's
// NativeHandle, D4_RULES C5), so `window` is used as one directly.
// ============================================================================

#include "platform_window.h"

#include <SDL3/SDL_video.h>

namespace Platform {
namespace Window {

namespace {
SDL_Window* Sdlw(void* window) { return static_cast<SDL_Window*>(window); }  // CAST-OK: posix
}  // namespace

bool IsVisible(void* window) {
    return (SDL_GetWindowFlags(Sdlw(window)) & SDL_WINDOW_HIDDEN) == 0;
}

void ShowDefault(void* window) {
    SDL_ShowWindow(Sdlw(window));
}

}  // namespace Window
}  // namespace Platform
