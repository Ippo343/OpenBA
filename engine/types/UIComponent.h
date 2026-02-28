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

    void*    uiData0;      // +0x28: zeroed
    void*    uiData1;      // +0x30: zeroed
};

static_assert(sizeof(UIComponent) == 0x38, "UIComponent size mismatch");
static_assert(offsetof(UIComponent, vtable) == 0x00, "");
static_assert(offsetof(UIComponent, flags) == 0x20, "");
static_assert(offsetof(UIComponent, uiData0) == 0x28, "");
static_assert(offsetof(UIComponent, uiData1) == 0x30, "");
