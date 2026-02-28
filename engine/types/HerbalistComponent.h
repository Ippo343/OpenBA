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
#include "ResourceProduceComponent.h"

namespace HerbalistComponentAddr {
    constexpr uintptr_t Factory = 0x140006b40;
    constexpr uintptr_t Vtable  = 0x140159860;
}

struct HerbalistComponent {
    void*    vtable;            // +0x00
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

static_assert(sizeof(HerbalistComponent) == 0x50, "HerbalistComponent size mismatch");
static_assert(offsetof(HerbalistComponent, vtable) == 0x00, "");
static_assert(offsetof(HerbalistComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(HerbalistComponent, selfPtr) == 0x40, "");
static_assert(offsetof(HerbalistComponent, produceFlag) == 0x48, "");
