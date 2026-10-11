// ============================================================================
// src/platform/win/window_win.cpp -- Platform::Window over Win32 (Phase D.4 SDL step W1)
// ============================================================================
// One Win32 call per primitive, with the arguments the callers passed before W1
// (src/platform/common/platform_window.h says which). The handles arrive as `void*`
// and become HWND / HMONITOR here, the one place that knows they are.
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
static_assert(Platform::Window::kMonitorDefaultToNull == MONITOR_DEFAULTTONULL,
              "MonitorFromRect flag");
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

namespace {
inline HWND Hwnd(void* window) { return static_cast<HWND>(window); }   // CAST-OK: win32
inline RECT* Win(Rect* r) { return reinterpret_cast<RECT*>(r); }       // CAST-OK: win32
}  // namespace

void GetClientRect(void* window, Rect* out) {
    ::GetClientRect(Hwnd(window), Win(out));
}

void GetWindowRect(void* window, Rect* out) {
    ::GetWindowRect(Hwnd(window), Win(out));
}

void ClientToScreen(void* window, Point* inOut) {
    ::ClientToScreen(Hwnd(window), reinterpret_cast<POINT*>(inOut));   // CAST-OK: win32
}

uint32_t GetShowCmd(void* window) {
    WINDOWPLACEMENT wp;
    std::memset(&wp, 0, sizeof(wp));
    wp.length = 0x2c;                       // the literal retail writes, = sizeof
    ::GetWindowPlacement(Hwnd(window), &wp);
    return wp.showCmd;
}

bool IsIconic(void* window) {
    return ::IsIconic(Hwnd(window)) != 0;
}

bool IsVisible(void* window) {
    return ::IsWindowVisible(Hwnd(window)) != 0;
}

void ShowDefault(void* window) {
    ::ShowWindow(Hwnd(window), SW_SHOWDEFAULT);
}

int32_t GetStyle(void* window) {
    return ::GetWindowLongW(Hwnd(window), GWL_STYLE);
}

void SetStyle(void* window, int32_t style) {
    ::SetWindowLongW(Hwnd(window), GWL_STYLE, style);
}

void SetWindowPos(void* window, ZOrder insertAfter, int32_t x, int32_t y,
                  int32_t width, int32_t height, uint32_t flags) {
    ::SetWindowPos(Hwnd(window),
                   insertAfter == ZOrder::NoTopmost ? HWND_NOTOPMOST : nullptr,
                   x, y, width, height, flags);
}

void AdjustWindowRect(Rect* inOut, uint32_t style, bool hasMenu) {
    ::AdjustWindowRect(Win(inOut), style, hasMenu ? TRUE : FALSE);
}

void* GetDesktopWindow() {
    return ::GetDesktopWindow();
}

void ClipCursor(const Rect* screenRect) {
    ::ClipCursor(reinterpret_cast<const RECT*>(screenRect));   // CAST-OK: win32
}

void GetMonitorRect(void* monitor, Rect* out) {
    MONITORINFO mi;
    std::memset(&mi, 0, 0x28);
    mi.cbSize = 0x28;
    ::GetMonitorInfoW(static_cast<HMONITOR>(monitor), &mi);   // CAST-OK: win32
    std::memcpy(out, &mi.rcMonitor, sizeof(*out));
}

void* MonitorFromRect(const Rect* rect, uint32_t flags) {
    return ::MonitorFromRect(reinterpret_cast<const RECT*>(rect), flags);   // CAST-OK: win32
}

void GetCurrentDisplaySize(int32_t* width, int32_t* height) {
    DEVMODEW dm;
    std::memset(&dm, 0, 0xdc);
    dm.dmSize = 0xdc;
    ::EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    *width  = static_cast<int32_t>(dm.dmPelsWidth);
    *height = static_cast<int32_t>(dm.dmPelsHeight);
}

int64_t DefaultWindowProc(void* window, uint32_t message, uint64_t wParam, int64_t lParam) {
    return ::DefWindowProcW(Hwnd(window), message, static_cast<WPARAM>(wParam),
                            static_cast<LPARAM>(lParam));
}

void PostQuitMessage(int32_t exitCode) {
    ::PostQuitMessage(exitCode);
}

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
