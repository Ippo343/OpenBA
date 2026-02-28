#pragma once
// NomadComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x78.
//
// Decompiled from: NomadComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x58
//
// UNUSUAL: Has own data fields +0x28..+0x50, then controller vtable at +0x58.
// Similar pattern to TradeComponent (controller sub-object not at +0x28).
//
// Constructor sets:
//   vtable = 0x14015a020 (overwrites 0x140162b80)
//   +0x28..+0x50 = 0 (zeroed, 6 fields)
//   controllerVtable at +0x58 = 0x140159fd0
//   selfPtr at +0x70 = this
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace NomadComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015a020;
    constexpr uintptr_t ControllerVtable  = 0x140159fd0;
}

struct NomadComponent {
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // Own data fields
    void*    field28;           // +0x28: zeroed
    void*    field30;           // +0x30: zeroed
    void*    field38;           // +0x38: zeroed
    void*    field40;           // +0x40: zeroed
    void*    field48;           // +0x48: zeroed
    uint32_t field50;           // +0x50: zeroed (dword)
    uint8_t  _pad54[4];         // +0x54: padding

    // Controller sub-object at +0x58
    void*    controllerVtable;  // +0x58
    void*    controllerData0;   // +0x60
    void*    controllerData1;   // +0x68
    void*    selfPtr;           // +0x70
};

static_assert(sizeof(NomadComponent) == 0x78, "NomadComponent size mismatch");
static_assert(offsetof(NomadComponent, vtable) == 0x00, "");
static_assert(offsetof(NomadComponent, flags) == 0x20, "");
static_assert(offsetof(NomadComponent, field28) == 0x28, "");
static_assert(offsetof(NomadComponent, controllerVtable) == 0x58, "");
static_assert(offsetof(NomadComponent, selfPtr) == 0x70, "");
