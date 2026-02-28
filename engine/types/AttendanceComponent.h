#pragma once
// AttendanceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: AttendanceComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x38
//
// UNUSUAL: Has own data at +0x28/+0x30, then controller vtable at +0x38.
// Constructor sets:
//   vtable = 0x140158ce0 (overwrites 0x140162b80)
//   +0x28 = 0, +0x30 = 0
//   controllerVtable at +0x38 = 0x140158c90
//   selfPtr at +0x50 = this
//   flags |= 0xA (10 decimal)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AttendanceComponentAddr {
    constexpr uintptr_t Vtable            = 0x140158ce0;
    constexpr uintptr_t ControllerVtable  = 0x140158c90;
}

struct AttendanceComponent {
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // Own data
    void*    field28;           // +0x28: zeroed
    void*    field30;           // +0x30: zeroed

    // Controller sub-object at +0x38
    void*    controllerVtable;  // +0x38
    void*    controllerData0;   // +0x40
    void*    controllerData1;   // +0x48
    void*    selfPtr;           // +0x50
};

static_assert(sizeof(AttendanceComponent) == 0x58, "AttendanceComponent size mismatch");
static_assert(offsetof(AttendanceComponent, vtable) == 0x00, "");
static_assert(offsetof(AttendanceComponent, flags) == 0x20, "");
static_assert(offsetof(AttendanceComponent, field28) == 0x28, "");
static_assert(offsetof(AttendanceComponent, controllerVtable) == 0x38, "");
static_assert(offsetof(AttendanceComponent, selfPtr) == 0x50, "");
