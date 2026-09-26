#pragma once
// ConsumeProduceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: ConsumeProduceComponent_Ctor (0x140119050)
// Parent: ResourceProduceComponent (calls ResourceProduceComponent_Ctor first)
//
// Constructor sets:
//   vtable = 0x140162a80 (overwrites ResourceProduceComponent vtable)
//   +0x50 = 0xFFFFFFFF (int32, invalid resource ID / sentinel)
//   +0x58 = 0 (float* consumeRates, zeroed)
//   +0x60 = 0 (int32 consumeCount, zeroed — dword in asm despite qword ctor clear)
//   +0x68 = 0 (int32 consumeState)
//   flags |= 0x80 (via BTS bit 7)

#include <cstdint>
#include <cstddef>

namespace ConsumeProduceComponentAddr {
    constexpr uintptr_t Ctor    = 0x140119050;
    constexpr uintptr_t Vtable  = 0x140162a80;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `ConsumeProduceComponent` is a real class now
// (code/Game/Component/ConsumeProduceComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/ConsumeProduceComponent.h"
