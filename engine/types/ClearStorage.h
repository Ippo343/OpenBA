#pragma once
// ClearStorage — Storage clearing work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: ClearStorage_Ctor (0x1400e74b0), ClearStorage_Dtor (0x1400e73b0)
// Inherits: Object::Internal -> Work (0x48) -> ClearStorage
// Extends Work directly.
//
// ClearStorage manages a BST of citizen InternalBasePtrs at +0x58 that tracks
// which citizens are currently assigned to clear items from this storage.
// Field +0x60 is the count of items remaining to clear.
// Fields +0x48 and +0x50 are auxiliary data pointers (freed during destruction).

#include <cstdint>
#include <cstddef>

namespace ClearStorageAddr {
    constexpr uintptr_t Ctor    = 0x1400e74b0;
    constexpr uintptr_t Vtable  = 0x1401611a0;
}

struct ClearStorage {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401611a0
    uint8_t  _workBase[0x40];   // +0x08: Work fields
    // --- ClearStorage fields ---
    void*    _field48;           // +0x48: auxiliary data ptr (freed in dtor)
    void*    _field50;           // +0x50: auxiliary data ptr (zeroed in dtor)
    void*    _field58;           // +0x58: BST root — tree of citizen InternalBasePtrs
    uint32_t _field60;           // +0x60: item count remaining to clear
    uint8_t  _pad64[4];         // +0x64: padding
};

static_assert(sizeof(ClearStorage) == 0x68, "ClearStorage size mismatch");
static_assert(offsetof(ClearStorage, vtable) == 0x00, "");
static_assert(offsetof(ClearStorage, _field48) == 0x48, "");
static_assert(offsetof(ClearStorage, _field50) == 0x50, "");
static_assert(offsetof(ClearStorage, _field58) == 0x58, "");
static_assert(offsetof(ClearStorage, _field60) == 0x60, "");
