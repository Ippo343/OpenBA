#pragma once
// FishingComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Created via: FishingComponent_Factory (0x140034350)
// Parent: ResourceProduceComponent
//
// Factory does: ResourceProduceComponent_Ctor + set vtable + set +0x50 = 0

#include <cstdint>
#include <cstddef>

namespace FishingComponentAddr {
    constexpr uintptr_t Factory = 0x140034350;
    constexpr uintptr_t Vtable  = 0x140163230;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `FishingComponent` is a real class now
// (code/Game/Component/FishingComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/FishingComponent.h"
