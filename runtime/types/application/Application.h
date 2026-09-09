#pragma once
// ============================================================================
// runtime/types/application/Application.h — Application::Application layout
//                                            (Wave 8, lane E)
// ============================================================================
//
// Every offset below was read out of the SHIPPED Runtime-x64.dll (image base
// 0x180000000) in Ghidra, 2026-09-07. This lane does NOT own or port the
// constructor — it is not in the wave's function list — so the layout here is
// only as complete as this lane's five member functions need, and says so
// rather than inventing the rest.
//
// NOT A POLYMORPHIC CLASS. The ctor (`Application`, 0x18009f590, found via the
// xrefs to the `_singleton` data export rather than by name — it has no
// mangled export of its own) is:
//
//   MOV qword ptr [RSP+0x8],RCX
//   ...
//   LEA RAX,[RCX+0x18]
//   XOR R8D,R8D
//   MOV qword ptr [RAX],R8          ; +0x18 = 0
//   MOV dword ptr [RAX+0x8],R8D     ; +0x20 = 0
//   MOV byte ptr [RCX+0x28],R8B     ; +0x28 = 0
//   MOV qword ptr [0x1801fc678],RCX ; publish _singleton
//   MOV qword ptr [RCX],R8          ; +0x00 = 0
//   MOV qword ptr [RCX+0x8],RDX     ; +0x08 = ctor's 2nd argument
//   MOV word ptr [RCX+0x10],0x100   ; +0x10 = 0x00, +0x11 = 0x01
//
// The FIRST store is a zeroed qword at +0x00, not a `LEA` of a `??_7…6B@`
// vtable symbol — the exact test WAVE8.md §1 requires of every lane. Combined
// with the DLL exporting no `??_7Application@Application@@6B@` (147 `??_7`
// exports total, none for this class — pre-wave measurement in WAVE8.md §2),
// PLAN §4.3 risk 3 (vtable ownership) does not apply to this class.
//
// `?_singleton@Application@1@0PEAV11@EA` is at 0x1801fc678, confirmed by the
// `MOV qword ptr [0x1801fc678],RCX` store above and cross-checked against
// pefile's export directory. `engine/funcs/engine_funcs.cpp`/`.h` carry a
// STALE comment claiming this address is `0x1801feb80` — that number is not
// used by any resolve call (the real resolution goes through
// `standalone/address_registry.cpp`'s dispatch-slot registration of
// `ResolveAddr(Module::Runtime, 0x1801fc678)`, which is correct) — fixed at
// its source by this lane (PLAN §6 step 8); see WAVE8_LANE_E.md §5.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtApplication {

