#pragma once
// BuildComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x78.
//
// Decompiled from: BuildComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162260
//   controllerVtable = 0x140162210
//   +0x48 = 0 (dword), +0x4c = 1 (dword), +0x50 = 0 (byte)
//   +0x58 = 0, +0x60 = 0, +0x68 = 0, +0x70 = 0
//   flags |= 0x83

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace BuildComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162260;
    constexpr uintptr_t ControllerVtable  = 0x140162210;
}

struct BuildComponent {
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

    uint32_t buildState;        // +0x48: zeroed
    uint32_t buildProgress;     // +0x4C: initialized to 1
    uint8_t  buildFlag;         // +0x50: zeroed (byte)
    uint8_t  _pad51[7];         // +0x51: padding
    void*    field58;           // +0x58: zeroed
    void*    field60;           // +0x60: zeroed
    void*    field68;           // +0x68: zeroed
    void*    field70;           // +0x70: zeroed
};

static_assert(sizeof(BuildComponent) == 0x78, "BuildComponent size mismatch");
static_assert(offsetof(BuildComponent, vtable) == 0x00, "");
static_assert(offsetof(BuildComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(BuildComponent, selfPtr) == 0x40, "");
static_assert(offsetof(BuildComponent, buildState) == 0x48, "");
static_assert(offsetof(BuildComponent, buildProgress) == 0x4C, "");
static_assert(offsetof(BuildComponent, buildFlag) == 0x50, "");
static_assert(offsetof(BuildComponent, field58) == 0x58, "");
static_assert(offsetof(BuildComponent, field70) == 0x70, "");
