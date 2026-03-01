// engine/reimpl/eventlogui.cpp
// Reimplementation of EventLogUI vtable methods from Application-x64.exe.
//
// EventLogUI (0x80 bytes), vtable at 0x14015bc40.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400114B0 — Dtor (scalar deleting destructor)
//   +0x28: 0x140067640 — Slot5 (handler)
//   +0x30: 0x140067630 — Slot6 (3-byte RET, unhookable)
//   +0x38: 0x140067440 — Slot7 (update handler)
//   +0x48: 0x1400675D0 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x1400113D0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x1400113C0 — 7 bytes, Slot2 (Cleanup getter)
//   +0x30: 0x140067630 — 3 bytes, Slot6 (RET 0)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x20: Slot4(stub), +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "EventLogUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot5_t)(void* self);
static void Reimpl_Slot5(void* self) {
    g_lastHook = "EventLogUI::Slot5";
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "EventLogUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "EventLogUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallEventLogUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400114B0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[EventLogUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEventLogUIDtor() { RemoveHook(&g_hookDtor); }

void InstallEventLogUISlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140067640);
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 13);
#ifdef _DEBUG
    printf("[EventLogUI] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveEventLogUISlot5() { RemoveHook(&g_hookSlot5); }

void InstallEventLogUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140067440);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[EventLogUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveEventLogUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallEventLogUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400675D0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[EventLogUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveEventLogUISlot9() { RemoveHook(&g_hookSlot9); }
