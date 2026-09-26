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

// ENDGAME B.3.8 VIS: the POD is gone — `StorageComponent` is a real class now
// (code/Game/Component/StorageComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/StorageComponent.h"
