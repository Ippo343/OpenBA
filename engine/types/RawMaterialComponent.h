#pragma once
// RawMaterialComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: RawMaterialComponent_Ctor (0x140129400)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x140164170
//   +0x30 = 0 (qword)
//   +0x38 = 0 (qword)
//   flags |= 0x89 (Serialize | bit3 | bit7)
//
// Note: +0x28 is NOT explicitly initialized by the ctor (left as whatever
// Component_Ctor set, which is just past the base). This likely gets
// initialized by Startup() or Serialize().

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace RawMaterialComponentAddr {
    constexpr uintptr_t Ctor    = 0x140129400;
    constexpr uintptr_t Vtable  = 0x140164170;
}

struct RawMaterialComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // RawMaterialComponent own fields
    void*    field28;      // +0x28: not initialized by ctor (set by Startup/Serialize)
    void*    field30;      // +0x30: zeroed
    void*    field38;      // +0x38: zeroed
};

static_assert(sizeof(RawMaterialComponent) == 0x40, "RawMaterialComponent size mismatch");
static_assert(offsetof(RawMaterialComponent, vtable) == 0x00, "");
static_assert(offsetof(RawMaterialComponent, flags) == 0x20, "");
static_assert(offsetof(RawMaterialComponent, field28) == 0x28, "");
static_assert(offsetof(RawMaterialComponent, field30) == 0x30, "");
static_assert(offsetof(RawMaterialComponent, field38) == 0x38, "");
