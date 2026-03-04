// engine/reimpl/attendanceui.cpp
// Reimplementation of AttendanceUI vtable methods from Application-x64.exe.
//
// AttendanceUI (0x38 bytes), vtable at 0x14015b070.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010560 — Dtor (scalar deleting destructor)
//   +0x20: 0x14005DD40 — Slot4 (setup/refresh)
//   +0x38: 0x14005DC50 — Slot7 (update handler)
//
// Unhookable (too small):
//   +0x08: 0x1400104A0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140010490 — 7 bytes, Slot2 (Cleanup getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

#include "ui/Panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot7;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "AttendanceUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, AttendanceUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (finds progress element, sets max from attendance capacity)
#include "Dialog/ui_panels.h"
static void Reimpl_Slot4(void* self) {
    g_lastHook = "AttendanceUI::Slot4";
    AttendanceUI_Slot4((AttendanceUI*)self);
}

// Slot7 — REIMPLEMENTED (updates progress bar with current attendance count)
static void Reimpl_Slot7(void* self) {
    g_lastHook = "AttendanceUI::Slot7";
    AttendanceUI_Slot7((AttendanceUI*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAttendanceUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010560);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[AttendanceUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveAttendanceUIDtor() { RemoveHook(&g_hookDtor); }

void InstallAttendanceUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005DD40);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[AttendanceUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveAttendanceUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallAttendanceUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005DC50);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 15);
#ifdef _DEBUG
    printf("[AttendanceUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveAttendanceUISlot7() { RemoveHook(&g_hookSlot7); }
