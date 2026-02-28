#pragma once
// Profession — Citizen profession definition, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: Profession_Ctor (0x1400a9030), Profession_Factory (0x14001e530)
// Base: Object::External (0x20 bytes)
// Defines a profession: name, description, priority, and skill multipliers.

#include <cstdint>
#include <cstddef>

namespace ProfessionAddr {
    constexpr uintptr_t Ctor         = 0x1400a9030;
    constexpr uintptr_t Factory      = 0x14001e530;
    constexpr uintptr_t Vtable       = 0x14015efa0;
    constexpr uintptr_t RegisterType = 0x14014cc90;
}

struct Profession {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015efa0
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- Profession fields ---
    void*    _field20;           // +0x20: zeroed (name string?)
    void*    _field28;           // +0x28: zeroed (description?)
    void*    _field30;           // +0x30: zeroed (tool type?)
    void*    _field38;           // +0x38: zeroed (animation set?)
    void*    _field40;           // +0x40: zeroed
    uint32_t _field48;           // +0x48: zeroed
    uint16_t _field4C;           // +0x4C: zeroed
    uint8_t  _field4E;           // +0x4E: 1 (bool, is available?)
    uint8_t  _pad4F;             // +0x4F: padding
    int32_t  priority;           // +0x50: 100 (default priority)
    float    skillMult1;         // +0x54: 1.0f (0x3f800000) (primary skill mult?)
    float    skillMult2;         // +0x58: 1.0f (0x3f800000) (secondary skill mult?)
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(Profession) == 0x60, "Profession size mismatch");
static_assert(offsetof(Profession, vtable) == 0x00, "");
static_assert(offsetof(Profession, priority) == 0x50, "");
static_assert(offsetof(Profession, skillMult1) == 0x54, "");
