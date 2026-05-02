// code/Game/Game/serialize_heightmap_mesh.cpp
//
// M3.5 Path B session 6 — heightmap mesh-build path.
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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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
// Late-bound IAT slot resolution (Runtime DLL function pointers)
// ============================================================
//
// The original exe calls go through .rdata dispatch slots in the
// 0x14015xxxx range, which are filled at startup by the Runtime DLL's name
// resolution machinery. In standalone we either pre-fill those slots
// (RegisterDispatchSlot in address_registry.cpp) or resolve fresh per-call
// via mangled-name export lookup. We follow the moveto.cpp pattern: cache
// once on first use, log a one-shot warning on miss.

typedef bool (*VertexBufferCreate_t)(void* self, int sizeBytes, bool dynamic, void* initialData);
typedef bool (*VertexBufferSet_t   )(void* self, int sizeBytes, int byteOffset, void* data);
typedef bool (*IndexBufferCreate_t )(void* self, int sizeBytes, bool dynamic, void* initialData);
typedef bool (*IndexBufferSet_t    )(void* self, int sizeBytes, int byteOffset, void* data);

// Vector3::Normalize uses MSVC hidden-retval ABI: RCX=this, RDX=retbuf.
typedef void* (*Vector3Normalize_t )(void* self, void* retbuf);

VertexBufferCreate_t s_VertexBufferCreate = nullptr;
VertexBufferSet_t    s_VertexBufferSet    = nullptr;
IndexBufferCreate_t  s_IndexBufferCreate  = nullptr;
IndexBufferSet_t     s_IndexBufferSet     = nullptr;
Vector3Normalize_t   s_Vector3Normalize   = nullptr;
bool s_runtimeResolved = false;

