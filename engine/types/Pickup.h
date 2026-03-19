#pragma once
// Pickup — Item pickup work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: Pickup_Ctor (0x1400f3c00), Pickup vtable (0x140161810)
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Extends Work directly (not AutoPickupWork).
//
// Fields:
//   +0x48 (uint32): status — 0=unassigned, 1=assigned, 2=completing
//   +0x50 (void*): citizenIBP — InternalBasePtr to assigned citizen entity
//   +0x58 (void*): candidateList — array/list used during pickup search (serialized v>=1)

#include <cstdint>
#include <cstddef>

namespace PickupAddr {
    constexpr uintptr_t Ctor    = 0x1400f3c00;
    constexpr uintptr_t Vtable  = 0x140161810;
}

struct Pickup {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161810
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- Pickup fields ---
    uint32_t status;             // +0x48: 0=unassigned, 1=assigned, 2=completing
    uint8_t  _pad4C[4];          // +0x4C: padding
    void*    citizenIBP;         // +0x50: InternalBasePtr to citizen entity
    void*    candidateList;      // +0x58: array/list for pickup candidates
};

static_assert(sizeof(Pickup) == 0x60, "Pickup size mismatch");
static_assert(offsetof(Pickup, vtable) == 0x00, "");
static_assert(offsetof(Pickup, status) == 0x48, "");
static_assert(offsetof(Pickup, citizenIBP) == 0x50, "");
static_assert(offsetof(Pickup, candidateList) == 0x58, "");
