// engine/reimpl/immigrationui.cpp
// Reimplementation of ImmigrationUI vtable methods from Application-x64.exe.
//
// ImmigrationUI (0x50 bytes), vtable at 0x14015c080.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140011FD0 — Dtor (scalar deleting destructor)
//   +0x20: 0x14006B330 — Slot4 (shutdown)
//   +0x38: 0x14006B130 — Slot7 (update handler)
//   +0x48: 0x14006B300 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x140011ED0 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140011EC0 — 7 bytes, Slot2 (RegisterType getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4, param5, param6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallImmigrationUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011FD0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ImmigrationUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveImmigrationUIDtor() { RemoveHook(&g_hookDtor); }

void InstallImmigrationUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006B330);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 17);
#ifdef _DEBUG
    printf("[ImmigrationUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveImmigrationUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallImmigrationUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006B130);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 16);
#ifdef _DEBUG
    printf("[ImmigrationUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveImmigrationUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallImmigrationUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006B300);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[ImmigrationUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveImmigrationUISlot9() { RemoveHook(&g_hookSlot9); }