void ResolveRuntimePtrs() {
    if (s_runtimeResolved) return;

    // Try IAT dispatch slots first (filled by RegisterDispatchSlot in standalone,
    // or by the Runtime DLL loader in DLL mode).
    auto loadSlot = [](uintptr_t slotAddr) -> void* {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, slotAddr));
        return slot ? *slot : nullptr;
    };

    s_VertexBufferCreate = reinterpret_cast<VertexBufferCreate_t>(loadSlot(0x140152668));
    s_VertexBufferSet    = reinterpret_cast<VertexBufferSet_t   >(loadSlot(0x140152690));
    s_IndexBufferCreate  = reinterpret_cast<IndexBufferCreate_t >(loadSlot(0x140152660));
    s_IndexBufferSet     = reinterpret_cast<IndexBufferSet_t    >(loadSlot(0x140152698));
    s_Vector3Normalize   = reinterpret_cast<Vector3Normalize_t  >(loadSlot(0x1401512c0));

    // Fallback: resolve directly from Runtime DLL exports via mangled names.
    if (!s_VertexBufferCreate) {
        s_VertexBufferCreate = reinterpret_cast<VertexBufferCreate_t>(
            ResolveExport("?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    }
    if (!s_VertexBufferSet) {
        s_VertexBufferSet = reinterpret_cast<VertexBufferSet_t>(
            ResolveExport("?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z"));
    }
    if (!s_IndexBufferCreate) {
        s_IndexBufferCreate = reinterpret_cast<IndexBufferCreate_t>(
            ResolveExport("?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    }
    if (!s_IndexBufferSet) {
        s_IndexBufferSet = reinterpret_cast<IndexBufferSet_t>(
            ResolveExport("?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z"));
    }
    if (!s_Vector3Normalize) {
        s_Vector3Normalize = reinterpret_cast<Vector3Normalize_t>(
            ResolveExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ"));
    }

    s_runtimeResolved = true;
}

// ============================================================
// Helper C — TradeWork_helper_e270 (vertex normal compute)
// ============================================================
//
// Reads heightmap samples around (col, row) in the heightmap buffer at
// self+0xd8 (gridLevels^2 grid at self+0xd0), computes a Vector3 normal from
// the discrete x/y deltas of the heightmap, and Normalize()s in place.
//
// param_1 (self) : Geometry container (RCX)
// param_2 (out)  : pointer to a 12-byte Vector3 in stack memory (RDX, hidden retval)
// param_3 (col)  : int (R8)
// param_4 (row)  : int (R9)
//
// Decompile:
//   nx = (h(col+1,row) - h(col-1,row)) * (-1/256)
//   ny = (h(col,row+1) - h(col,row-1)) * (-1/256)
//   nz = 2.0
//   *(Vector3*)param_2 = { nx, ny, nz }
//   Math::Vector3::Normalize(param_2);   // hidden-retval discarded — modifies via this
//
// Coordinate clamping: each index is clamped to [0, gridLevels-1] independently.
extern "C" void* TradeWork_helper_e270_Body(void* self, void* retbuf, int col, int row) {
    if (!self || !retbuf) return retbuf;

    int gridLevels = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0xd0);
    if (gridLevels <= 0) {
        // Degenerate — store 0,0,2 unnormalized to avoid div-by-zero in Normalize.
        float* v = reinterpret_cast<float*>(retbuf);
        v[0] = 0.0f; v[1] = 0.0f; v[2] = kNormalDefaultZ;
        return retbuf;
    }

    int16_t* heightmap = *reinterpret_cast<int16_t**>(reinterpret_cast<uint8_t*>(self) + 0xd8);
    if (!heightmap) {
        float* v = reinterpret_cast<float*>(retbuf);
        v[0] = 0.0f; v[1] = 0.0f; v[2] = kNormalDefaultZ;
        return retbuf;
    }

    auto clamp = [&](int v) -> int {
        if (v < 0) return 0;
        if (v >= gridLevels) return gridLevels - 1;
        return v;
    };

    auto sample = [&](int c, int r) -> int {
        int cc = clamp(c);
        int rr = clamp(r);
        // heightmap[r * gridLevels + c]
        return static_cast<int>(heightmap[static_cast<size_t>(rr) * gridLevels + cc]);
    };

    // Mirrors disasm:
    //   x-delta: h(col+1, row) - h(col-1, row)  (clamped)
    //   y-delta: h(col, row+1) - h(col, row-1)  (clamped)
    int dx = sample(col + 1, row) - sample(col - 1, row);
    int dy = sample(col, row + 1) - sample(col, row - 1);

    float nx = static_cast<float>(dx) * kNormalDeltaStep;
    float ny = static_cast<float>(dy) * kNormalDeltaStep;
    float nz = kNormalDefaultZ;

    float* out = reinterpret_cast<float*>(retbuf);
    out[0] = nx;
    out[1] = ny;
    out[2] = nz;

    ResolveRuntimePtrs();
    if (s_Vector3Normalize) {
        // Hidden-retval ABI: RCX=this (retbuf), RDX=retbuf2 (junk; original
        // discards return value). The slot at 0x1401512c0 is the by-value
        // Normalize variant, but we just want the in-place side-effect.
        // Pass a small stack buffer for the discarded return.
        uint8_t junk[16] = {0};
        s_Vector3Normalize(retbuf, junk);
    }
    // If unresolved, leave (nx, ny, nz) un-normalized — slightly wrong shading
    // but mesh still renders. One-shot warning emitted by ResolveRuntimePtrs's
    // chain (the helper-A path will already have logged a Create failure).

    return retbuf;
}

// ============================================================
// Helper B — FUN_1400ac880 (per-cell vertex block compute)
// ============================================================
//
// Fills a buffer of `cellSize * cellSize` 16-byte vertices for the rectangular
// block (colStart..colStart+cellSize, rowStart..rowStart+cellSize). Called
// once per master grid cell from sub_c1e0; the resulting block is uploaded
// into the appropriate slice of the master VertexBuffer.
//
// Original signature (param ordering, R8 = third int arg etc.):
//   void FUN_1400ac880(self, colStart, rowStart, cellSize_x, cellSize_y, vertOut)
//
// param_1 (RCX, self)        : Geometry container
// param_2 (RDX, colStart)    : starting column in master grid
// param_3 (R8 , rowStart)    : starting row in master grid
// param_4 (R9 , cellSize_x)  : block width in cells
// param_5 (stack, cellSize_y): block height in cells (equal to _x in practice)
// param_6 (stack, vertOut)   : 16-byte-stride output buffer
//
// Per-vertex (`*psVar10` is the running pointer, advances 8 shorts = 16 bytes):
//   psVar10[0] = (short)col - (gridSize >> 1)
//   psVar10[1] = (short)row - (gridSize >> 1)
//   psVar10[2] = heightmap[row * gridLevels + col]   (via clamped col/row)
//   psVar10[3] = 0xff                                (Pre-byte: alpha or pad)
//   psVar10[4] = (short)((col / gridSize) * UVToShortScale)
//   psVar10[5] = (short)((row / gridSize) * UVToShortScale)
//   psVar10[6] (low byte ) = (char)((nx + UVScale) * ByteScale)  (offset 0xc)
//   psVar10[6] (high byte) = (char)((ny + UVScale) * ByteScale)  (offset 0xd)
//   psVar10[7] (low byte ) = (char)((nz + UVScale) * ByteScale)  (offset 0xe)
//   *(byte*)(psVar10 + 0xf) = 0
extern "C" void FUN_1400ac880_Body(void* self,
                                   int colStart,
                                   int rowStart,
                                   int cellSize_x,
                                   int cellSize_y,
                                   int16_t* vertOut) {
    if (!self || !vertOut) return;

    int rowEnd = rowStart + cellSize_y;
    int colEnd = colStart + cellSize_x;

    int gridSize   = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0xc0);
    int gridLevels = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0xd0);
    int16_t* heightmap = *reinterpret_cast<int16_t**>(reinterpret_cast<uint8_t*>(self) + 0xd8);

    if (!heightmap) {
        // Without a heightmap, the loop would deref garbage. Zero the block
        // and bail — caller (sub_c1e0) will still produce a flat plane.
        size_t totalVerts = static_cast<size_t>(cellSize_x) * cellSize_y;
        for (size_t i = 0; i < totalVerts * 8; ++i) {
            vertOut[i] = 0;
        }
        return;
    }

    int16_t halfGrid = static_cast<int16_t>(gridSize >> 1);
    int16_t* psVar10 = vertOut;

    for (int row = rowStart; row < rowEnd; ++row) {
        for (int col = colStart; col < colEnd; ++col) {
            // (1) Compute normal via helper_e270 — output to local Vector3.
            float normalBuf[3] = {0.0f, 0.0f, 0.0f};
            TradeWork_helper_e270_Body(self, normalBuf, col, row);

            // (2) Position xy (relative to grid centre).
            psVar10[0] = static_cast<int16_t>(col) - halfGrid;
            psVar10[1] = static_cast<int16_t>(row) - halfGrid;

            // (3) Heightmap sample (clamped col/row).
            int cc = col;
            if (cc < 0) cc = 0;
            if (cc >= gridLevels) cc = gridLevels - 1;
            int rr = row;
            if (rr < 0) rr = 0;
            if (rr >= gridLevels) rr = gridLevels - 1;

            int16_t height = heightmap[static_cast<size_t>(rr) * gridLevels + cc];
            psVar10[2] = height;

            // (4) Disasm shows: word[3] = 0xff (=255), then byte[0xf]=0
            // (these overlap in the 16-byte vertex layout). The disasm sets
            // word[3] = R15W (=0xff), then byte[0xd] = (ny+UVScale)*kByteScale,
            // byte[0xe] = (nz+UVScale)*kByteScale, byte[0xf] = 0.
            // We replicate by writing word[3] first then overwriting byte[0xd-0xf].
            psVar10[3] = static_cast<int16_t>(0x00ff);

            // (5) UV (col/gridSize * UVToShortScale, row/gridSize * UVToShortScale).
            float gridSizeF = static_cast<float>(gridSize);
            float uvCol = (gridSizeF != 0.0f)
                            ? (static_cast<float>(col) / gridSizeF) * kUVToShortScale
                            : 0.0f;
            float uvRow = (gridSizeF != 0.0f)
                            ? (static_cast<float>(row) / gridSizeF) * kUVToShortScale
                            : 0.0f;
            psVar10[4] = static_cast<int16_t>(static_cast<int>(uvCol));
            psVar10[5] = static_cast<int16_t>(static_cast<int>(uvRow));

            // (6) Normal bytes — disasm writes byte[0xc] first then byte[0xd]
            // and byte[0xe], with byte[0xf] = 0.
            uint8_t* p8 = reinterpret_cast<uint8_t*>(psVar10);
            p8[0xc] = static_cast<uint8_t>(static_cast<int>((normalBuf[0] + kUVScale) * kByteScale));
            p8[0xd] = static_cast<uint8_t>(static_cast<int>((normalBuf[1] + kUVScale) * kByteScale));
            p8[0xe] = static_cast<uint8_t>(static_cast<int>((normalBuf[2] + kUVScale) * kByteScale));
            p8[0xf] = 0;

            psVar10 += 8;  // advance one 16-byte vertex (8 shorts)
        }
    }
}

// ============================================================
// Helper A — TradeWork_impl_sub_c1e0 (heightmap mesh builder)
// ============================================================
//
// Top-level helper: builds the vertex buffer + index buffer for the entire
// terrain mesh. Walks the chunk grid (chunksPerSide × chunksPerSide), fills
// each chunk's vertex block via FUN_1400ac880, uploads to VertexBuffer, then
// constructs a single per-chunk triangle-list index buffer with checkerboard
// winding (every chunk shares the same IB).
//
// param_1 (self) : Geometry container — fields used:
//   self+0xC0 : int gridSize       (full heightmap edge in cells, e.g. 768)
//   self+0xC8 : int chunkSize      (per-chunk cell edge, e.g. 32). The original
//                                    disasm reads this as `*(param_1+200)` and
//                                    uses it BOTH as the divisor for
//                                    chunksPerSide AND as the chunk-stride and
//                                    the IB inner-loop bound.
//   self+0xCC : int vertsPerSide   (per-chunk vertex edge = chunkSize+1, e.g. 33)
//   self+0xD0 : int gridLevels     (heightmap-storage dimension, e.g. gridSize+1)
//   self+0xD4 : int vertsPerChunk  (vertsPerSide², e.g. 1089)
//   self+0xD8 : int16* heightmap   (deserialized heightmap buffer)
//   self+0x100: VertexBuffer       (master mesh vertex buffer)
//   self+0x120: IndexBuffer        (per-chunk index buffer, shared across chunks)
//
// chunksPerSide = gridSize / chunkSize is computed locally — NOT a stored
// field. (Earlier this reimpl conflated +0xC8 with chunksPerSide; that was
// wrong and over-allocated the VB by a factor of (chunkSize/chunksPerSide)².)
extern "C" void TradeWork_impl_sub_c1e0_Body(void* self) {
    if (!self) return;

    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_MemFreeStatic) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c1e0] MemAlloc/Free unavailable — skipping mesh build (self=%p)\n", self);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    ResolveRuntimePtrs();
    if (!s_VertexBufferCreate || !s_VertexBufferSet ||
        !s_IndexBufferCreate  || !s_IndexBufferSet) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c1e0] VertexBuffer/IndexBuffer Runtime exports unresolved — "
                   "skipping mesh build (cascade-safe no-op). Geometry will lack render data "
                   "but menu can still launch.\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);

    // Field map (verified vs original sub_c1e0 disasm + Geometry CollectDraw/ItemDraw):
    //   self+0xC0 : int gridSize       — full heightmap edge in cells (e.g. 768)
    //   self+0xC8 : int chunkSize      — per-chunk cell edge (e.g. 32). Original
    //                                     disasm reads this as `*(param_1+200)` and
    //                                     uses it BOTH as the divisor for
    //                                     chunksPerSide AND as the chunk-stride.
    //   self+0xCC : int vertsPerSide   — per-chunk vertex edge (chunkSize+1, e.g. 33).
    //                                     Used as the vertex-grid edge passed to
    //                                     FUN_1400ac880, AND as the row-stride for
    //                                     index-buffer vertex IDs.
    //   self+0xD4 : int vertsPerChunk  — vertsPerSide² (e.g. 1089).
    //
    // chunksPerSide = gridSize / chunkSize is computed locally — NOT a stored
    // field. Earlier the variable was named `cellGridSize` and read directly
    // from +0xC8 (which is wrong — that's chunkSize).
    int gridSize       = *reinterpret_cast<int*>(selfBytes + 0xC0);
    int chunkSize      = *reinterpret_cast<int*>(selfBytes + 0xC8);
    int vertsPerSide   = *reinterpret_cast<int*>(selfBytes + 0xCC);
    int vertsPerChunk  = *reinterpret_cast<int*>(selfBytes + 0xD4);
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

    // (1) Allocate the per-chunk scratch vert buffer using the existing
    //     stride-0x10 resize helper. arrayHeader is a 16-byte stack-local.
    struct ArrayHeader16 { void* data; int32_t count; int32_t cap; };
    ArrayHeader16 scratch{nullptr, 0, 0};

    if (fn_Array_Resize_Stride16B) {
        fn_Array_Resize_Stride16B(&scratch, vertsPerChunk);
    } else {
        // Fallback: direct alloc.
        scratch.cap   = vertsPerChunk;
        scratch.count = vertsPerChunk;
        scratch.data  = fn_MemAlloc(g_pMemAllocSingleton,
                                    static_cast<uint64_t>(vertsPerChunk) * 0x10, 2);
    }

    int16_t* vertScratch = reinterpret_cast<int16_t*>(scratch.data);

    // (2) Allocate the master VertexBuffer at self+0x100.
    //     Total size = vertsPerChunk * chunksPerSide² * 16 bytes.
    int totalVerts  = vertsPerChunk * chunksPerSide * chunksPerSide;
    int totalVbSize = totalVerts * 0x10;
    bool createOk = s_VertexBufferCreate(selfBytes + 0x100, totalVbSize, /*dynamic=*/false,
                                         /*initialData=*/nullptr);

    printf("[sub_c1e0] entry self=%p gridSize=%d chunkSize=%d chunksPerSide=%d "
           "vertsPerSide=%d vertsPerChunk=%d totalVbSize=%d (0x%x) createOk=%d\n",
           self, gridSize, chunkSize, chunksPerSide, vertsPerSide, vertsPerChunk,
           totalVbSize, totalVbSize, createOk);
    {
        // Dump the freshly-Created VB struct so we can see the post-Create state
        // of the VertexBufferData wrapper and its vtable.
        const uint64_t* vbq = reinterpret_cast<const uint64_t*>(selfBytes + 0x100);
        printf("[sub_c1e0] VB after Create: q0=%016llx q1=%016llx q2=%016llx q3=%016llx\n",
               (unsigned long long)vbq[0], (unsigned long long)vbq[1],
               (unsigned long long)vbq[2], (unsigned long long)vbq[3]);
        if (vbq[1]) {
            const uint64_t* vbd = reinterpret_cast<const uint64_t*>(vbq[1]);
            printf("[sub_c1e0] VertexBufferData @%p: vtable=%016llx q1=%016llx q2=%016llx\n",
                   (void*)vbq[1], (unsigned long long)vbd[0],
                   (unsigned long long)vbd[1], (unsigned long long)vbd[2]);
        }
        fflush(stdout);
    }

    // (3) Walk the chunk grid, filling each chunk and uploading to VertexBuffer.
    int firstSetReturned = -1;
    if (vertScratch) {
        for (int chunkRow = 0; chunkRow < chunksPerSide; ++chunkRow) {
            for (int chunkCol = 0; chunkCol < chunksPerSide; ++chunkCol) {
                int colStart = chunkSize * chunkCol;
                int rowStart = chunkSize * chunkRow;

                FUN_1400ac880_Body(self, colStart, rowStart,
                                   vertsPerSide, vertsPerSide, vertScratch);

                int byteOffset = (chunkRow * chunksPerSide + chunkCol) * vertsPerChunk * 0x10;
                int byteSize   = vertsPerChunk * 0x10;
                bool ok = s_VertexBufferSet(selfBytes + 0x100, byteSize, byteOffset, vertScratch);

                if (chunkRow == 0 && chunkCol == 0) {
                    firstSetReturned = ok ? 1 : 0;
                    // Dump first vertex from scratch (should be (-gridSize/2, -gridSize/2, h, 0xFF))
                    // to confirm FUN_1400ac880 wrote real data.
                    int16_t* v0 = vertScratch;
                    printf("[sub_c1e0] chunk(0,0) scratch v0: x=%d y=%d h=%d w3=%d uvU=%d uvV=%d "
                           "n_xyzpad=%02x %02x %02x %02x  Set->%d  byteOff=%d byteSz=%d\n",
                           v0[0], v0[1], v0[2], v0[3], v0[4], v0[5],
                           ((uint8_t*)v0)[0xC], ((uint8_t*)v0)[0xD],
                           ((uint8_t*)v0)[0xE], ((uint8_t*)v0)[0xF],
                           ok ? 1 : 0, byteOffset, byteSize);
                    fflush(stdout);
                }
            }
        }
    }
    printf("[sub_c1e0] cell loop complete; first Set returned %d\n", firstSetReturned);
    fflush(stdout);

    // (4) Build the index buffer. The IB describes the topology of ONE chunk
    //     (chunkSize × chunkSize quads); every chunk shares this IB. Vertex IDs
    //     use `row * vertsPerSide + col` because each chunk has vertsPerSide²
    //     vertices in row-major order.
    int quadsPerChunk = chunkSize * chunkSize;
    int idxCount      = quadsPerChunk * 6;
    int idxBytes      = quadsPerChunk * 12;

    int16_t* idxBuf = nullptr;
    if (idxCount > 0) {
        idxBuf = reinterpret_cast<int16_t*>(
            fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(idxCount) * 2, 2));
    }

    // The original disasm has a strange "self-copy initialise" loop that runs
    // before the real indices are written:
    //   for (i = 0; i < idxCount; ++i)
    //     if (signed(prev_iter) < 0 && idxBuf != null)
    //       idxBuf[i] = idxBuf[i - prev_iter];
    // It compares against negative loop iterator, so it never executes. We
    // skip emulating this dead branch.

    // (5) Allocate the master IndexBuffer at self+0x120 (idxBytes total).
    s_IndexBufferCreate(selfBytes + 0x120, idxBytes, /*dynamic=*/false,
                        /*initialData=*/nullptr);

    // (6) Fill the index buffer — checkerboard quad winding.
    //     Two triangles per quad, alternating diagonal direction by parity.
    //     Loop iterates over the chunkSize × chunkSize quads of one chunk;
    //     vertex IDs are `row * vertsPerSide + col`.
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
        bool ibSetOk = s_IndexBufferSet(selfBytes + 0x120, idxBytes, 0, idxBuf);
        printf("[sub_c1e0] IB Set returned %d (idxBytes=%d quadsPerChunk=%d)\n",
               ibSetOk ? 1 : 0, idxBytes, quadsPerChunk);
        fflush(stdout);
    }

    // (8) Free the scratch buffers (matches the original's MemFree(idxBuf);
    //     MemFree(scratch.data) at the end of sub_c1e0).
    if (idxBuf) {
        fn_MemFreeStatic(idxBuf);
    }
    if (scratch.data) {
        fn_MemFreeStatic(scratch.data);
    }
}

