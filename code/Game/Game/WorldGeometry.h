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
#include "Action/PathSearch.h"   // RawArray<T> (Collection::Array<T>'s three raw fields)

namespace Graphics { class MaterialInstance; }
namespace Video { class ImageBuffer; }
namespace Math { class Vector2; }

// ---------------------------------------------------------------------------
// The element types of WorldGeometry's arrays. Retail has no names for them;
// the names are descriptive (B4_RULES H3), the layouts are retail's.
// ---------------------------------------------------------------------------

// +0x1A0: the packed water vertex b260 (0x1400ab260) appends, 0x10 bytes:
// position int16 x/y/z/0 (x, y = world * 48), uv int16 (* 1024), colour bytes.
struct WaterVertex {
    int16_t position[4];         // +0x00
    int16_t uv[2];               // +0x08
    uint8_t color[4];            // +0x0C
};
static_assert(sizeof(WaterVertex) == 0x10, "WaterVertex is b260's 0x10 stride");
static_assert(offsetof(WaterVertex, uv) == 0x08, "WaterVertex::uv @ +0x08");
static_assert(offsetof(WaterVertex, color) == 0x0C, "WaterVertex::color @ +0x0C");

// +0x1C0 / +0x1D0: one draw range per water body / tarn feature (0x10 bytes).
struct WaterDrawRange {
    int32_t indexStart;          // +0x00
    int32_t vertexStart;         // +0x04
    int32_t vertexCount;         // +0x08
    int32_t indexCount;          // +0x0C
};
static_assert(sizeof(WaterDrawRange) == 0x10, "WaterDrawRange stride 0x10");
static_assert(offsetof(WaterDrawRange, vertexStart) == 0x04, "WaterDrawRange::vertexStart @ +0x04");
static_assert(offsetof(WaterDrawRange, vertexCount) == 0x08, "WaterDrawRange::vertexCount @ +0x08");
static_assert(offsetof(WaterDrawRange, indexCount) == 0x0C, "WaterDrawRange::indexCount @ +0x0C");

// The 0x14-byte terrain-mesh vertex (stride 0x14, align 2: Array<T>::SetSize
// 0x14001ee40 / 0x14001f8d0; serialized by 0x14001f9c0 as 4 + 4 shorts and 4 bytes).
struct TerrainMeshVertex {
    int16_t  x;                  // +0x00
    int16_t  y;                  // +0x02
    int16_t  height;             // +0x04
    uint16_t blend;              // +0x06
    int16_t  aoU;                // +0x08
    int16_t  aoV;                // +0x0A
    uint16_t tileU;              // +0x0C
    uint16_t tileV;              // +0x0E
    int8_t   normal[3];          // +0x10
    uint8_t  normalW;            // +0x13
};
static_assert(sizeof(TerrainMeshVertex) == 0x14, "TerrainMeshVertex stride 0x14");
static_assert(alignof(TerrainMeshVertex) == 2, "TerrainMeshVertex align 2");
static_assert(offsetof(TerrainMeshVertex, aoU) == 0x08, "TerrainMeshVertex::aoU @ +0x08");
static_assert(offsetof(TerrainMeshVertex, normal) == 0x10, "TerrainMeshVertex::normal @ +0x10");

