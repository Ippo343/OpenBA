#pragma once
// ============================================================================
// code/Game/Game/Map.h — ENDGAME B.4 W0-H2
//
// The 0xF8-byte world map object at GameUpdateLoop+0xF8 (m_game): the tile
// grid, the map's own Picking::Scene, the pathfinder / region objects the
// tree seeds into it. Load builds it with MemAlloc(0xf8, 8) + ctor
// 0x14009ee70; serialized by 0x14009f100.
//
// CLASS NAME NOT SETTLED. The tree calls it "Game" (m_game); the profile
// build lists `Game\Map.cpp` and the tree's own comments call its functions
// Map::CreateNaturalResource / Map_CalcDistance. `Map` is used until the
// profile exe (Application-x64-profile.exe, not open in Ghidra) confirms it.
// No vtable: a plain struct, built in raw memory (Load placement-news it,
// which constructs nothing, then runs the ctor body).
//
// FIELDS (retail ctor 0x14009ee70 unless noted; names only where retail's
// behaviour gives one, else a typed guess):
//   +0x00 int32  size        Serialize(int&) first (0x14009f100); squared
//                            as the cell count (size * size) by the loader
//   +0x08 Picking::Scene     embedded, Picking::Scene::Scene(+0x08) — raw
//                            storage (B.4 H5: the member's ctor would run)
//   +0x80 cell array         Collection::Array of 0x30-byte cells: data +0x80,
//                            count +0x88 (Serialize(int&) + resize
//                            0x14009aa90), capacity +0x8C
//   +0x90 int16[9]           Serialize(9, short*) (0x14009f100); ctor zero
//   +0xA2 bool               ctor 1; the loader sets it to 1
//   +0xE8 / +0xF0            a BST root (children at node+0x10 / +0x18,
//                            int key at +0) and its count (0x14009f100)
//   every other qword / dword: zeroed by the ctor (+0xD0 = 8), meaning not
//   settled.
// ============================================================================

#include <cstdint>
#include <cstddef>

class Map {
public:
    int32_t  size;               // +0x00 map edge in cells (Serialize int&)
    uint8_t  _pad04[4];          // +0x04 not written by the ctor
    uint8_t  pickingScene[0x60]; // +0x08 Picking::Scene (embedded; raw, H5)
    void*    field_0x68;         // +0x68 ctor 0 (the tree seeds its pathfinder worker here)
    void*    field_0x70;         // +0x70 ctor 0 (the tree: region-connectivity graph)
    void*    field_0x78;         // +0x78 ctor 0 (the tree: region pair cache)
    void*    cells;              // +0x80 cell Array data (0x30-byte cells)
    int32_t  cellCount;          // +0x88 cell Array count (Serialize int&)
    int32_t  cellCapacity;       // +0x8C cell Array capacity
    int16_t  field_0x90[9];      // +0x90 Serialize(9, short*)
    bool     field_0xA2;         // +0xA2 ctor 1
    uint8_t  _padA3[5];          // +0xA3
    void*    field_0xA8;         // +0xA8 ctor 0
    void*    field_0xB0;         // +0xB0 ctor 0
    int32_t  field_0xB8;         // +0xB8 ctor 0
    uint8_t  _padBC[4];          // +0xBC
    void*    field_0xC0;         // +0xC0 ctor 0
    void*    field_0xC8;         // +0xC8 ctor 0
    uint64_t field_0xD0;         // +0xD0 ctor 8
    void*    field_0xD8;         // +0xD8 ctor 0
    int32_t  field_0xE0;         // +0xE0 ctor 0
    uint8_t  _padE4[4];          // +0xE4
    void*    field_0xE8;         // +0xE8 BST root (ctor 0)
    int32_t  field_0xF0;         // +0xF0 BST count (ctor 0)
    uint8_t  _padF4[4];          // +0xF4
};

static_assert(sizeof(Map) == 0xF8, "Map is 0xF8 (Load MemAlloc(0xf8, 8))");
static_assert(alignof(Map) == 8, "Map is qword-aligned");
static_assert(offsetof(Map, size) == 0x00, "Map::size @ +0x00");
static_assert(offsetof(Map, _pad04) == 0x04, "Map +0x04");
static_assert(offsetof(Map, pickingScene) == 0x08, "Map::pickingScene @ +0x08");
static_assert(offsetof(Map, field_0x68) == 0x68, "Map +0x68");
static_assert(offsetof(Map, field_0x70) == 0x70, "Map +0x70");
static_assert(offsetof(Map, field_0x78) == 0x78, "Map +0x78");
static_assert(offsetof(Map, cells) == 0x80, "Map::cells @ +0x80");
static_assert(offsetof(Map, cellCount) == 0x88, "Map::cellCount @ +0x88");
static_assert(offsetof(Map, cellCapacity) == 0x8C, "Map::cellCapacity @ +0x8C");
static_assert(offsetof(Map, field_0x90) == 0x90, "Map +0x90");
static_assert(offsetof(Map, field_0xA2) == 0xA2, "Map +0xA2");
static_assert(offsetof(Map, _padA3) == 0xA3, "Map +0xA3");
static_assert(offsetof(Map, field_0xA8) == 0xA8, "Map +0xA8");
static_assert(offsetof(Map, field_0xB0) == 0xB0, "Map +0xB0");
static_assert(offsetof(Map, field_0xB8) == 0xB8, "Map +0xB8");
static_assert(offsetof(Map, _padBC) == 0xBC, "Map +0xBC");
static_assert(offsetof(Map, field_0xC0) == 0xC0, "Map +0xC0");
static_assert(offsetof(Map, field_0xC8) == 0xC8, "Map +0xC8");
static_assert(offsetof(Map, field_0xD0) == 0xD0, "Map +0xD0");
static_assert(offsetof(Map, field_0xD8) == 0xD8, "Map +0xD8");
static_assert(offsetof(Map, field_0xE0) == 0xE0, "Map +0xE0");
static_assert(offsetof(Map, _padE4) == 0xE4, "Map +0xE4");
static_assert(offsetof(Map, field_0xE8) == 0xE8, "Map +0xE8");
static_assert(offsetof(Map, field_0xF0) == 0xF0, "Map +0xF0");
static_assert(offsetof(Map, _padF4) == 0xF4, "Map +0xF4");
