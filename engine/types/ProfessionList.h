#pragma once
// ProfessionList — Container for all professions, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: ProfessionList_Factory (0x14001e5c0) — inline ctor
// Base: Object::External (0x20 bytes)
// Contains array/list of Profession objects.

#include <cstdint>
#include <cstddef>

namespace ProfessionListAddr {
    constexpr uintptr_t Factory      = 0x14001e5c0;
    constexpr uintptr_t Vtable       = 0x14015ef50;
    constexpr uintptr_t RegisterType = 0x14014ccd0;
    constexpr uintptr_t Cleanup      = 0x14001e630;
}

struct ProfessionList {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ef50
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- ProfessionList fields ---
    void*    arrayData;          // +0x20: dynamic array data pointer (freed in Dtor)
    uint64_t arraySize;          // +0x28: array capacity/size (zeroed in Dtor)
    void*    profRef1;           // +0x30: ExternalPtr<Profession> (serialized via FUN_140018020)
    void*    profRef2;           // +0x38: ExternalPtr<Profession> (serialized via FUN_140018020)
    void*    profRef3;           // +0x40: ExternalPtr<Profession> (serialized via FUN_140018020)
    void*    profRef4;           // +0x48: ExternalPtr<Profession> (serialized via FUN_140018020)
};

static_assert(sizeof(ProfessionList) == 0x50, "ProfessionList size mismatch");
static_assert(offsetof(ProfessionList, vtable) == 0x00, "");
static_assert(offsetof(ProfessionList, arrayData) == 0x20, "");
static_assert(offsetof(ProfessionList, arraySize) == 0x28, "");
static_assert(offsetof(ProfessionList, profRef1) == 0x30, "");
static_assert(offsetof(ProfessionList, profRef2) == 0x38, "");
static_assert(offsetof(ProfessionList, profRef3) == 0x40, "");
static_assert(offsetof(ProfessionList, profRef4) == 0x48, "");
