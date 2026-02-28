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
//   +0x58 = 0 (qword)
//   +0x60 = 0 (qword)
//   +0x68 = 0 (uint32)
//   flags |= 0x80 (via BTS bit 7)

#include <cstdint>
#include <cstddef>
#include "ResourceProduceComponent.h"

namespace ConsumeProduceComponentAddr {
    constexpr uintptr_t Ctor    = 0x140119050;
    constexpr uintptr_t Vtable  = 0x140162a80;
}

struct ConsumeProduceComponent {
    // ResourceProduceComponent base (0x50 bytes)
    void*    vtable;            // +0x00: 0x140162a80
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    void*    controllerVtable;  // +0x28
    void*    controllerData0;   // +0x30
    void*    controllerData1;   // +0x38
    void*    selfPtr;           // +0x40

    uint8_t  produceFlag;       // +0x48: from ResourceProduceComponent
    uint8_t  _pad49[7];         // +0x49: padding to 0x50

    // ConsumeProduceComponent own fields
    int32_t  consumeResourceId; // +0x50: initialized to -1 (0xFFFFFFFF), resource type sentinel
    uint8_t  _pad54[4];         // +0x54: padding
    void*    consumeData0;      // +0x58: zeroed
    void*    consumeData1;      // +0x60: zeroed
    uint32_t consumeState;      // +0x68: zeroed
    uint8_t  _pad6C[4];         // +0x6C: padding to 0x70
};

static_assert(sizeof(ConsumeProduceComponent) == 0x70, "ConsumeProduceComponent size mismatch");
static_assert(offsetof(ConsumeProduceComponent, vtable) == 0x00, "");
static_assert(offsetof(ConsumeProduceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ConsumeProduceComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ConsumeProduceComponent, produceFlag) == 0x48, "");
static_assert(offsetof(ConsumeProduceComponent, consumeResourceId) == 0x50, "");
static_assert(offsetof(ConsumeProduceComponent, consumeData0) == 0x58, "");
static_assert(offsetof(ConsumeProduceComponent, consumeData1) == 0x60, "");
static_assert(offsetof(ConsumeProduceComponent, consumeState) == 0x68, "");
