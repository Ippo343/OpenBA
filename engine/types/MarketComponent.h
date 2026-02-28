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
#include "ResourceProduceComponent.h"

namespace MarketComponentAddr {
    constexpr uintptr_t Factory = 0x140008220;
    constexpr uintptr_t Vtable  = 0x140159c40;
    constexpr uintptr_t RegisterType = 0x14014b0c0;
}

struct MarketComponent {
    // ResourceProduceComponent base (0x50 bytes) — no additional fields
    void*    vtable;            // +0x00: 0x140159c40
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    void*    controllerVtable;  // +0x28
    void*    controllerData0;   // +0x30
    void*    controllerData1;   // +0x38
    void*    selfPtr;           // +0x40

    uint8_t  produceFlag;       // +0x48
    uint8_t  _pad49[7];         // +0x49
};

static_assert(sizeof(MarketComponent) == 0x50, "MarketComponent size mismatch");
static_assert(offsetof(MarketComponent, vtable) == 0x00, "");
static_assert(offsetof(MarketComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(MarketComponent, selfPtr) == 0x40, "");
static_assert(offsetof(MarketComponent, produceFlag) == 0x48, "");
