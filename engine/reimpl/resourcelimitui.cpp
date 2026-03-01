// engine/reimpl/resourcelimitui.cpp
// Reimplementation of ResourceLimitUI vtable methods from Application-x64.exe.
//
// ResourceLimitUI (0x40 bytes), vtable at 0x14015d1f0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400138D0 — Dtor (scalar deleting destructor)
//   +0x20: 0x140077970 — Slot4 (shutdown)
//   +0x38: 0x1400778D0 — Slot7 (update handler)
//   +0x48: 0x1400777E0 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x1400136A0 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140013690 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14003A95A — 6 bytes, Slot3 (thunk to Runtime)
//   +0x28: 0x14003A960 — 6 bytes, Slot5 (thunk to Runtime)
//   +0x30: 0x14003A966 — 6 bytes, Slot6 (thunk to Runtime)
//   +0x40: 0x14003A96C — 6 bytes, Slot8 (thunk to Runtime)
//   +0x50: 0x14003A978 — 6 bytes, Slot10 (thunk to Runtime)

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
    g_lastHook = "ResourceLimitUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ResourceLimitUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "ResourceLimitUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "ResourceLimitUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4, param5, param6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallResourceLimitUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400138D0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ResourceLimitUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveResourceLimitUIDtor() { RemoveHook(&g_hookDtor); }

void InstallResourceLimitUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140077970);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 17);
#ifdef _DEBUG
    printf("[ResourceLimitUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveResourceLimitUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallResourceLimitUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400778D0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[ResourceLimitUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveResourceLimitUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallResourceLimitUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400777E0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 16);
#ifdef _DEBUG
    printf("[ResourceLimitUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveResourceLimitUISlot9() { RemoveHook(&g_hookSlot9); }
