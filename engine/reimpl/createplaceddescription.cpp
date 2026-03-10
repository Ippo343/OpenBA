// engine/reimpl/createplaceddescription.cpp
// Reimplementation of CreatePlacedDescription vtable methods from Application-x64.exe.
//
// CreatePlacedDescription (0x58 bytes), vtable at 0x140162cf0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140032ad0 — Slot0 Dtor(self, flags)
//   +0x28: 0x1401198d0 — Slot5 Serialize(self, stream)
//   +0x30: 0x140032ab0 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140032a50 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140032a40 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1401199f0 — 6 bytes, Slot3 GetSerializeVersion (returns 4)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140004950 — tiny, Slot8
//   +0x48: 0x140032aa0 — 12 bytes (whole function), Slot9 CreateInPlace

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_CreatePlacedDescription = 0x140162cf0;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;

// ============================================================
// Field layout:
//   +0x20: uint32_t   flags/param0 (initialized to 0)
//   +0x28: String     (8-byte COW string)
//   +0x30: int*       ptr to int array (heap)
//   +0x38: int        count of ints in array
//   +0x40: int        (initialized to 0)
//   +0x44: int        (initialized to 1)
//   +0x48: int        (initialized to 1)
//   +0x50: ExternalPtr (8-byte external pointer)
// ============================================================

// Dtor: frees int array at +0x30, zeros +0x30/+0x38, String::~String at +0x28,
//       External::~External, conditional free.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CreatePlacedDescription::Dtor";
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x30));
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    fn_String_Dtor((void*)(s + 0x28));
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: IO::Serializer, serialize External at +0x20 via encoding vcall,
//            serialize count at +0x38, resize int array at +0x30, serialize each int,
//            String::Serialize at +0x28, ints at +0x44/+0x48/+0x40,
//            ExternalPtr at +0x50 via fn_ExternalPtr_Serialize3.
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CreatePlacedDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize External base at +0x20 via encoding virtual call
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x20));

    // Serialize count into local (matching original — does NOT write to +0x38 on load)
    uint32_t localCount = *(uint32_t*)(s + 0x38);
    g_fn_Serialize_Int(serializer, (void*)&localCount, nullptr);

    // Mode check: original reads *(int*)(serializer_field0 + 8)
    void* serField0 = *(void**)serializer;
    int mode = *(int*)((uintptr_t)serField0 + 8);
    if (mode == 0) {
        fn_ExeArrayResize2((void*)(s + 0x30), (int)localCount);
    }

    // Serialize each int in the array via encoding vcall
    void* arr = *(void**)(s + 0x30);
    for (uint32_t i = 0; i < (int)localCount; i++) {
        SerializerEncodingVCall(serializer, 0x98, (void*)((uintptr_t)arr + i * 4));
    }

    // String::Serialize at +0x28
    g_fn_String_Serialize((void*)(s + 0x28), serializer);

    // Serialize ints at +0x44, +0x48, +0x40
    g_fn_Serialize_Int(serializer, (void*)(s + 0x44), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x48), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x40), nullptr);

    // ExternalPtr at +0x50
    fn_ExternalPtr_Serialize3((void*)(s + 0x50), serializer);

    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(self, 0) to destroy fields, then re-init.
// Mirrors CreateInPlace (0x140032aa0): External::External + set vtable + init fields.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "CreatePlacedDescription::Cleanup";
    typedef void* (*fn_Dtor_t)(void*, uint32_t);
    ((fn_Dtor_t)(*(void***)self)[0])(self, 0);

    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_CreatePlacedDescription);
    *(uint32_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
    *(uint32_t*)(s + 0x44) = 1;
    *(uint32_t*)(s + 0x48) = 1;
    *(uint64_t*)(s + 0x50) = 0;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCreatePlacedDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032ad0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallCreatePlacedDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401198d0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreatePlacedDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140032ab0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[CreatePlacedDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveCreatePlacedDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
