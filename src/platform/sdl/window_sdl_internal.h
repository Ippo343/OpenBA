#pragma once
// ============================================================================
// src/platform/sdl/window_sdl_internal.h -- shared state of the SDL window backend
// ============================================================================
// Phase D.4 SDL step W3. Only src/platform/sdl/*.cpp include this: the engine sees the
// window through platform_window.h's opaque native handle, never an SDL_Window*.
// ============================================================================

struct SDL_Window;

namespace Platform {
namespace Window {
namespace Sdl {

// The one game window window_sdl.cpp's Create made; nullptr before / after.
SDL_Window* GameWindow();

// pump_sdl.cpp: install the platform-specific event sources the pump needs (the
// Windows message hook that reports the end of a user move / size loop). Create calls
// it once the window exists.
void InstallPumpHooks();
void RemovePumpHooks();
// Queue the end of a user move / size loop (Win32: WM_EXITSIZEMOVE) as an event, in
// order with the rest of the queue; the pump turns it into the EXITSIZEMOVE tuple.
void QueueMoveSizeEnded();

// src/platform/win/window_hook_win.cpp (Windows only): SDL_SetWindowsMessageHook with an
// observer that calls QueueMoveSizeEnded on WM_EXITSIZEMOVE.
void InstallWindowsMessageHook();
void RemoveWindowsMessageHook();

}  // namespace Sdl
}  // namespace Window
}  // namespace Platform
