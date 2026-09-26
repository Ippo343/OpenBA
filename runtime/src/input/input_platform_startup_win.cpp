// ============================================================================
// runtime/src/input/input_platform_startup_win.cpp — Wave 9 lane PLAT
//   ?PlatformStartup@Device@Input@@AEAA_NXZ   retail 0x1800e1880  ord 3404
//   and the three exported message callbacks it installs.
// ============================================================================
//
// The LAST of the four `PlatformStartup` members our ported `Engine::Startup`
// calls: engine_lifecycle.cpp:741, `g_startupFailAt == 13`. `AEAA` is PRIVATE,
// not unexported; every name in this file was read out of the shipped export
// directory with pefile before a body was written:
//
//   | Export                                                | Retail      | Ord  |
//   |-------------------------------------------------------|-------------|------|
//   | ?PlatformStartup@Device@Input@@AEAA_NXZ                | 0x1800e1880 | 3404 |
//   | ?OnFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z   | 0x1800e1800 | 3275 |
//   | ?OnLostFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z | 0x1800e17d0 | 3276 |
//   | ?OnMouseWheelMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z | 0x1800e17a0 | 3277 |
//   | ?PlatformUpdate@Device@Input@@AEAAXXZ                  | 0x1800e2130 | 3411 |
//
// A CORRECTION THIS LANE OWES THE TREE
// ------------------------------------
// `runtime/src/input/input_win.cpp`'s registration block says, of the last row:
//
//     "PlatformUpdate (0x1800E2130) is NOT registered by name — it has no
//      mangled export in either the retail or the profile DLL (checked against
//      the export directory with pefile before writing a line of this file)."
//
// THAT IS FALSE. It is ordinal 3411 in retail and ordinal 3564 in the profile
// build, at 0x1800e2130 and 0x1800e27b0 respectively — re-checked with pefile
// against BOTH files by this lane before saying so. It is the `AEAA` trap:
// private, and this DLL exports its private members. The consequence was live,
// not cosmetic — any resolve of that NAME fell through to the DLL, and the
// three callbacks below are exactly such callers. The missing
// `RegisterRuntimeExport` is added in input_win.cpp by this lane and the
// comment corrected there.
//
// THE ONE FIELD THIS PORT CANNOT REPRODUCE, AND WHAT IT DOES INSTEAD
// -------------------------------------------------------------------
// `DirectInput8Create`'s first argument is read from `[0x1801fc680]`
// (0x1800e18aa). That global is NOT an export, and its only writer is
// `0x18009fbb0` — which the xrefs show is called twice from `0x1800803ec`, the
// CRT's DllMain dispatcher, with `fdwReason` in EDX and a `CMOVZ` that stores
// only when the reason is 1 (DLL_PROCESS_ATTACH). In other words
// **0x1801fc680 is Runtime-x64.dll's own HINSTANCE, captured in its DllMain**,
// and at cutover there is no such module and no export name for the mint to
// cover.
//
// This port passes `GetModuleHandleW(nullptr)` — the reimpl exe's own instance
// — and that is a REAL BEHAVIOURAL DIFFERENCE, stated here rather than buried:
// pre-cutover retail hands DirectInput the DLL's instance and we hand it the
// exe's. DirectInput uses the handle only for its own per-module bookkeeping
// (version and lifetime), not to address anything the caller owns, and both
// handles name modules that live for the whole process. It is the only
// substitution in this file that is not a transcription, apart from the data
// formats below.
//
// THE DATA FORMATS
// ----------------
// Retail passes its own embedded `DIDATAFORMAT` blobs at 0x18014bc20 and
// 0x18014be30. Their headers were read out of the shipped .rdata:
//   0x18014bc20  dwDataSize=256 dwNumObjs=256  -> c_dfDIKeyboard
//   0x18014be30  dwDataSize=20  dwNumObjs=11   -> c_dfDIMouse2 (NOT c_dfDIMouse,
//                                                 which is 16 bytes / 7 objects)
// Those addresses do not exist at cutover, so this file uses the
// `c_dfDIKeyboard` / `c_dfDIMouse2` that ship in MinGW's dinput8 import
// library. That substitution is not asserted — it is MEASURED, field by field
// including every one of the 267 object entries, by
// `RtLanePlatInputFormatProbe()` at the bottom of this file, which has both an
// effectiveness control (the count of fields compared) and a negative control
// (keyboard vs mouse, which must mismatch).
//
// THE COM VTABLE SLOTS
// --------------------
// IDirectInput8W:        +0x18 CreateDevice
// IDirectInputDevice8W:  +0x38 Acquire  +0x58 SetDataFormat  +0x68 SetCooperativeLevel
// The two device slots are the same ones runtime/types/input/Input.h already
// pinned from the OTHER side (`+0x38 Acquire`, `+0x48 GetDeviceState`, read out
// of PlatformGetKeyboardState/PlatformGetMouseState), so this file and that
// header agree on the interface without either having assumed it.
// ============================================================================

