#pragma once
// HerbalistComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Created via: HerbalistComponent_Factory (0x140006b40)
// Parent: ResourceProduceComponent (exactly, no additional fields)
//
// Factory does: ResourceProduceComponent_Ctor + set vtable to 0x140159860
// Same minimal pattern as MarketComponent.

#include <cstdint>
#include <cstddef>

namespace HerbalistComponentAddr {
    constexpr uintptr_t Factory = 0x140006b40;
    constexpr uintptr_t Vtable  = 0x140159860;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `HerbalistComponent` is a real class now
// (code/Game/Component/HerbalistComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/HerbalistComponent.h"
