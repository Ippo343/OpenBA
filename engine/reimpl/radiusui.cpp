// engine/reimpl/radiusui.cpp
// Reimplementation of RadiusUI vtable methods from Application-x64.exe.
//
// RadiusUI (0x38 bytes), vtable at 0x14015d160.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140013640 — Dtor (scalar deleting destructor)
//   +0x28: 0x140077650 — Slot5 (handler)
//   +0x30: 0x140077600 — Slot6 (handler)
//
// Unhookable (too small):
//   +0x08: 0x140013580 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140013570 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14003A95A — 6 bytes, Slot3 (thunk to Runtime)
//   +0x20: 0x14003A984 — 6 bytes, Slot4 (thunk to Runtime)
//
// Stubs (base class no-ops):
//   +0x38: Slot7, +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

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
static HookInfo g_hookSlot6;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot5_t)(void* self);
static void Reimpl_Slot5(void* self) {
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self);
}

typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallRadiusUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013640);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[RadiusUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusUIDtor() { RemoveHook(&g_hookDtor); }

void InstallRadiusUISlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140077650);
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 16);
#ifdef _DEBUG
    printf("[RadiusUI] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusUISlot5() { RemoveHook(&g_hookSlot5); }

void InstallRadiusUISlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140077600);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[RadiusUI] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusUISlot6() { RemoveHook(&g_hookSlot6); }