#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../../types/input/Input.h"

// Declared locally rather than by including standalone/resolve.h, which drags
// in <windows.h> under different macros (PLAN §4.1 decision 1). Byte-identical
// to runtime/src/collection/array.cpp:133.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
extern "C" void RtLanePlatInputFormatProbe();

namespace {

// `?_singleton@Configuration@Input@@0PEAV12@EA` @ 0x1801fe768 and
// `?_singleton@Application@1@0PEAV11@EA` @ 0x1801fc678, both through the
// port-aware door.
Input::Configuration** InputConfigSlot() {
    return RT_DATA_SLOT(Input::Configuration**, "?_singleton@Configuration@Input@@0PEAV12@EA");
}
Application::Application** AppSingletonSlot() {
    return RT_DATA_SLOT(Application::Application**, "?_singleton@Application@1@0PEAV11@EA");
}

void ErrorSetText(const char* text, bool flag) {
    using Fn = void (*)(const char*, bool);
    Fn fn = RT_EXPORT_ONCE(Fn, "?SetText@Error@System@@SAXPEBD_N@Z");
    if (fn) fn(text, flag);
}

// ENDGAME B.3.7 B5: two by-NAME doors of this file became member calls.
// `?PlatformUpdate@Device@Input@@AEAAXXZ` (RT_EXPORT_ONCE, input_win.cpp's
// body) is now `Device::PlatformUpdate()`, and
// `?RegisterMessageCallback@Application@1@...` (RT_EXPORT_ONCE, block B1's
// message_callbacks.cpp body) is now
// `Application::Application::RegisterMessageCallback`. Both doors resolved to
// the same bodies the member calls reach; their `if (fn)` miss arms, which
// could only fire with the port unregistered, go with them.

inline void* const* VtblOf(void* obj) {
    return *reinterpret_cast<void* const* const*>(obj);
}
template <typename Fn>
inline Fn Slot(void* obj, size_t byteOffset) {
    return reinterpret_cast<Fn>(VtblOf(obj)[byteOffset / sizeof(void*)]);
}

// `Input::Configuration` +0x08 / +0x09 are the only two bytes this body reads
// of it: +0x09 (`_legacy`) is the "skip DirectInput entirely" flag
// (`CMP byte [RAX+9],0` at 0x1800e1899 jumps past ALL device creation) and
// +0x08 (`_mouseClipping`) seeds the device's mouse-clipping flag at
// 0x1800e1ac4. The device's +0x11D8 is `Device::_directInput`
// (`LEA R9,[RCX+0x11d8]` at 0x1800e18a3).

// DISCL_FOREGROUND | DISCL_NONEXCLUSIVE. Retail passes the literal 6
// (`MOV R8D,0x6` at 0x1800e1965 and 0x1800e1a21).
constexpr DWORD kCoopLevel = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

using CreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(void*, const GUID&, void**, void*);
using SetDataFormatFn = HRESULT (STDMETHODCALLTYPE*)(void*, const DIDATAFORMAT*);
using SetCoopFn = HRESULT (STDMETHODCALLTYPE*)(void*, HWND, DWORD);
using AcquireFn = HRESULT (STDMETHODCALLTYPE*)(void*);

}  // namespace

