#pragma once
// CemeteryComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: CemeteryComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401590a0
//   controllerVtable = 0x140159050
//   +0x48 = 0, +0x50 = 0
//   flags |= 0x83

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace CemeteryComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401590a0;
    constexpr uintptr_t ControllerVtable  = 0x140159050;
}

struct CemeteryComponent {
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
};

static_assert(sizeof(CemeteryComponent) == 0x58, "CemeteryComponent size mismatch");
static_assert(offsetof(CemeteryComponent, vtable) == 0x00, "");
static_assert(offsetof(CemeteryComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(CemeteryComponent, selfPtr) == 0x40, "");
static_assert(offsetof(CemeteryComponent, field48) == 0x48, "");
