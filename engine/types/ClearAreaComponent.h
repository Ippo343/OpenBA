#pragma once
// ClearAreaComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: ClearAreaComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401626d0
//   controllerVtable = 0x140162680
//   +0x48 = 0 (dword)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ClearAreaComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401626d0;
    constexpr uintptr_t ControllerVtable  = 0x140162680;
}

struct ClearAreaComponent {
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

    uint32_t clearState;        // +0x48: zeroed (dword)
    uint8_t  _pad4C[4];         // +0x4C: padding to 0x50
};

static_assert(sizeof(ClearAreaComponent) == 0x50, "ClearAreaComponent size mismatch");
static_assert(offsetof(ClearAreaComponent, vtable) == 0x00, "");
static_assert(offsetof(ClearAreaComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ClearAreaComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ClearAreaComponent, clearState) == 0x48, "");
