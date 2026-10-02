// code/Game/Game/serialize_heightmap_mesh.cpp
//
// M3.5 Path B session 6 — heightmap mesh-build path.
//
// B.4 GAME-b: the receiver is WorldGeometry (settled from retail's callers, see
// WorldGeometryElements.h): BuildTerrainMesh (0x1400ac1e0), FillTerrainVertices
// (0x1400ac880) and TerrainNormal (0x1400ae270) are members reading its fields;
// the vertex is TerrainVertex. The standalone flat water sheet
// (WorldGen_BuildWaterMesh) had no caller left since the b260 contour mesh and
// went with this change.
//
// Three exe-internal helpers + the Runtime DX wiring that constructs the
// renderable terrain mesh from the deserialized heightmap raw bytes:
//
//   sub_c1e0   @ 0x1400ac1e0  — TradeWork_impl_sub_c1e0 (mesh builder)
//                                Allocates Video::VertexBuffer at +0x100,
//                                walks the grid filling vertex blocks per cell,
//                                builds index buffer, allocates Video::IndexBuffer at +0x120.
//   FUN_1400ac880               — per-cell vertex compute (writes 16 bytes/vert).
//   helper_e270 @ 0x1400ae270   — vertex position (normal) compute via heightmap deltas.
//
// All three are invoked from `TradeWork_impl_04d0` (Geometry::Serialize body)
// step 5 in M35_geom_serialize_findings.md. They are NOT yet wired into
// Geometry::Serialize itself (that's session 8). The goal of session 6 is
// just that the helpers exist + are callable + standalone build links cleanly.
//
// Math constants (verified via Ghidra `inspect_memory_content`):
//   g_fUVScale       @ 0x14017214c = 0x3F800000 = 1.0f
//   DAT_140172214    @ 0x140172214 = 0x42FF0000 = 127.5f   (byte-encoding scale)
//   DAT_140172264    @ 0x140172264 = 0x46800000 = 16384.0f (UV-to-short scale)
//   DAT_140172270    @ 0x140172270 = 0xBB800000 = -1/256.0 (normal delta scale)
//   DAT_140172174    @ 0x140172174 = 0x40000000 = 2.0f     (default normal Z)
//
// Vertex layout written by FUN_1400ac880 (16 bytes per vertex):
//   +0x00 : int16   x          (col - grid/2)
//   +0x02 : int16   z          (row - grid/2)
//   +0x04 : int16   height     (heightmap sample at (col,row))
//   +0x06 : int16   uv0?       (== 0xff 0x00 — see comment below)
//   +0x08 : int16   uv1        (col / grid * UVScaleShort)
//   +0x0a : int16   uv2        (row / grid * UVScaleShort)
//   +0x0c : byte    nx         (normal.x scaled to [0,255])
//   +0x0d : byte    ny         (normal.y scaled to [0,255])
//   +0x0e : byte    nz         (normal.z scaled to [0,255])
//   +0x0f : byte    pad/zero
// (The original disasm writes these in a fairly idiosyncratic order — see
//  fAUC_1400ac880 below for line-for-line decompile.)

// ENDGAME B.3.5 Video/Graphics caller batch: Video::{Vertex,Index}Buffer are
// real classes. Ahead of "resolve.h" — resolve.h pulls <windows.h>, whose
// <wingdi.h> declares a global `Rectangle` FUNCTION that hides
// `Volume::Rectangle`'s class name inside Volume.h's own static_asserts.
#include "video/Buffers.h"
#include "WorldGeometry.h"   // caller batch CB6: RebuildDirtyChunks is the class's
#include <atomic>
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Game/exe_bodies.h"
#include "Game/WorldGeometryElements.h"
#include "Action/PathAvl.h"
#include "math/Math.h"

