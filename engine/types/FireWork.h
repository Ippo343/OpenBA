#pragma once
// FireWork — Fire suppression work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: FireWork_Ctor (0x1400e9b50), FireWork_Factory (0x14002a1e0)
// Inherits: Object::Internal -> Work (0x48) -> FireWork
// NOTE: FireWork extends Work directly, NOT AutoPickupWork.

#include <cstdint>
#include <cstddef>

namespace FireWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400e9b50;
    constexpr uintptr_t Factory = 0x14002a1e0;
    constexpr uintptr_t Vtable  = 0x1401612a0;
}

struct FireWork {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401612a0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    _field10;           // +0x10: zeroed
    void*    _field18;           // +0x18: zeroed
    int32_t  workplaceId;        // +0x20: -1 sentinel
    uint32_t _field24;           // +0x24: zeroed
    int32_t  _field28;           // +0x28: -1 sentinel
    uint32_t _field2C;           // +0x2C: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    uint8_t  _pad44[4];          // +0x44: padding

    // --- FireWork fields ---
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    uint32_t _field58;           // +0x58: zeroed
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(FireWork) == 0x60, "FireWork size mismatch");
static_assert(offsetof(FireWork, vtable) == 0x00, "");
