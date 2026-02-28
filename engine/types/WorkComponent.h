#pragma once
// WorkComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: WorkComponent_Ctor (0x14012e820)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x140164be0
//   +0x28 = 0xFFFFFFFF (int32, invalid ID / sentinel)
//   +0x2c = 0x80000000 (uint32, flag or sentinel)
//   +0x30 = 0 (int32)
//   flags |= 0x3 (Serialize | Draw?)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace WorkComponentAddr {
    constexpr uintptr_t Ctor    = 0x14012e820;
    constexpr uintptr_t Vtable  = 0x140164be0;
}

struct WorkComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // WorkComponent own fields
    int32_t  workId;       // +0x28: initialized to -1 (0xFFFFFFFF)
    uint32_t workFlags;    // +0x2c: initialized to 0x80000000
    int32_t  workState;    // +0x30: initialized to 0
    uint8_t  _pad34[4];    // +0x34: padding to 0x38
};

static_assert(sizeof(WorkComponent) == 0x38, "WorkComponent size mismatch");
static_assert(offsetof(WorkComponent, vtable) == 0x00, "");
static_assert(offsetof(WorkComponent, next) == 0x08, "");
static_assert(offsetof(WorkComponent, owner) == 0x10, "");
static_assert(offsetof(WorkComponent, flags) == 0x20, "");
static_assert(offsetof(WorkComponent, workId) == 0x28, "");
static_assert(offsetof(WorkComponent, workFlags) == 0x2c, "");
static_assert(offsetof(WorkComponent, workState) == 0x30, "");