namespace {

// ============================================================
// Embedded math constants
// ============================================================
//
// We embed these inline rather than resolving the exe addresses at runtime —
// they're true compile-time constants (the exe stores them in .rdata as
// read-only float literals). Documented exe addresses preserved in comments
// for future cross-reference.
constexpr float kUVScale         = 1.0f;        // 0x14017214c
constexpr float kByteScale       = 127.5f;      // 0x140172214
constexpr float kUVToShortScale  = 16384.0f;    // 0x140172264
constexpr float kNormalDeltaStep = -1.0f / 256.0f; // 0x140172270 (-0.00390625f)
constexpr float kNormalDefaultZ  = 2.0f;        // 0x140172174

// ============================================================
// ENDGAME B.3.5 Video/Graphics caller batch — the four buffer doors are gone
// ============================================================
//
// This file used to reach `Video::{Vertex,Index}Buffer::{Create,Set}` through
// four file-local caches, bound first from the exe's .rdata dispatch slots
// (0x140152668 / 0x140152690 / 0x140152660 / 0x140152698) and then, on a miss,
// from the mangled exports
//   ?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z
//   ?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z
//   ?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z
//   ?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z
// All four are real members of block D's `Video::VertexBuffer` /
// `Video::IndexBuffer` (runtime/types/video/Buffers.h, bodies in
// runtime/src/video/buffers.cpp), so every call below is a direct member call
// and the caches, their typedefs, the `loadSlot` lambda, the latch and the
// three door-readiness guards went with the last caller.
//
// The two `int` arguments of `Set` are passed in exactly the order the door took
// them. This file's old typedef named them `(sizeBytes, byteOffset)` while
// Buffers.h named them `(offset, count)`; ENDGAME B.3.5's closing pass settled it
// from the retail bytes -- the three-instruction trampoline at 0x1800f38a0 passes
// RDX and R8 through untouched to `{Index,Vertex}BufferData` slot 2, whose
// parameters are `(sizeBytes, offsetBytes, src)`, and dx11's port of that slot
// builds `box.left = offsetBytes`, `box.right = sizeBytes + offsetBytes`,
// `srcRowPitch = sizeBytes`. THIS FILE WAS RIGHT; Buffers.h's names were wrong and
// are now fixed. No argument order changed anywhere.

} // namespace

// ============================================================
// 0x1400ae270 — WorldGeometry::TerrainNormal (vertex normal from the heightmap)
// ============================================================
//
// Reads the int16 heightmap (_arrayD8, row stride _fieldD0 = gridLevels) around
// (col, row), computes a normal from the discrete x/y deltas and normalizes it.
//
// Decompile:
//   nx = (h(col+1,row) - h(col-1,row)) * (-1/256)
//   ny = (h(col,row+1) - h(col,row-1)) * (-1/256)
//   nz = 2.0
//   Math::Vector3::Normalize(...)
//
// Coordinate clamping: each index is clamped to [0, gridLevels-1] independently.
Math::Vector3 WorldGeometry::TerrainNormal(int col, int row) const {
    int gridLevels = _fieldD0;
    // Degenerate — 0,0,2 unnormalized to avoid div-by-zero in Normalize.
    if (gridLevels <= 0) return Math::Vector3(0.0f, 0.0f, kNormalDefaultZ);
    const int16_t* heightmap = _arrayD8;
    if (!heightmap) return Math::Vector3(0.0f, 0.0f, kNormalDefaultZ);

    auto clamp = [&](int v) -> int {
        if (v < 0) return 0;
        if (v >= gridLevels) return gridLevels - 1;
        return v;
    };
    auto sample = [&](int c, int r) -> int {
        return static_cast<int>(heightmap[static_cast<size_t>(clamp(r)) * gridLevels + clamp(c)]);
    };

    // Mirrors disasm:
    //   x-delta: h(col+1, row) - h(col-1, row)  (clamped)
    //   y-delta: h(col, row+1) - h(col, row-1)  (clamped)
    int dx = sample(col + 1, row) - sample(col - 1, row);
    int dy = sample(col, row + 1) - sample(col, row - 1);

    float nx = static_cast<float>(dx) * kNormalDeltaStep;
    float ny = static_cast<float>(dy) * kNormalDeltaStep;
    float nz = kNormalDefaultZ;

    // Normalize IN PLACE (the original e270 ends with Math::Vector3::Normalize on
    // the local vector, mutating it). The vertex writer ac880 byte-encodes each
    // component as (n + 1.0) * 127.5 into a UNORM8; with nz left at the raw 2.0
    // every terrain normal decodes flat/sideways (RenderDoc A/B vs retail: retail
    // nz byte ~200-249, ours was 126). Plain C++, not Math::Vector3::Normalize: that
    // is the by-value (const) overload — it returns a normalized COPY.
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) {
        float inv = 1.0f / len;
        return Math::Vector3(nx * inv, ny * inv, nz * inv);
    }
    // Degenerate (perfectly flat): straight up.
    return Math::Vector3(0.0f, 0.0f, 1.0f);
}