// ============================================================================
// The three static message callbacks, ordinals 3275-3277.
// ============================================================================
// All three are `CA` — static, no `this` — and all three reach the device
// through `?_singleton@Device@Input@@0PEAV12@EA` (0x1801fe770) rather than
// through a captured pointer. All three return 0, i.e. "not handled": the
// WindowProc's caller then falls through to its own switch and on to
// DefWindowProcW, which is why WM_SETFOCUS still reaches Application's own
// focus bookkeeping.
namespace {
Input::Device** InputDeviceSlot() {
    return RT_DATA_SLOT(Input::Device**, "?_singleton@Device@Input@@0PEAV12@EA");
}
}  // namespace

namespace Input {

// 0x1800e1800: `_singleton->_hasFocus = 1; PlatformUpdate(_singleton); return 0;`
// Note it re-loads the singleton for the second use (0x1800e1812) instead of
// keeping the first in a register. Kept.
int Device::OnFocusMessage(void*, uint32_t, uint64_t, int64_t) {
    Device** slot = InputDeviceSlot();
    Device* dev = slot ? *slot : nullptr;
    dev->_hasFocus = 1;                            // 0x1800e180b, unguarded in retail
    Device* dev2 = slot ? *slot : nullptr;         // 0x1800e1812, retail re-reads
    dev2->PlatformUpdate();
    return 0;
}

// 0x1800e17d0: the same, with 0.
int Device::OnLostFocusMessage(void*, uint32_t, uint64_t, int64_t) {
    Device** slot = InputDeviceSlot();
    Device* dev = slot ? *slot : nullptr;
    dev->_hasFocus = 0;                            // 0x1800e17db
    Device* dev2 = slot ? *slot : nullptr;
    dev2->PlatformUpdate();
    return 0;
}

// 0x1800e17a0: the wheel. Nine instructions and no branch:
//     OR EAX,-1 ; SHR R8,0x10 ; MOV ECX,1 ; TEST R8W,R8W ; CMOVS ECX,EAX
//     MOV [singleton+0x11f4],ECX ; XOR EAX,EAX ; RET
// So it stores the SIGN of the wheel delta as -1 or +1 — NOT the delta itself,
// and NOT an accumulation. `TEST R8W,R8W` is a 16-bit test of the wParam's high
// word after the shift, i.e. the signed WHEEL_DELTA multiple, and CMOVS picks
// -1 only when that 16-bit value is negative. A 32-bit test here would read the
// wParam's upper half as part of the sign and be wrong for any wParam with bit
// 31 set.
int Device::OnMouseWheelMessage(void*, uint32_t, uint64_t wParam, int64_t) {
    const int16_t delta = static_cast<int16_t>((wParam >> 16) & 0xFFFF);
    const int32_t sign = (delta < 0) ? -1 : 1;
    Device** slot = InputDeviceSlot();
    Device* dev = slot ? *slot : nullptr;
    dev->_wheelAccum = sign;
    return 0;
}

}  // namespace Input

