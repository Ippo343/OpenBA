#pragma once
// ============================================================================
// src/platform/common/platform_window.h -- Platform::Window, the window / display seam
// ============================================================================
// Phase D.4 (D4_RULES C1, SDL plan §C, step W1). Free functions, no OS header, no SDL
// header. Implementations: src/platform/win/window_win.cpp (Win32) and, from SDL step W3
// on, src/platform/sdl/window_sdl.cpp + pump_sdl.cpp (the window, its lifetime, the
// pump, the default procedure and the quit request).
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
// Application::WindowProc falls through to. From W3 SDL owns the real WndProc and has
// already run DefWindowProcW on the real message, so the SDL body returns 0 and does
// nothing (src/platform/sdl/window_sdl.cpp).
int64_t DefaultWindowProc(void* window, uint32_t message, uint64_t wParam, int64_t lParam);
// PostQuitMessage: WindowProc's WM_DESTROY arm, the frame's and PlatformUpdate's quit
// request. From W3 it raises the quit FLAG that Platform::Pump reports once the queue is
// drained, which is Win32's own WM_QUIT rule (src/platform/sdl/pump_sdl.cpp).
void PostQuitMessage(int32_t exitCode);

// ---------------------------------------------------------------------------
// SDL step W3: the window's lifetime and the message pump (src/platform/sdl/).
// ---------------------------------------------------------------------------
// True when the video subsystem is up (src/app/main.cpp's SDL_Init(SDL_INIT_VIDEO)
// and the window class registered with it). Application::PlatformStartup's
// "failed to register a window class" arm is the false case.
bool VideoAvailable();
// Create THE game window: hidden, `width` x `height` client pixels, titled `title`
// (UTF-8). Returns its native handle (NativeHandle()), or nullptr on failure.
void* Create(const char* title, int32_t width, int32_t height);
// Destroy the window Create made. The caller's copy of the handle is not touched.
void Destroy(void* window);
// The renderer's handle for the game window (D4_RULES C5): the HWND on Windows, from
// SDL_PROP_WINDOW_WIN32_HWND_POINTER; nullptr before Create / after Destroy.
void* NativeHandle();

// The shape of Application::WindowProc and of every registered message callback's
// caller: (window, message id, wParam, lParam), the Win32 message vocabulary above.
using MessageHandler = int64_t (*)(void* window, uint32_t message, uint64_t wParam,
                                   int64_t lParam);
// A diagnostic observer called with each message BEFORE the handler, on the pumping
// (main) thread (src/app/main.cpp's [WndMsg] logger; it replaces a WH_CALLWNDPROC hook).
// nullptr removes it.
using MessageObserver = void (*)(void* window, uint32_t message, uint64_t wParam,
                                 int64_t lParam);
void SetMessageObserver(MessageObserver observer);

}  // namespace Window

// The message pump (SDL step W3): drain the window system's event queue, translate
// each event into the (message, wParam, lParam) tuples retail's WindowProc received
// (src/platform/common/window_messages.h) and hand them to `handler`. Returns true
// when the drain met WM_QUIT (a posted quit, or the window system's own quit event):
// the caller's "WM_QUIT seen" exit, as retail's PeekMessageW loops break on it.
bool Pump(Window::MessageHandler handler);

}  // namespace Platform
