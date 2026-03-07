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
// Reimplementations
// ============================================================

// Dtor: resets vtable to path then base, calls External::~External, conditionally frees
// FUN_140004d30 decompilation: sets vtables, External::~External(self), MemFree if flags&1
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreatePathDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: CreatePath has no extra fields — just Serializer ctor/dtor, returns 1
// FUN_140047a70 decompilation: Serializer ctor, Serializer dtor, return 1
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreatePathDescription::Serialize";
    (void)self;
    uint8_t serializer[32];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: call vtable[0] dtor (passes through all params), then External ctor + set vtable
// FUN_140004ce0 decompilation:
//   (*(vtable[0]))(self, 0, param_3, param_4, 0xFFFFFFFFFFFFFFFE);
//   Object::External::External(self);
//   vtable = PTR_FUN_1401629a0; vtable = PTR_FUN_140159210;
// Note: actual function takes 4 params — pass them through to preserve ABI
static void* Reimpl_Cleanup(void* self, uint64_t /*param2*/, uint64_t param3, uint64_t param4) {
    g_lastHook = "CreatePathDescription::Cleanup";
    typedef void* (*fn_Dtor5_t)(void*, uint32_t, uint64_t, uint64_t, int64_t);
    ((fn_Dtor5_t)(*(void***)self)[0])(self, 0, param3, param4, (int64_t)-2);
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x1401629a0);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x140159210);
    return self;
}

// CreateInPlace: allocate and initialize a CreatePathDescription in place
// FUN_140004c90 decompilation: null check, External ctor, set vtable (base then path)
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CreatePathDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x1401629a0);
    *(void**)self = (void*)ResolveAddr(Module::Exe, 0x140159210);
    return self;
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
