// ============================================================================
// src/platform/win/input_dinput_win.cpp -- Platform::Input over DirectInput 8 + user32
// ============================================================================
// Phase D.4 SDL step I1 (plan §A.4). Every DirectInput 8 call in the tree and the
// user32 calls of the legacy input path, moved here from
// src/platform/win/input_platform_startup_win.cpp (the device creation),
// src/platform/win/input_platform_shutdown_win.cpp (Unacquire / Release) and
// src/engine/input/input_device_update.cpp (the two pollers), with the arguments those
// callers passed. The callers keep retail's control flow, their literals and their
// retail comments; only the OS / COM calls moved (src/platform/common/platform_input.h).
//
// THE COM VTABLE SLOTS (all through <dinput.h>'s interfaces, the slots retail calls)
// --------------------
// IDirectInput8W:        +0x18 CreateDevice
// IDirectInputDevice8W:  +0x38 Acquire  +0x40 Unacquire  +0x48 GetDeviceState
//                        +0x58 SetDataFormat  +0x68 SetCooperativeLevel
// IUnknown:              +0x10 Release
//
// THE FACTORY'S MODULE HANDLE (moved with the creation from
// input_platform_startup_win.cpp)
// ---------------------------
// `DirectInput8Create`'s first argument is read from `[0x1801fc680]` (0x1800e18aa).
// That global is NOT an export, and its only writer is `0x18009fbb0` -- which the xrefs
// show is called twice from `0x1800803ec`, the CRT's DllMain dispatcher, with
// `fdwReason` in EDX and a `CMOVZ` that stores only when the reason is 1
// (DLL_PROCESS_ATTACH). In other words **0x1801fc680 is Runtime-x64.dll's own
// HINSTANCE, captured in its DllMain**, and at cutover there is no such module.
// This port passes `GetModuleHandleW(nullptr)` -- the reimpl exe's own instance -- a
// REAL BEHAVIOURAL DIFFERENCE, stated rather than buried: DirectInput uses the handle
// only for its own per-module bookkeeping (version and lifetime), not to address
// anything the caller owns, and both handles name modules that live for the process.
//
// THE DATA FORMATS
// ----------------
// Retail passes its own embedded `DIDATAFORMAT` blobs at 0x18014bc20 and 0x18014be30.
// Their headers were read out of the shipped .rdata:
//   0x18014bc20  dwDataSize=256 dwNumObjs=256  -> c_dfDIKeyboard
//   0x18014be30  dwDataSize=20  dwNumObjs=11   -> c_dfDIMouse2 (NOT c_dfDIMouse, which is
//                                                 16 bytes / 7 objects)
// Those addresses do not exist at cutover, so this file uses the `c_dfDIKeyboard` /
// `c_dfDIMouse2` that ship in MinGW's dinput8 import library.
// ============================================================================

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <cstddef>
#include <cstdint>

#include "platform_input.h"

static_assert(sizeof(Platform::Input::MouseState) == sizeof(DIMOUSESTATE2),
              "Platform::Input::MouseState is a DIMOUSESTATE2 (c_dfDIMouse2, 20 bytes)");
static_assert(offsetof(Platform::Input::MouseState, x) == offsetof(DIMOUSESTATE2, lX), "lX");
static_assert(offsetof(Platform::Input::MouseState, y) == offsetof(DIMOUSESTATE2, lY), "lY");
static_assert(offsetof(Platform::Input::MouseState, z) == offsetof(DIMOUSESTATE2, lZ), "lZ");
static_assert(offsetof(Platform::Input::MouseState, buttons) ==
              offsetof(DIMOUSESTATE2, rgbButtons), "rgbButtons");
static_assert(Platform::Input::kInputLost == static_cast<int32_t>(DIERR_INPUTLOST),
              "DIERR_INPUTLOST");
static_assert(Platform::Input::kOk == DI_OK, "DI_OK");
static_assert(Platform::Input::kNotAcquired == static_cast<int32_t>(DIERR_NOTACQUIRED),
              "DIERR_NOTACQUIRED");
static_assert(sizeof(Platform::Point) == sizeof(POINT), "Platform::Point must be a POINT");

