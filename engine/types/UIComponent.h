#pragma once
// UIComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: UIComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x1401649f0
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace UIComponentAddr {
    constexpr uintptr_t Vtable = 0x1401649f0;
}

struct UIComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // +0x28..+0x37: a growable uint8_t array header (verified against retail
    // slot-3 Serialize, 0x14012dd00, and its resize helper FUN_14000a590):
    // ptr@+0x28, count@+0x30, cap@+0x34. Ctor zeroes all of it, but it is a
    // real per-instance buffer, not two scratch pointers — see
    // uicomponent_serialize.cpp for the port.
    void*    uiData0;      // +0x28: byte-array pointer (zeroed at ctor)
    void*    uiData1;      // +0x30: {int32 count; int32 cap;} (zeroed at ctor)
};

static_assert(sizeof(UIComponent) == 0x38, "UIComponent size mismatch");
static_assert(offsetof(UIComponent, vtable) == 0x00, "");
static_assert(offsetof(UIComponent, flags) == 0x20, "");
static_assert(offsetof(UIComponent, uiData0) == 0x28, "");
static_assert(offsetof(UIComponent, uiData1) == 0x30, "");
