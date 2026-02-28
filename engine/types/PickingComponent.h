#pragma once
// PickingComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: PickingComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140164040
//   +0x28 = -1 (int32, sentinel/invalid picking ID)
//   +0x30 = 0 (qword)
//   flags |= 0xC3 (Draw | Update | Create | Serialize | bit6)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PickingComponentAddr {
    constexpr uintptr_t Vtable = 0x140164040;
}

struct PickingComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    int32_t  pickingId;    // +0x28: initialized to -1
    uint8_t  _pad2C[4];    // +0x2C: padding
    void*    field30;      // +0x30: zeroed
};

static_assert(sizeof(PickingComponent) == 0x38, "PickingComponent size mismatch");
static_assert(offsetof(PickingComponent, vtable) == 0x00, "");
static_assert(offsetof(PickingComponent, flags) == 0x20, "");
static_assert(offsetof(PickingComponent, pickingId) == 0x28, "");
static_assert(offsetof(PickingComponent, field30) == 0x30, "");
