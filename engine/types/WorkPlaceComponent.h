#pragma once
// WorkPlaceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: WorkPlaceComponent_Ctor (0x14012f2e0)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140164d30 (overwrites ControllerComponent base 0x140162b80)
//   controllerVtable = 0x140164ce0 (overwrites purecall placeholder)
//   +0x48 = 0 (qword, ptr/data)
//   +0x50 = 1 (byte, flag/bool — likely "is accepting workers")
//   +0x58 = 0 (qword)
//   +0x60 = 0 (qword)
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace WorkPlaceComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012f2e0;
    constexpr uintptr_t Vtable            = 0x140164d30;
    constexpr uintptr_t ControllerVtable  = 0x140164ce0;
}

struct WorkPlaceComponent {
    // ControllerComponent base (0x48 bytes)
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

    // WorkPlaceComponent own fields
    void*    workerData;        // +0x48: zeroed (ptr or qword data)
    uint8_t  isAccepting;       // +0x50: initialized to 1 (bool flag)
    uint8_t  _pad51[7];         // +0x51: padding
    void*    field58;           // +0x58: zeroed
    void*    field60;           // +0x60: zeroed
};

static_assert(sizeof(WorkPlaceComponent) == 0x68, "WorkPlaceComponent size mismatch");
static_assert(offsetof(WorkPlaceComponent, vtable) == 0x00, "");
static_assert(offsetof(WorkPlaceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(WorkPlaceComponent, selfPtr) == 0x40, "");
static_assert(offsetof(WorkPlaceComponent, workerData) == 0x48, "");
static_assert(offsetof(WorkPlaceComponent, isAccepting) == 0x50, "");
static_assert(offsetof(WorkPlaceComponent, field58) == 0x58, "");
static_assert(offsetof(WorkPlaceComponent, field60) == 0x60, "");
