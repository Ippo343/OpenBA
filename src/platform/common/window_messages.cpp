// ============================================================================
// src/platform/common/window_messages.cpp -- window events -> retail's message tuples
// ============================================================================
// Phase D.4 SDL step W3. The table and the Win32 facts behind it are in
// window_messages.h. Pure: no OS call, no SDL call (tests/unit/sdl_event_translate_test.cpp).
// ============================================================================

#include "window_messages.h"

#include "platform_window.h"

namespace Platform {
namespace Window {

namespace {

// MAKELPARAM(lo, hi): two 16-bit halves, as WM_SIZE / WM_SETCURSOR pack them.
int64_t MakeLParam(int32_t lo, int32_t hi) {
    return static_cast<int64_t>((static_cast<uint32_t>(lo) & 0xFFFFu) |
                                ((static_cast<uint32_t>(hi) & 0xFFFFu) << 16));
}

void Put(WindowMessage& m, void* window, uint32_t message, uint64_t wParam, int64_t lParam) {
    m.window  = window;
    m.message = message;
    m.wParam  = wParam;
    m.lParam  = lParam;
}

uint32_t ButtonDownMessage(uint8_t button) {
    switch (button) {
        case 2:  return kMsgMButtonDown;
        case 3:  return kMsgRButtonDown;
        case 4:
        case 5:  return kMsgXButtonDown;
        default: return kMsgLButtonDown;
    }
}

}  // namespace

int TranslateWindowEvent(const WindowEvent& e, void* window,
                         WindowMessage out[kMaxMessagesPerEvent]) {
    switch (e.kind) {
        case WindowEventKind::FocusGained:
            // WM_ACTIVATE first, then WM_SETFOCUS: Win32's activation order. Input's
            // OnFocusMessage and the switch's `_running = 1` see SETFOCUS after
            // DeviceData_OnActivate has seen ACTIVATE, as they did.
            Put(out[0], window, kMsgActivate, kActivateActive, 0);
            Put(out[1], window, kMsgSetFocus, 0, 0);
            return 2;
        case WindowEventKind::FocusLost:
            Put(out[0], window, kMsgActivate, kActivateInactive, 0);
            Put(out[1], window, kMsgKillFocus, 0, 0);
            return 2;
        case WindowEventKind::SizeChanged:
            Put(out[0], window, kMsgSize, e.maximized ? kSizeMaximized : kSizeRestored,
                MakeLParam(e.width, e.height));
            return 1;
        case WindowEventKind::Minimized:
            Put(out[0], window, kMsgSize, kSizeMinimized, 0);
            return 1;
        case WindowEventKind::Maximized:
            Put(out[0], window, kMsgSize, kSizeMaximized, MakeLParam(e.width, e.height));
            return 1;
        case WindowEventKind::Restored:
            Put(out[0], window, kMsgSize, kSizeRestored, MakeLParam(e.width, e.height));
            return 1;
        case WindowEventKind::MoveSizeEnded:
            Put(out[0], window, kMsgExitSizeMove, 0, 0);
            return 1;
        case WindowEventKind::CloseRequested:
            // DefWindowProcW's WM_CLOSE is DestroyWindow, whose WM_DESTROY reaches the
            // WndProc next: WindowProc's WM_DESTROY arm posts the quit. SDL keeps the
            // window itself until PlatformShutdown destroys it.
            Put(out[0], window, kMsgClose, 0, 0);
            Put(out[1], window, kMsgDestroy, 0, 0);
            return 2;
        case WindowEventKind::MouseMotion:
            Put(out[0], window, kMsgSetCursor, reinterpret_cast<uint64_t>(window),  // CAST-OK: ptr_int
                MakeLParam(static_cast<int32_t>(kHitClient), kMsgMouseMove));
            return 1;
        case WindowEventKind::MouseButtonDown:
            Put(out[0], window, kMsgSetCursor, reinterpret_cast<uint64_t>(window),  // CAST-OK: ptr_int
                MakeLParam(static_cast<int32_t>(kHitClient), ButtonDownMessage(e.button)));
            return 1;
        case WindowEventKind::MouseWheel: {
            // Input::Device::OnMouseWheelMessage keeps only the sign of the high word.
            // A horizontal-only event (y == 0) is WM_MOUSEHWHEEL on Win32: not this id.
            if (e.wheelY == 0.0f) return 0;
            bool up = e.wheelY > 0.0f;
            if (e.wheelFlipped) up = !up;
            const int16_t delta = up ? kWheelDelta : static_cast<int16_t>(-kWheelDelta);
            const uint64_t wParam =
                static_cast<uint64_t>(static_cast<uint16_t>(delta)) << 16;
            Put(out[0], window, kMsgMouseWheel, wParam, 0);
            return 1;
        }
        case WindowEventKind::Quit:
            Put(out[0], window, kMsgQuit, 0, 0);
            return 1;
        case WindowEventKind::RawMessage:
            Put(out[0], window, e.rawMessage, e.rawWParam, e.rawLParam);
            return 1;
        case WindowEventKind::None:
        default:
            return 0;
    }
}

}  // namespace Window
}  // namespace Platform