// ============================================================================
// ?PlatformStartup@Device@Input@@AEAA_NXZ              retail 0x1800e1880
// ============================================================================
// Retail's shape:
//
//   if (Configuration::_singleton->_noDirectInput) goto tail;   // 0x1800e1899
//   DirectInput8Create(hinst, 0x800, IID_IDirectInput8W, &_di8, NULL);
//   ... seven HRESULT checks, each with its own Error::SetText and `return false`
//   ...
//   tail:
//   RegisterMessageCallback(app, WM_SETFOCUS,   OnFocusMessage);
//   RegisterMessageCallback(app, WM_KILLFOCUS,  OnLostFocusMessage);
//   RegisterMessageCallback(app, WM_MOUSEWHEEL, OnMouseWheelMessage);
//   _mouseClipping = Configuration::_singleton->_mouseClipping;
//   PlatformUpdate(this);
//   GetCursorPos(&p); ScreenToClient(app->_windowHandle, &p);
//   _lastCursorX = p.x; _lastCursorY = p.y;
//   if (!Configuration::_singleton->_noDirectInput) { keyboard->Acquire(); mouse->Acquire(); }
//   return true;
//
// FOUR DETAILS WORTH THE INK
// --------------------------
//  1. EVERY HRESULT TEST IS `TEST EAX,EAX ; JZ`, an EQUALITY test against S_OK,
//     not `JS`. A DirectInput call that succeeded with a non-zero success code
//     would take the failure branch. Reproduced.
//  2. The `_noDirectInput` flag is re-read from the singleton at the END
//     (0x1800e1b0a) rather than reused from the entry test, so a value that
//     changed in between changes which Acquire branch runs. Kept as two reads.
//  3. The tail runs even when DirectInput was skipped entirely — the message
//     callbacks, the clip flag and the cursor origin are installed either way.
//     That is what makes the skip path usable rather than a stub.
//  4. On EVERY failure path the interfaces created so far are LEAKED: retail
//     Releases nothing before returning false. Faithful; `Engine::Startup`
//     ends the process on that path anyway.
//
// NO ORACLE, and the reason is not shyness. A replay would create a SECOND
// IDirectInput8 with two more devices and Acquire them against the same window,
// and would re-run three RegisterMessageCallback calls into the LIVE Set. The
// one substitution that is not a transcription — the data formats — is measured
// separately and honestly by RtLanePlatInputFormatProbe().
// ============================================================================
namespace Input {

bool Device::PlatformStartup() {

    Configuration** cfgSlot = InputConfigSlot();
    Configuration* cfg = cfgSlot ? *cfgSlot : nullptr;
    Application::Application** appSlot = AppSingletonSlot();
    Application::Application* app = appSlot ? *appSlot : nullptr;

    // 0x1800e1899 — unguarded in retail, and a null here is an upstream fault
    // to find rather than one to paper over.
    const bool skipDirectInput = cfg->_legacy != 0;

    void** di8Slot   = &_directInput;
    void** kbdSlot   = &_keyboardDevice;
    void** mouseSlot = &_mouseDevice;

    if (!skipDirectInput) {
        // See the header: retail's first argument is Runtime-x64.dll's own
        // HINSTANCE, captured in its DllMain at a global with no export name.
        HINSTANCE hinst = GetModuleHandleW(nullptr);

        HRESULT hr = DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                        di8Slot, nullptr);       // 0x1800e18c6
        if (hr != 0 || *di8Slot == nullptr) {                    // 0x1800e18cc, 0x1800e18db
            ErrorSetText("InputDevice: The application failed to create a DirectInput interface.", false);
            return false;                                        // 0x1800e1a55
        }
        void* di8 = *di8Slot;

        hr = Slot<CreateDeviceFn>(di8, 0x18)(di8, GUID_SysKeyboard, kbdSlot, nullptr);
        if (hr != 0) {                                           // 0x1800e18fb
            ErrorSetText("InputDevice: The application failed to create a keyboard device interface.", false);
            return false;
        }
        void* kbd = *kbdSlot;
        hr = Slot<SetDataFormatFn>(kbd, 0x58)(kbd, &c_dfDIKeyboard);  // 0x1800e1930
        if (hr != 0) {
            ErrorSetText("InputDevice: The application failed to set the keyboard device data format.", false);
            return false;
        }
        // 0x1800e195e: the window handle comes from Application::_singleton[0].
        HWND hwnd = app ? static_cast<HWND>(app->_windowHandle) : nullptr;
        hr = Slot<SetCoopFn>(kbd, 0x68)(kbd, hwnd, kCoopLevel);  // 0x1800e1971
        if (hr != 0) {
            ErrorSetText("InputDevice: The application failed to set the keyboard device cooperative level.", false);
            return false;
        }

        hr = Slot<CreateDeviceFn>(di8, 0x18)(di8, GUID_SysMouse, mouseSlot, nullptr);
        if (hr != 0) {                                           // 0x1800e19b7
            ErrorSetText("InputDevice: The application failed to create a mouse device interface.", false);
            return false;
        }
        void* mouse = *mouseSlot;
        hr = Slot<SetDataFormatFn>(mouse, 0x58)(mouse, &c_dfDIMouse2);  // 0x1800e19ec
        if (hr != 0) {
            ErrorSetText("InputDevice: The application failed to set the mouse device data format.", false);
            return false;
        }
        hr = Slot<SetCoopFn>(mouse, 0x68)(mouse, hwnd, kCoopLevel);     // 0x1800e1a2d
        if (hr != 0) {
            ErrorSetText("InputDevice: The application failed to set the mouse device cooperative level.", false);
            return false;
        }
    }

