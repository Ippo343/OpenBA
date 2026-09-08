// ============================================================================
// runtime/src/input/input_win.cpp — Wave 9 lane IOA
//   Input::Device::EnableMouseClipping  +  Input::Device::PlatformUpdate
// ============================================================================
//
//   ?EnableMouseClipping@Device@Input@@QEAAX_N@Z   retail 0x1800E1780  ord 2296
//   (its only callee)  Device::PlatformUpdate      retail 0x1800E2130  NOT exported
//
// The Win32 half of Input::Device, behind PLAN.md §4.1 decision 1's platform
// seam: <windows.h> must not appear in input.cpp, which is why this is its own
// file rather than three more functions at the end of that one.
//
// ---------------------------------------------------------------------------
// THE STATED BLOCKER FOR THIS ROW WAS WRONG, AND THIS IS THE CORRECTION
// ---------------------------------------------------------------------------
// WAVE8_LANE_D.md §"?EnableMouseClipping@Device@Input@@QEAAX_N@Z" holds it back
// like this:
//
//     "One field write (this[0x11D0] = param) followed by a call to the same
//      unexported PlatformUpdate Update depends on. Porting the field write
//      alone would silently drop the Win32 clip-rect side effect the DLL's
//      call actually produces; porting the whole thing needs the same
//      unresolved internal helper Update does. Held back for the same reason,
//      not a separate one."
//
// The first two sentences are exactly right. The third is false, and it is
// false because `PlatformUpdate` was never disassembled — it was assumed to
// share `Update`'s problem because `Update` also ends in a tail-jump to it.
// Its whole body is 0xB8 bytes (0x1800E2130..0x1800E21E8) and it contains:
//
//   * NO DirectInput8 anything. No COM pointer, no `IDirectInputDevice8`
//     vtable call, nothing past +0x11F0.
//   * NO unexported Runtime callee. Not one `CALL <rva>`.
//   * FOUR calls, all of them USER32 imports, resolved from the DLL's own
//     import table with pefile: GetClientRect (0x18014B558), ClientToScreen
//     (0x18014B530, twice) and ClipCursor (0x18014B528).
//   * ONE Runtime data read: `Application::Application::_singleton`
//     (0x1801FC678), which our tree has MINTED since Wave 9 lane C2
//     (standalone/rt_data_mint_tables.inc), and whose +0x00 our tree ALREADY
//     documents as the HWND — runtime/types/video/Device.h:180 says
//     "+0x90 HWND, copied from Application::_singleton[0] by Startup".
//
// So the helper `Update` and `EnableMouseClipping` share is satisfied on our
// side; what `Update` is actually blocked on is the OTHER three helpers
// (PlatformGetKeyboardState 0x1800E1C60, UpdateState 0x1800E0FC0,
// PlatformGetMouseState 0x1800E1E10), which really are DirectInput and really
// are unexported. Lane D's hold-back on `Update` stands unchanged; its
// hold-back on `EnableMouseClipping` does not, and the row closes here.
// Three of the "four internal helpers" remain; this is the fourth.
//
// ---------------------------------------------------------------------------
// ORACLE — there is none, and saying so is the honest answer
// ---------------------------------------------------------------------------
// Both functions END in `ClipCursor`, which is global mutable OS state for the
// whole desktop. Calling the DLL's copy afterwards would clip the cursor a
// second time to the same rectangle — a repeat of the side effect, not a
// comparison of it (the same reason application_win.cpp's PlatformUpdate,
// SleepThread and YieldToThreads are registered with `RT_DIFF_SHADOW(true,…)`).
// What CAN be checked is the field write and the branch taken, so both sites
// carry `RT_DIFF_WORK` with the port's own guard verdict: `calls=` then counts
// entries and the work column counts the calls that actually reached
// `ClipCursor` with a rectangle. That is an effectiveness control, not an
// oracle, and this file does not claim otherwise.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include "../../funcs/rt_port.h"
#include "../../types/input/Input.h"

