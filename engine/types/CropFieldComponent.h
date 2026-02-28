#pragma once
// CropFieldComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: CropFieldComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162ec0
//   controllerVtable = 0x140162e70
//   +0x48 = 0x100 (uint16, likely two packed bytes: 0x00, 0x01)
//   +0x50 = 0, +0x58 = 0, +0x60 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace CropFieldComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162ec0;
    constexpr uintptr_t ControllerVtable  = 0x140162e70;
}

struct CropFieldComponent {
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

    uint16_t cropFlags;         // +0x48: initialized to 0x100
    uint8_t  _pad4A[6];         // +0x4A: padding
    void*    field50;           // +0x50: zeroed
    void*    field58;           // +0x58: zeroed
    void*    field60;           // +0x60: zeroed
};

static_assert(sizeof(CropFieldComponent) == 0x68, "CropFieldComponent size mismatch");
static_assert(offsetof(CropFieldComponent, vtable) == 0x00, "");
static_assert(offsetof(CropFieldComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(CropFieldComponent, selfPtr) == 0x40, "");
static_assert(offsetof(CropFieldComponent, cropFlags) == 0x48, "");
static_assert(offsetof(CropFieldComponent, field50) == 0x50, "");
static_assert(offsetof(CropFieldComponent, field60) == 0x60, "");
