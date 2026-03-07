// engine/reimpl/createbridgedescription.cpp
// Reimplementation of CreateBridgeDescription vtable methods from Application-x64.exe.
//
// CreateBridgeDescription (0x60 bytes), vtable at 0x140162c30.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400328f0 — Slot0 Dtor(self, flags)
//   +0x28: 0x1401195d0 — Slot5 Serialize(self, stream)
//   +0x30: 0x1400328d0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140032870 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032860 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1401196a0 — 6 bytes, Slot3 GetSerializeVersion (returns 2)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x1400328c0 — 12 bytes (whole function), Slot9 CreateInPlace

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
// Reimplementations
// ============================================================

// Dtor: resets vtable to bridge then base, calls External::~External, conditionally frees
// FUN_1400328f0 decompilation: sets vtables, External::~External(self), MemFree if flags&1
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreateBridgeDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: wraps fields in Serializer, uses encoding vcall for ExternalPtr fields
// FUN_1401195d0: Serializer ctor/dtor, 5x SerializerEncodingVCall (offsets +0x20,+0x28,+0x2c,+0x24,+0x30),
//                Serialize_Int (+0x34), ExternalRefArray count=5 at +0x38
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreateBridgeDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x20));
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x28));
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x2c));
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x24));
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x30));
    g_fn_Serialize_Int(serializer, (void*)(s + 0x34), nullptr);
    fn_ExternalRefArray_Serialize(serializer, 5, (void*)(s + 0x38), nullptr);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: call vtable[0] dtor with flags=0, then call Ctor to reinitialize in place
// FUN_1400328d0: (*(vtable[0]))(self, 0); CreateBridgeDescription_Ctor(self);
// Ctor = CreateInPlace at 0x1400328c0 (unhookable whole-function, resolve inline)
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreateBridgeDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);
    typedef void (*fn_Ctor_t)(void*);
    static fn_Ctor_t fn_Ctor = nullptr;
    if (!fn_Ctor) fn_Ctor = (fn_Ctor_t)ResolveAddr(Module::Exe, 0x1400328c0);
    fn_Ctor(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreateBridgeDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400328f0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreateBridgeDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateBridgeDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreateBridgeDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401195d0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateBridgeDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateBridgeDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateBridgeDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400328d0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreateBridgeDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateBridgeDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
