#pragma once
// ============================================================================
// src/platform/common/platform_window.h -- Platform::Window, the window / display seam
// ============================================================================
// Phase D.4 (D4_RULES C1, SDL plan §C, step W1). Free functions, no OS header, no SDL
// header. Implementations: src/platform/win/window_win.cpp (Win32, today) and, from SDL
// step W3 on, src/platform/sdl/window_sdl.cpp.
//
// W1 is a PURE MOVE: every primitive below is ONE Win32 call (plus the struct set-up that
// call needs), with the arguments the retail-faithful callers passed. The callers keep
// their arithmetic, their literals and their retail comments; only the OS call moved.
//
// A window and a monitor are opaque handles (`void*`): the HWND / HMONITOR on Win32,
// which is what `Application::_windowHandle` (+0x00) and `Video::Device::windowHandle`
// (+0x90) / `monitor` (+0xA0) already hold. `Rect` and `Point` are binary-identical to
// Win32's RECT / POINT (window_win.cpp static_asserts it), so a caller that punned a RECT
// corner as a POINT still can.
// ============================================================================

#include <cstdint>

namespace Platform {

struct Point {
    int32_t x;
    int32_t y;
};

struct Rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

namespace Window {

// SetWindowPos's insert-after argument, the two values retail passes.
enum class ZOrder : int32_t {
    Top,         // nullptr (== HWND_TOP): AdjustDisplay's fullscreen branch
    NoTopmost,   // HWND_NOTOPMOST: AdjustDisplay's windowed branch
};

// SetWindowPos flags retail passes: SWP_FRAMECHANGED | SWP_NOACTIVATE.
constexpr uint32_t kPosFrameChangedNoActivate = 0x30;
// MonitorFromRect flag retail passes: MONITOR_DEFAULTTONULL.
constexpr uint32_t kMonitorDefaultToNull = 0;

// GetClientRect.
void GetClientRect(void* window, Rect* out);
// GetWindowRect.
void GetWindowRect(void* window, Rect* out);
// ClientToScreen, in place.
void ClientToScreen(void* window, Point* inOut);
// GetWindowPlacement on a zeroed WINDOWPLACEMENT with length 0x2C; returns its showCmd.
uint32_t GetShowCmd(void* window);
// IsIconic.
bool IsIconic(void* window);
// GetWindowLongW(window, GWL_STYLE).
int32_t GetStyle(void* window);
// SetWindowLongW(window, GWL_STYLE, style).
void SetStyle(void* window, int32_t style);
// SetWindowPos.
void SetWindowPos(void* window, ZOrder insertAfter, int32_t x, int32_t y,
                  int32_t width, int32_t height, uint32_t flags);
// AdjustWindowRect, in place.
void AdjustWindowRect(Rect* inOut, uint32_t style, bool hasMenu);
// GetDesktopWindow.
void* GetDesktopWindow();
// ClipCursor; nullptr releases the clip.
void ClipCursor(const Rect* screenRect);
// GetMonitorInfoW on a zeroed MONITORINFO (cbSize 0x28); out = its rcMonitor (zeros when
// the call fails, as the zeroed struct was).
void GetMonitorRect(void* monitor, Rect* out);
// MonitorFromRect; returns the HMONITOR, or nullptr.
void* MonitorFromRect(const Rect* rect, uint32_t flags);
// EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS) on a zeroed DEVMODEW (dmSize 0xDC);
// out = dmPelsWidth / dmPelsHeight. The return value is not checked (retail's): a failure
// leaves zeros.
void GetCurrentDisplaySize(int32_t* width, int32_t* height);

}  // namespace Window
}  // namespace Platform
