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

// ---------------------------------------------------------------------------
// Window message ids (SDL plan W2, the A.2 table). `Application::_messageCallbacks` is
// keyed by these Win32 ids (its layout is retail's) and `Application::WindowProc`
// switches on them; from W3 the SDL pump synthesises them from SDL events. The values are
// the WM_* ids (window_win.cpp static_asserts each against <windows.h>).
// ---------------------------------------------------------------------------
constexpr uint32_t kMsgDestroy         = 0x0002;   // WM_DESTROY
constexpr uint32_t kMsgSize            = 0x0005;   // WM_SIZE        Video::Device::SizeWindow
constexpr uint32_t kMsgActivate        = 0x0006;   // WM_ACTIVATE    DeviceData_OnActivate
constexpr uint32_t kMsgSetFocus        = 0x0007;   // WM_SETFOCUS    Input::Device::OnFocusMessage
constexpr uint32_t kMsgKillFocus       = 0x0008;   // WM_KILLFOCUS   Input::Device::OnLostFocusMessage
constexpr uint32_t kMsgClose           = 0x0010;   // WM_CLOSE
constexpr uint32_t kMsgQuit            = 0x0012;   // WM_QUIT
constexpr uint32_t kMsgEraseBackground = 0x0014;   // WM_ERASEBKGND
constexpr uint32_t kMsgSetCursor       = 0x0020;   // WM_SETCURSOR   DeviceData_OnSetCursor
constexpr uint32_t kMsgSysKeyDown      = 0x0104;   // WM_SYSKEYDOWN
constexpr uint32_t kMsgMouseWheel      = 0x020A;   // WM_MOUSEWHEEL  Input::Device::OnMouseWheelMessage
constexpr uint32_t kMsgExitSizeMove    = 0x0232;   // WM_EXITSIZEMOVE Video::Device::MoveWindow
// The one key WindowProc tests: VK_F10 (WM_SYSKEYDOWN's wParam).
constexpr uint64_t kKeyF10             = 0x79;

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
// IsWindowVisible (RENDER step 3: DeviceData_Resize's early-out, TraceGeom).
bool IsVisible(void* window);
// ShowWindow(window, SW_SHOWDEFAULT) (RENDER step 3: CreateDevice step 6 and
// BootWindow::ShowNow, the deferred reveal). The show-state then honours the STARTUPINFO
// the process was launched with.
void ShowDefault(void* window);
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

// The window procedure's OS half (SDL plan W2). DefWindowProcW: the default handling
// Application::WindowProc falls through to (W3 deletes it: SDL owns the real WndProc).
int64_t DefaultWindowProc(void* window, uint32_t message, uint64_t wParam, int64_t lParam);
// PostQuitMessage: WindowProc's WM_DESTROY arm.
void PostQuitMessage(int32_t exitCode);

}  // namespace Window

// ---------------------------------------------------------------------------
// The mouse cursor (RENDER plan step 3, section C.2: VideoDX11::DeviceData slot 8,
// slot 3 and the WM_SETCURSOR callback). A cursor is an opaque 8-byte handle: the
// HCURSOR on Win32 (window_win.cpp static_asserts the size), an SDL_Cursor* once SDL
// step W5 swaps the implementation. `VideoDX11::DeviceData::cursor` (+0x48) holds one.
// ---------------------------------------------------------------------------
struct CursorObject;                  // never defined: the handle is opaque
using CursorHandle = CursorObject*;

namespace Cursor {

// CreateCursor(instance, hotX, hotY, width, height, andPlane, xorPlane), the argument
// order Win32 takes. The planes are monochrome, 1 bit per pixel, each row width / 8
// bytes, the leftmost pixel the MSB of the row's first byte. AND set = transparent; AND
// clear and XOR set = white; both clear = black. `instance` is the module handle
// (Application::GetInstance, the HINSTANCE); an SDL implementation ignores it. Returns
// nullptr when the OS call fails.
CursorHandle Create(void* instance, int32_t hotX, int32_t hotY, int32_t width,
                    int32_t height, const void* andPlane, const void* xorPlane);
// SetCursor. The previous cursor it returns is not used by any caller.
void Set(CursorHandle cursor);
// ShowCursor(show). The display counter it returns is not used by any caller.
void Show(bool show);
// DestroyCursor.
void Destroy(CursorHandle cursor);

}  // namespace Cursor
}  // namespace Platform
