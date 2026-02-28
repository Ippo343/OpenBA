#pragma once
// DestroyComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: DestroyComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140163120
//   controllerVtable = 0x1401630d0
//   +0x48 = 0, +0x50 = 0 (uint16), +0x54 = 0 (qword), +0x60 = 0 (qword)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace DestroyComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163120;
    constexpr uintptr_t ControllerVtable  = 0x1401630d0;
}

struct DestroyComponent {
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

    void*    field48;           // +0x48: zeroed
    uint16_t destroyFlags;      // +0x50: zeroed (uint16)
    uint8_t  _pad52[2];         // +0x52: padding
    uint32_t field54;           // +0x54: zeroed (part of qword write)
    void*    field58;           // +0x58: zeroed (part of +0x54 qword)
    void*    field60;           // +0x60: zeroed
};

static_assert(sizeof(DestroyComponent) == 0x68, "DestroyComponent size mismatch");
static_assert(offsetof(DestroyComponent, vtable) == 0x00, "");
static_assert(offsetof(DestroyComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(DestroyComponent, selfPtr) == 0x40, "");
static_assert(offsetof(DestroyComponent, field48) == 0x48, "");
static_assert(offsetof(DestroyComponent, destroyFlags) == 0x50, "");
static_assert(offsetof(DestroyComponent, field60) == 0x60, "");
