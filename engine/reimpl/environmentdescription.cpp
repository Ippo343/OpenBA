// engine/reimpl/environmentdescription.cpp
// Reimplementation of EnvironmentDescription vtable methods from Application-x64.exe.
//
// EnvironmentDescription (0x1b8 bytes), vtable at 0x14015e1f0.
// Extends Object::External (0x20 bytes base).
// Fields:
//   +0x20: ExternalPtr (name/path reference)
//   +0x28: void* (dynamic array ptr, loaded from serialize)
//   +0x30: uint32 (dynamic array count)
//   +0x38..+0x1b8: 12 weather entries, each 0x20 bytes:
//     +0x00: Color (ambient sky)
//     +0x04: Color (ambient ground)
//     +0x08: Color (ambient fog)
//     +0x0c: float (fog start)
//     +0x10: float (fog end)
//     +0x14: float (sun intensity)
//     +0x18: float (sun angle)
//     +0x1c: float (sun spread)
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140019190 — Slot0 Dtor(self, flags)
//   +0x28: 0x140094040 — Slot5 Serialize(self, stream)
//   +0x30: 0x140019170 — Slot6 Cleanup(self)
//
// Unhookable (too small or whole-function = 12 bytes):
//   +0x08: 0x140019110 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140019100 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x1400941b0 — 6 bytes, Slot3 GetSerializeVersion (returns 7)
//   +0x20: 0x14003a912 — 6 bytes, Slot4 JMP thunk
//   +0x38: 0x140001640 — 7 bytes, Slot7 thunk
//   +0x40: 0x140019160 — 12 bytes (whole function), Slot8 CreateInPlace

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_EnvironmentDescription = 0x14015e1f0;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookCleanup;

// ============================================================
// Reimplemented functions
// ============================================================

// Dtor: frees dynamic array at +0x28, zeroes +0x28/+0x30, calls External::~External
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "EnvironmentDescription::Dtor";
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x28));
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    fn_External_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: ExternalPtr at +0x20, dynamic weather array at +0x28, 12 weather entries at +0x38
// Ghidra: FUN_140094040
static uint64_t Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "EnvironmentDescription::Serialize";
    uintptr_t s = (uintptr_t)self;

    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // ExternalPtr at +0x20
    fn_ExternalPtr_BuildSerialize((void*)(s + 0x20), serializer);

    // Serialize count at +0x30
    uint32_t count = *(uint32_t*)(s + 0x30);
    g_fn_Serialize_Int(serializer, (void*)&count, NULL);

    // On read: resize array
    if (*(int*)(*(uintptr_t*)serializer + 8) == 0) {
        fn_WeatherResize((void*)(s + 0x28), (int)count);
    }

    // Loop through dynamic array entries (16 bytes each: 2x longlong)
    uintptr_t* arrPtr = *(uintptr_t**)(s + 0x28);
    for (uint32_t i = 0; i < count; i++) {
        fn_WeatherExternalSerialize(arrPtr, serializer);
        fn_ExternalPtr_BuildSerialize(arrPtr + 1, serializer);
        arrPtr += 2;
    }

    // Loop 12 weather entries at +0x38, each 0x20 bytes
    // Each: 3 Colors (uint32) + 5 floats — all 4-byte serialize
    uintptr_t entry = s + 0x38;
    for (int i = 0; i < 12; i++) {
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x00), NULL); // Color: ambient sky
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x04), NULL); // Color: ambient ground
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x08), NULL); // Color: ambient fog
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x0c), NULL); // float: fog start
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x10), NULL); // float: fog end
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x14), NULL); // float: sun intensity
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x18), NULL); // float: sun angle
        g_fn_Serialize_Int(serializer, (void*)(entry + 0x1c), NULL); // float: sun spread
        entry += 0x20;
    }

    g_fn_Serializer_Dtor(serializer);
    return 1;
}

// Cleanup: virtual dtor(no-free) + re-construct (same as CreateInPlace body minus null check)
// CreateInPlace: External::External, set vtable, zero +0x20/+0x28/+0x30, init 12 weather entries.
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "EnvironmentDescription::Cleanup";
    // Call virtual dtor with flags=0 (destroy fields, don't free memory)
    typedef void* (*fn_VDtor_t)(void*, uint32_t);
    ((fn_VDtor_t)(*(void***)self)[0])(self, 0);
    // Re-construct: External::External + set vtable + init fields
    fn_External_External(self);
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_EnvironmentDescription);
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    // Init 12 weather entries at +0x38, each 0x20 bytes (default values from CreateInPlace)
    uint32_t* w = (uint32_t*)(s + 0x38);
    for (int i = 0; i < 12; ++i) {
        w[0] = 0xff595965; // Color ambient sky
        w[1] = 0xffcbcbbc; // Color ambient ground
        w[2] = 0xffbfbff2; // Color ambient fog
        w[3] = 0x42b40000; // float fog start = 90.0f
        w[4] = 0x43610000; // float fog end = 225.0f
        w[5] = 0x3f733333; // float sun intensity = 0.95f
        w[6] = 0x42f00000; // float sun angle = 120.0f
        w[7] = 0x42820000; // float sun spread = 65.0f
        w += 8; // advance 0x20 bytes
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallEnvironmentDescriptionDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140019190);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionDtor() { RemoveHook(&g_hookDtor); }

void InstallEnvironmentDescriptionSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140094040);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionSerialize() { RemoveHook(&g_hookSerialize); }

void InstallEnvironmentDescriptionCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140019170);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[EnvironmentDescription] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveEnvironmentDescriptionCleanup() { RemoveHook(&g_hookCleanup); }
