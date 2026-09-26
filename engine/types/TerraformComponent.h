#pragma once
// TerraformComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: TerraformComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a880
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0   (8-byte zero stores, so the
//                                                 caps at +0x34 / +0x44 clear too)
//   flags |= 0x3
//
// The two arrays are the UNDO RECORD for the terrain deformation this component
// applied when its owner finished building (vtable slot 12, message id 4):
//   +0x28/+0x30/+0x34 — Array<Math::Vector3>: the ORIGINAL terrain sample points
//                       (x, y, originalHeight) for every heightmap vertex the
//                       CEILING pass raised. Grown in place by the slot-12 body
//                       (MemAlloc, cap*3 growth) and serialized by slot 3.
//   +0x38/+0x40/+0x44 — Array<int32>: the map cell indices whose terrain type the
//                       footprint pass flipped from 10 to 6.
// Slot 7 (OnRemove) replays both arrays in reverse to restore the terrain.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TerraformComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a880;
}

// ENDGAME B.3.8 VIS: the POD is gone — `TerraformComponent` is a real class now
// (code/Game/Component/TerraformComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/TerraformComponent.h"
