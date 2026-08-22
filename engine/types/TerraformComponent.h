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

struct TerraformComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10  entity InternalBasePtr
    void*    description;  // +0x18  TerraformDescription*
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    heightRecord; // +0x28: Vector3[] of original terrain points
    int32_t  heightCount;  // +0x30
    int32_t  heightCap;    // +0x34
    void*    tileRecord;   // +0x38: int32[] of retyped map cell indices
    int32_t  tileCount;    // +0x40
    int32_t  tileCap;      // +0x44
};

static_assert(sizeof(TerraformComponent) == 0x48, "TerraformComponent size mismatch");
static_assert(offsetof(TerraformComponent, vtable) == 0x00, "");
static_assert(offsetof(TerraformComponent, flags) == 0x20, "");
static_assert(offsetof(TerraformComponent, heightRecord) == 0x28, "");
static_assert(offsetof(TerraformComponent, heightCount) == 0x30, "");
static_assert(offsetof(TerraformComponent, heightCap) == 0x34, "");
static_assert(offsetof(TerraformComponent, tileRecord) == 0x38, "");
static_assert(offsetof(TerraformComponent, tileCount) == 0x40, "");
static_assert(offsetof(TerraformComponent, tileCap) == 0x44, "");
