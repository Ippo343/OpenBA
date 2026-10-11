// ============================================================================
// tests/unit/sdl_event_translate_test.cpp -- window events -> retail's message tuples
// ============================================================================
// Phase D.4 SDL step W3. Platform::Window::TranslateWindowEvent
// (src/platform/common/window_messages.cpp) is the pure half of the SDL pump: what it
// returns is exactly what Application::WindowProc and the registered callbacks receive.
// The table it implements is in window_messages.h; this file checks each row, the wheel
// sign (Input::Device::OnMouseWheelMessage keeps only the sign of the high word) and
// the Win32 order ACTIVATE before SET/KILLFOCUS. No SDL is linked (D4_RULES C7).
// ============================================================================
#include "unit.h"

#include <cstdint>

#include "platform_window.h"
#include "window_messages.h"

using Platform::Window::TranslateWindowEvent;
using Platform::Window::WindowEvent;
using Platform::Window::WindowEventKind;
using Platform::Window::WindowMessage;
using Platform::Window::kMaxMessagesPerEvent;

namespace {

void* const kWnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234560));  // CAST-OK: ptr_int

WindowEvent Ev(WindowEventKind k) {
    WindowEvent e;
    e.kind = k;
    return e;
}

// The high word of a WM_MOUSEWHEEL wParam, as OnMouseWheelMessage reads it.
int16_t WheelDelta(uint64_t wParam) {
    return static_cast<int16_t>((wParam >> 16) & 0xFFFF);
}

}  // namespace

UNIT_TEST(sdl_translate_focus_gained_is_activate_then_setfocus) {
    WindowMessage m[kMaxMessagesPerEvent];
    const int n = TranslateWindowEvent(Ev(WindowEventKind::FocusGained), kWnd, m);
    CHECK_EQ(n, 2);
    CHECK_EQ(m[0].message, Platform::Window::kMsgActivate);
    CHECK_EQ(m[0].wParam & 0xFFFFu, uint64_t(1));               // WA_ACTIVE
    CHECK_EQ(m[1].message, Platform::Window::kMsgSetFocus);
    CHECK(m[0].window == kWnd);
    CHECK(m[1].window == kWnd);
}

UNIT_TEST(sdl_translate_focus_lost_is_activate_inactive_then_killfocus) {
    WindowMessage m[kMaxMessagesPerEvent];
    const int n = TranslateWindowEvent(Ev(WindowEventKind::FocusLost), kWnd, m);
    CHECK_EQ(n, 2);
    CHECK_EQ(m[0].message, Platform::Window::kMsgActivate);
    CHECK_EQ(m[0].wParam & 0xFFFFu, uint64_t(0));               // WA_INACTIVE
    CHECK_EQ(m[1].message, Platform::Window::kMsgKillFocus);
}

UNIT_TEST(sdl_translate_size_packs_client_size_and_type) {
    WindowMessage m[kMaxMessagesPerEvent];
    WindowEvent e = Ev(WindowEventKind::SizeChanged);
    e.width = 1280;
    e.height = 720;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgSize);
    CHECK_EQ(m[0].wParam, uint64_t(0));                          // SIZE_RESTORED
    CHECK_EQ(m[0].lParam, int64_t(1280 | (720 << 16)));

    e.maximized = true;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 1);
    CHECK_EQ(m[0].wParam, uint64_t(2));                          // SIZE_MAXIMIZED
}

UNIT_TEST(sdl_translate_minimize_maximize_restore) {
    WindowMessage m[kMaxMessagesPerEvent];
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::Minimized), kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgSize);
    CHECK_EQ(m[0].wParam, uint64_t(1));                          // SIZE_MINIMIZED
    CHECK_EQ(m[0].lParam, int64_t(0));

    WindowEvent e = Ev(WindowEventKind::Maximized);
    e.width = 1920;
    e.height = 1017;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 1);
    CHECK_EQ(m[0].wParam, uint64_t(2));
    CHECK_EQ(m[0].lParam, int64_t(1920 | (1017 << 16)));

    e.kind = WindowEventKind::Restored;
    e.width = 800;
    e.height = 600;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 1);
    CHECK_EQ(m[0].wParam, uint64_t(0));
    CHECK_EQ(m[0].lParam, int64_t(800 | (600 << 16)));
}