// Only the two bytes this lane's functions read or write are named. Every
// other range is an unnamed, unproven gap — PLAN §6 step 8: say what is not
// known rather than guess. No sizeof() assertion: the object's true extent
// past +0x28 was never observed by any function this lane ported.
struct Application {
    // +0x00  THE MAIN WINDOW HANDLE. Named by Wave 9 lane VID2:
    // `?PlatformStartup@Device@Video@@AEAA_NXZ` opens with
    //     MOV RAX,[0x1801fc678] ; MOV RDX,[RAX] ; MOV [RCX+0x90],RDX
    // i.e. it copies `*(void**)Application::_singleton` into
    // `Video::Device::windowHandle` (+0x90), which runtime/types/video/Device.h
    // already documented from the other side as "HWND, copied from
    // Application::_singleton[0] by Startup". Two independent bodies, same
    // conclusion. The ctor zeroes it; something later in bring-up fills it.
    void*   _windowHandle;     // +0x00
    // +0x08  THE PLATFORM HANDLE (HINSTANCE). The ctor's 2nd argument, stored
    // by `MOV qword ptr [RCX+0x8],RDX` above. Named by Wave 9 lane PLAT:
    // `?PlatformStartup@Application@1@AEAA_NXZ` (0x18009fc10) loads it TWICE —
    //     18009fc59: MOV RAX,[RDI+0x8]  -> WNDCLASSW::hInstance
    //     18009fcb0: MOV RAX,[RDI+0x8]  -> CreateWindowExW's hInstance argument
    // and both uses are HINSTANCE-shaped. `Engine::Startup` passes
    // `this->_platformHandle` into the ctor (engine_lifecycle.cpp:635), which
    // is the module handle WinMain received, so the two sides agree.
    //
    // Declared `void*` rather than `HINSTANCE`: PLAN §4.1 decision 1 keeps
    // <windows.h> out of every header under runtime/.
    void*   _platformHandle;   // +0x08
    // +0x10  THE CURSOR-ENABLED FLAG. Was `_stateLow`, commented "low byte of
    // the ctor's word store (0x00). Not read." BOTH halves of that were
    // wrong about the read, and Wave 12 found it twice independently:
    //   * lane A from `?IsCursorEnabled@Application@1@QEAA_NXZ` (0x18000a0c0),
    //     which is exactly `movzx eax,byte [rcx+0x10]; ret`;
    //   * lane C from `?EnableCursor@Application@1@QEAAX_N@Z` (0x18009f800),
    //     which reads this byte before writing it (`CMP BYTE PTR [RCX+0x10],DL`)
    //     and early-outs when the value is unchanged.
    // The ctor's word store (`MOV word ptr [RCX+0x10],0x100`) writes this byte
    // 0x00 and _running (+0x11) 0x01 in the same instruction, so cursor-enabled
    // starts FALSE. Corrected at its source per PLAN.md §6 step 8, not only in
    // a wave doc.
    uint8_t _cursorEnabled;    // +0x10
    uint8_t _running;          // +0x11  IsPaused: this[0x11] == 0 means PAUSED. Ctor sets 1.
    uint8_t _unknown12[0x06];  // +0x12..+0x17  unproven.

    // +0x18  THE MESSAGE-CALLBACK SET. Named by Wave 9 lane VID2 from
    // `?RegisterMessageCallback@Application@1@…` (0x18009fa60), which does
    // `LEA RDI,[RCX+0x18]` and then treats RDI as a `Collection::Set` — root at
    // +0x00, count at +0x08 (`INC dword ptr [RDI+8]`, and the erase decrements
    // the same dword at 0x18000abae). `?UnregisterMessageCallback@` (0x18009fa20)
    // and `?PlatformShutdown@Device@Video@@` (0x1800f83f0) descend the same tree
    // from the same offset. That is exactly the "+0x18 (qword) and +0x20 (dword)
    // proven zeroed by the ctor" range this comment used to leave unnamed — the
    // ctor is zero-initialising an empty Set.
    //
    // The element is {unsigned message; int (*)(HWND, unsigned, WPARAM, LPARAM)}.
    // Declared as raw storage rather than as `rtsetavl::Set` so this header
    // stays free of runtime/src/ includes; the two layouts are asserted against
    // each other in runtime/src/application/message_callbacks.cpp.
    void*   _messageCallbackRoot;   // +0x18  Set::root
    int32_t _messageCallbackCount;  // +0x20  Set::count
    int32_t _unknown24;            // +0x24  unproven.
    uint8_t _quitRequested;        // +0x28  Quit() sets 1; PlatformUpdate() reads it.
};
static_assert(offsetof(Application, _windowHandle)         == 0x00, "Application._windowHandle @ 0x00");
static_assert(offsetof(Application, _platformHandle)       == 0x08, "Application._platformHandle @ 0x08");
static_assert(offsetof(Application, _cursorEnabled)        == 0x10, "Application._cursorEnabled @ 0x10");
static_assert(offsetof(Application, _running)              == 0x11, "Application._running @ 0x11");
static_assert(offsetof(Application, _messageCallbackRoot)  == 0x18, "Application._messageCallbackRoot @ 0x18");
static_assert(offsetof(Application, _messageCallbackCount) == 0x20, "Application._messageCallbackCount @ 0x20");
static_assert(offsetof(Application, _quitRequested)        == 0x28, "Application._quitRequested @ 0x28");

}  // namespace RtApplication
