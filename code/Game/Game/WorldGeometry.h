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

#include "Game/WorldGeometryElements.h"   // RawArray<T> and the arrays' element types (B.4 GAME-c)

// B.4 GAME-b: forward declarations for the appended members.
struct AvlNode28;
class TerrainGenerator;       // Game/TerrainGenerator.h
struct TerrainHeightfield;    // Game/TerrainHeightfield.h
namespace Video { class ImageBuffer; }
namespace Graphics { class MaterialInstance; }
namespace IO { class Stream; }
namespace Object { class External; }

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
    // B.4 GAME-b (WorldGeometryElements.h; Serialize bodies in geometry_serialize.cpp,
    // the mesh / image helpers in serialize_heightmap_mesh.cpp / serialize_image_upload.cpp)
    void Serialize(IO::Stream& stream);                               // 0x1400b04d0
    void SerializeLegacyBatches(IO::Stream& stream);                  // 0x1400af1a0
    void BuildTerrainMesh();                                          // 0x1400ac1e0
    void UploadHeightImage();                                         // 0x1400ac780
    void FillTerrainVertices(int colStart, int rowStart, int width, int height,
                             TerrainVertex* out) const;               // 0x1400ac880
    Math::Vector3 TerrainNormal(int col, int row) const;              // 0x1400ae270
    // The nine array headers by element type (bodies in geometry_draw.cpp: they
    // return the named fields below, B.4 GAME-c).
    RawArray<TerrainVertex>&           ContourVertices();             // +0x1A0
    RawArray<uint16_t>&                ContourIndices();              // +0x1B0
    RawArray<GeometryDrawRange>&       ContourRangesA();              // +0x1C0 mode 0x6000
    RawArray<GeometryDrawRange>&       ContourRangesB();              // +0x1D0 mode 0x8000
    RawArray<RawArray<Math::Vector2>>& ContourOutlines();             // +0x1E0
    RawArray<GeometryBatch>&           Decals();                      // +0x1F0 mode 0x2000
    RawArray<GeometryBatch>&           RoadBatches();                 // +0x200 mode 0x4000
    RawArray<Object::External*>&       BatchMaterials();              // +0x210
    RawArray<int32_t>&                 BatchCounters();               // +0x220
    // B.4 GAME-c: the new-game terrain build (worldgen.cpp, water_contour_mesh.cpp)
    void BuildTerrain(TerrainGenerator* generator, int sizeIndex, uint32_t seed);  // 0x1400b0230
    void BuildFromHeightfield(TerrainHeightfield* heightfield);       // 0x1400ac4b0
    void BuildWaterMesh(TerrainHeightfield* heightfield);             // 0x1400ab260
    void BuildWaterBodies(const TerrainHeightfield* heightfield);     // 0x1400ab260 phase 1 (B5: split body)
    void BuildWaterShores(const TerrainHeightfield* heightfield);     // 0x1400ab260 phase 2