// ============================================================
// 0x1400ac880 — WorldGeometry::FillTerrainVertices (one chunk's vertex block)
// ============================================================
//
// Fills `width * height` TerrainVertex records for the block (colStart..,
// rowStart..); called once per chunk from BuildTerrainMesh / RebuildDirtyChunks,
// the block then uploaded into its slice of the master VertexBuffer.
//
// Per vertex:
//   position = { col - grid/2, row - grid/2, heightmap[row][col] (clamped), 0x00FF }
//   uv       = { col / grid * 16384, row / grid * 16384 }
//   normal   = { (nx + 1) * 127.5, (ny + 1) * 127.5, (nz + 1) * 127.5, 0 }
void WorldGeometry::FillTerrainVertices(int colStart, int rowStart, int width, int height,
                                        TerrainVertex* out) const {
    if (!out) return;

    int rowEnd = rowStart + height;
    int colEnd = colStart + width;

    int gridSize   = _gridSize;
    int gridLevels = _fieldD0;
    const int16_t* heightmap = _arrayD8;

    if (!heightmap) {
        // Without a heightmap, the loop would deref garbage. Zero the block
        // and bail — the caller will still produce a flat plane.
        size_t totalVerts = static_cast<size_t>(width) * height;
        for (size_t i = 0; i < totalVerts; ++i) out[i] = TerrainVertex{};
        return;
    }

    int16_t halfGrid = static_cast<int16_t>(gridSize >> 1);
    TerrainVertex* v = out;

    for (int row = rowStart; row < rowEnd; ++row) {
        for (int col = colStart; col < colEnd; ++col) {
            // (1) The normal (0x1400ae270).
            const Math::Vector3 normal = TerrainNormal(col, row);

            // (2) Position xy (relative to grid centre).
            v->position[0] = static_cast<int16_t>(static_cast<int16_t>(col) - halfGrid);
            v->position[1] = static_cast<int16_t>(static_cast<int16_t>(row) - halfGrid);

            // (3) Heightmap sample (clamped col/row).
            int cc = col;
            if (cc < 0) cc = 0;
            if (cc >= gridLevels) cc = gridLevels - 1;
            int rr = row;
            if (rr < 0) rr = 0;
            if (rr >= gridLevels) rr = gridLevels - 1;
            v->position[2] = heightmap[static_cast<size_t>(rr) * gridLevels + cc];

            // (4) word[3] = 0xff.
            v->position[3] = static_cast<int16_t>(0x00ff);

            // (5) UV (col/gridSize * UVToShortScale, row/gridSize * UVToShortScale).
            float gridSizeF = static_cast<float>(gridSize);
            float uvCol = (gridSizeF != 0.0f)
                            ? (static_cast<float>(col) / gridSizeF) * kUVToShortScale
                            : 0.0f;
            float uvRow = (gridSizeF != 0.0f)
                            ? (static_cast<float>(row) / gridSizeF) * kUVToShortScale
                            : 0.0f;
            v->uv[0] = static_cast<int16_t>(static_cast<int>(uvCol));
            v->uv[1] = static_cast<int16_t>(static_cast<int>(uvRow));

            // (6) Normal bytes — byte[0xc], [0xd], [0xe], then [0xf] = 0.
            v->normal[0] = static_cast<uint8_t>(static_cast<int>((normal.x + kUVScale) * kByteScale));
            v->normal[1] = static_cast<uint8_t>(static_cast<int>((normal.y + kUVScale) * kByteScale));
            v->normal[2] = static_cast<uint8_t>(static_cast<int>((normal.z + kUVScale) * kByteScale));
            v->normal[3] = 0;

            ++v;
        }
    }
}

