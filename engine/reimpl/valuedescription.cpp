// engine/reimpl/valuedescription.cpp
// Reimplementation of ValueDescription vtable methods from Application-x64.exe.
//
// ValueDescription (0x50 bytes), vtable at 0x14015add0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x14000fbf0 — Slot0 Dtor(self, flags)
//   +0x28: 0x14005cd60 — Slot5 Serialize(self, stream)
//   +0x30: 0x14000fb80 — Slot6 Cleanup(self)
//   +0x48: 0x14000fb10 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x14000fa90 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x14000fa80 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14005ce10 — 6 bytes, Slot3 GetSerializeVersion (returns 3)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8

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
static HookInfo g_hookCreateInPlace;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ValueDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ValueDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "ValueDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

typedef void* (*fn_CreateInPlace_t)(void* self);
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "ValueDescription::CreateInPlace";
    return ((fn_CreateInPlace_t)g_hookCreateInPlace.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallValueDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fbf0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ValueDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallValueDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cd60);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ValueDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallValueDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fb80);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[ValueDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallValueDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fb10);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[ValueDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
