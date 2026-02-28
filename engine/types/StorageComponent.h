#pragma once
// StorageComponent — binary-compatible struct layout. PERF HOTSPOT.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: StorageComponent_Ctor (0x14012c8d0)
// Parent: Component (direct, NOT ControllerComponent)
//
// Key method: StorageComponent_QueryCount (0x14012cf10, 44 xrefs)
//   O(n*m) per citizen per tick — iterates storage slots, for each slot iterates
//   items checking type via IsDerivedFrom. Major performance hotspot.
//
// Constructor sets:
//   vtable = 0x140164720
//   +0x28 = 0 (qword, ptr to storage slot array)
//   +0x30 = 0 (qword, storage slot count or secondary data)
//   flags |= 0x81 (Serialize | bit7)
//
// Storage data model (from QueryCount decompilation):
//   +0x28: ptr to array of storage slot entries (each 0x10 bytes: [ptr, count])
//   +0x30: int count of storage slots
//   Each slot entry has its own item list at +0x28/+0x30 (nested array of item ptrs)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StorageComponentAddr {
    constexpr uintptr_t Ctor        = 0x14012c8d0;
    constexpr uintptr_t Vtable      = 0x140164720;
    constexpr uintptr_t QueryCount  = 0x14012cf10;  // O(n*m) perf hotspot, 44 xrefs
}

struct StorageComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // StorageComponent own fields
    void*    storageSlots; // +0x28: ptr to array of storage slot descriptors
    int32_t  slotCount;    // +0x30: number of storage slots
    uint8_t  _pad34[4];    // +0x34: padding
    void*    field38;      // +0x38: implicit (struct pads to 0x40)
};

static_assert(sizeof(StorageComponent) == 0x40, "StorageComponent size mismatch");
static_assert(offsetof(StorageComponent, vtable) == 0x00, "");
static_assert(offsetof(StorageComponent, next) == 0x08, "");
static_assert(offsetof(StorageComponent, owner) == 0x10, "");
static_assert(offsetof(StorageComponent, flags) == 0x20, "");
static_assert(offsetof(StorageComponent, storageSlots) == 0x28, "");
static_assert(offsetof(StorageComponent, slotCount) == 0x30, "");
