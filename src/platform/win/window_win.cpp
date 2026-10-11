// ============================================================================
// src/platform/win/window_win.cpp -- Platform::Window over Win32 (Phase D.4 SDL step W1)
// ============================================================================
// One Win32 call per primitive, with the arguments the callers passed before W1
// (src/platform/common/platform_window.h says which). From SDL W3 / W4 the window, the
// pump and the geometry are SDL's (src/platform/sdl/); what is left here is ClipCursor
// and the compile-time proof that the platform's message ids, Point / Rect and flags are
// Win32's.
// ============================================================================

#include <windows.h>
#include <cstddef>
#include <cstring>

#include "platform_window.h"

static_assert(sizeof(Platform::Point) == sizeof(POINT), "Platform::Point must be a POINT");
static_assert(offsetof(Platform::Point, x) == offsetof(POINT, x), "Point::x");
static_assert(offsetof(Platform::Point, y) == offsetof(POINT, y), "Point::y");
static_assert(sizeof(Platform::Rect) == sizeof(RECT), "Platform::Rect must be a RECT");
static_assert(offsetof(Platform::Rect, left) == offsetof(RECT, left), "Rect::left");
static_assert(offsetof(Platform::Rect, top) == offsetof(RECT, top), "Rect::top");
static_assert(offsetof(Platform::Rect, right) == offsetof(RECT, right), "Rect::right");
static_assert(offsetof(Platform::Rect, bottom) == offsetof(RECT, bottom), "Rect::bottom");
static_assert(Platform::Window::kPosFrameChangedNoActivate == (SWP_FRAMECHANGED | SWP_NOACTIVATE),
              "SetWindowPos flags");
static_assert(Platform::Window::kMsgDestroy == WM_DESTROY, "WM_DESTROY");
static_assert(Platform::Window::kMsgSize == WM_SIZE, "WM_SIZE");
static_assert(Platform::Window::kMsgActivate == WM_ACTIVATE, "WM_ACTIVATE");
static_assert(Platform::Window::kMsgSetFocus == WM_SETFOCUS, "WM_SETFOCUS");
static_assert(Platform::Window::kMsgKillFocus == WM_KILLFOCUS, "WM_KILLFOCUS");
static_assert(Platform::Window::kMsgClose == WM_CLOSE, "WM_CLOSE");
static_assert(Platform::Window::kMsgQuit == WM_QUIT, "WM_QUIT");
static_assert(Platform::Window::kMsgEraseBackground == WM_ERASEBKGND, "WM_ERASEBKGND");
static_assert(Platform::Window::kMsgSetCursor == WM_SETCURSOR, "WM_SETCURSOR");
static_assert(Platform::Window::kMsgSysKeyDown == WM_SYSKEYDOWN, "WM_SYSKEYDOWN");
static_assert(Platform::Window::kMsgMouseWheel == WM_MOUSEWHEEL, "WM_MOUSEWHEEL");
static_assert(Platform::Window::kMsgExitSizeMove == WM_EXITSIZEMOVE, "WM_EXITSIZEMOVE");
static_assert(Platform::Window::kKeyF10 == VK_F10, "VK_F10");
static_assert(sizeof(Platform::CursorHandle) == sizeof(HCURSOR),
              "Platform::CursorHandle holds an HCURSOR (VideoDX11::DeviceData +0x48)");

namespace Platform {
namespace Window {

// Phase D.4 SDL W4: GetClientRect, GetWindowRect, ClientToScreen, GetShowCmd, IsIconic,
// the style pair (now SetFramed), SetWindowPos, AdjustWindowRect, GetDesktopWindow (now
// GetPrimaryDisplayRect), GetMonitorRect, MonitorFromRect (now RectOnAnyDisplay) and
// GetCurrentDisplaySize are SDL's (src/platform/sdl/display_sdl.cpp). ClipCursor is the
// input block's (SDL plan I3: SDL_SetWindowMouseRect) and stays Win32 until then.
// IsVisible / ShowDefault (RENDER step 3) stay Win32 on the SDL-created HWND: SDL's
// WndProc sees the WM_SHOWWINDOW ShowWindow sends and keeps its own flags in step.
namespace {
inline HWND Hwnd(void* window) { return static_cast<HWND>(window); }   // CAST-OK: win32
}  // namespace

bool IsVisible(void* window) {
    return ::IsWindowVisible(Hwnd(window)) != 0;
}

void ShowDefault(void* window) {
    ::ShowWindow(Hwnd(window), SW_SHOWDEFAULT);
}

void ClipCursor(const Rect* screenRect) {
    ::ClipCursor(reinterpret_cast<const RECT*>(screenRect));   // CAST-OK: win32
}

// Phase D.4 SDL W3: DefaultWindowProc and PostQuitMessage are SDL's now
// (src/platform/sdl/window_sdl.cpp, pump_sdl.cpp): SDL owns the window procedure, so
// DefWindowProcW has already run on the real message, and the quit request is the pump's
// flag.

}  // namespace Window

namespace Cursor {

namespace {
inline HCURSOR Hcursor(CursorHandle c) { return reinterpret_cast<HCURSOR>(c); }   // CAST-OK: win32
}  // namespace

CursorHandle Create(void* instance, int32_t hotX, int32_t hotY, int32_t width,
                    int32_t height, const void* andPlane, const void* xorPlane) {
    HCURSOR c = ::CreateCursor(static_cast<HINSTANCE>(instance),   // CAST-OK: win32
                               hotX, hotY, width, height, andPlane, xorPlane);
    return reinterpret_cast<CursorHandle>(c);   // CAST-OK: win32
}

void Set(CursorHandle cursor) {
    ::SetCursor(Hcursor(cursor));
}

void Show(bool show) {
    ::ShowCursor(show ? TRUE : FALSE);
}

void Destroy(CursorHandle cursor) {
    ::DestroyCursor(Hcursor(cursor));
}

}  // namespace Cursor
}  // namespace Platform
