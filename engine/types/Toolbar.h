#pragma once
// Toolbar — Toolbar UI container, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: Toolbar_Ctor (0x1400d2130), Toolbar_Factory (0x140024ba0),
//                  Toolbar_Slot5 (0x1400d2010)
// Base: Object::External (0x20 bytes)
// Contains toolbar items, layout config, selection state.
//
// Serialize order from Slot5:
//   ExtPtr2 +0x20, ExtPtr +0x28, String +0x30, String +0x38,
//   SerializerVCall(0x98) +0x48, ExtPtrToolbar +0x60, ExtPtrBuild +0x58,
//   SerializerVCall(0x98) +0x40, bool +0x45, bool +0x44, int +0x4c, int +0x50

#include <cstdint>
#include <cstddef>

namespace ToolbarAddr {
    constexpr uintptr_t Ctor    = 0x1400d2130;
    constexpr uintptr_t Factory = 0x140024ba0;
    constexpr uintptr_t Vtable  = 0x140160120;
    constexpr uintptr_t Dtor    = 0x140024c10;
    constexpr uintptr_t Slot5   = 0x1400d2010;  // Serialize
    constexpr uintptr_t Slot6   = 0x140024bf0;  // Cleanup
    constexpr uintptr_t Slot8   = 0x140024be0;  // Init (NOT HOOKED)
}

struct Toolbar {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160120
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- Toolbar fields ---
    void*    _field20;           // +0x20: ExternalPtr (serialized via fn_ExternalPtr_Serialize2)
    void*    _field28;           // +0x28: ExternalPtr (serialized via fn_ExternalPtr_Serialize)
    void*    _field30;           // +0x30: System::String (serialized)
    void*    _field38;           // +0x38: System::String (serialized)
    int32_t  _field40;           // +0x40: 8 (serialized via encoding vcall 0x98)
    bool     _field44;           // +0x44: bool (serialized, default 1)
    bool     _field45;           // +0x45: bool (serialized, default 1)
    uint8_t  _pad46[2];          // +0x46: padding
    int32_t  _field48;           // +0x48: int (serialized via encoding vcall 0x98)
    int32_t  selectedIndex;      // +0x4C: -1 sentinel (serialized as int)
    int32_t  _field50;           // +0x50: int (serialized as int)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: ExternalPtr (serialized via fn_ExternalPtr_BuildSerialize)
    void*    _field60;           // +0x60: ExternalPtr (serialized via FUN_140024c70 / Toolbar typed)
};

static_assert(sizeof(Toolbar) == 0x68, "Toolbar size mismatch");
static_assert(offsetof(Toolbar, vtable) == 0x00, "");
static_assert(offsetof(Toolbar, _field20) == 0x20, "");
static_assert(offsetof(Toolbar, _field30) == 0x30, "");
static_assert(offsetof(Toolbar, _field38) == 0x38, "");
static_assert(offsetof(Toolbar, _field40) == 0x40, "");
static_assert(offsetof(Toolbar, _field44) == 0x44, "");
static_assert(offsetof(Toolbar, _field45) == 0x45, "");
static_assert(offsetof(Toolbar, _field48) == 0x48, "");
static_assert(offsetof(Toolbar, selectedIndex) == 0x4C, "");
static_assert(offsetof(Toolbar, _field50) == 0x50, "");
static_assert(offsetof(Toolbar, _field58) == 0x58, "");
static_assert(offsetof(Toolbar, _field60) == 0x60, "");