// ============================================================
// Chunk-streaming — FUN_1400AAD50 (per-frame dirty-chunk upload)
// ============================================================
//
// Vtable +0x08 of the derived Geometry class. Decompiled from exe 0x1400AAD50
// (the function that JMP-thunk 0x1400AED40 dispatches to). The original walks
// a binary tree of "dirty" chunk indices at self+0xE8, regenerates each
// chunk's vertices via FUN_1400ac880, and uploads them into the master
// VertexBuffer at self+0x100 via Video::VertexBuffer::Set.
//
// Tree layout — each node is a 0x28-byte allocation (System::MemoryAllocator::MemAlloc
// with align=8). Field offsets (verified from disasm):
//   +0x00  int   chunkIdx (the linear chunk index = row*chunksPerSide + col)
//   +0x08  void* parent
//   +0x10  void* left  child
//   +0x18  void* right child
//   +0x20  uint  AVL balance / height (set by inserter, ignored here)
//
// In addition to chunk-streaming, the original also handles re-marking entity
// elements at self+0x1F0 whose bbox overlaps any rebuilt chunk. We omit that
// path for now — the menu scene has zero entities (self+0x1F8 == 0) so it's
// dead code there; document the omission and the disasm for the in-game
// follow-up. See `notes/M35_geom_serialize_findings.md` for the spec.
//
// Cascade safety: if VertexBuffer::Set is unresolved we soft-fail with a
// one-shot warning and return early — better than corrupting the dirty tree.
extern "C" void  Reimpl_FUN_1400ac880(void* self, int colStart, int rowStart,
                                      int cellSize_x, int cellSize_y,
                                      int16_t* vertOut);

