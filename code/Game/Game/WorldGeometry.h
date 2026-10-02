#pragma once
// ============================================================================
// code/Game/Game/WorldGeometry.h — ENDGAME B.3.8 block LOOP (B38_RULES sec. 5,
// "the 5 unreflected exe subclasses of Runtime classes")
//
// `WorldGeometry` (0x230): the exe's unreflected `Graphics::Geometry` subclass
// that draws the terrain (heightmap chunks), the water contour mesh and the
// decal SoA arrays — GameUpdateLoop+0x110 in game, MenuScene+0x28 on the menu.
// Retail has no name for it (B38_SLOTS.md "{exe Graphics::Geometry subclass A}");
// the name is the tree's.
//
// RETAIL (Application-x64.exe): table 0x14015f110, 9 slots, MemAlloc(0x230),
// ctor 0x1400b00e0 (and the inlined copy in TradeWork_impl_ed50).
//   [0] 0x14001ecc0 scalar deleting dtor (body 0x1400aed50)
//   [1] 0x1400aed40 Update       (JMP 0x1400aad50, the dirty-chunk rebuild)
//   [2] 0x1400aea10 Draw         (per draw item)
//   [3] 0x1400ae490 AddDrawItems (frustum cull + Renderer::AddDrawItem)
//   [4]..[8] Graphics::Geometry's (imports)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "graphics/Geometry.h"
#include "video/Buffers.h"

class WorldGeometry final : public Graphics::Geometry {
public:
    WorldGeometry();                                                  // 0x1400b00e0

    // body 0x1400aed50
    ~WorldGeometry() override;                                        // [0] 0x14001ecc0
    void Update(const System::Time& time) override;                   // [1] 0x1400aed40
    void Draw(Video::Context& context, const Graphics::Camera& camera,
              Graphics::ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;     // [2] 0x1400aea10
    void AddDrawItems(const Graphics::Scene& scene,
                      Graphics::Renderer& renderer) const override;   // [3] 0x1400ae490

    // 0x1400aad50: the dirty-chunk rebuild slot 1 JMPs to (a non-virtual member;
    // also reached by address through the Reimpl_GeometryChunkUpdate door).
    void RebuildDirtyChunks();
    void RemoveDecal(int decalId);                                   // 0x1400b0d00 (defined in Component/decalcomponent.cpp)
    // B.4 ACT-a (bodies in Action/action_pf_road.cpp)
    void MarkChunksDirty(int x, int y);                              // 0x1400ab150
    void RebuildRoadMesh(class Map* map);                            // 0x1400af370


public:
    int32_t  _gridSize;           // +0xC0  heightmap edge (cells)
    int32_t  _fieldC4;            // +0xC4
    int32_t  _chunkSize;          // +0xC8  cells per chunk edge
    int32_t  _fieldCC;            // +0xCC
    int32_t  _fieldD0;            // +0xD0
    int32_t  _chunkIndexStride;   // +0xD4  index count per chunk (the ctor leaves it)
    int16_t* _arrayD8;            // +0xD8  {data, (height words: 0x14009e0e0 stores one; B.4 ACT-a H4)
    uint64_t _arrayD8Size;        // +0xE0   count|cap}
    void*    _dirtyTreeRoot;      // +0xE8  tree of dirty chunks (nodes: children +0x10/+0x18)
    int32_t  _dirtyTreeCount;     // +0xF0
    uint8_t  _padF4[4];           // +0xF4
    void*    _terrainMaterial;    // +0xF8  MaterialInstance* for the chunks
    Video::VertexBuffer _vertexBuffer;       // +0x100 terrain chunk vertices
    Video::IndexBuffer  _indexBuffer;        // +0x120
    void*    _heightBytes;        // +0x140 raw heightmap (4 bytes/cell)
    void*    _heightImage;        // +0x148 Video::ImageBuffer* uploaded from +0x140
    void*    _waterMaterialA;     // +0x150
    void*    _waterMaterialB;     // +0x158
    Video::VertexBuffer _waterVertexBuffer;  // +0x160 water contour mesh
    Video::IndexBuffer  _waterIndexBuffer;   // +0x180
    // Nine {data, count|cap} headers, +0x1A0..+0x22F:
    //   +0x1A0 / +0x1B0 contour vertex / index accumulators, +0x1C0 / +0x1D0 the
    //   int4 draw-range tables (modes 0x6000 / 0x8000), +0x1E0 array of arrays,
    //   +0x1F0 / +0x200 the 0x60-byte decal elements (modes 0x2000 / 0x4000),
    //   +0x210 External* refs, +0x220 ints.
    struct Header { void* data; uint64_t sizeCap; };
    Header   _arrays[9];          // +0x1A0
};

static_assert(sizeof(WorldGeometry) == 0x230, "WorldGeometry is MemAlloc(0x230)");
static_assert(alignof(WorldGeometry) == 8, "qword-aligned");
static_assert(offsetof(WorldGeometry, _gridSize) == 0xC0, "+0xC0");
static_assert(offsetof(WorldGeometry, _chunkSize) == 0xC8, "+0xC8");
static_assert(offsetof(WorldGeometry, _chunkIndexStride) == 0xD4, "+0xD4");
static_assert(offsetof(WorldGeometry, _arrayD8) == 0xD8, "+0xD8");
static_assert(offsetof(WorldGeometry, _dirtyTreeRoot) == 0xE8, "+0xE8");
static_assert(offsetof(WorldGeometry, _dirtyTreeCount) == 0xF0, "+0xF0");
static_assert(offsetof(WorldGeometry, _terrainMaterial) == 0xF8, "+0xF8");
static_assert(offsetof(WorldGeometry, _vertexBuffer) == 0x100, "+0x100");
static_assert(offsetof(WorldGeometry, _indexBuffer) == 0x120, "+0x120");
static_assert(offsetof(WorldGeometry, _heightBytes) == 0x140, "+0x140");
static_assert(offsetof(WorldGeometry, _heightImage) == 0x148, "+0x148");
static_assert(offsetof(WorldGeometry, _waterMaterialA) == 0x150, "+0x150");
static_assert(offsetof(WorldGeometry, _waterVertexBuffer) == 0x160, "+0x160");
static_assert(offsetof(WorldGeometry, _waterIndexBuffer) == 0x180, "+0x180");
static_assert(offsetof(WorldGeometry, _arrays) == 0x1A0, "+0x1A0");
