// engine/reimpl/toolbardescription.cpp
// Reimplementation of ToolbarDescription vtable methods from Application-x64.exe.
//
// ToolbarDescription (0x68 bytes), vtable at 0x140164980.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140039500 — Slot0 Dtor(self, flags)
//   +0x28: 0x14012da30 — Slot5 Serialize(self, stream)
//   +0x30: 0x1400394e0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140039480 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140039470 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14012db60 — 6 bytes, Slot3 GetSerializeVersion (returns 4)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x1400394d0 — 12 bytes (whole function), Slot9 CreateInPlace

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
    g_lastHook = "ToolbarDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ToolbarDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "ToolbarDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallToolbarDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140039500);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 12);
#ifdef _DEBUG
    printf("[ToolbarDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallToolbarDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012da30);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ToolbarDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallToolbarDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400394e0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[ToolbarDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