namespace Platform {
namespace Input {

namespace {

// DISCL_FOREGROUND | DISCL_NONEXCLUSIVE. Retail passes the literal 6
// (`MOV R8D,0x6` at 0x1800e1965 and 0x1800e1a21).
constexpr DWORD kCoopLevel = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

inline IDirectInputDevice8W* Dev(void* d) { return static_cast<IDirectInputDevice8W*>(d); }   // CAST-OK: win32
inline HWND Hwnd(void* w) { return static_cast<HWND>(w); }   // CAST-OK: win32

}  // namespace

// ---------------------------------------------------------------------------
// Input::Device::PlatformStartup's DirectInput block (0x1800e18a3..0x1800e1a55), in
// retail's order. EVERY HRESULT TEST IS `TEST EAX,EAX ; JZ`, an EQUALITY test against
// S_OK, not `JS`: a call that succeeded with a non-zero success code takes the failure
// branch. On EVERY failure path the interfaces created so far are LEAKED: retail
// Releases nothing before returning false.
// ---------------------------------------------------------------------------
const char* CreateDevices(void* window, void** factory, void** keyboard, void** mouse) {
    // See the banner: retail's first argument is Runtime-x64.dll's own HINSTANCE.
    HINSTANCE hinst = GetModuleHandleW(nullptr);

    IDirectInput8W* di8 = nullptr;
    HRESULT hr = DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                    reinterpret_cast<void**>(&di8),  // CAST-OK: com_iid
                                    nullptr);                // 0x1800e18c6
    *factory = di8;
    if (hr != 0 || di8 == nullptr)                           // 0x1800e18cc, 0x1800e18db
        return "InputDevice: The application failed to create a DirectInput interface.";

    IDirectInputDevice8W* kbd = nullptr;
    hr = di8->CreateDevice(GUID_SysKeyboard, &kbd, nullptr);   // +0x18
    *keyboard = kbd;
    if (hr != 0)                                             // 0x1800e18fb
        return "InputDevice: The application failed to create a keyboard device interface.";
    hr = kbd->SetDataFormat(&c_dfDIKeyboard);                // +0x58, 0x1800e1930
    if (hr != 0)
        return "InputDevice: The application failed to set the keyboard device data format.";
    // 0x1800e195e: the window handle comes from Application::_singleton[0]
    // (0x1800e196e MOV RDX,[RAX], no test: B6 14d G18).
    hr = kbd->SetCooperativeLevel(Hwnd(window), kCoopLevel); // +0x68, 0x1800e1971
    if (hr != 0)
        return "InputDevice: The application failed to set the keyboard device cooperative level.";

    IDirectInputDevice8W* ms = nullptr;
    hr = di8->CreateDevice(GUID_SysMouse, &ms, nullptr);     // +0x18
    *mouse = ms;
    if (hr != 0)                                             // 0x1800e19b7
        return "InputDevice: The application failed to create a mouse device interface.";
    hr = ms->SetDataFormat(&c_dfDIMouse2);                   // +0x58, 0x1800e19ec
    if (hr != 0)
        return "InputDevice: The application failed to set the mouse device data format.";
    hr = ms->SetCooperativeLevel(Hwnd(window), kCoopLevel);  // +0x68, 0x1800e1a2d
    if (hr != 0)
        return "InputDevice: The application failed to set the mouse device cooperative level.";

    return nullptr;
}

int32_t Acquire(void* device) {
    return static_cast<int32_t>(Dev(device)->Acquire());          // +0x38
}

int32_t Unacquire(void* device) {
    return static_cast<int32_t>(Dev(device)->Unacquire());        // +0x40
}

void Release(void* object) {
    static_cast<IUnknown*>(object)->Release();                    // +0x10  CAST-OK: win32
}

// Phase D.4 SDL I2: ReadKeyboard, IsForeground and AsyncKeyState are SDL's
// (src/platform/sdl/input_sdl.cpp; one body per primitive).

// Phase D.4 SDL I3: ReadMouse, AsyncMouseButtonState, GetCursorPos, ScreenToClient and
// ButtonsSwapped are SDL's (src/platform/sdl/input_sdl.cpp).


}  // namespace Input
}  // namespace Platform
