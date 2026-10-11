// ============================================================================
// src/platform/posix/window_posix.cpp -- the Win32 remainder of Platform::Window, over SDL
// ============================================================================
// The POSIX twin of src/platform/win/window_win.cpp (Phase D.4 POSIX). Everything else
// in platform_window.h is SDL's already (src/platform/sdl/: the window, the pump, the
// geometry, the message box) and is portable as it stands; this file covers only what
// window_win.cpp still does with Win32 calls on the SDL-created HWND:
//
//   IsVisible / ShowDefault   IsWindowVisible / ShowWindow(SW_SHOWDEFAULT)
//                             -> SDL_GetWindowFlags & SDL_WINDOW_HIDDEN / SDL_ShowWindow
//   ClipCursor                ClipCursor(screen rect) -> SDL_SetWindowMouseRect (the
//                             rect made window-relative; nullptr releases it)
//   Cursor::*                 CreateCursor / SetCursor / ShowCursor / DestroyCursor
//                             -> SDL_CreateCursor / SDL_SetCursor / SDL_ShowCursor /
//                             SDL_HideCursor / SDL_DestroyCursor
//
// Under dxvk-native the native handle IS the SDL_Window* (window_sdl.cpp's
// NativeHandle, D4_RULES C5), so `window` is used as one directly.
//
// When SDL step W5 (the cursor) and I3 (the clip) land their SDL bodies in
// src/platform/sdl/, the matching functions here are deleted, not kept twice.
// ============================================================================

#include "platform_window.h"
#include "../sdl/window_sdl_internal.h"   // Sdl::GameWindow

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

#include <cstdint>
#include <vector>

namespace Platform {
namespace Window {

namespace {
SDL_Window* Sdlw(void* window) { return static_cast<SDL_Window*>(window); }  // CAST-OK: posix
}  // namespace

bool IsVisible(void* window) {
    return (SDL_GetWindowFlags(Sdlw(window)) & SDL_WINDOW_HIDDEN) == 0;
}

// SW_SHOWDEFAULT honours the launch's STARTUPINFO; a POSIX launch carries none, so it
// is a plain show.
void ShowDefault(void* window) {
    SDL_ShowWindow(Sdlw(window));
}

// ClipCursor takes SCREEN coordinates; SDL confines relative to the window.
void ClipCursor(const Rect* screenRect) {
    SDL_Window* w = Sdl::GameWindow();
    if (w == nullptr) return;
    if (screenRect == nullptr) {
        SDL_SetWindowMouseRect(w, nullptr);
        return;
    }
    int x = 0, y = 0;
    SDL_GetWindowPosition(w, &x, &y);
    SDL_Rect r;
    r.x = screenRect->left - x;
    r.y = screenRect->top - y;
    r.w = screenRect->right - screenRect->left;
    r.h = screenRect->bottom - screenRect->top;
    SDL_SetWindowMouseRect(w, &r);
}

}  // namespace Window

// SDL_Cursor is the complete type behind the handle here: the handle carries the
// SDL_Cursor* in its 8 bytes.
namespace Cursor {

namespace {
SDL_Cursor* Sdlc(CursorHandle c) { return reinterpret_cast<SDL_Cursor*>(c); }      // CAST-OK: posix
CursorHandle FromSdl(SDL_Cursor* c) { return reinterpret_cast<CursorHandle>(c); }  // CAST-OK: posix
}  // namespace

// Win32's AND / XOR planes -> SDL's data / mask (SDL_CreateCursor):
//   AND XOR  Win32        SDL data mask
//    1   0   transparent       0    0
//    0   0   black             1    1
//    0   1   white             0    1
//    1   1   inverted          1    0
// so data = ~(AND ^ XOR) and mask = ~AND, bit for bit. Both are MSB-first rows of
// width / 8 bytes (platform_window.h).
CursorHandle Create(void* /*instance*/, int32_t hotX, int32_t hotY, int32_t width,
                    int32_t height, const void* andPlane, const void* xorPlane) {
    const size_t bytes = static_cast<size_t>(width / 8) * static_cast<size_t>(height);
    const uint8_t* a = static_cast<const uint8_t*>(andPlane);  // CAST-OK: byte_io
    const uint8_t* x = static_cast<const uint8_t*>(xorPlane);  // CAST-OK: byte_io
    std::vector<uint8_t> data(bytes), mask(bytes);
    for (size_t i = 0; i < bytes; ++i) {
        data[i] = static_cast<uint8_t>(~(a[i] ^ x[i]));
        mask[i] = static_cast<uint8_t>(~a[i]);
    }
    return FromSdl(SDL_CreateCursor(data.data(), mask.data(), width, height, hotX, hotY));
}

void Set(CursorHandle cursor) {
    SDL_SetCursor(Sdlc(cursor));
}

void Show(bool show) {
    if (show) SDL_ShowCursor();
    else SDL_HideCursor();
}

void Destroy(CursorHandle cursor) {
    SDL_DestroyCursor(Sdlc(cursor));
}

}  // namespace Cursor
}  // namespace Platform
