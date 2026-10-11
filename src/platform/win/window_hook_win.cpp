// ============================================================================
// src/platform/win/window_hook_win.cpp -- the end of a user move / size loop, on Windows
// ============================================================================
// Phase D.4 SDL step W3. Video::Device::MoveWindow is retail's WM_EXITSIZEMOVE (0x232)
// callback: it persists the window position once the USER has finished dragging. SDL3
// has no event for that; SDL_EVENT_WINDOW_MOVED also fires for the window's own
// SetWindowPos and for DXGI's mode switches, after which MoveWindow would persist the
// client origin of every programmatic placement (src/platform/sdl/pump_sdl.cpp's banner).
//
// SDL calls the Windows message hook for every message its WndProc receives while it is
// inside a modal move / size loop (`data->in_modal_loop`, SDL_windowsevents.c), and
// WM_EXITSIZEMOVE arrives while that count is still 1, so the hook sees it. It only
// observes: it queues the pump's private event and returns true, which lets SDL process
// the message exactly as it would have. SDL also calls it for every POSTED message its
// PeekMessage loop takes; those are one compare each and are left alone.
//
// It lives under src/platform/win/ because it reads a Win32 MSG (<windows.h>); the rest of
// the SDL backend has no OS header.
// ============================================================================

#include <windows.h>

#include <SDL3/SDL_system.h>

#include "../sdl/window_sdl_internal.h"

namespace Platform {
namespace Window {
namespace Sdl {

namespace {

bool SDLCALL MessageHook(void* /*userdata*/, MSG* msg) {
    if (msg->message == WM_EXITSIZEMOVE) QueueMoveSizeEnded();
    return true;
}

}  // namespace

void InstallWindowsMessageHook() {
    SDL_SetWindowsMessageHook(&MessageHook, nullptr);
}

void RemoveWindowsMessageHook() {
    SDL_SetWindowsMessageHook(nullptr, nullptr);
}

}  // namespace Sdl
}  // namespace Window
}  // namespace Platform
