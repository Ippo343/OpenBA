#pragma once
// ForesterComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Created via: ForesterComponent_Factory (0x140034870)
// Parent: ResourceProduceComponent
//
// Factory does: ResourceProduceComponent_Ctor + set vtable + set +0x50 = 0x101
// +0x50: uint16 = 0x101 (two packed bytes: 0x01, 0x01)

#include <cstdint>
#include <cstddef>

namespace ForesterComponentAddr {
    constexpr uintptr_t Factory = 0x140034870;
    constexpr uintptr_t Vtable  = 0x140163460;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `ForesterComponent` is a real class now
// (code/Game/Component/ForesterComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/ForesterComponent.h"
