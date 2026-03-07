// engine/reimpl/createroaddescription.cpp
// Reimplementation of CreateRoadDescription vtable methods from Application-x64.exe.
//
// CreateRoadDescription (0x30 bytes), vtable at 0x140162df0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140032e50 — Slot0 Dtor(self, flags)
//   +0x28: 0x140119c80 — Slot5 Serialize(self, stream)
//   +0x30: 0x140032df0 — Slot6 Cleanup(self)
//   +0x48: 0x140032da0 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x140032d20 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032d10 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140119ce0 — 6 bytes, Slot3 GetSerializeVersion (returns 2)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_CreateRoadDescription = 0x140162df0;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;
static HookInfo g_hookCreateInPlace;

// ============================================================
// Field layout (0x30 bytes total):
//   +0x20: bool       (initialized to 0)
//   +0x28: ExternalPtr (8-byte external pointer)
// ============================================================

// Dtor: no extra fields to destroy — just External::~External, conditional free.
// (Decompilation only sets vtable and calls External::~External before free.)
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreateRoadDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: bool at +0x20, ExternalPtr at +0x28 via fn_ExternalPtr_Serialize3.
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreateRoadDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // bool at +0x20
    g_fn_Serialize_Int(serializer, (void*)(s + 0x20), nullptr);

    // ExternalPtr at +0x28
    fn_ExternalPtr_Serialize3((void*)(s + 0x28), serializer);

    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(self, 0) to destroy fields, then re-init.
// Decompilation calls External::External, sets vtable, zeros +0x20 and +0x28.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreateRoadDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);

    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_CreateRoadDescription);
    *(uint8_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
}

// CreateInPlace: null check, External ctor, set vtable, zero fields.
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CreateRoadDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_CreateRoadDescription);
    *(uint8_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    return self;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreateRoadDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032e50);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreateRoadDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRoadDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreateRoadDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140119c80);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateRoadDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRoadDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateRoadDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032df0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[CreateRoadDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRoadDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallCreateRoadDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032da0);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[CreateRoadDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRoadDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
