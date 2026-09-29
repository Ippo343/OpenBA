#pragma once
// ============================================================================
// code/Game/Component/CharacterGrid.h — B.4 COMP-a
//
// The character spatial hash at GameUpdateLoop+0x88 (`m_characterGrid`): a loose
// implicit quadtree over every live CharacterComponent. Retail has no vtable and
// no reflected name for it (MemAlloc(0x60, 8) in the world load); the name is the
// GameUpdateLoop member's. Its members are the bodies charmanager.cpp ports:
//   0x1400183f0 Register   0x140018560 Release   0x140018660 Place
//   0x140018c40 Link       0x140018d00 Unlink
// Layout read off those bodies' disassembly (see charmanager.cpp); the slot and
// free-list arrays are Collection::Array headers {data, int count, int capacity}.
//
// Implicit-quadtree layout: a node's children are 4i+1..4i+4, parent = (i-1)>>2,
// level-L base = (4^L-1)/3.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"

class CharacterComponent;   // Component/CharacterComponent.h

// One slot per registered character (0x18 bytes, indexed by the character index
// CharacterComponent+0x40 holds).
struct CharacterGridSlot {
    Object::InternalPtr<CharacterComponent> character;  // +0x00
    int32_t prev;                                        // +0x08 circular list within a cell
    int32_t next;                                        // +0x0C
    int32_t cell;                                        // +0x10 global cell index (-1 = none)
    uint8_t _pad14[4];                                   // +0x14
};
static_assert(sizeof(CharacterGridSlot) == 0x18, "slot stride 0x18 (0x140018c40)");
static_assert(offsetof(CharacterGridSlot, prev) == 0x08, "CharacterGridSlot::prev @ +0x08");
static_assert(offsetof(CharacterGridSlot, next) == 0x0C, "CharacterGridSlot::next @ +0x0C");
static_assert(offsetof(CharacterGridSlot, cell) == 0x10, "CharacterGridSlot::cell @ +0x10");

class CharacterGrid {
public:
    // ---- members (retail addresses) ----
    int64_t Register(Object::InternalPtr<CharacterComponent>* character);   // 0x1400183f0
    void    Release(int index);                                            // 0x140018560
    void    Place(int index);                                              // 0x140018660
    void    Link(int index);                                               // 0x140018c40
    void    Unlink(int index);                                             // 0x140018d00

    float              worldSize;         // +0x00 full world extent in world units
    float              centerX;           // +0x04 worldSize / 2
    float              centerY;           // +0x08
    int32_t            levels;            // +0x0C quadtree depth
    CharacterGridSlot* slots;             // +0x10 Array data
    int32_t            slotCount;         // +0x18
    int32_t            slotCapacity;      // +0x1C
    int32_t*           freeList;          // +0x20 recycled character indices (Array data)
    int32_t            freeCount;         // +0x28
    int32_t            freeCapacity;      // +0x2C
    int32_t*           cellHead;          // +0x30 per cell: the head character index, -1 = empty
    int32_t            field_0x38;        // +0x38 (no ported body reads it)
    int32_t            field_0x3C;        // +0x3C
    int32_t*           levelOffsets;      // +0x40 the first cell of each level, (4^L-1)/3
    int32_t            field_0x48;        // +0x48 (no ported body reads it)
    int32_t            field_0x4C;        // +0x4C
    int32_t*           cellOccupancy;     // +0x50 per cell: characters in its subtree
    int32_t            field_0x58;        // +0x58 (no ported body reads it)
    int32_t            field_0x5C;        // +0x5C
};

static_assert(sizeof(CharacterGrid) == 0x60, "CharacterGrid is MemAlloc(0x60, 8)");
static_assert(offsetof(CharacterGrid, worldSize) == 0x00, "CharacterGrid::worldSize @ +0x00");
static_assert(offsetof(CharacterGrid, centerX) == 0x04, "CharacterGrid::centerX @ +0x04");
static_assert(offsetof(CharacterGrid, centerY) == 0x08, "CharacterGrid::centerY @ +0x08");
static_assert(offsetof(CharacterGrid, levels) == 0x0C, "CharacterGrid::levels @ +0x0C");
static_assert(offsetof(CharacterGrid, slots) == 0x10, "CharacterGrid::slots @ +0x10");
static_assert(offsetof(CharacterGrid, slotCount) == 0x18, "CharacterGrid::slotCount @ +0x18");
static_assert(offsetof(CharacterGrid, slotCapacity) == 0x1C, "CharacterGrid::slotCapacity @ +0x1C");
static_assert(offsetof(CharacterGrid, freeList) == 0x20, "CharacterGrid::freeList @ +0x20");
static_assert(offsetof(CharacterGrid, freeCount) == 0x28, "CharacterGrid::freeCount @ +0x28");
static_assert(offsetof(CharacterGrid, freeCapacity) == 0x2C, "CharacterGrid::freeCapacity @ +0x2C");
static_assert(offsetof(CharacterGrid, cellHead) == 0x30, "CharacterGrid::cellHead @ +0x30");
static_assert(offsetof(CharacterGrid, field_0x38) == 0x38, "CharacterGrid +0x38");
static_assert(offsetof(CharacterGrid, field_0x3C) == 0x3C, "CharacterGrid +0x3C");
static_assert(offsetof(CharacterGrid, levelOffsets) == 0x40, "CharacterGrid::levelOffsets @ +0x40");
static_assert(offsetof(CharacterGrid, field_0x48) == 0x48, "CharacterGrid +0x48");
static_assert(offsetof(CharacterGrid, field_0x4C) == 0x4C, "CharacterGrid +0x4C");
static_assert(offsetof(CharacterGrid, cellOccupancy) == 0x50, "CharacterGrid::cellOccupancy @ +0x50");
static_assert(offsetof(CharacterGrid, field_0x58) == 0x58, "CharacterGrid +0x58");
static_assert(offsetof(CharacterGrid, field_0x5C) == 0x5C, "CharacterGrid +0x5C");
