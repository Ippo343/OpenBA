#pragma once
// ============================================================================
// code/Game/Work/GridTile.h — B.4 wave X (X-G1-2b)
//
// The 16-byte tile record the map's GetTilesInArea (0x14009fc00) pushes into a
// Collection::Array and SortTilesByDistance (0x14009d7c0) sorts (its comparator
// 0x14009d7b0 subtracts the dwords at +0x04). The callers (ClearArea's work body,
// DiseaseComponent 0x140048031) hold the array as a stack-local {data, size,
// capacity} triple and MemFree its data by hand: RawArray<GridTile>, no dtor.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/PathSearch.h"   // RawArray

class ComponentEntity;

struct GridTile {
    int32_t          cell;       // +0x00 the map cell index
    int32_t          distance;   // +0x04 squared grid distance (SortTilesByDistance; 0 when pushed)
    ComponentEntity* entity;     // +0x08 the cell's occupant (InternalPtr), or null
};
static_assert(sizeof(GridTile) == 0x10, "GridTile stride 0x10 (0x14009fc00 MemAlloc(cap << 4))");
static_assert(offsetof(GridTile, distance) == 0x04, "GridTile::distance @ +0x04 (0x14009d7b0)");
static_assert(offsetof(GridTile, entity) == 0x08, "GridTile::entity @ +0x08");
