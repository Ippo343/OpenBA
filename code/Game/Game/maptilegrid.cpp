// code/Game/Game/maptilegrid.cpp
// Gameplay tile-type grid — the front-half of Map::CreateNaturalResource
// (CreateNaturalResource_impl_ef20, exe 0x14009ef20).
//
// New-game Load step 31 builds the terrain mesh (worldgen.cpp) and then this
// per-cell TILE GRID at Game+0x80: a dim*dim array of 0x30-byte cells, each
// tagged with a terrain TYPE (buildable / shore / water / mountain). The grid is
// the prerequisite for the start-town spiral spawn-finder (FUN_1400a2880), which
// scans for a square of buildable (type 2) cells and otherwise loops forever.
//
// We reimplement only the classification front-half here. DEFERRED to a later
// slice (matches the original ordering, see project_starttown_a3300):
//   * FUN_14009c480 + CreateNaturalResource_impl_sub_e8c0 — tree/stone scatter
//     (spawns worker threads)
//   * the buildable<->water neighbour-link loop (FUN_140088b80) — shore linkage,
//     not needed by the spawn-finder
//
// Cell layout (0x30 bytes), fresh-init values from sub_aa90's new-element branch:
//   +0x18 dword = 0xffffffff,  +0x2a byte = 0xff,  type @+0x29 = 0, flag @+0x2d = 0.
//
// Classifier (helper_e2c0) thresholds are the exact exe .rdata float constants
// (read_memory, verified 2026-05-31). Slope is the vertical component of the
// surface normal (fn_MapGetTerrainPos's outTri[2]); see mapquery.cpp.

#include "Game/exe_bodies.h"
#include "Game/Map.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "perf_terrain.h"
#include <cstdint>
#include <cstring>

// --- classifier thresholds (exe .rdata) -------------------------------------
static constexpr float kCenterBias   = 0.5f;    // DAT_140172114 (cell-center offset)
static constexpr float kBuildableMax = 0.75f;   // DAT_140172124
static constexpr float kSlopeMin     = 0.9f;    // DAT_140172134
static constexpr float kMountainMax  = 2.0f;    // DAT_140172174
static constexpr float kShoreMax     = -0.25f;  // DAT_140172274
static constexpr float kDeepWaterMax = -1.4f;   // DAT_140172284

// Terrain tile types.
enum TileType { TILE_BUILDABLE = 2, TILE_MOUNTAIN_HI = 6, TILE_SHORE = 7,
                TILE_DEEP_WATER = 8, TILE_MOUNTAIN_LO = 10 };

// CreateNaturalResource_helper_e2c0 — classify one cell into a terrain type.
static int MapTileGrid_Classify(void* game, int dim, int idx) {
    const float center = (float)(dim >> 1) - kCenterBias;
    float in[3]  = { (float)(idx % dim) - center, (float)(idx / dim) - center, 0.0f };
    float out[3] = { 0, 0, 0 };
    float tri[4] = { 0, 0, 0, 0 };
    Reimpl_MapGetTerrainPos(game, out, in, 1, tri);   // flag=1 => height-only, no picking refine
    const float h     = out[2];
    const float slope = tri[2];
    if (h < kDeepWaterMax) return TILE_DEEP_WATER;
    if (h < kShoreMax)     return TILE_SHORE;
    if (h >= kShoreMax && h <= kBuildableMax && slope >= kSlopeMin) return TILE_BUILDABLE;
    return (h < kMountainMax) ? TILE_MOUNTAIN_LO : TILE_MOUNTAIN_HI;
}

// Build the dim*dim tile grid at Map::cells (front-half of CreateNaturalResource).
extern "C" void MapTileGrid_Build(void* gameVoid, int dim) {
    if (!gameVoid || dim <= 0) return;
    // LEFT: gameupdateloop_load.cpp (GAME-a) declares this locally with a `void*` map.
    Map* game = (Map*)gameVoid;
    game->size = dim;

    const int    count = dim * dim;
    const size_t bytes = (size_t)count * sizeof(MapCell);
    MapCell* cells = static_cast<MapCell*>(System::MemoryAllocator::MemAlloc(bytes, 8));
    if (!cells) return;

    // Fresh-cell init (sub_aa90 new-element branch): zeroed except +0x18/+0x2a.
    memset(cells, 0, bytes);
    for (int i = 0; i < count; ++i) {
        cells[i].regionId   = -1;     // +0x18 dword 0xffffffff
        cells[i].field_0x2A = 0xff;   // +0x2a
    }

    // Publish the array header {data, count, capacity} at Map +0x80 before the
    // classify pass (classify uses flag=1 and never reads the cell array).
    game->cells        = cells;
    game->cellCount    = count;
    game->cellCapacity = count;

    // Classify pass — write the terrain type (+0x29) for every cell.
    for (int i = 0; i < count; ++i)
        cells[i].terrainType = (uint8_t)MapTileGrid_Classify(gameVoid, dim, i);
    PerfX::TerrainInvalidate();   // [PerfX] fresh grid: mirror rebuilds on first read
}
