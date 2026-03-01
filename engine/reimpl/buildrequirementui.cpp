// engine/reimpl/buildrequirementui.cpp
// Reimplementation of BuildRequirementUI vtable methods from Application-x64.exe.
//
// BuildRequirementUI (0x60 bytes), vtable at 0x14015b1d0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010860 — Dtor (scalar deleting destructor)
//   +0x18: 0x14005EE80 — Slot3 (3-byte RET, unhookable)
//   +0x20: 0x14005E7C0 — Slot4 (setup/refresh)
//
// Unhookable (too small):
//   +0x08: 0x1400107E0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x1400107D0 — 7 bytes, Slot2 (Cleanup getter)
//   +0x18: 0x14005EE80 — 3 bytes, Slot3 (RET 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x38: Slot7(stub), +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

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

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildRequirementUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010860);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[BuildRequirementUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRequirementUIDtor() { RemoveHook(&g_hookDtor); }

void InstallBuildRequirementUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005E7C0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[BuildRequirementUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRequirementUISlot4() { RemoveHook(&g_hookSlot4); }
