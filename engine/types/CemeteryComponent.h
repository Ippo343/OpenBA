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
//   +0x48 = 0 (plot array), +0x50 = 0 (count), +0x54 = 0 (capacity)
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

    // Grave-plot cell list. A plain DynArray<int>: the plot-grid builder
    // (slot 6, 0x140047200) fills it from the footprint, and both the state-2
    // "built" enter (0x140046fc0) and Cemetery_PickFreeSlot_6c60 walk it.
    int32_t* plots;             // +0x48: cell indices
    int32_t  plotCount;         // +0x50
    int32_t  plotCapacity;      // +0x54
};

static_assert(sizeof(CemeteryComponent) == 0x58, "CemeteryComponent size mismatch");
static_assert(offsetof(CemeteryComponent, vtable) == 0x00, "");
static_assert(offsetof(CemeteryComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(CemeteryComponent, selfPtr) == 0x40, "");
static_assert(offsetof(CemeteryComponent, plots) == 0x48, "");
static_assert(offsetof(CemeteryComponent, plotCount) == 0x50, "");
static_assert(offsetof(CemeteryComponent, plotCapacity) == 0x54, "");
