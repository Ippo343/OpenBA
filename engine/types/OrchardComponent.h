#pragma once
// OrchardComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: OrchardComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x14015a180
//   controllerVtable = 0x14015a130
//   +0x48 = 0 (byte), +0x4c = -1 (int32), +0x50 = 0, +0x58 = 0, +0x60 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace OrchardComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015a180;
    constexpr uintptr_t ControllerVtable  = 0x14015a130;
}

struct OrchardComponent {
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

    uint8_t  orchardFlag;       // +0x48: zeroed (byte)
    uint8_t  _pad49[3];         // +0x49: padding
    int32_t  orchardId;         // +0x4C: initialized to -1
    void*    field50;           // +0x50: zeroed
    void*    field58;           // +0x58: zeroed
    void*    field60;           // +0x60: zeroed
};

static_assert(sizeof(OrchardComponent) == 0x68, "OrchardComponent size mismatch");
static_assert(offsetof(OrchardComponent, vtable) == 0x00, "");
static_assert(offsetof(OrchardComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(OrchardComponent, selfPtr) == 0x40, "");
static_assert(offsetof(OrchardComponent, orchardFlag) == 0x48, "");
static_assert(offsetof(OrchardComponent, orchardId) == 0x4C, "");
static_assert(offsetof(OrchardComponent, field50) == 0x50, "");
