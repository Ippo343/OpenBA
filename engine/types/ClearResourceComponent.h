#pragma once
// ClearResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: ClearResourceComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162840
//   controllerVtable = 0x1401627f0
//   NO flag modification
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ClearResourceComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162840;
    constexpr uintptr_t ControllerVtable  = 0x1401627f0;
}

struct ClearResourceComponent {
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

static_assert(sizeof(ClearResourceComponent) == 0x48, "ClearResourceComponent size mismatch");
static_assert(offsetof(ClearResourceComponent, vtable) == 0x00, "");
static_assert(offsetof(ClearResourceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ClearResourceComponent, selfPtr) == 0x40, "");
