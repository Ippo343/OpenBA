// engine/reimpl/residenceui.cpp
// Reimplementation of ResidenceUI vtable methods from Application-x64.exe.
//
// ResidenceUI (0x40 bytes), vtable at 0x14015f6d0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140020FC0 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400B5850 — Slot4 (shutdown)
//   +0x38: 0x1400B51F0 — Slot7 (update handler)
//   +0x48: 0x1400B5820 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x140020F50 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140020F40 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x1400B5DB0 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ResidenceUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ResidenceUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "ResidenceUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "ResidenceUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4, param5, param6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallResidenceUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020FC0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ResidenceUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceUIDtor() { RemoveHook(&g_hookDtor); }

void InstallResidenceUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B5850);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ResidenceUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallResidenceUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B51F0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 17);
#ifdef _DEBUG
    printf("[ResidenceUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallResidenceUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B5820);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[ResidenceUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveResidenceUISlot9() { RemoveHook(&g_hookSlot9); }
