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
#include "ControllerComponent.h"

namespace StorageLocationComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012d360;
    constexpr uintptr_t Vtable            = 0x140164880;
    constexpr uintptr_t ControllerVtable  = 0x140164830;
}

struct StorageLocationComponent {
    // ControllerComponent base (0x48 bytes) — no additional fields
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    void*    controllerVtable;  // +0x28
    void*    controllerData0;   // +0x30
    void*    controllerData1;   // +0x38
    void*    selfPtr;           // +0x40
};

static_assert(sizeof(StorageLocationComponent) == 0x48, "StorageLocationComponent size mismatch");
static_assert(offsetof(StorageLocationComponent, vtable) == 0x00, "");
static_assert(offsetof(StorageLocationComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(StorageLocationComponent, selfPtr) == 0x40, "");
