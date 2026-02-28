#pragma once
// ResourceProduceComponent — ABSTRACT base for resource-producing components.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: ResourceProduceComponent_Ctor (0x14012b3d0)
// Parent: ControllerComponent (pattern)
// Children: MarketComponent, ConsumeProduceComponent
//
// Layout:
//   +0x00..+0x47: ControllerComponent base (0x48 bytes)
//   +0x48: bool/byte field (zeroed)
//   +0x49..+0x4F: padding to 0x50
//
// Constructor sets:
//   vtable = 0x1401643f0 (ResourceProduceComponent vtable)
//   controllerVtable = 0x1401643a0 (ResourceProduceComponent controller vtable)
//   +0x48 = 0 (byte)

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ResourceProduceComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012b3d0;
    constexpr uintptr_t Vtable            = 0x1401643f0;
    constexpr uintptr_t ControllerVtable  = 0x1401643a0;
}

struct ResourceProduceComponent {
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

    // ResourceProduceComponent own fields
    uint8_t  produceFlag;       // +0x48: bool/flag, zeroed by ctor
    uint8_t  _pad49[7];         // +0x49: padding to 0x50
};

static_assert(sizeof(ResourceProduceComponent) == 0x50, "ResourceProduceComponent size mismatch");
static_assert(offsetof(ResourceProduceComponent, vtable) == 0x00, "");
static_assert(offsetof(ResourceProduceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ResourceProduceComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ResourceProduceComponent, produceFlag) == 0x48, "");
