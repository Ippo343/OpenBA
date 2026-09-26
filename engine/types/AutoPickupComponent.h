#pragma once
// AutoPickupComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: AutoPickupComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140158df0
//   +0x28 = 1 (byte, likely "enabled" flag)
//   +0x30 = 0, +0x38 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AutoPickupComponentAddr {
    constexpr uintptr_t Vtable = 0x140158df0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `AutoPickupComponent` is a real class now
// (code/Game/Component/AutoPickupComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/AutoPickupComponent.h"
