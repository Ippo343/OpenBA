// engine/reimpl/createplaceddescription.cpp
// Reimplementation of CreatePlacedDescription vtable methods from Application-x64.exe.
//
// CreatePlacedDescription (0x58 bytes), vtable at 0x140162cf0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140032ad0 — Slot0 Dtor(self, flags)
//   +0x28: 0x1401198d0 — Slot5 Serialize(self, stream)
//   +0x30: 0x140032ab0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140032a50 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032a40 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1401199f0 — 6 bytes, Slot3 GetSerializeVersion (returns 4)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x140032aa0 — 12 bytes (whole function), Slot9 CreateInPlace

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
    g_lastHook = "CreatePlacedDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreatePlacedDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreatePlacedDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreatePlacedDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032ad0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreatePlacedDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401198d0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreatePlacedDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032ab0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
