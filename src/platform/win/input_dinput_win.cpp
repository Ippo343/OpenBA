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

// Phase D.4 SDL I4: no body is left here. Every Platform::Input primitive is SDL's
// (src/platform/sdl/input_sdl.cpp): the keyboard (I2), the mouse and the cursor reads (I3),
// and the device lifetime (I4: SDL owns its devices, so CreateDevices creates nothing and
// the cooperative level is gone with it). The DirectInput bodies are in this file's
// history (SDL I1, cac6ce3b7). The asserts above stay: they pin MouseState and the
// result codes to DirectInput's, which retail's poller was written against. Block 5
// deletes this file with the rest of the Win32 input backend.