public:
    int32_t  _gridSize;           // +0xC0  heightmap edge (cells)
    int32_t  _fieldC4;            // +0xC4
    int32_t  _chunkSize;          // +0xC8  cells per chunk edge
    int32_t  _fieldCC;            // +0xCC
    int32_t  _fieldD0;            // +0xD0
    int32_t  _chunkIndexStride;   // +0xD4  index count per chunk (the ctor leaves it)
    int16_t* _arrayD8;            // +0xD8  {data, (height words: 0x14009e0e0 stores one; B.4 ACT-a H4)
    uint64_t _arrayD8Size;        // +0xE0   count|cap}
    AvlNode28* _dirtyTreeRoot;    // +0xE8  tree of dirty chunks (nodes: children +0x10/+0x18)
    int32_t  _dirtyTreeCount;     // +0xF0
    uint8_t  _padF4[4];           // +0xF4
    Graphics::MaterialInstance* _terrainMaterial; // +0xF8  MaterialInstance* for the chunks
    Video::VertexBuffer _vertexBuffer;       // +0x100 terrain chunk vertices
    Video::IndexBuffer  _indexBuffer;        // +0x120
    uint8_t* _heightBytes;        // +0x140 raw heightmap (4 bytes/cell)
    Video::ImageBuffer* _heightImage; // +0x148 Video::ImageBuffer* uploaded from +0x140
    Graphics::MaterialInstance* _waterMaterialA;  // +0x150
    Graphics::MaterialInstance* _waterMaterialB;  // +0x158
    Video::VertexBuffer _waterVertexBuffer;  // +0x160 water contour mesh
    Video::IndexBuffer  _waterIndexBuffer;   // +0x180
    // Nine Collection::Array<T> headers {data, size, capacity}, +0x1A0..+0x22F,
    // as raw fields (RawArray): ~WorldGeometry (0x1400aed50) frees each by hand,
    // in reverse order; a member destructor would free them twice (H5). The
    // accessors above return them.
    //
    // `_arrays` / `Header` are the OLD spelling of the same nine headers, kept in
    // the union only because files of other blocks still read them by name
    // (geometry_draw.cpp's destructor, action_pf_road.cpp, decalcomponent.cpp,
    // boatcomponent_route.cpp, statvaluedispatch.cpp, roadtool_calcpath.cpp); new
    // code reads the named fields. The second pass deletes them and the union.
    struct Header { void* data; uint64_t sizeCap; };
    union {
        Header _arrays[9];                              // +0x1A0 (old spelling)
        struct {
            RawArray<TerrainVertex>           contourVertices;  // +0x1A0 water vertex accumulator (b260 0x1400ab260)
            RawArray<uint16_t>                contourIndices;   // +0x1B0 water index accumulator
            RawArray<GeometryDrawRange>       contourRangesA;   // +0x1C0 one per water body (draw mode 0x6000)
            RawArray<GeometryDrawRange>       contourRangesB;   // +0x1D0 one per tarn feature (draw mode 0x8000)
            RawArray<RawArray<Math::Vector2>> contourOutlines;  // +0x1E0 the map's feature polylines
                                                                //        (0x1400ac4b0's tail; freed by 0x1400175d0)
            RawArray<GeometryBatch>           decals;           // +0x1F0 draw mode 0x2000 (0x1400b0d00)
            RawArray<GeometryBatch>           roadBatches;      // +0x200 draw mode 0x4000 (0x1400af370)
            RawArray<Object::External*>       batchMaterials;   // +0x210 serialized by 0x140020150
            RawArray<int32_t>                 batchCounters;    // +0x220 (0x1400af370 phase 1)
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
static_assert(offsetof(WorldGeometry, _arrays) == 0x1A0, "+0x1A0");
static_assert(offsetof(WorldGeometry, contourVertices) == 0x1A0, "+0x1A0");
static_assert(offsetof(WorldGeometry, contourIndices) == 0x1B0, "+0x1B0");
static_assert(offsetof(WorldGeometry, contourRangesA) == 0x1C0, "+0x1C0");
static_assert(offsetof(WorldGeometry, contourRangesB) == 0x1D0, "+0x1D0");
static_assert(offsetof(WorldGeometry, contourOutlines) == 0x1E0, "+0x1E0");
static_assert(offsetof(WorldGeometry, decals) == 0x1F0, "+0x1F0");
static_assert(offsetof(WorldGeometry, roadBatches) == 0x200, "+0x200");
static_assert(offsetof(WorldGeometry, batchMaterials) == 0x210, "+0x210");
static_assert(offsetof(WorldGeometry, batchCounters) == 0x220, "+0x220");
static_assert(offsetof(WorldGeometry, _fieldC4) == 0xC4, "+0xC4");
static_assert(offsetof(WorldGeometry, _fieldCC) == 0xCC, "+0xCC");
static_assert(offsetof(WorldGeometry, _fieldD0) == 0xD0, "+0xD0");
static_assert(offsetof(WorldGeometry, _arrayD8Size) == 0xE0, "+0xE0");
static_assert(offsetof(WorldGeometry, _waterMaterialB) == 0x158, "+0x158");
