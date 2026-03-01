// engine/reimpl/createremovaldescription.cpp
// Reimplementation of CreateRemovalDescription vtable methods from Application-x64.exe.
//
// CreateRemovalDescription (0x48 bytes), vtable at 0x140162d80.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140032ca0 — Slot0 Dtor(self, flags)
//   +0x28: 0x140119ac0 — Slot5 Serialize(self, stream)
//   +0x30: 0x140032c40 — Slot6 Cleanup(self)
//   +0x48: 0x140032be0 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x140032b60 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032b50 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140119bc0 — 6 bytes, Slot3 GetSerializeVersion (returns 1)
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
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

typedef void* (*fn_CreateInPlace_t)(void* self);
static void* Reimpl_CreateInPlace(void* self) {
    return ((fn_CreateInPlace_t)g_hookCreateInPlace.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreateRemovalDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032ca0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreateRemovalDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140119ac0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateRemovalDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032c40);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallCreateRemovalDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032be0);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 14);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
