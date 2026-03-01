// engine/reimpl/environmentdescription.cpp
// Reimplementation of EnvironmentDescription vtable methods from Application-x64.exe.
//
// EnvironmentDescription (0x1b8 bytes), vtable at 0x14015e1f0.
// Extends Object::External (0x20 bytes base).
// Large struct: 12 weather entries at +0x38 (each 0x20 bytes, loop count 0xb+1=12).
//
// NOTE: Only 9 vtable slots (not 10). Slot8 is the last.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140019190 — Slot0 Dtor(self, flags)
//   +0x28: 0x140094040 — Slot5 Serialize(self, stream)
//   +0x30: 0x140019170 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140019110 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140019100 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1400941b0 — 6 bytes, Slot3 GetSerializeVersion (returns 7)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140019160 — 12 bytes (whole function), Slot8 CreateInPlace

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "EnvironmentDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "EnvironmentDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "EnvironmentDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallEnvironmentDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140019190);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallEnvironmentDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140094040);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallEnvironmentDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140019170);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
