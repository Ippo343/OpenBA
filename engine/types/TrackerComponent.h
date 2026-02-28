#pragma once
// TrackerComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Created via: TrackerComponent_Factory (0x14000d7a0)
// Parent: Component (direct, no named Ctor)
//
// Factory does: MemAlloc(0x50) + Component_Ctor + set vtable + init fields
//   vtable = 0x14015ab80
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   +0x48 = -1 (int32)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TrackerComponentAddr {
    constexpr uintptr_t Factory = 0x14000d7a0;
    constexpr uintptr_t Vtable  = 0x14015ab80;
}

struct TrackerComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
    void*    field38;      // +0x38: zeroed
    void*    field40;      // +0x40: zeroed
    int32_t  trackerId;    // +0x48: initialized to -1
    uint8_t  _pad4C[4];    // +0x4C: padding to 0x50
};

static_assert(sizeof(TrackerComponent) == 0x50, "TrackerComponent size mismatch");
static_assert(offsetof(TrackerComponent, vtable) == 0x00, "");
static_assert(offsetof(TrackerComponent, flags) == 0x20, "");
static_assert(offsetof(TrackerComponent, field28) == 0x28, "");
static_assert(offsetof(TrackerComponent, trackerId) == 0x48, "");
