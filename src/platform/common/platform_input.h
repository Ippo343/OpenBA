#pragma once
// ============================================================================
// src/platform/common/platform_input.h -- Platform::Input, the keyboard / mouse seam
// ============================================================================
// Phase D.4 (D4_RULES C1, SDL plan §A.4, step I1). Free functions, no OS header, no SDL
// header. Input::Device (src/engine/input/) keeps retail's two input paths and calls
// these instead of DirectInput 8 / user32:
//
//  * the DirectInput path (Input::Configuration::_legacy == 0, the default): a keyboard
//    read as 256 scan-code bytes (the DIK_* codes) and a mouse read as a DIMOUSESTATE2
//    record (relative x / y / wheel, 8 buttons);
//  * the legacy path (_legacy != 0, persisted `Input/Legacy`): per-key async state by
//    virtual-key code, the cursor position and the window-has-the-foreground test.
//
// Implementations: src/platform/win/input_dinput_win.cpp (DirectInput 8 + user32, step
// I1: one Win32 call per primitive, as the callers made them).
//
// Device handles are opaque (`void*`): the IDirectInput8W / IDirectInputDevice8W the
// Win32 backend creates, which Input::Device keeps at retail's offsets (+0x11D8 / +0x11E0
// / +0x11E8). Results are DirectInput's HRESULT values (kOk, kInputLost): the pollers'
// re-acquire loop tests them, as retail's does.
// ============================================================================

#include <cstdint>

#include "platform_window.h"   // Platform::Point

namespace Platform {
namespace Input {

constexpr int32_t kOk        = 0;                                  // DI_OK
constexpr int32_t kInputLost = static_cast<int32_t>(0x8007001Eu);  // DIERR_INPUTLOST

// DIMOUSESTATE2: relative x / y / wheel since the last read, then 8 buttons, each down
// when bit 7 is set. 20 bytes (input_dinput_win.cpp asserts it against <dinput.h>).
struct MouseState {
    int32_t x;
    int32_t y;
    int32_t z;
    uint8_t buttons[8];
};

// ---------------------------------------------------------------------------
// Device lifetime.
// ---------------------------------------------------------------------------
// Create the backend's factory, keyboard and mouse for `window`, in retail's order
// (Input::Device::PlatformStartup 0x1800e1880): the factory, the keyboard with its data
// format and cooperative level, the mouse likewise. Returns nullptr on success, else the
// retail Error::SetText text of the step that failed; whatever was created before it is
// left in the out-parameters and NOT released (retail leaks it; Engine::Startup ends the
// process on that path).
const char* CreateDevices(void* window, void** factory, void** keyboard, void** mouse);
// IDirectInputDevice8::Acquire / Unacquire.
int32_t Acquire(void* device);
int32_t Unacquire(void* device);
// IUnknown::Release of a device or of the factory.
void Release(void* object);

// ---------------------------------------------------------------------------
// The DirectInput path's two reads (IDirectInputDevice8::GetDeviceState).
// ---------------------------------------------------------------------------
// All 256 scan-code bytes (DIK_* index, bit 7 = down). On failure `dik` is not written.
int32_t ReadKeyboard(void* device, uint8_t dik[256]);
// The mouse record. On failure `out` is not written.
int32_t ReadMouse(void* device, MouseState* out);

// ---------------------------------------------------------------------------
// The legacy path, and the cursor position both paths read.
// ---------------------------------------------------------------------------
// GetForegroundWindow() == window.
bool IsForeground(void* window);
// GetAsyncKeyState(vk) for a keyboard key (bit 15 = down now).
int16_t AsyncKeyState(int32_t vk);
// GetAsyncKeyState(vk) for a mouse button (VK_LBUTTON 1, VK_RBUTTON 2, VK_MBUTTON 4).
int16_t AsyncMouseButtonState(int32_t vk);
// GetCursorPos: the cursor in screen coordinates.
void GetCursorPos(Point* out);
// ScreenToClient, in place.
void ScreenToClient(void* window, Point* inOut);
// GetSystemMetrics(SM_SWAPBUTTON) != 0: the pollers swap buttons 0 and 1 when true.
bool ButtonsSwapped();

}  // namespace Input
}  // namespace Platform
