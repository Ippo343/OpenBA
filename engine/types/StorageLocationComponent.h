#pragma once
// StorageLocationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: StorageLocationComponent_Ctor (0x14012d360)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140164880 (overwrites ControllerComponent base)
//   controllerVtable = 0x140164830
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>

namespace StorageLocationComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012d360;
    constexpr uintptr_t Vtable            = 0x140164880;
    constexpr uintptr_t ControllerVtable  = 0x140164830;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `StorageLocationComponent` is a real class now
// (code/Game/Component/StorageLocationComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/StorageLocationComponent.h"
