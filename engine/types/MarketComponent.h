#pragma once
// MarketComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Created via: MarketComponent_Factory (0x140008220)
// Parent: ResourceProduceComponent (0x50, which extends ControllerComponent pattern)
//
// MarketComponent has NO dedicated constructor — the factory does:
//   1. MemAlloc(0x50, 8)
//   2. ResourceProduceComponent_Ctor(ptr)
//   3. Set vtable = 0x140159c40
//
// This means MarketComponent adds NO fields beyond ResourceProduceComponent.
// Its size is exactly 0x50 (same as parent). It differs only in vtable.

#include <cstdint>
#include <cstddef>

namespace MarketComponentAddr {
    constexpr uintptr_t Factory = 0x140008220;
    constexpr uintptr_t Vtable  = 0x140159c40;
    constexpr uintptr_t RegisterType = 0x14014b0c0;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `MarketComponent` is a real class now
// (code/Game/Component/MarketComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/MarketComponent.h"
