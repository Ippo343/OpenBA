#pragma once
// MoveTo — Navigation/pathfinding action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x480 (1152 bytes).
// LARGEST state machine action — handles citizen movement/pathfinding.
//
// Decompiled from: MoveTo_Ctor (0x1400c90e0), MoveTo_Factory (0x1400228c0)
// Inherits: Object::Internal -> Action (base 0x20) -> MoveTo
//
// Constructor shows:
//   - First sets Action vtable (0x14015faa0), then overwrites with Work vtable (0x14015fe40)
//   - Most of the 0x480 bytes are zero-initialized (pathfinding data, waypoints, etc.)
//   - Two -1 sentinel fields at +0x20 and +0x60 (invalid IDs)
//   - Float 1.0 at +0x478 (speed multiplier?)

#include <cstdint>
#include <cstddef>

namespace MoveToAddr {
    constexpr uintptr_t Ctor         = 0x1400c90e0;
    constexpr uintptr_t Factory      = 0x1400228c0;
    constexpr uintptr_t Vtable       = 0x14015fe40;
    constexpr uintptr_t RegisterType = 0x14014d250;
}

struct MoveTo {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fe40
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[3];          // +0x19: padding

    // --- MoveTo fields ---
    int32_t  targetId;           // +0x20: target ID (-1 = invalid)
    uint32_t _field24;           // +0x24: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    int32_t  _field60;           // +0x60: -1 sentinel (secondary ID?)
    uint32_t _field64;           // +0x64: zeroed
    uint8_t  _pathData[0x40C];   // +0x68: large pathfinding/waypoint data block (zeroed)
    uint8_t  _field474;          // +0x474: zeroed
    uint8_t  _pad475[3];         // +0x475: padding
    float    speedMultiplier;    // +0x478: 1.0f (0x3f800000)
    uint8_t  _pad47C[4];         // +0x47C: padding to 0x480
};

static_assert(sizeof(MoveTo) == 0x480, "MoveTo size mismatch");
static_assert(offsetof(MoveTo, vtable) == 0x00, "");
static_assert(offsetof(MoveTo, targetId) == 0x20, "");
static_assert(offsetof(MoveTo, _field60) == 0x60, "");
static_assert(offsetof(MoveTo, speedMultiplier) == 0x478, "");