void GeometryChunkUpdate_Body(void* self) {
    if (!self) return;

    uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);

    // self+0xF0 is the "dirty tree non-empty" flag (4 bytes). The original
    // tests this as the first thing — when no chunks are dirty, return.
    if (*reinterpret_cast<int32_t*>(selfBytes + 0xF0) == 0) return;

    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_MemFreeStatic) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[GeometryChunkUpdate] MemAlloc/Free unavailable — skipping\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    ResolveRuntimePtrs();
    if (!s_VertexBufferSet) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[GeometryChunkUpdate] VertexBuffer::Set unresolved — "
                   "dirty chunks will not upload (terrain stops updating)\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    int gridSize       = *reinterpret_cast<int*>(selfBytes + 0xC0);
    int chunkSize      = *reinterpret_cast<int*>(selfBytes + 0xC8);
    int vertsPerSide   = *reinterpret_cast<int*>(selfBytes + 0xCC);
    int vertsPerChunk  = *reinterpret_cast<int*>(selfBytes + 0xD4);
    int chunksPerSide  = (chunkSize > 0) ? (gridSize / chunkSize) : 0;

    if (chunksPerSide <= 0 || vertsPerChunk <= 0) return;

    // Allocate the per-chunk scratch vertex buffer (16-byte stride).
    struct ArrayHeader16 { void* data; int32_t count; int32_t cap; };
    ArrayHeader16 scratch{nullptr, 0, 0};
    if (fn_Array_Resize_Stride16B) {
        fn_Array_Resize_Stride16B(&scratch, vertsPerChunk);
    } else {
        scratch.cap = scratch.count = vertsPerChunk;
        scratch.data = fn_MemAlloc(g_pMemAllocSingleton,
                                   static_cast<uint64_t>(vertsPerChunk) * 0x10, 2);
    }
    int16_t* vertScratch = reinterpret_cast<int16_t*>(scratch.data);
    if (!vertScratch) return;

    // Recursive in-order walk of the dirty-chunk tree. Mirrors the original's
    // threaded-BST traversal but using the call stack — depth is O(log N) for
    // a balanced AVL, max ~10 even for 18x18 chunks.
    auto* treeRoot = *reinterpret_cast<void**>(selfBytes + 0xE8);

    auto upload = [&](void* node) {
        uint8_t* n = reinterpret_cast<uint8_t*>(node);
        int chunkIdx = *reinterpret_cast<int*>(n);
        int row = chunkIdx / chunksPerSide;
        int col = chunkIdx % chunksPerSide;

        // Generate this chunk's vertices into the scratch buffer, then upload.
        Reimpl_FUN_1400ac880(self,
                             col * chunkSize, row * chunkSize,
                             vertsPerSide, vertsPerSide,
                             vertScratch);

        int byteSize   = vertsPerChunk * 0x10;
        int byteOffset = (row * chunksPerSide + col) * vertsPerChunk * 0x10;
        s_VertexBufferSet(selfBytes + 0x100, byteSize, byteOffset, vertScratch);
    };

    // Iterative in-order walk to avoid std::function or recursive lambdas.
    // We use the parent links on each node (offset 0x08) to ascend.
    if (treeRoot) {
        // Descend to leftmost.
        void* node = treeRoot;
        while (true) {
            void* leftChild = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x10);
            if (!leftChild) break;
            node = leftChild;
        }

        while (node) {
            upload(node);

            // Move to in-order successor:
            void* rightChild = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x18);
            if (rightChild) {
                node = rightChild;
                while (true) {
                    void* leftChild = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(node) + 0x10);
                    if (!leftChild) break;
                    node = leftChild;
                }
            } else {
                // Walk up while we're the right child of our parent.
                while (true) {
                    void* parent = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(node) + 0x08);
                    if (!parent) { node = nullptr; break; }
                    void* parentRight = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(parent) + 0x18);
                    if (parentRight != node) {
                        node = parent;
                        break;
                    }
                    node = parent;
                }
            }
        }
    }

    // Free the dirty tree (recursive post-order via FUN_140014c30 in the
    // original — equivalent to "free children, then self").
    auto freeNode = [&](auto&& self_, void* node) -> void {
        if (!node) return;
        uint8_t* n = reinterpret_cast<uint8_t*>(node);
        self_(self_, *reinterpret_cast<void**>(n + 0x10));
        self_(self_, *reinterpret_cast<void**>(n + 0x18));
        fn_MemFreeStatic(node);
    };
    freeNode(freeNode, treeRoot);

    *reinterpret_cast<void**>(selfBytes + 0xE8) = nullptr;
    *reinterpret_cast<int32_t*>(selfBytes + 0xF0) = 0;

    // Free scratch buffer.
    if (scratch.data) {
        fn_MemFreeStatic(scratch.data);
    }

    // TODO (in-game): the original also walks the entity SoA at self+0x1F0
    // (count +0x1F8) for each rebuilt chunk, and re-meshes any entity whose
    // bbox overlaps the chunk's region (via FUN_1400ace40). Skipped here
    // because the menu scene has no entities. Re-add when in-game terrain
    // editing wants entity re-meshing on terraform.
}

} // anonymous namespace

