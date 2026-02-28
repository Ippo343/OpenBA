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
    void*    _field20;           // +0x20: zeroed (array data?)
    void*    _field28;           // +0x28: zeroed (array size?)
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
};

static_assert(sizeof(ProfessionList) == 0x50, "ProfessionList size mismatch");
static_assert(offsetof(ProfessionList, vtable) == 0x00, "");