namespace {

// ---------------------------------------------------------------------------
// Input::Device::PlatformUpdate — retail 0x1800E2130. NOT exported, so it gets
// no RegisterRuntimeExport of its own; it exists because EnableMouseClipping
// calls it, exactly as retail's does.
//
// Retail, instruction for instruction:
//
//   1800e2134  CMP byte ptr [RCX+0x11d0],0   ; _mouseClipping
//   1800e213b  JZ  -> 1800e21dc
//   1800e2141  CMP byte ptr [RCX+0x11f0],0   ; _hasFocus
//   1800e2148  JZ  -> 1800e21dc
//   1800e214e  MOV RAX,[0x1801fc678]         ; Application::_singleton
//   1800e215f  MOV RBX,[RAX]                 ; app->hwnd   (+0x00)
//   1800e2165  CALL [GetClientRect]  (hwnd, RSP+0x20)
//   1800e216b  copy the 4 dwords RSP+0x20 -> RSP+0x40
//   1800e2193  CALL [ClientToScreen] (hwnd, RSP+0x40)   ; the (left,top) corner
//   1800e21a1  CALL [ClientToScreen] (hwnd, RSP+0x48)   ; the (right,bottom) corner
//   1800e21a7  copy back: [0x20]=[0x40] [0x28]=[0x48] [0x24]=[0x44] [0x2c]=[0x4c]
//   1800e21cc  CALL [ClipCursor] (RSP+0x20)
//   1800e21dc  XOR ECX,ECX ; JMP [ClipCursor]           ; ClipCursor(NULL)
//
// The interleaved copy at 0x1800E21A7 is an IDENTITY copy — left<-left,
// right<-right, top<-top, bottom<-bottom — the compiler's own scheduling of
// two POINTs back into one RECT, not a swap. It is written out below in the
// obvious form rather than reproduced as four dword moves, because the two
// are the same bytes in the RECT either way and the shuffled spelling reads as
// a bug it is not.
//
// The `*(HWND*)app` deref is UNGUARDED here because it is unguarded in retail.
// CLAUDE.md's rule applies: retail relies on `Application::_singleton` being
// published (its ctor is what publishes it) and on +0x00 holding the window by
// the time any focus message or mouse-clipping toggle can arrive. A null check
// would not restore the behaviour, it would skip it. If this ever faults, the
// upstream fact to fix is that our Application was never given its HWND.
void Device_PlatformUpdate(void* selfPtr) {
    RT_DIFF_BEGIN("Input::Device::PlatformUpdate");

    auto* self = static_cast<uint8_t*>(selfPtr);
    const bool clipping = self[RtInput::kMouseClippingOffset] != 0;
    const bool focused  = self[RtInput::kHasFocusOffset] != 0;

    // "Did work" = took the clipping branch. The other branch is the common
    // one (every lost-focus message runs it), so without this the site's
    // calls= column would say nothing about whether the port was ever
    // exercised in the state that matters.
    RT_DIFF_WORK(clipping && focused);

    if (!clipping || !focused) {
        ClipCursor(nullptr);
        RT_DIFF_SHADOW(true, "released the cursor clip", "not diffable (global OS state)");
        return;
    }

    void** appSlot = RT_DATA_SLOT(void**, "?_singleton@Application@1@0PEAV11@EA");
    void*  app     = appSlot ? *appSlot : nullptr;
    HWND   hwnd    = *reinterpret_cast<HWND*>(app);   // Application +0x00

    RECT rc;
    GetClientRect(hwnd, &rc);
    ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rc.left));    // (left, top)
    ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rc.right));   // (right, bottom)
    ClipCursor(&rc);

    RT_DIFF_SHADOW(true, "clipped the cursor to the client rect",
                   "not diffable (global OS state)");
}

}  // namespace

// ---------------------------------------------------------------------------
// Input::Device::EnableMouseClipping — retail 0x1800E1780, ord 2296. Nine
// bytes:
//
//   1800e1780  MOV byte ptr [RCX+0x11d0],DL
//   1800e1786  JMP 0x1800e2130                 ; tail call PlatformUpdate
//
// The parameter is taken as `uint8_t`, not `bool`, on purpose. Retail stores
// DL RAW — whatever byte the caller left in the register — and does not
// normalise it to 0/1. The two spellings are ABI-identical in the Microsoft
// x64 convention (a `_N` argument is one byte in DL either way), so a caller
// holding a `void(*)(void*, bool)` typedef reaches this unchanged, and the
// raw-byte store stays faithful.
// ---------------------------------------------------------------------------
void RtInput_Device_EnableMouseClipping(void* selfPtr, uint8_t enable) {
    RT_DIFF_BEGIN("Input::Device::EnableMouseClipping");

    auto* self = static_cast<uint8_t*>(selfPtr);
    const bool changed = self[RtInput::kMouseClippingOffset] != enable;
    self[RtInput::kMouseClippingOffset] = enable;

    // "Did work" = the flag actually changed value. A run whose work column is
    // zero has only ever re-asserted the setting it already had, which is the
    // negative case worth being able to see.
    RT_DIFF_WORK(changed);

    Device_PlatformUpdate(selfPtr);

    RT_DIFF_SHADOW(true, "wrote _mouseClipping and re-applied the clip",
                   "not diffable (ClipCursor is global OS state)");
}

int RtInputWinRegister() {
    int n = 0;
    // Registered under the name AND the address. engine_funcs.cpp reaches this
    // by name (engine_funcs.cpp:3950); the address registration is what makes
    // `ResolveAddr(Module::Runtime, 0x1800E1780)` reach us too.
    //
    // To be exact about which door does what, because this project has
    // re-learned it more than once: `ResolveAddr(Module::Runtime, …)` is NOT
    // "always the DLL" — it consults the port registry FIRST and only then the
    // module handle (standalone/resolve_standalone.cpp:471). So the address
    // door lands on the DLL exactly when the address is UNREGISTERED. Wave 9
    // lane UIF found a function registered by name only, which made every
    // by-address caller fall through silently; registering both is the fix,
    // and it is why both lines are here rather than just the first.
    RegisterRuntimeExport("?EnableMouseClipping@Device@Input@@QEAAX_N@Z",
                          (void*)&RtInput_Device_EnableMouseClipping);
    RegisterRuntimeAddr(0x1800e1780, (void*)&RtInput_Device_EnableMouseClipping); ++n;

    // PlatformUpdate (0x1800E2130) is NOT registered by name — it has no
    // mangled export in either the retail or the profile DLL (checked against
    // the export directory with pefile before writing a line of this file).
    // The ADDRESS is registered so that any ResolveAddr(Module::Runtime,
    // 0x1800E2130) caller on OUR side reaches ours. It does NOT redirect the
    // tail-jump inside the DLL's own `Input::Device::Update` (0x1800E1490),
    // which is still on the DLL — an internal jump needs a detour, not a
    // registration, and this lane did not install one.
    RegisterRuntimeAddr(0x1800e2130, (void*)&Device_PlatformUpdate);
    return n;
}
