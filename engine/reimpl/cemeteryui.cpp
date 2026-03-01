// engine/reimpl/cemeteryui.cpp
// Reimplementation of CemeteryUI vtable methods from Application-x64.exe.
//
// CemeteryUI (0x38 bytes), vtable at 0x14015b240.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400109B0 — Dtor (scalar deleting destructor)
//   +0x20: 0x14005F090 — Slot4 (setup/refresh)
//   +0x38: 0x14005EF50 — Slot7 (update handler)
//
// Unhookable (too small):
//   +0x08: 0x1400108F0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x1400108E0 — 7 bytes, Slot2 (Cleanup getter)
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
    g_lastHook = "CemeteryUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "CemeteryUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "CemeteryUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCemeteryUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400109B0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CemeteryUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryUIDtor() { RemoveHook(&g_hookDtor); }

void InstallCemeteryUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F090);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[CemeteryUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallCemeteryUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005EF50);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 17);
#ifdef _DEBUG
    printf("[CemeteryUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryUISlot7() { RemoveHook(&g_hookSlot7); }
