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
    void*    externalPtr1;       // +0x20: ExternalPtr (serialized via fn_ExternalPtr_Serialize2)
    void*    externalPtr2;       // +0x28: ExternalPtr (serialized via fn_ExternalPtr_Serialize)
    uint64_t stringName;         // +0x30: System::String (profession name)
    uint64_t stringDesc;         // +0x38: System::String (profession description)
    uint64_t stringTool;         // +0x40: System::String (tool type / animation set)
    int32_t  field48;            // +0x48: int (unknown, serialized)
    uint8_t  field4C;            // +0x4C: bool (serialized)
    uint8_t  field4D;            // +0x4D: bool (serialized)
    uint8_t  isAvailable;        // +0x4E: bool, default 1 (serialized)
    uint8_t  _pad4F;             // +0x4F: padding
    int32_t  priority;           // +0x50: 100 (default priority, serialized)
    float    skillMult1;         // +0x54: 1.0f (primary skill mult, serialized)
    float    skillMult2;         // +0x58: 1.0f (secondary skill mult, serialized)
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(Profession) == 0x60, "Profession size mismatch");
static_assert(offsetof(Profession, vtable) == 0x00, "");
static_assert(offsetof(Profession, externalPtr1) == 0x20, "");
static_assert(offsetof(Profession, externalPtr2) == 0x28, "");
static_assert(offsetof(Profession, stringName) == 0x30, "");
static_assert(offsetof(Profession, stringDesc) == 0x38, "");
static_assert(offsetof(Profession, stringTool) == 0x40, "");
static_assert(offsetof(Profession, field48) == 0x48, "");
static_assert(offsetof(Profession, field4C) == 0x4C, "");
static_assert(offsetof(Profession, field4D) == 0x4D, "");
static_assert(offsetof(Profession, isAvailable) == 0x4E, "");
static_assert(offsetof(Profession, priority) == 0x50, "");
static_assert(offsetof(Profession, skillMult1) == 0x54, "");
static_assert(offsetof(Profession, skillMult2) == 0x58, "");
