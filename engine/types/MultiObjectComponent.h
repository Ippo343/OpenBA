#pragma once
// MultiObjectComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: MultiObjectComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140163cf0
//   controllerVtable = 0x140163ca0
//   +0x48 = 0, +0x50 = 0, +0x58 = 1 (dword)
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace MultiObjectComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163cf0;
    constexpr uintptr_t ControllerVtable  = 0x140163ca0;
}

struct MultiObjectComponent {
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
    void*    field50;           // +0x50: zeroed
    uint32_t multiCount;        // +0x58: initialized to 1
    uint8_t  _pad5C[4];         // +0x5C: padding to 0x60
};

static_assert(sizeof(MultiObjectComponent) == 0x60, "MultiObjectComponent size mismatch");
static_assert(offsetof(MultiObjectComponent, vtable) == 0x00, "");
static_assert(offsetof(MultiObjectComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(MultiObjectComponent, selfPtr) == 0x40, "");
static_assert(offsetof(MultiObjectComponent, field48) == 0x48, "");
static_assert(offsetof(MultiObjectComponent, multiCount) == 0x58, "");
