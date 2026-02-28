#pragma once
// ImmigrationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: ImmigrationComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159b30
//   controllerVtable = 0x140159ae0
//   +0x48 = 0, +0x50 = 0 (dword), +0x58 = 0, +0x60 = 0
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ImmigrationComponentAddr {
    constexpr uintptr_t Vtable            = 0x140159b30;
    constexpr uintptr_t ControllerVtable  = 0x140159ae0;
}

struct ImmigrationComponent {
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
    uint32_t field50;           // +0x50: zeroed (dword)
    uint8_t  _pad54[4];         // +0x54: padding
    void*    field58;           // +0x58: zeroed
    void*    field60;           // +0x60: zeroed
};

static_assert(sizeof(ImmigrationComponent) == 0x68, "ImmigrationComponent size mismatch");
static_assert(offsetof(ImmigrationComponent, vtable) == 0x00, "");
static_assert(offsetof(ImmigrationComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ImmigrationComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ImmigrationComponent, field48) == 0x48, "");
static_assert(offsetof(ImmigrationComponent, field60) == 0x60, "");