    // ---- the tail at 0x1800e1a75, reached both ways --------------------------
    app->RegisterMessageCallback(WM_SETFOCUS,   &Device::OnFocusMessage);
    app->RegisterMessageCallback(WM_KILLFOCUS,  &Device::OnLostFocusMessage);
    app->RegisterMessageCallback(WM_MOUSEWHEEL, &Device::OnMouseWheelMessage);

    // 0x1800e1abd — retail RE-READS the configuration singleton here.
    Configuration* cfg2 = cfgSlot ? *cfgSlot : nullptr;
    _mouseClipping = static_cast<uint8_t>(cfg2->_mouseClipping);

    PlatformUpdate();                                            // 0x1800e1ad1

    POINT p;
    GetCursorPos(&p);                                            // 0x1800e1adb
    ScreenToClient(app ? static_cast<HWND>(app->_windowHandle) : nullptr, &p);  // 0x1800e1af0
    _lastCursorX = p.x;
    _lastCursorY = p.y;

    // 0x1800e1b0a — the THIRD read of the configuration singleton, and it is a
    // separate read in retail rather than the entry value reused.
    Configuration* cfg3 = cfgSlot ? *cfgSlot : nullptr;
    if (cfg3->_legacy == 0) {
        void* kbd = *kbdSlot;
        Slot<AcquireFn>(kbd, 0x38)(kbd);                         // 0x1800e1b21
        void* mouse = *mouseSlot;
        Slot<AcquireFn>(mouse, 0x38)(mouse);                     // 0x1800e1b2e
    }

    return true;                                                 // 0x1800e1b3b
}

}  // namespace Input

// ============================================================================
// The registered DOORS (ENDGAME B.3.7 B5): name and address, one QUALIFIED
// call each.
// ============================================================================
extern "C" bool RtInput_Device_PlatformStartup(void* self) {
    return reinterpret_cast<Input::Device*>(self)->Input::Device::PlatformStartup();
}
extern "C" int RtInput_Device_OnFocusMessage(void* hwnd, unsigned msg, uint64_t wp, int64_t lp) {
    return Input::Device::OnFocusMessage(hwnd, msg, wp, lp);
}
extern "C" int RtInput_Device_OnLostFocusMessage(void* hwnd, unsigned msg, uint64_t wp, int64_t lp) {
    return Input::Device::OnLostFocusMessage(hwnd, msg, wp, lp);
}
extern "C" int RtInput_Device_OnMouseWheelMessage(void* hwnd, unsigned msg, uint64_t wp, int64_t lp) {
    return Input::Device::OnMouseWheelMessage(hwnd, msg, wp, lp);
}

