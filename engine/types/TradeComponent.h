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

struct TradeComponent {
    // Component base (0x28 bytes)
    void*    vtable;            // +0x00: 0x14015acd0
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // TradeComponent own data fields (+0x28..+0x87)
    uint32_t tradeState;        // +0x28: initialized to 0
    int32_t  tradeId;           // +0x2c: initialized to -1 (sentinel)
    void*    field30;           // +0x30: zeroed
    void*    field38;           // +0x38: zeroed
    void*    field40;           // +0x40: zeroed
    void*    field48;           // +0x48: zeroed
    void*    field50;           // +0x50: zeroed
    void*    field58;           // +0x58: zeroed

    // Sub-object at +0x60 (12 bytes used)
    void*    field60;           // +0x60: zeroed (qword)
    uint32_t field68;           // +0x68: zeroed (dword)
    uint8_t  _pad6C[5];         // +0x6C: padding (5 bytes to +0x71)
    uint8_t  tradeFlag;         // +0x71: byte, initialized to 0
    uint8_t  _pad72[6];         // +0x72: padding to +0x78
    void*    field78;           // +0x78: zeroed
    void*    field80;           // +0x80: zeroed

    // Controller sub-object at +0x88 (same pattern as ControllerComponent but offset)
    void*    controllerVtable;  // +0x88: secondary controller vtable (0x14015ac80)
    void*    controllerData0;   // +0x90: zeroed
    void*    controllerData1;   // +0x98: zeroed
    void*    selfPtr;           // +0xa0: back-pointer to this component
};

static_assert(sizeof(TradeComponent) == 0xa8, "TradeComponent size mismatch");
static_assert(offsetof(TradeComponent, vtable) == 0x00, "");
static_assert(offsetof(TradeComponent, flags) == 0x20, "");
static_assert(offsetof(TradeComponent, tradeState) == 0x28, "");
static_assert(offsetof(TradeComponent, tradeId) == 0x2c, "");
static_assert(offsetof(TradeComponent, field30) == 0x30, "");
static_assert(offsetof(TradeComponent, field38) == 0x38, "");
static_assert(offsetof(TradeComponent, field40) == 0x40, "");
static_assert(offsetof(TradeComponent, field48) == 0x48, "");
static_assert(offsetof(TradeComponent, field50) == 0x50, "");
static_assert(offsetof(TradeComponent, field58) == 0x58, "");
static_assert(offsetof(TradeComponent, field60) == 0x60, "");
static_assert(offsetof(TradeComponent, field68) == 0x68, "");
static_assert(offsetof(TradeComponent, tradeFlag) == 0x71, "");
static_assert(offsetof(TradeComponent, field78) == 0x78, "");
static_assert(offsetof(TradeComponent, field80) == 0x80, "");
static_assert(offsetof(TradeComponent, controllerVtable) == 0x88, "");
static_assert(offsetof(TradeComponent, controllerData0) == 0x90, "");
static_assert(offsetof(TradeComponent, controllerData1) == 0x98, "");
static_assert(offsetof(TradeComponent, selfPtr) == 0xa0, "");