// ============================================================
// 0x1400ac1e0 — WorldGeometry::BuildTerrainMesh
// ============================================================
//
// Builds the vertex buffer + index buffer for the entire terrain mesh. Walks the
// chunk grid (chunksPerSide x chunksPerSide), fills each chunk's vertex block
// (0x1400ac880), uploads it to the VertexBuffer, then builds a single per-chunk
// triangle-list index buffer with checkerboard winding (every chunk shares it).
//
// Fields:
//   _gridSize (+0xC0)          full heightmap edge in cells (e.g. 768)
//   _chunkSize (+0xC8)         per-chunk cell edge (e.g. 32): the divisor for
//                              chunksPerSide, the chunk stride and the IB loop bound
//   _fieldCC (+0xCC)           vertsPerSide = chunkSize + 1 (e.g. 33): the vertex
//                              block edge and the row stride of the index ids
//   _chunkIndexStride (+0xD4)  vertsPerChunk = vertsPerSide^2 (e.g. 1089)
//   _vertexBuffer (+0x100), _indexBuffer (+0x120)
//
// chunksPerSide = gridSize / chunkSize is computed locally — NOT a stored field.
void WorldGeometry::BuildTerrainMesh() {
    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c1e0] MemAlloc/Free unavailable — skipping mesh build (self=%p)\n", (void*)this);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    int gridSize       = _gridSize;
    int chunkSize      = _chunkSize;
    int vertsPerSide   = _fieldCC;
    int vertsPerChunk  = _chunkIndexStride;
    int chunksPerSide  = (chunkSize > 0) ? (gridSize / chunkSize) : 0;

    if (chunksPerSide <= 0 || chunkSize <= 0 || vertsPerSide <= 0 || vertsPerChunk <= 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c1e0] zero/negative grid params (gridSize=%d chunkSize=%d "
                   "vertsPerSide=%d vertsPerChunk=%d chunksPerSide=%d) — skipping\n",
                   gridSize, chunkSize, vertsPerSide, vertsPerChunk, chunksPerSide);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // (1) The per-chunk scratch vertex array (the stride-0x10 resize, 0x14001ef10).
    RawArray<TerrainVertex> scratch{nullptr, 0, 0};
    Reimpl_Array_Resize_Stride16B(&scratch, vertsPerChunk);
    TerrainVertex* vertScratch = scratch.data;

    // (2) The master VertexBuffer: vertsPerChunk * chunksPerSide^2 * 16 bytes.
    int totalVerts  = vertsPerChunk * chunksPerSide * chunksPerSide;
    int totalVbSize = totalVerts * 0x10;
    bool createOk = _vertexBuffer.Create(totalVbSize, /*dynamic=*/false, /*initialData=*/nullptr);

    printf("[sub_c1e0] entry self=%p gridSize=%d chunkSize=%d chunksPerSide=%d "
           "vertsPerSide=%d vertsPerChunk=%d totalVbSize=%d (0x%x) createOk=%d\n",
           (void*)this, gridSize, chunkSize, chunksPerSide, vertsPerSide, vertsPerChunk,
           totalVbSize, totalVbSize, createOk);
    fflush(stdout);

    // (3) Walk the chunk grid, filling each chunk and uploading it.
    int firstSetReturned = -1;
    if (vertScratch) {
        for (int chunkRow = 0; chunkRow < chunksPerSide; ++chunkRow) {
            for (int chunkCol = 0; chunkCol < chunksPerSide; ++chunkCol) {
                int colStart = chunkSize * chunkCol;
                int rowStart = chunkSize * chunkRow;

                FillTerrainVertices(colStart, rowStart, vertsPerSide, vertsPerSide, vertScratch);

                int byteOffset = (chunkRow * chunksPerSide + chunkCol) * vertsPerChunk * 0x10;
                int byteSize   = vertsPerChunk * 0x10;
                bool ok = _vertexBuffer.Set(byteSize, byteOffset, vertScratch);

                if (chunkRow == 0 && chunkCol == 0) {
                    firstSetReturned = ok ? 1 : 0;
                    // The first vertex (should be (-gridSize/2, -gridSize/2, h, 0xFF)).
                    [[maybe_unused]] const TerrainVertex& v0 = vertScratch[0];
                    printf("[sub_c1e0] chunk(0,0) scratch v0: x=%d y=%d h=%d w3=%d uvU=%d uvV=%d "
                           "n_xyzpad=%02x %02x %02x %02x  Set->%d  byteOff=%d byteSz=%d\n",
                           v0.position[0], v0.position[1], v0.position[2], v0.position[3],
                           v0.uv[0], v0.uv[1],
                           v0.normal[0], v0.normal[1], v0.normal[2], v0.normal[3],
                           ok ? 1 : 0, byteOffset, byteSize);
                    fflush(stdout);
                }
            }
        }
    }
    printf("[sub_c1e0] cell loop complete; first Set returned %d\n", firstSetReturned);
    fflush(stdout);

    // (4) The index buffer: ONE chunk's topology (chunkSize x chunkSize quads),
    //     shared by every chunk; vertex ids `row * vertsPerSide + col`.
    int quadsPerChunk = chunkSize * chunkSize;
    int idxCount      = quadsPerChunk * 6;
    int idxBytes      = quadsPerChunk * 12;

    int16_t* idxBuf = nullptr;
    if (idxCount > 0) {
        idxBuf = static_cast<int16_t*>(
            System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(idxCount) * 2, 2));
    }

    // The original disasm has a "self-copy initialise" loop that runs before the
    // real indices are written; it compares against a negative loop iterator, so
    // it never executes. We skip emulating this dead branch.

    // (5) The master IndexBuffer (idxBytes total).
    _indexBuffer.Create(idxBytes, /*dynamic=*/false, /*initialData=*/nullptr);

    // (6) Fill the index buffer — checkerboard quad winding, two triangles per
    //     quad, alternating diagonal direction by parity.
    if (idxBuf && idxCount > 0) {
        int idx = 0;
        for (int row = 0; row < chunkSize; ++row) {
            for (int col = 0; col < chunkSize; ++col) {
                int16_t v_tl = static_cast<int16_t>(
                    static_cast<int16_t>(row) * static_cast<int16_t>(vertsPerSide) +
                    static_cast<int16_t>(col));            // top-left
                int16_t v_tr = static_cast<int16_t>(v_tl + 1); // top-right
                int16_t v_bl = static_cast<int16_t>(
                    (static_cast<int16_t>(row) + 1) * static_cast<int16_t>(vertsPerSide) +
                    static_cast<int16_t>(col));            // bottom-left
                int16_t v_br = static_cast<int16_t>(v_bl + 1); // bottom-right

                idxBuf[idx + 0] = v_tl;
                if (((col + row) & 1) == 0) {
                    // Diagonal TL-BR
                    idxBuf[idx + 1] = v_bl;
                    idxBuf[idx + 2] = v_tr;
                    idxBuf[idx + 3] = v_tr;
                } else {
                    // Diagonal TR-BL (mirror)
                    idxBuf[idx + 1] = v_br;  // == v_bl + 1
                    idxBuf[idx + 2] = v_tr;
                    idxBuf[idx + 3] = v_tl;
                }
                idxBuf[idx + 4] = v_bl;
                idxBuf[idx + 5] = v_br;       // == v_bl + 1

                idx += 6;
            }
        }

        // (7) Upload the indices.
        bool ibSetOk = _indexBuffer.Set(idxBytes, 0, idxBuf);
        printf("[sub_c1e0] IB Set returned %d (idxBytes=%d quadsPerChunk=%d)\n",
               ibSetOk ? 1 : 0, idxBytes, quadsPerChunk);
        fflush(stdout);
    }

    // (8) Free the scratch buffers (the original's MemFree(idxBuf);
    //     MemFree(scratch.data) at the end of sub_c1e0).
    if (idxBuf) {
        System::MemoryAllocator::MemFree(idxBuf);
    }
    if (scratch.data) {
        System::MemoryAllocator::MemFree(scratch.data);
    }
}

