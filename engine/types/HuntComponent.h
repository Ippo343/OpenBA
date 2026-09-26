#pragma once
// HuntComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: HuntComponent_Ctor
// Parent: ResourceProduceComponent (calls ResourceProduceComponent_Ctor)
//
// Constructor sets:
//   vtable = 0x140163770
//   +0x50 = 0 (qword)
//   flags |= 0x8

#include <cstdint>
#include <cstddef>

namespace HuntComponentAddr {
    constexpr uintptr_t Vtable = 0x140163770;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `HuntComponent` is a real class now
// (code/Game/Component/HuntComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/HuntComponent.h"
