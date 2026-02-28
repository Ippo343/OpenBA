#pragma once
// PastureComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: PastureComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x48
//
// UNUSUAL: Has own data at +0x28..+0x44, controller vtable at +0x48, selfPtr at +0x60.
// Constructor sets:
//   vtable = 0x140163f60 (overwrites 0x140162b80)
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x44 = 0 (dword)
//   controllerVtable at +0x48 = 0x140163f10
//   selfPtr at +0x60 = this
//   flags |= 0x8B

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PastureComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163f60;
    constexpr uintptr_t ControllerVtable  = 0x140163f10;
}

struct PastureComponent {
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // Own data
    void*    field28;           // +0x28: zeroed
    void*    field30;           // +0x30: zeroed
    void*    field38;           // +0x38: zeroed
    uint8_t  _pad40[4];         // +0x40: implicit padding
    uint32_t field44;           // +0x44: zeroed (dword)

    // Controller sub-object at +0x48
    void*    controllerVtable;  // +0x48
    void*    controllerData0;   // +0x50
    void*    controllerData1;   // +0x58
    void*    selfPtr;           // +0x60
};

static_assert(sizeof(PastureComponent) == 0x68, "PastureComponent size mismatch");
static_assert(offsetof(PastureComponent, vtable) == 0x00, "");
static_assert(offsetof(PastureComponent, flags) == 0x20, "");
static_assert(offsetof(PastureComponent, field28) == 0x28, "");
static_assert(offsetof(PastureComponent, field44) == 0x44, "");
static_assert(offsetof(PastureComponent, controllerVtable) == 0x48, "");
static_assert(offsetof(PastureComponent, selfPtr) == 0x60, "");
