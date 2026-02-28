#pragma once
// StorageDisplayComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: StorageDisplayComponent_Ctor (0x140056a00)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x14015a780
//   +0x28 = 0 (qword)
//   +0x30 = 0 (qword)
//   flags |= 0xCB (Serialize | Draw | Create | Update | bit6 | bit7)
//
// The heavy flag mask 0xCB suggests this component participates in
// drawing, updating, serialization, and entity lifecycle events.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StorageDisplayComponentAddr {
    constexpr uintptr_t Ctor    = 0x140056a00;
    constexpr uintptr_t Vtable  = 0x14015a780;
}

struct StorageDisplayComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // StorageDisplayComponent own fields
    void*    displayData0; // +0x28: zeroed (ptr or data)
    void*    displayData1; // +0x30: zeroed (ptr or data)
};

static_assert(sizeof(StorageDisplayComponent) == 0x38, "StorageDisplayComponent size mismatch");
static_assert(offsetof(StorageDisplayComponent, vtable) == 0x00, "");
static_assert(offsetof(StorageDisplayComponent, flags) == 0x20, "");
static_assert(offsetof(StorageDisplayComponent, displayData0) == 0x28, "");
static_assert(offsetof(StorageDisplayComponent, displayData1) == 0x30, "");
