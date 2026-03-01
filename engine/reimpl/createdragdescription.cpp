// engine/reimpl/createdragdescription.cpp
// Reimplementation of CreateDragDescription vtable methods from Application-x64.exe.
//
// CreateDragDescription (0x40 bytes), vtable at 0x140162ca0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400329e0 — Slot0 Dtor(self, flags)
//   +0x28: 0x140119750 — Slot5 Serialize(self, stream)
//   +0x30: 0x1400329c0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140032960 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032950 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140119810 — 6 bytes, Slot3 GetSerializeVersion (returns 2)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x1400329b0 — 12 bytes (whole function), Slot9 CreateInPlace

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

// ============================================================
// Install / Remove
// ============================================================
void InstallCreateDragDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400329e0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreateDragDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateDragDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreateDragDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140119750);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[CreateDragDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateDragDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateDragDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400329c0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 13);
#ifdef _DEBUG
    printf("[CreateDragDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateDragDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