// ============================================================
// Chunk-streaming — FUN_1400AAD50 (per-frame dirty-chunk upload)
// ============================================================
//
// Vtable +0x08 of WorldGeometry (slot 1 0x1400aed40 is a JMP to it). Walks the
// AVL tree of dirty chunk indices at _dirtyTreeRoot (+0xE8; nodes AvlNode28:
// key = the linear chunk index row*chunksPerSide + col, parent +0x08, left
// +0x10, right +0x18), regenerates each chunk's vertices (0x1400ac880) and
// uploads them into the master VertexBuffer.
//
// The original also re-meshes the decals at +0x1F0 whose box overlaps a rebuilt
// chunk (0x1400ace40); omitted, see the TODO at the end.
namespace { std::atomic<bool> s_chunkUpdateWarned{false}; }

void WorldGeometry::RebuildDirtyChunks() {
    // +0xF0 is the "dirty tree non-empty" count. The original tests this first.
    if (_dirtyTreeCount == 0) return;

    if (!System::MemoryAllocator::_singleton) {
        if (!s_chunkUpdateWarned.exchange(true)) {
            printf("[GeometryChunkUpdate] MemAlloc/Free unavailable — skipping\n");
            fflush(stdout);
        }
        return;
    }

    int gridSize       = _gridSize;           // +0xC0
    int chunkSize      = _chunkSize;          // +0xC8
    int vertsPerSide   = _fieldCC;            // +0xCC
    int vertsPerChunk  = _chunkIndexStride;   // +0xD4
    int chunksPerSide  = (chunkSize > 0) ? (gridSize / chunkSize) : 0;

    if (chunksPerSide <= 0 || vertsPerChunk <= 0) return;

    // The per-chunk scratch vertex array (16-byte stride).
    RawArray<TerrainVertex> scratch{nullptr, 0, 0};
    Reimpl_Array_Resize_Stride16B(&scratch, vertsPerChunk);
    TerrainVertex* vertScratch = scratch.data;
    if (!vertScratch) return;

    AvlNode28* treeRoot = _dirtyTreeRoot;     // +0xE8

    auto upload = [&](const AvlNode28* node) {
        int chunkIdx = node->key;
        int row = chunkIdx / chunksPerSide;
        int col = chunkIdx % chunksPerSide;

        // Generate this chunk's vertices into the scratch buffer, then upload.
        FillTerrainVertices(col * chunkSize, row * chunkSize, vertsPerSide, vertsPerSide, vertScratch);

        int byteSize   = vertsPerChunk * 0x10;
        int byteOffset = (row * chunksPerSide + col) * vertsPerChunk * 0x10;
        _vertexBuffer.Set(byteSize, byteOffset, vertScratch);   // +0x100
    };

    // Iterative in-order walk using the parent links to ascend.
    if (treeRoot) {
        AvlNode28* node = treeRoot;
        while (node->left) node = node->left;   // leftmost

        while (node) {
            upload(node);

            // The in-order successor.
            if (node->right) {
                node = node->right;
                while (node->left) node = node->left;
            } else {
                // Walk up while we're the right child of our parent.
                while (true) {
                    AvlNode28* parent = node->parent;
                    if (!parent) { node = nullptr; break; }
                    if (parent->right != node) {
                        node = parent;
                        break;
                    }
                    node = parent;
                }
            }
        }
    }

    // Free the dirty tree (post-order via FUN_140014c30 in the original —
    // "free children, then self").
    auto freeNode = [&](auto&& self_, AvlNode28* node) -> void {
        if (!node) return;
        self_(self_, node->left);
        self_(self_, node->right);
        System::MemoryAllocator::MemFree(node);
    };
    freeNode(freeNode, treeRoot);

    _dirtyTreeRoot = nullptr;    // +0xE8
    _dirtyTreeCount = 0;         // +0xF0

    if (scratch.data) {
        System::MemoryAllocator::MemFree(scratch.data);
    }

    // TODO (in-game): the original also walks the decals at +0x1F0 for each
    // rebuilt chunk and re-meshes any whose box overlaps the chunk's region
    // (FUN_1400ace40). Skipped here (the menu scene has none).
}


// ============================================================
// The C entry points
// ============================================================
// B4 FORWARD: 1 outside caller (worldgen.cpp, GAME-c, holds the geometry as
// void*). LEFT: that void*.
extern "C" void Reimpl_TradeWork_sub_c1e0(void* self) {
    if (!self) return;
    reinterpret_cast<WorldGeometry*>(self)->BuildTerrainMesh();
}

// B4 FORWARD: 1 outside caller (Action/action_pf_road.cpp, ACT-a: a float[3]).
extern "C" void* Reimpl_TradeWork_helper_e270(const WorldGeometry* self, float* out, int col, int row) {
    if (!self || !out) return out;
    const Math::Vector3 n = self->TerrainNormal(col, row);
    out[0] = n.x;
    out[1] = n.y;
    out[2] = n.z;
    return out;
}
