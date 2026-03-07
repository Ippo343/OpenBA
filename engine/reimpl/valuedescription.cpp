// engine/reimpl/valuedescription.cpp
// Reimplementation of ValueDescription vtable methods from Application-x64.exe.
//
// ValueDescription (0x50 bytes), vtable at 0x14015add0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x14000fbf0 — Slot0 Dtor(self, flags)
//   +0x28: 0x14005cd60 — Slot5 Serialize(self, stream)
//   +0x30: 0x14000fb80 — Slot6 Cleanup(self)
//   +0x48: 0x14000fb10 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x14000fa90 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x14000fa80 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14005ce10 — 6 bytes, Slot3 GetSerializeVersion (returns 3)
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
// Reimplemented functions
// ============================================================

// Dtor: set vtable, ~String(+0x38), ~String(+0x28), External::~External, free
// From decompile: sets vtable 0x14015add0, destroys strings, sets base vtable, calls ~External
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ValueDescription::Dtor";
    uintptr_t s = (uintptr_t)self;
    static void* vtable = nullptr;
    if (!vtable) vtable = (void*)ResolveAddr(Module::Exe, 0x14015add0);
    *(void**)self = vtable;
    fn_String_Dtor((void*)(s + 0x38));
    fn_String_Dtor((void*)(s + 0x28));
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: ExternalPtr at +0x20, String at +0x28, ExternalPtr2 at +0x30,
// String at +0x38, Serialize int at +0x40, +0x44, +0x48
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ValueDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_Serialize((void*)(s + 0x20), serializer);
    g_fn_String_Serialize((void*)(s + 0x28), serializer);
    fn_ExternalPtr_Serialize2((void*)(s + 0x30), serializer);
    g_fn_String_Serialize((void*)(s + 0x38), serializer);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x40), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x44), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x48), nullptr);
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(no-free) + External::External + set vtable + zero fields
// Note: +0x44 = 100 is the default (from CreateInPlace decompile: *(undefined8)(+0x44) = 100)
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "ValueDescription::Cleanup";
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    static void* vtable = nullptr;
    if (!vtable) vtable = (void*)ResolveAddr(Module::Exe, 0x14015add0);
    *(void**)self = vtable;
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x44) = 100; // default min/max or step value
}

// CreateInPlace: null check, External::External, set vtable, zero fields, +0x44=100
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "ValueDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    static void* vtable = nullptr;
    if (!vtable) vtable = (void*)ResolveAddr(Module::Exe, 0x14015add0);
    *(void**)self = vtable;
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x44) = 100;
    return self;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallValueDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fbf0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ValueDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallValueDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cd60);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ValueDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallValueDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fb80);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[ValueDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallValueDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000fb10);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[ValueDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveValueDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