// ============================================================================
// RtLanePlatInputFormatProbe — the one substitution, measured
// ============================================================================
// This port hands DirectInput MinGW's `c_dfDIKeyboard` / `c_dfDIMouse2` where
// retail hands its own embedded copies at 0x18014bc20 / 0x18014be30. That is
// the only place in this file where a DIFFERENT object is passed, so it is the
// only place that needs proving, and it is provable exactly while the DLL is
// still loaded.
//
// It compares the DIDATAFORMAT header and then every DIOBJECTDATAFORMAT entry:
// the GUID BY VALUE (the pointers are in two different modules and can never be
// equal — comparing them would fail 100% of entries for the one reason that is
// correct), plus dwOfs, dwType and dwFlags. A null pguid on one side and not
// the other counts as a mismatch.
//
// EFFECTIVENESS CONTROL: `fields` — the number of individual comparisons made.
// For the two formats together that must be 2*5 + (256 + 11)*4 = 1078. A run
// reporting `mismatch=0` with a small `fields` proved nothing.
//
// NEGATIVE CONTROL: the same comparator is then run on keyboard-vs-mouse, which
// MUST report a mismatch. It is unconditional, not env-gated, precisely because
// a control nobody runs is not a control.
// ============================================================================
namespace {

int CompareFormat(const DIDATAFORMAT* ours, const DIDATAFORMAT* theirs,
                  int* fields) {
    int bad = 0;
    auto cmp = [&](unsigned long long x, unsigned long long y) {
        ++*fields;
        if (x != y) ++bad;
    };
    cmp(ours->dwSize, theirs->dwSize);
    cmp(ours->dwObjSize, theirs->dwObjSize);
    cmp(ours->dwFlags, theirs->dwFlags);
    cmp(ours->dwDataSize, theirs->dwDataSize);
    cmp(ours->dwNumObjs, theirs->dwNumObjs);
    if (bad != 0) return bad;   // a header mismatch makes the object walk unsafe
    for (DWORD i = 0; i < ours->dwNumObjs; ++i) {
        const DIOBJECTDATAFORMAT& a = ours->rgodf[i];
        const DIOBJECTDATAFORMAT& b = theirs->rgodf[i];
        ++*fields;
        if ((a.pguid == nullptr) != (b.pguid == nullptr)) ++bad;
        else if (a.pguid && std::memcmp(a.pguid, b.pguid, sizeof(GUID)) != 0) ++bad;
        cmp(a.dwOfs, b.dwOfs);
        cmp(a.dwType, b.dwType);
        cmp(a.dwFlags, b.dwFlags);
    }
    return bad;
}

}  // namespace

extern "C" void RtLanePlatInputFormatProbe() {
    static bool done = false;
    if (done) return;
    done = true;

    const auto* theirKbd = reinterpret_cast<const DIDATAFORMAT*>(
        ResolveAddr(Module::Runtime, 0x18014bc20));
    const auto* theirMouse = reinterpret_cast<const DIDATAFORMAT*>(
        ResolveAddr(Module::Runtime, 0x18014be30));
    if (!theirKbd || !theirMouse) {
        printf("[PlatProbe] DIDATAFORMAT probe SKIPPED: the shipped blobs did not resolve\n");
        return;
    }

    int fields = 0;
    const int badKbd = CompareFormat(&c_dfDIKeyboard, theirKbd, &fields);
    const int badMouse = CompareFormat(&c_dfDIMouse2, theirMouse, &fields);

    // The negative control, run every time: the keyboard format compared
    // against the mouse's MUST disagree. If it does not, the comparator is
    // broken and the two zeros above mean nothing.
    int negFields = 0;
    const int negBad = CompareFormat(&c_dfDIKeyboard, theirMouse, &negFields);

    printf("[PlatProbe] DIDATAFORMAT keyboard_mismatch=%d mouse_mismatch=%d "
                "fields=%d | NEGATIVE-CONTROL(kbd vs mouse) mismatch=%d %s\n",
                badKbd, badMouse, fields, negBad,
                negBad > 0 ? "(fired, as it must)" : "(DID NOT FIRE — comparator is broken)");
}

// ============================================================================
// Registration
// ============================================================================
int RtInputPlatformStartupRegister() {
    int n = 0;
    RegisterRuntimeExport("?PlatformStartup@Device@Input@@AEAA_NXZ",     // 3404
                          (void*)&RtInput_Device_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x1800e1880, (void*)&RtInput_Device_PlatformStartup);

    RegisterRuntimeExport("?OnFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z",  // 3275
                          (void*)&RtInput_Device_OnFocusMessage); ++n;
    RegisterRuntimeAddr(0x1800e1800, (void*)&RtInput_Device_OnFocusMessage);

    RegisterRuntimeExport("?OnLostFocusMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z",  // 3276
                          (void*)&RtInput_Device_OnLostFocusMessage); ++n;
    RegisterRuntimeAddr(0x1800e17d0, (void*)&RtInput_Device_OnLostFocusMessage);

    RegisterRuntimeExport("?OnMouseWheelMessage@Device@Input@@CAHPEAUHWND__@@I_K_J@Z",  // 3277
                          (void*)&RtInput_Device_OnMouseWheelMessage); ++n;
    RegisterRuntimeAddr(0x1800e17a0, (void*)&RtInput_Device_OnMouseWheelMessage);

    return n;
}