// ============================================================
// Public reimpl entry points
// ============================================================
//
// Each entry tries the original exe address first (DLL mode + standalone
// when the exe code page is mapped). If that pointer is null (standalone
// mode, no exe code), we fall through to our cleaned C body.

extern "C" void Reimpl_TradeWork_sub_c1e0(void* self) {
#ifndef STANDALONE_EXE
    if (fn_TradeWork_sub_c1e0) {
        fn_TradeWork_sub_c1e0(self);
        return;
    }
#endif
    TradeWork_impl_sub_c1e0_Body(self);
}

extern "C" void Reimpl_FUN_1400ac880(void* self,
                                     int colStart,
                                     int rowStart,
                                     int cellSize_x,
                                     int cellSize_y,
                                     int16_t* vertOut) {
#ifndef STANDALONE_EXE
    if (fn_TradeWork_FUN_ac880) {
        fn_TradeWork_FUN_ac880(self, colStart, rowStart, cellSize_x, cellSize_y, vertOut);
        return;
    }
#endif
    FUN_1400ac880_Body(self, colStart, rowStart, cellSize_x, cellSize_y, vertOut);
}

extern "C" void* Reimpl_TradeWork_helper_e270(void* self, void* retbuf, int col, int row) {
#ifndef STANDALONE_EXE
    if (fn_TradeWork_helper_e270) {
        return fn_TradeWork_helper_e270(self, retbuf, col, row);
    }
#endif
    return TradeWork_helper_e270_Body(self, retbuf, col, row);
}

extern "C" void Reimpl_GeometryChunkUpdate(void* self) {
#ifndef STANDALONE_EXE
    if (fn_Geometry_ChunkUpdate) {
        fn_Geometry_ChunkUpdate(self);
        return;
    }
#endif
    GeometryChunkUpdate_Body(self);
}