// +0x1F0 / +0x200: the 0x60-byte mesh batch (one element type for both arrays:
// destroy range 0x14001fb80 on both in ~WorldGeometry, serialize 0x14001ffa0). Fields
// past +0x3C have no name in retail: typed guess names from the serializer's
// overloads (int / uchar / int / bool / five floats / bool / bool).
struct TerrainMeshBatch {
    Graphics::MaterialInstance* material;     // +0x00 ExternalPtr<MaterialInstance> (0x140007140)
    RawArray<TerrainMeshVertex> vertices;     // +0x08
    RawArray<uint16_t>          indices;      // +0x18 (Serialize(count, ushort*))
    int32_t  id;                              // +0x28 slot id, -1 = free (0x1400b0d00)
    int32_t  minX;                            // +0x2C culling box in tile coordinates
    int32_t  minY;                            // +0x30
    int32_t  maxX;                            // +0x34
    int32_t  maxY;                            // +0x38
    uint8_t  field_0x3C;                      // +0x3C
    uint8_t  _pad3D[3];                       // +0x3D
    int32_t  field_0x40;                      // +0x40
    bool     field_0x44;                      // +0x44
    uint8_t  _pad45[3];                       // +0x45
    float    field_0x48[5];                   // +0x48..+0x58
    bool     field_0x5C;                      // +0x5C
    bool     field_0x5D;                      // +0x5D (serializer version >= 2)
    uint8_t  _pad5E[2];                       // +0x5E
};
static_assert(sizeof(TerrainMeshBatch) == 0x60, "TerrainMeshBatch stride 0x60");
static_assert(offsetof(TerrainMeshBatch, vertices) == 0x08, "TerrainMeshBatch::vertices @ +0x08");
static_assert(offsetof(TerrainMeshBatch, indices) == 0x18, "TerrainMeshBatch::indices @ +0x18");
static_assert(offsetof(TerrainMeshBatch, id) == 0x28, "TerrainMeshBatch::id @ +0x28");
static_assert(offsetof(TerrainMeshBatch, minX) == 0x2C, "TerrainMeshBatch::minX @ +0x2C");
static_assert(offsetof(TerrainMeshBatch, maxY) == 0x38, "TerrainMeshBatch::maxY @ +0x38");
static_assert(offsetof(TerrainMeshBatch, field_0x3C) == 0x3C, "TerrainMeshBatch +0x3C");
static_assert(offsetof(TerrainMeshBatch, field_0x40) == 0x40, "TerrainMeshBatch +0x40");
static_assert(offsetof(TerrainMeshBatch, field_0x44) == 0x44, "TerrainMeshBatch +0x44");
static_assert(offsetof(TerrainMeshBatch, field_0x48) == 0x48, "TerrainMeshBatch +0x48");
static_assert(offsetof(TerrainMeshBatch, field_0x5C) == 0x5C, "TerrainMeshBatch +0x5C");
static_assert(offsetof(TerrainMeshBatch, field_0x5D) == 0x5D, "TerrainMeshBatch +0x5D");

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
    Graphics::MaterialInstance* _terrainMaterial;   // +0xF8  the chunks' material (TerrainGenerator triple 1)
    Video::VertexBuffer _vertexBuffer;       // +0x100 terrain chunk vertices
    Video::IndexBuffer  _indexBuffer;        // +0x120
    void*    _heightBytes;        // +0x140 raw heightmap (4 bytes/cell)
    Video::ImageBuffer* _heightImage;        // +0x148 uploaded from +0x140 (deleted through slot 0 by ~WorldGeometry)
    Graphics::MaterialInstance* _waterMaterialA;    // +0x150 (TerrainGenerator triple 2)
    Graphics::MaterialInstance* _waterMaterialB;    // +0x158 (TerrainGenerator triple 3)
    Video::VertexBuffer _waterVertexBuffer;  // +0x160 water contour mesh
    Video::IndexBuffer  _waterIndexBuffer;   // +0x180
    // Nine Collection::Array<T> {data, size, capacity} headers, +0x1A0..+0x22F.
    // Raw fields (RawArray): ~WorldGeometry (0x1400aed50) frees each by hand, in
    // reverse order, and zeroes its {size, capacity} qword; a member destructor
    // would free them twice (H5).
    //
    // `_arrays` is the OLD spelling of the same nine headers, kept only because
    // files of other blocks still read it by name (geometry_draw.cpp,
    // action_pf_road.cpp, decalcomponent.cpp, boatcomponent_route.cpp,
    // statvaluedispatch.cpp); new code reads the named fields. The second pass
    // deletes `_arrays`, `Header` and the union.
    struct Header { void* data; uint64_t sizeCap; };
    union {
        Header _arrays[9];        // +0x1A0 (old spelling, see above)
        struct {
            RawArray<WaterVertex>    waterVertices;      // +0x1A0 contour vertex accumulator (b260 0x1400ab260)
            RawArray<uint16_t>       waterIndices;       // +0x1B0 contour index accumulator
            RawArray<WaterDrawRange> waterBodyRanges;    // +0x1C0 one per water body (draw mode 0x6000)
            RawArray<WaterDrawRange> waterFeatureRanges; // +0x1D0 one per tarn feature (draw mode 0x8000)
            RawArray<RawArray<Math::Vector2>> featurePaths; // +0x1E0 the map's feature polylines
                                                         //        (c4b0's tail; freed by 0x1400175d0)
            RawArray<TerrainMeshBatch> decals;           // +0x1F0 terrain decals (draw mode 0x2000; 0x1400b0d00)
            RawArray<TerrainMeshBatch> roadBatches;      // +0x200 road mesh batches (0x1400af370; mode 0x4000)
            RawArray<Graphics::MaterialInstance*> batchMaterials; // +0x210 one per batch (serialized by 0x140020150)
            RawArray<int32_t>        batchCounters;      // +0x220 per-batch cell counts (0x1400af370 phase 1)
        };
    };
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
static_assert(offsetof(WorldGeometry, _fieldC4) == 0xC4, "+0xC4");
static_assert(offsetof(WorldGeometry, _fieldCC) == 0xCC, "+0xCC");
static_assert(offsetof(WorldGeometry, _fieldD0) == 0xD0, "+0xD0");
static_assert(offsetof(WorldGeometry, _arrayD8Size) == 0xE0, "+0xE0");
static_assert(offsetof(WorldGeometry, _waterMaterialB) == 0x158, "+0x158");
static_assert(offsetof(WorldGeometry, _arrays) == 0x1A0, "+0x1A0");
static_assert(offsetof(WorldGeometry, waterVertices) == 0x1A0, "+0x1A0");
static_assert(offsetof(WorldGeometry, waterIndices) == 0x1B0, "+0x1B0");
static_assert(offsetof(WorldGeometry, waterBodyRanges) == 0x1C0, "+0x1C0");
static_assert(offsetof(WorldGeometry, waterFeatureRanges) == 0x1D0, "+0x1D0");
static_assert(offsetof(WorldGeometry, featurePaths) == 0x1E0, "+0x1E0");
static_assert(offsetof(WorldGeometry, decals) == 0x1F0, "+0x1F0");
static_assert(offsetof(WorldGeometry, roadBatches) == 0x200, "+0x200");
static_assert(offsetof(WorldGeometry, batchMaterials) == 0x210, "+0x210");
static_assert(offsetof(WorldGeometry, batchCounters) == 0x220, "+0x220");
