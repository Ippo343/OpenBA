#pragma once
// PerformWork — Generic work-performing action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: PerformWork_Ctor (0x1400c96b0), PerformWork_Factory (0x140022b90)
// Inherits: Object::Internal -> Action (0x20) -> PerformWork
//
// Constructor sequence:
//   Object::Internal::Internal() -> set Action vtable -> init Action fields ->
//   set PerformWork vtable (0x14015fea0) -> init PerformWork fields

#include <cstdint>
#include <cstddef>

namespace PerformWorkAddr {
    constexpr uintptr_t Ctor         = 0x1400c96b0;
    constexpr uintptr_t Factory      = 0x140022b90;
    constexpr uintptr_t Vtable       = 0x14015fea0;
    constexpr uintptr_t RegisterType = 0x14014d290;
}

struct PerformWork {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fea0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- PerformWork fields ---
    void*    _field20;           // +0x20: zeroed
    uint32_t _field2C;           // +0x2C: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint8_t  _field40;           // +0x40: zeroed
    uint8_t  _pad41[3];          // +0x41: padding
    int32_t  _field44;           // +0x44: -1 sentinel
};

static_assert(sizeof(PerformWork) == 0x48, "PerformWork size mismatch");
static_assert(offsetof(PerformWork, vtable) == 0x00, "");
static_assert(offsetof(PerformWork, _field44) == 0x44, "");
