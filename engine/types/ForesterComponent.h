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
#include "ResourceProduceComponent.h"

namespace ForesterComponentAddr {
    constexpr uintptr_t Factory = 0x140034870;
    constexpr uintptr_t Vtable  = 0x140163460;
}

struct ForesterComponent {
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

    uint16_t foresterFlags;     // +0x50: initialized to 0x101
    uint8_t  _pad52[6];         // +0x52: padding to 0x58
};

static_assert(sizeof(ForesterComponent) == 0x58, "ForesterComponent size mismatch");
static_assert(offsetof(ForesterComponent, vtable) == 0x00, "");
static_assert(offsetof(ForesterComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ForesterComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ForesterComponent, produceFlag) == 0x48, "");
static_assert(offsetof(ForesterComponent, foresterFlags) == 0x50, "");
