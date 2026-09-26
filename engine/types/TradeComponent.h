#pragma once
// TradeComponent — binary-compatible struct layout. Largest ControllerComponent.
// Application-x64.exe, build 170910. Size = 0xa8.
//
// Decompiled from: TradeComponent_Ctor (0x14005c430)
// Parent: Component (direct call to Component_Ctor, NOT ControllerComponent at +0x28)
//
// TradeComponent is unusual: it has a ControllerComponent-style secondary vtable
// sub-object, but at offset +0x88 instead of +0x28. The intervening 0x60 bytes
// (+0x28..+0x87) are TradeComponent's own data fields.
//
// Constructor sets:
//   vtable = 0x14015acd0 (overwritten twice, first 0x140162b80 then final)
//   +0x28: uint32 = 0
//   +0x2c: int32 = -1 (0xFFFFFFFF) — sentinel / invalid trade ID
//   +0x30..+0x58: zeroed (6 qwords of trade state data)
//   +0x60..+0x68: sub-object (qword + dword, zeroed)
//   +0x71: byte = 0 (bool flag)
//   +0x78..+0x80: zeroed (2 qwords)
//   +0x88: controller secondary vtable (purecall -> 0x14015ac80)
//   +0x90: zeroed
//   +0x98: zeroed
//   +0xa0: back-pointer to self
//   flags |= 0x3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TradeComponentAddr {
    constexpr uintptr_t Ctor              = 0x14005c430;
    constexpr uintptr_t Vtable            = 0x14015acd0;
    constexpr uintptr_t ControllerVtable  = 0x14015ac80;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `TradeComponent` is a real class now
// (code/Game/Component/TradeComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/TradeComponent.h"
