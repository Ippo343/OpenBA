#pragma once
// GrowthComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: GrowthComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163590
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   +0x48 = 0x3f800000 (float 1.0), +0x4C = 0x3f800000 (float 1.0)
//   flags |= 0xCB

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace GrowthComponentAddr {
    constexpr uintptr_t Vtable = 0x140163590;
}

// ENDGAME B.3.8 CIT: the POD is gone — `GrowthComponent` is a real class now
// (code/Game/Component/GrowthComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/GrowthComponent.h"

