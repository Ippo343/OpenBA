#pragma once
// ============================================================================
// code/Game/Game/WorldGeometryElements.h — ENDGAME B.4 GAME-b
//
// The element records of WorldGeometry's nine {data, size|capacity} headers
// (WorldGeometry::_arrays, +0x1A0..+0x22F) and of the terrain vertex buffer.
// WorldGeometry.h (GAME-c) keeps the nine headers untyped (`Header`); until it
// splits them into typed arrays, they are read through nine named accessors
// (WorldGeometry::ContourVertices() .. BatchCounters(), geometry_draw.cpp), whose
// bodies are the only casts (LEFT: owner GAME-c).
//
// RETAIL (Application-x64.exe), the receivers settled from the callers, not the
// Ghidra labels: 0x1400b04d0 (Serialize, "TradeWork_impl_04d0"), 0x1400af1a0
// (SerializeLegacyBatches, "TradeWork_impl_f1a0"), 0x1400ac1e0 (BuildTerrainMesh,
// "TradeWork_impl_sub_c1e0") and 0x1400ac780 (UploadHeightImage,
// "TradeWork_impl_sub_c780") are WorldGeometry members: MenuScene's Serialize
// (0x1400a3630) builds +0x28 with MemAlloc(0x230) + 0x1400b00e0 (the
// WorldGeometry ctor) and calls 04d0 / f1a0 on it; the game's serialize
// (0x140095c72) passes GameUpdateLoop+0x110 (m_worldGeometry); c1e0 / c780 are
// called from 04d0 / 0x1400b0c50 / 0x1400ac4b0 on the same RCX.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/PathSearch.h"   // RawArray<T>

namespace Object { class External; }
namespace Math { class Vector2; }
namespace Graphics { class MaterialInstance; }

// One terrain vertex (stride 0x10): the master VertexBuffer at +0x100 and the
// contour vertices at +0x1A0 (0x1400ac880 writes it; Serialize: 4 shorts, 2 shorts,
// 4 bytes).
struct TerrainVertex {
    int16_t position[4];   // +0x00 x (col - grid/2), y (row - grid/2), height, 0x00FF
    int16_t uv[2];         // +0x08 col / grid * 16384, row / grid * 16384
    uint8_t normal[4];     // +0x0C (n + 1) * 127.5 per component, then 0
};
static_assert(sizeof(TerrainVertex) == 0x10, "TerrainVertex stride 0x10");
static_assert(offsetof(TerrainVertex, uv) == 0x08, "TerrainVertex::uv @ +0x08");
static_assert(offsetof(TerrainVertex, normal) == 0x0C, "TerrainVertex::normal @ +0x0C");

// One batch vertex (stride 0x14): a GeometryBatch's vertices (0x14001fdb0 copies
// it, 0x14001f9c0 serializes 4 shorts, 4 shorts, 4 bytes). ACT-a's RoadVertex is the
// same record.
struct GeometryBatchVertex {
    int16_t position[4];   // +0x00 x, y, height, blend
    int16_t uv[4];         // +0x08
    uint8_t normal[4];     // +0x10
};
static_assert(sizeof(GeometryBatchVertex) == 0x14, "GeometryBatchVertex stride 0x14");
static_assert(offsetof(GeometryBatchVertex, uv) == 0x08, "GeometryBatchVertex::uv @ +0x08");
static_assert(offsetof(GeometryBatchVertex, normal) == 0x10, "GeometryBatchVertex::normal @ +0x10");

// One mesh batch (stride 0x60): the decals at +0x1F0 (draw mode 0x2000) and the
// road meshes at +0x200 (mode 0x4000). Resize 0x14001f250, copy 0x14001f5e0,
// destroy 0x14001fb80, serialize 0x14001ffa0. ACT-a's RoadBatch is the same record.
struct GeometryBatch {
    Graphics::MaterialInstance*      material;     // +0x00 (0x140007140 serializes it; null = unused slot)
    RawArray<GeometryBatchVertex>    vertices;     // +0x08
    RawArray<uint16_t>               indices;      // +0x18
    int32_t  slot;                                 // +0x28 -1 = free
    int32_t  minX;                                 // +0x2C culling box, tile coords
    int32_t  minY;                                 // +0x30
    int32_t  maxX;                                 // +0x34
    int32_t  maxY;                                 // +0x38
    uint8_t  field_0x3C;                           // +0x3C (Serialize uchar)
    uint8_t  _pad3D[3];                            // +0x3D
    int32_t  field_0x40;                           // +0x40 (Serialize int)
    bool     field_0x44;                           // +0x44 (Serialize bool)
    uint8_t  _pad45[3];                            // +0x45
    float    field_0x48[5];                        // +0x48 .. +0x58 (Serialize float x5)
    bool     field_0x5C;                           // +0x5C (Serialize bool)
    bool     alternateKey;                         // +0x5D (version >= 2) decal draw key 3 instead of 1
    uint8_t  _pad5E[2];                            // +0x5E
};
static_assert(sizeof(GeometryBatch) == 0x60, "GeometryBatch stride 0x60");
static_assert(offsetof(GeometryBatch, vertices) == 0x08, "GeometryBatch::vertices @ +0x08");
static_assert(offsetof(GeometryBatch, indices) == 0x18, "GeometryBatch::indices @ +0x18");
static_assert(offsetof(GeometryBatch, slot) == 0x28, "GeometryBatch::slot @ +0x28");
static_assert(offsetof(GeometryBatch, minX) == 0x2C, "GeometryBatch::minX @ +0x2C");
static_assert(offsetof(GeometryBatch, maxY) == 0x38, "GeometryBatch::maxY @ +0x38");
static_assert(offsetof(GeometryBatch, field_0x3C) == 0x3C, "GeometryBatch +0x3C");
static_assert(offsetof(GeometryBatch, field_0x40) == 0x40, "GeometryBatch +0x40");
static_assert(offsetof(GeometryBatch, field_0x44) == 0x44, "GeometryBatch +0x44");
static_assert(offsetof(GeometryBatch, field_0x48) == 0x48, "GeometryBatch +0x48");
static_assert(offsetof(GeometryBatch, field_0x5C) == 0x5C, "GeometryBatch +0x5C");
static_assert(offsetof(GeometryBatch, alternateKey) == 0x5D, "GeometryBatch +0x5D");

// One contour draw range (stride 0x10): +0x1C0 (mode 0x6000) and +0x1D0 (mode
// 0x8000). Draw passes DrawIndexed(0, [1], [2], [0], [3], 1).
struct GeometryDrawRange {
    int32_t values[4];
};
static_assert(sizeof(GeometryDrawRange) == 0x10, "GeometryDrawRange stride 0x10");

namespace IO { class Serializer; }

// The batch helpers (serialize_struct60b.cpp / serialize_struct60b_serialize.cpp).
void GeometryBatch_Resize(RawArray<GeometryBatch>& array, unsigned int newCount);        // 0x14001f250
void GeometryBatch_CopyConstruct(GeometryBatch& dst, const GeometryBatch& src);        // 0x14001f5e0
void GeometryBatch_DestroyRange(GeometryBatch* first, int count);                      // 0x14001fb80
void GeometryBatch_Serialize(IO::Serializer& serializer, int count, GeometryBatch* batches);  // 0x14001ffa0
void SerializeMaterialRefs(IO::Serializer& serializer, int count, Object::External** refs);  // 0x140020150
