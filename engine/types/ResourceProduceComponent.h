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

namespace ResourceProduceComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012b3d0;
    constexpr uintptr_t Vtable            = 0x1401643f0;
    constexpr uintptr_t ControllerVtable  = 0x1401643a0;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `ResourceProduceComponent` is a real class now
// (code/Game/Component/ResourceProduceComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/ResourceProduceComponent.h"
