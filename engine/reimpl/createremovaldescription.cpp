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
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_CreateRemovalDescription = 0x140162d80;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;
static HookInfo g_hookCreateInPlace;

// ============================================================
// Field layout:
//   +0x20: bool       (initialized to 0)
//   +0x28: void*      ptr to ExternalPtr array (heap, 8-byte elements)
//   +0x30: int        count of ExternalPtrs in array
//   +0x38: ExternalPtr (8-byte external pointer)
//   +0x40: bool
//   +0x41: bool
//   +0x42: bool
//   +0x43: bool
// ============================================================

// Dtor: frees ptr array at +0x28, zeros +0x28/+0x30, External::~External, conditional free.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreateRemovalDescription::Dtor";
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x28));
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: bool at +0x20, count at +0x30, resize ExternalPtr array at +0x28,
//            serialize array via fn_ExternalRefArray_Serialize, ExternalPtr at +0x38,
//            bools at +0x40/+0x41/+0x42/+0x43.
static uint64_t Reimpl_Serialize(void* self, void* stream, void* /*param3*/, void* param4) {
    g_lastHook = "CreateRemovalDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // bool at +0x20
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x20), nullptr);

    // int count at +0x30 — serialize into local (matching original pattern)
    int localCount = *(int*)(s + 0x30);
    g_fn_Serialize_Int(serializer, (void*)&localCount, nullptr);

    // Mode check: original reads *(int*)(serializer_field0 + 8)
    void* serField0 = *(void**)serializer;
    int mode = *(int*)((uintptr_t)serField0 + 8);
    if (mode == 0) {
        fn_ExeArrayResize3((void*)(s + 0x28), localCount);
    }

    // Serialize ExternalPtr array — pass param4 through (required by FUN_1400049c0)
    fn_ExternalRefArray_Serialize(serializer, localCount, *(void**)(s + 0x28), param4);

    // ExternalPtr at +0x38
    fn_ExternalPtr_Serialize3((void*)(s + 0x38), serializer);

    // bools at +0x40/+0x41/+0x42/+0x43
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x40), nullptr);
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x41), nullptr);
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x42), nullptr);
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x43), nullptr);

    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(self, 0) to destroy fields, then re-init.
// Decompilation calls External::External, sets vtable, zeros all fields.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreateRemovalDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);

    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_CreateRemovalDescription);
    *(uint8_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
}

// CreateInPlace: null check, External ctor, set vtable, zero all fields.
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "CreateRemovalDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_CreateRemovalDescription);
    *(uint8_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
    return self;
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
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateRemovalDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032c40);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallCreateRemovalDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032be0);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[CreateRemovalDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveCreateRemovalDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
