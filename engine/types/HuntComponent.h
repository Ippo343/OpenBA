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
#include "ResourceProduceComponent.h"

namespace HuntComponentAddr {
    constexpr uintptr_t Vtable = 0x140163770;
}

struct HuntComponent {
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

    void*    huntData;          // +0x50: zeroed
};

static_assert(sizeof(HuntComponent) == 0x58, "HuntComponent size mismatch");
static_assert(offsetof(HuntComponent, vtable) == 0x00, "");
static_assert(offsetof(HuntComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(HuntComponent, selfPtr) == 0x40, "");
static_assert(offsetof(HuntComponent, produceFlag) == 0x48, "");
static_assert(offsetof(HuntComponent, huntData) == 0x50, "");
