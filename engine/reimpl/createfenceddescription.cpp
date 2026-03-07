// engine/reimpl/createfenceddescription.cpp
// Reimplementation of CreateFencedDescription vtable methods from Application-x64.exe.
//
// CreateFencedDescription (0x78 bytes), vtable at 0x1401591a0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140004960 — Slot0 Dtor(self, flags)
//   +0x28: 0x1400478f0 — Slot5 Serialize(self, stream)
//   +0x30: 0x140004930 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x1400048d0 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x1400048c0 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1400479c0 — 6 bytes, Slot3 GetSerializeVersion (returns 3)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x140004920 — 12 bytes (whole function), Slot9 CreateInPlace

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

// Dtor: resets vtable to fenced then base, calls External::~External, conditionally frees
// FUN_140004960 decompilation: sets vtables, External::~External(self), MemFree if flags&1
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreateFencedDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// SerializerEncodingVCall with extra int param (3-arg variant for Fenced encoding call)
// Ghidra shows: (**(code **)(*encoding + 0x98))(encoding, field, 0)
static inline void SerializerEncodingVCall3(void* serializer, int vtableOffset, void* field, int extra) {
    void* encoding = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(serializer) + 8);
    void* vtable = *reinterpret_cast<void**>(encoding);
    typedef void (*VFunc_t)(void*, void*, int);
    auto fn = *reinterpret_cast<VFunc_t*>(reinterpret_cast<uintptr_t>(vtable) + vtableOffset);
    fn(encoding, field, extra);
}

// Serialize: wraps fields in Serializer
// FUN_1400478f0: Serializer ctor/dtor, SerializerEncodingVCall3 (+0x20, extra=0),
//                ExternalRefArray count=6 at +0x28, 4x Serialize_Int (+0x58,+0x5c,+0x60,+0x64),
//                1x bool Serialize_Int (+0x68), FUN_140002340 (ExternalPtr helper) at +0x70
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreateFencedDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    SerializerEncodingVCall3(serializer, 0x98, (void*)(s + 0x20), 0);
    fn_ExternalRefArray_Serialize(serializer, 6, (void*)(s + 0x28), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x58), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x5c), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x60), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 100), nullptr);    // 0x64 = 100
    g_fn_Serialize_Int(serializer, (void*)(s + 0x68), nullptr);
    // FUN_140002340: ExternalPtr<BuildingDescription> field serialize (checks DAT_1401c7318)
    fn_ExternalPtr_BuildSerialize((void*)(s + 0x70), serializer);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: call vtable[0] dtor with flags=0, then call Ctor to reinitialize in place
// FUN_140004930: (*(vtable[0]))(self, 0); CreateFencedDescription_Ctor(self);
// Ctor = CreateInPlace at 0x140004920 (unhookable whole-function, in engine_funcs)
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreateFencedDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);
    fn_CreateFencedDescription_Ctor(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreateFencedDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140004960);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreateFencedDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateFencedDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreateFencedDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400478f0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateFencedDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateFencedDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateFencedDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140004930);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreateFencedDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateFencedDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
