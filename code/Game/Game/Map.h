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
//
// MapCell (B.4 GAME-a): the element of the cell array at +0x80, stride 0x30,
// align 8. Stride and fields from retail's readers:
//   resize 0x14001aa90 (copies +0x00..+0x24 and +0x26..+0x2D; init +0x18 = -1,
//     +0x2A = 0xFF), bulk serializer 0x14001ac10 (IBP, IBP, ExternalPtr,
//     Serialize(9, uchar*) at +0x1C, ushort +0x26, five uchar +0x28..+0x2C,
//     signed char +0x2D);
//   +0x00 blocker   the footprint rasterizer 0x14009d360 stores the entity for a
//                   non-'.' footprint cell and tells the path worker the cell
//                   is blocked (0x1400a6f00's argument `entity != 0`)
//   +0x08 occupant  0x14009dc20 stores the placed entity per footprint cell
//   +0x18 regionId  init -1 (0x14001aa90); the route query 0x14012cd80
//                   compares two cells' ids
//   +0x26 zone      path-region word; cleared and rebuilt by 0x14009c480
//   +0x28 footprintCount  0x14009d360 adds +1 / -1 for a '.' footprint cell;
//                   0x14009f100's load pass reads 0xFF as "unset"
//   +0x29 terrainType     the classify helper 0x14009e2c0 (2, 6, 7, 8, 10);
//                   tested as a bit index (1 << (t & 0x1F)) by the walk masks
//   +0x2D floorCount      signed: the Map query 0x14009f3e0 ray-picks the
//                   floor when > 0 (0x14009f4e9 CMP; JLE)
//   the rest keep typed guess names.
// ============================================================================

#include <cstdint>
#include <cstddef>

class ComponentEntity;
namespace IO { class Stream; }

struct MapCell {
    ComponentEntity* blocker;        // +0x00 InternalBasePtr<ComponentEntity> (0x14009d360)
    ComponentEntity* occupant;       // +0x08 InternalBasePtr<ComponentEntity> (0x14009dc20)
    void*    field_0x10;             // +0x10 ExternalPtr, string-table serialized (0x140002340)
    int32_t  regionId;               // +0x18 init -1; not serialized, copied on resize
    uint8_t  field_0x1C[9];          // +0x1C Serialize(9, uchar*) (0x14001aca0); +0x22 read by 0x140115be0
    uint8_t  _pad25;                 // +0x25 written by neither resize arm
    uint16_t zone;                   // +0x26 path-region word (Serialize ushort)
    uint8_t  footprintCount;         // +0x28 (Serialize uchar)
    uint8_t  terrainType;            // +0x29 (Serialize uchar)
    uint8_t  field_0x2A;             // +0x2A init 0xFF (Serialize uchar); the tree: road batch id
    uint8_t  field_0x2B;             // +0x2B (Serialize uchar); the tree: road blend / orientation
    uint8_t  field_0x2C;             // +0x2C (Serialize uchar); the tree: growth level (/255)
    int8_t   floorCount;             // +0x2D (Serialize signed char)
    uint8_t  _pad2E[2];              // +0x2E written by neither resize arm
};

static_assert(sizeof(MapCell) == 0x30, "MapCell stride 0x30 (0x14001aa90 / 0x14001ac10)");
static_assert(alignof(MapCell) == 8, "MapCell is qword-aligned");
static_assert(offsetof(MapCell, blocker) == 0x00, "MapCell::blocker @ +0x00");
static_assert(offsetof(MapCell, occupant) == 0x08, "MapCell::occupant @ +0x08");
static_assert(offsetof(MapCell, field_0x10) == 0x10, "MapCell +0x10");
static_assert(offsetof(MapCell, regionId) == 0x18, "MapCell::regionId @ +0x18");
static_assert(offsetof(MapCell, field_0x1C) == 0x1C, "MapCell +0x1C");
static_assert(offsetof(MapCell, _pad25) == 0x25, "MapCell +0x25");
static_assert(offsetof(MapCell, zone) == 0x26, "MapCell::zone @ +0x26");
static_assert(offsetof(MapCell, footprintCount) == 0x28, "MapCell::footprintCount @ +0x28");
static_assert(offsetof(MapCell, terrainType) == 0x29, "MapCell::terrainType @ +0x29");
static_assert(offsetof(MapCell, field_0x2A) == 0x2A, "MapCell +0x2A");
static_assert(offsetof(MapCell, field_0x2B) == 0x2B, "MapCell +0x2B");
static_assert(offsetof(MapCell, field_0x2C) == 0x2C, "MapCell +0x2C");
static_assert(offsetof(MapCell, floorCount) == 0x2D, "MapCell::floorCount @ +0x2D");
static_assert(offsetof(MapCell, _pad2E) == 0x2E, "MapCell +0x2E");

class Map {
public:
    Map();                       // 0x14009ee70 (defined in gameupdateloop_load.cpp)
    void Serialize(IO::Stream& stream);   // 0x14009f100 (gamegrid_serialize.cpp)

    int32_t  size;               // +0x00 map edge in cells (Serialize int&)
    uint8_t  _pad04[4];          // +0x04 not written by the ctor
    uint8_t  pickingScene[0x60]; // +0x08 Picking::Scene (embedded; raw, H5)
    void*    field_0x68;         // +0x68 ctor 0 (the tree seeds its pathfinder worker here)
    void*    field_0x70;         // +0x70 ctor 0 (the tree: region-connectivity graph)
    int64_t* regionPairCache;    // +0x78 ctor 0; the region pair cache (Update hands it to 0x1400a5d10,
                                 //       whose tree body takes int64_t*)
    MapCell* cells;              // +0x80 cell Array data (MapCell, stride 0x30)
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

    void RemoveFloor(uint32_t floorId, int x, int y, int w, int h);   // 0x14009cba0 (defined in Component/floorcomponent.cpp)
    void StampInfluence(int state, int cx, int cy, int r, short delta);  // 0x14009f990 (defined in Component/happinesscomponent.cpp)
    bool HasIdentityInRect(int x, int y, int w, int h, const class ComponentDescription* identity) const;  // 0x14009fb60 (Component/diseasecomponent.cpp)
    void StampDanger(int xMin, int yMin, int xMax, int yMax, short delta);   // 0x14009fa80 (Component/firecomponent_ignite.cpp)
};

static_assert(sizeof(Map) == 0xF8, "Map is 0xF8 (Load MemAlloc(0xf8, 8))");
static_assert(alignof(Map) == 8, "Map is qword-aligned");
static_assert(offsetof(Map, size) == 0x00, "Map::size @ +0x00");
static_assert(offsetof(Map, _pad04) == 0x04, "Map +0x04");
static_assert(offsetof(Map, pickingScene) == 0x08, "Map::pickingScene @ +0x08");
static_assert(offsetof(Map, field_0x68) == 0x68, "Map +0x68");
static_assert(offsetof(Map, field_0x70) == 0x70, "Map +0x70");
static_assert(offsetof(Map, regionPairCache) == 0x78, "Map::regionPairCache @ +0x78");
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
