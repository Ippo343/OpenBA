#pragma once
// CompleteRoad — Action to complete road construction, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: CompleteRoad_Factory (0x140021ec0, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> CompleteRoad

#include <cstdint>
#include <cstddef>

namespace CompleteRoadAddr {
    constexpr uintptr_t Factory = 0x140021ec0;
    constexpr uintptr_t Vtable  = 0x14015fb70;
}

struct CompleteRoad {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fb70
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- CompleteRoad fields ---
    int32_t  roadTileIndex;      // +0x20: terrain tile index
    int32_t  roadType;           // +0x24: road type ID
    uint8_t  roadDirection;      // +0x28: direction
    uint8_t  _pad29[7];          // +0x29: padding
};

static_assert(sizeof(CompleteRoad) == 0x30, "CompleteRoad size mismatch");
static_assert(offsetof(CompleteRoad, vtable) == 0x00, "");
static_assert(offsetof(CompleteRoad, roadTileIndex) == 0x20, "");
static_assert(offsetof(CompleteRoad, roadType) == 0x24, "");
static_assert(offsetof(CompleteRoad, roadDirection) == 0x28, "");
