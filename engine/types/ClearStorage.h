#pragma once
// ClearStorage — Storage clearing work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: ClearStorage_Ctor (0x1400e74b0)
// Inherits: Object::Internal -> Work (0x48) -> ClearStorage
// Extends Work directly.

#include <cstdint>
#include <cstddef>

namespace ClearStorageAddr {
    constexpr uintptr_t Ctor    = 0x1400e74b0;
    constexpr uintptr_t Vtable  = 0x1401611a0;
}

struct ClearStorage {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401611a0
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- ClearStorage fields ---
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed (BST root for Serialize)
    uint32_t _field60;           // +0x60: zeroed
    uint8_t  _pad64[4];          // +0x64: padding
};

static_assert(sizeof(ClearStorage) == 0x68, "ClearStorage size mismatch");
static_assert(offsetof(ClearStorage, vtable) == 0x00, "");
