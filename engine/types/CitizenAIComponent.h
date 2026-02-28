#pragma once
// CitizenAIComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0xD0.
//
// Decompiled from: CitizenAIComponent_Ctor (0x14010fb70)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Core AI component for citizen decision-making.
// Flags: base | 0x89
//
// Contains 16 Time objects (0x40..0xBF, 8 bytes each = 128 bytes).
// Ctor init pattern:
//   *(u32)(+0x28) = 0x0C (AI state enum/index)
//   *(u32)(+0x2C) = 0xFFFFFFFF (invalid ID sentinel)
//   *(u16)(+0x30) = 0
//   *(u64)(+0x38) = 0
//   Time::Time() x16 at +0x40..+0xBF
//   *(u64)(+0xC0) = 0
//   *(u64)(+0xC8) = 0

#include <cstdint>
#include <cstddef>
#include "common.h"
#include "Component.h"

namespace CitizenAIComponentAddr {
    constexpr uintptr_t Ctor       = 0x14010fb70;
    constexpr uintptr_t Vtable     = 0x140162470;
    constexpr uintptr_t ScalarDtor = 0x14002fcf0; // vtable[0]
    constexpr uintptr_t Startup    = 0x14002fb00; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x14010f960; // vtable[3]
    constexpr uintptr_t Serialize  = 0x14010f920; // vtable[4]
    constexpr uintptr_t Draw       = 0x14010f880; // vtable[6]
    constexpr uintptr_t Update     = 0x14010f3e0; // vtable[9]
}

struct CitizenAIComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // CitizenAIComponent fields
    uint32_t aiState;      // +0x28: initialized to 0x0C (AI state/mode)
    int32_t  targetId;     // +0x2C: initialized to -1 (invalid entity ID)
    uint16_t data30;       // +0x30: zeroed
    uint8_t  _pad32[6];    // +0x32: gap to next 8-byte aligned field
    uint64_t data38;       // +0x38: zeroed
    Time     timers[16];   // +0x40: 16 Time objects (128 bytes, +0x40..+0xBF)
    uint64_t dataC0;       // +0xC0: zeroed
    uint64_t dataC8;       // +0xC8: zeroed
};

static_assert(sizeof(CitizenAIComponent) == 0xD0, "CitizenAIComponent size mismatch");
static_assert(offsetof(CitizenAIComponent, vtable)  == 0x00, "");
static_assert(offsetof(CitizenAIComponent, next)    == 0x08, "");
static_assert(offsetof(CitizenAIComponent, owner)   == 0x10, "");
static_assert(offsetof(CitizenAIComponent, flags)   == 0x20, "");
static_assert(offsetof(CitizenAIComponent, aiState) == 0x28, "");
static_assert(offsetof(CitizenAIComponent, targetId)== 0x2C, "");
static_assert(offsetof(CitizenAIComponent, data30)  == 0x30, "");
static_assert(offsetof(CitizenAIComponent, data38)  == 0x38, "");
static_assert(offsetof(CitizenAIComponent, timers)  == 0x40, "");
static_assert(offsetof(CitizenAIComponent, dataC0)  == 0xC0, "");
static_assert(offsetof(CitizenAIComponent, dataC8)  == 0xC8, "");
