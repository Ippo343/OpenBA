// engine/reimpl/selectdescription.cpp
// Reimplementation of SelectDescription vtable methods from Application-x64.exe.
//
// SelectDescription (0x48 bytes), vtable at 0x14015a5f0.
// Extends Object::External (0x20 bytes base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x14000bcc0 — Slot0 Dtor(self, flags)
//   +0x28: 0x140055630 — Slot5 Serialize(self, stream)
//   +0x30: 0x14000bc60 — Slot6 Cleanup(self)
//   +0x48: 0x14000bc00 — Slot9 CreateInPlace(self) -> self
//
// Unhookable (too small):
//   +0x08: 0x14000bb80 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x14000bb70 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1400556c0 — 6 bytes, Slot3 GetSerializeVersion (returns 1)
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
static const uintptr_t kVtable_SelectDescription = 0x14015a5f0;

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
// From decompile: sets vtable 0x14015a5f0, destroys strings, sets base vtable, calls ~External
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "SelectDescription::Dtor";
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_SelectDescription);
    fn_String_Dtor((void*)(s + 0x38));
    fn_String_Dtor((void*)(s + 0x28));
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: ExternalPtr at +0x20, String at +0x28, ExternalPtr2 at +0x30,
// String at +0x38, vcall via serializer encoding vtable[0x98] for +0x40
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "SelectDescription::Serialize";
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_Serialize((void*)(s + 0x20), serializer);
    g_fn_String_Serialize((void*)(s + 0x28), serializer);
    fn_ExternalPtr_Serialize2((void*)(s + 0x30), serializer);
    g_fn_String_Serialize((void*)(s + 0x38), serializer);
    SerializerEncodingVCall(serializer, 0x98, (void*)(s + 0x40));
    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(no-free) + External::External + set vtable + zero fields
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "SelectDescription::Cleanup";
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_SelectDescription);
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
}

// CreateInPlace: null check, External::External, set vtable, zero fields
static void* Reimpl_CreateInPlace(void* self) {
    g_lastHook = "SelectDescription::CreateInPlace";
    if (!self) return nullptr;
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_SelectDescription);
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint32_t*)(s + 0x40) = 0;
    return self;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallSelectDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000bcc0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[SelectDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveSelectDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallSelectDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140055630);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[SelectDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveSelectDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallSelectDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000bc60);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[SelectDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveSelectDescriptionCleanup() { RemoveHook(&g_hookCleanup); }

void InstallSelectDescriptionCreateInPlace() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14000bc00);
    InstallHook(target, (void*)Reimpl_CreateInPlace, &g_hookCreateInPlace, 15);
#ifdef _DEBUG
    printf("[SelectDescription] CreateInPlace hooked at %p\n", (void*)target);
#endif
}
void RemoveSelectDescriptionCreateInPlace() { RemoveHook(&g_hookCreateInPlace); }
