// engine/reimpl/mapui.cpp
// Reimplementation of MapUI vtable methods from Application-x64.exe.
//
// MapUI (0x50 bytes), vtable at 0x14015c350.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400124D0 — Dtor (scalar deleting destructor)
//   +0x20: 0x14006CA80 — Slot4 (shutdown)
//   +0x48: 0x14006C9E0 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x140012230 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140012220 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14006CE80 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x38: Slot7, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4, param5, param6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMapUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400124D0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[MapUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMapUIDtor() { RemoveHook(&g_hookDtor); }

void InstallMapUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006CA80);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[MapUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveMapUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallMapUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006C9E0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 16);
#ifdef _DEBUG
    printf("[MapUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveMapUISlot9() { RemoveHook(&g_hookSlot9); }
