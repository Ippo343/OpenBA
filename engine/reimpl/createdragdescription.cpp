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
// Reimplementations
// ============================================================

// Dtor: resets vtable to drag then base, calls External::~External, conditionally frees
// FUN_1400329e0 decompilation: sets vtables, External::~External(self), MemFree if flags&1
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreateDragDescription::Dtor";
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: wraps fields in Serializer
// FUN_140119750: Serializer ctor/dtor, 1x SerializerEncodingVCall (+0x20),
//                4x Serialize_Int (+0x28,+0x2c,+0x30,+0x34), 1x bool Serialize_Int (+0x24),
//                FUN_140002340 (ExternalPtr serialize helper) at +0x38
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreateDragDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x20));
    g_fn_Serialize_Int(serializer, (void*)(s + 0x28), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x2c), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x30), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x34), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x24), nullptr);
    // FUN_140002340: ExternalPtr<BuildingDescription> field serialize (checks DAT_1401c7318)
    fn_ExternalPtr_BuildSerialize((void*)(s + 0x38), serializer);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: call vtable[0] dtor with flags=0, then call Ctor to reinitialize in place
// FUN_1400329c0: (*(vtable[0]))(self, 0); CreateDragDescription_Ctor(self);
// Ctor = CreateInPlace at 0x1400329b0 (unhookable whole-function, in engine_funcs)
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreateDragDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);
    fn_CreateDragDescription_Ctor(self);
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
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateDragDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateDragDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateDragDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400329c0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreateDragDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateDragDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
