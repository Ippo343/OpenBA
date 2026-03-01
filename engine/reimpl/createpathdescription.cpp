// engine/reimpl/createpathdescription.cpp
// Reimplementation of CreatePathDescription vtable methods from Application-x64.exe.
//
// CreatePathDescription (0x20 bytes), vtable at 0x140159210.
// Extends Object::External (0x20 bytes base). Minimal — no extra fields.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140004d30 — Slot0 Dtor(self, flags)
//   +0x28: 0x140047a70 — Slot5 Serialize(self, stream)
//   +0x30: 0x140004ce0 — Slot6 Cleanup(self)
//   +0x48: 0x140004c90 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x140004c20 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140004c10 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140047ab0 — 6 bytes, Slot3 GetSerializeVersion (returns 1)
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
    g_lastHook = "CreatePathDescription::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreatePathDescription::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreatePathDescription::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

typedef void* (*fn_CreateInPlace_t)(void* self);
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CreatePathDescription::CreateInPlace";
    return ((fn_CreateInPlace_t)g_hookCreateInPlace.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreatePathDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140004d30);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreatePathDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePathDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreatePathDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140047a70);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 13);
#ifdef _DEBUG
    printf("[CreatePathDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePathDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreatePathDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140004ce0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[CreatePathDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePathDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallCreatePathDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140004c90);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[CreatePathDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePathDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
