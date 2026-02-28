#pragma once
// Toolbar — Toolbar UI container, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: Toolbar_Ctor (0x1400d2130), Toolbar_Factory (0x140024ba0)
// Base: Object::External (0x20 bytes)
// Contains toolbar items, layout config, selection state.

#include <cstdint>
#include <cstddef>

namespace ToolbarAddr {
    constexpr uintptr_t Ctor    = 0x1400d2130;
    constexpr uintptr_t Factory = 0x140024ba0;
    constexpr uintptr_t Vtable  = 0x140160120;
}

struct Toolbar {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160120
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- Toolbar fields ---
    void*    _field20;           // +0x20: zeroed (items array?)
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    int32_t  _field40;           // +0x40: 8 (max items? columns?)
    uint16_t _field44;           // +0x44: 0x101 (two byte flags, both set)
    uint8_t  _pad46[2];          // +0x46: padding
    uint32_t _field48;           // +0x48: zeroed
    int32_t  selectedIndex;      // +0x4C: -1 sentinel (no selection)
    uint8_t  _pad50[8];          // +0x50: gap
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
};

static_assert(sizeof(Toolbar) == 0x68, "Toolbar size mismatch");
static_assert(offsetof(Toolbar, vtable) == 0x00, "");
static_assert(offsetof(Toolbar, selectedIndex) == 0x4C, "");