UNIT_TEST(sdl_translate_move_size_end_and_close) {
    WindowMessage m[kMaxMessagesPerEvent];
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::MoveSizeEnded), kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgExitSizeMove);

    // DefWindowProcW(WM_CLOSE) -> DestroyWindow -> WM_DESTROY: WindowProc's quit arm.
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::CloseRequested), kWnd, m), 2);
    CHECK_EQ(m[0].message, Platform::Window::kMsgClose);
    CHECK_EQ(m[1].message, Platform::Window::kMsgDestroy);
}

UNIT_TEST(sdl_translate_mouse_is_setcursor_over_the_client) {
    WindowMessage m[kMaxMessagesPerEvent];
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::MouseMotion), kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgSetCursor);
    CHECK_EQ(m[0].wParam, reinterpret_cast<uint64_t>(kWnd));    // CAST-OK: ptr_int
    CHECK_EQ(m[0].lParam & 0xFFFF, int64_t(1));                 // HTCLIENT
    CHECK_EQ((m[0].lParam >> 16) & 0xFFFF, int64_t(0x200));     // WM_MOUSEMOVE

    WindowEvent e = Ev(WindowEventKind::MouseButtonDown);
    e.button = 1;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ((m[0].lParam >> 16) & 0xFFFF, int64_t(0x201));     // WM_LBUTTONDOWN
    e.button = 2;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ((m[0].lParam >> 16) & 0xFFFF, int64_t(0x207));     // WM_MBUTTONDOWN
    e.button = 3;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ((m[0].lParam >> 16) & 0xFFFF, int64_t(0x204));     // WM_RBUTTONDOWN
    e.button = 4;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ((m[0].lParam >> 16) & 0xFFFF, int64_t(0x20B));     // WM_XBUTTONDOWN
}

UNIT_TEST(sdl_translate_wheel_sign) {
    WindowMessage m[kMaxMessagesPerEvent];
    WindowEvent e = Ev(WindowEventKind::MouseWheel);

    e.wheelY = 1.0f;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgMouseWheel);
    CHECK_EQ(WheelDelta(m[0].wParam), int16_t(120));
    CHECK_EQ(m[0].wParam & 0xFFFFu, uint64_t(0));

    e.wheelY = -3.0f;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ(WheelDelta(m[0].wParam), int16_t(-120));

    // A high-resolution fraction still carries its sign.
    e.wheelY = 0.25f;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ(WheelDelta(m[0].wParam), int16_t(120));

    // SDL_MOUSEWHEEL_FLIPPED: the platform inverted the reported direction.
    e.wheelY = 1.0f;
    e.wheelFlipped = true;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ(WheelDelta(m[0].wParam), int16_t(-120));
    e.wheelY = -1.0f;
    TranslateWindowEvent(e, kWnd, m);
    CHECK_EQ(WheelDelta(m[0].wParam), int16_t(120));

    // Horizontal only: Win32 sends WM_MOUSEHWHEEL, not this id.
    e.wheelY = 0.0f;
    e.wheelFlipped = false;
    CHECK_EQ(TranslateWindowEvent(e, kWnd, m), 0);
}

UNIT_TEST(sdl_translate_quit_and_untranslated) {
    WindowMessage m[kMaxMessagesPerEvent];
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::Quit), kWnd, m), 1);
    CHECK_EQ(m[0].message, Platform::Window::kMsgQuit);
    CHECK_EQ(TranslateWindowEvent(Ev(WindowEventKind::None), kWnd, m), 0);
}
