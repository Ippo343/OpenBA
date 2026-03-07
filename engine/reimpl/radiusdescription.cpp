// engine/reimpl/radiusdescription.cpp
// Reimplementation of RadiusDescription vtable methods from Application-x64.exe.
//
// RadiusDescription (0x30 bytes), vtable at 0x14015a580.
// Extends Object::External (0x20 bytes base).
// Fields: +0x20 = ExternalPtr (8 bytes), +0x28 = int (default 0x1e = 30)
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x14000bb10 — Slot0 Dtor(self, flags)
//   +0x28: 0x140055530 — Slot5 Serialize(self, stream)
//   +0x30: 0x14000bab0 — Slot6 Cleanup(self)
//   +0x48: 0x14000ba50 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x14000b9d0 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x14000b9c0 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x140055590 — 6 bytes, Slot3 GetSerializeVersion (returns 1)
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
static const uintptr_t kVtable_RadiusDescription = 0x14015a580;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;
static HookInfo g_hookCreateInPlace;

// ============================================================
// Reimplemented functions
// ============================================================

// Dtor: sets vtable, calls External::~External, conditionally frees
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "RadiusDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: creates Serializer, serializes int at +0x28, ExternalPtr at +0x20
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "RadiusDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x28), nullptr);
    fn_ExternalPtr_Serialize3((void*)(s + 0x20), serializer);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(no-free) + re-construct
// The Cleanup decompilation calls External::External then sets vtable and inits fields.
// This IS the Ctor logic inline — we replicate it directly.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "RadiusDescription::Cleanup";
    // Virtual dtor with flags=0 (destroy fields, don't free memory)
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);
    // Re-construct: External::External + set vtable + init fields
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_RadiusDescription);
    *(uint64_t*)(s + 0x20) = 0;        // ExternalPtr = null
    *(uint32_t*)(s + 0x28) = 0x1e;     // int = 30
}

// CreateInPlace: null check, External ctor, set vtable, init fields
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "RadiusDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_RadiusDescription);
    *(uint64_t*)(s + 0x20) = 0;
    *(uint32_t*)(s + 0x28) = 0x1e;
    return self;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallRadiusDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000bb10);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[RadiusDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallRadiusDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140055530);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[RadiusDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRadiusDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000bab0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[RadiusDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallRadiusDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000ba50);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[RadiusDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveRadiusDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
