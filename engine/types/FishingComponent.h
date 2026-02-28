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
#include "ResourceProduceComponent.h"

namespace FishingComponentAddr {
    constexpr uintptr_t Factory = 0x140034350;
    constexpr uintptr_t Vtable  = 0x140163230;
}

struct FishingComponent {
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

    uint8_t  fishingFlag;       // +0x50: initialized to 0
    uint8_t  _pad51[7];         // +0x51: padding to 0x58
};

static_assert(sizeof(FishingComponent) == 0x58, "FishingComponent size mismatch");
static_assert(offsetof(FishingComponent, vtable) == 0x00, "");
static_assert(offsetof(FishingComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(FishingComponent, selfPtr) == 0x40, "");
static_assert(offsetof(FishingComponent, fishingFlag) == 0x50, "");
