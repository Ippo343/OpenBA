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
    uint8_t _unknown00[0x10];  // +0x00 qword, ctor-zeroed; +0x08 qword, ctor's 2nd arg. Not read by this lane.
    uint8_t _stateLow;         // +0x10  low byte of the ctor's word store (0x00). Not read by this lane.
    uint8_t _running;          // +0x11  IsPaused: this[0x11] == 0 means PAUSED. Ctor sets 1 (not paused at construction).
    uint8_t _unknown12[0x16];  // +0x12..+0x27  +0x18 (qword) and +0x20 (dword) proven zeroed by the ctor; rest unproven.
    uint8_t _quitRequested;    // +0x28  Quit() sets 1; PlatformUpdate() reads it and calls PostQuitMessage(0).
};
static_assert(offsetof(Application, _running)       == 0x11, "Application._running @ 0x11");
static_assert(offsetof(Application, _quitRequested) == 0x28, "Application._quitRequested @ 0x28");

}  // namespace RtApplication
