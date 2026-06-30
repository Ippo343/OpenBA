// code/Game/Game/water_contour_mesh.cpp
// ============================================================================
// CONTOUR WATER MESH — faithful RE port of Banished's TradeWork_impl_b260.
// ============================================================================
//
// Replaces the flat substitute water with the real contour mesh that follows
// the world's water bodies (shore band / rivers flow, lake/pond shore rings).
//
// Source functions (Application-x64.exe, base 0x140000000):
//   * TradeWork_impl_b260 @ 0x1400ab260 — orchestrator. Builds the water mesh
//       into geom VB +0x160 / IB +0x180, with per-body draw-range tables at
//       +0x1c0 (range A, count +0x1c8) and +0x1d0 (range B, count +0x1d8).
//       Called as b260(param_1 = WorldGeometry geom, param_2 = hf TradeData).
//   * TradeWork_impl_fb00 @ 0x1400cfb00 — per-water-body contour vertex/UV
//       generator. Reads the body's centerline polyline and lays a flow-aligned
//       quad strip (V = arc length along the flow, U = 0..1 across the channel).
//   * helper f3e0 @ 0x14001f3e0 — vertex weld/dedup -> index (phase-2 weld).
//   * helper sub_74a0 @ 0x1400074a0 — 16-byte vertex append (the growable-vector
//       idiom, equivalent to engine_funcs' fn_Array_Resize_Stride16B).
//
// DATA FLOW (verified, this session):
//   b260's param_2 == the worldgen heightfield scratch `hf` (the same struct
//   wg_TradeCore_c4b0 builds in worldgen.cpp). Phase-1 water bodies live in the
//   patch list at hf+0x18 (record {centerlinePtr@+0, ptCount@+8}, stride 0x10;
//   count at hf+0x20). Each centerline point is stride 0x1c: {x@+0, y@+4,
//   halfWidth@+0x14}. Phase-2 features live at hf+0x48 (stride 0xc {x,y,radius},
//   count hf+0x50). Confirmed at runtime (STEP-1 de-risk): a fresh Easy map has
//   1 phase-1 body (the 513-point coastline ring, halfWidth 4.0) + 2 phase-2
//   features.
//
// FAITHFULNESS NOTES:
//   * Phase 1 packing, phase 2 (heightmap shore ring), the range tables, the
//     vertex/index accumulators, all .rdata constants, and the final
//     VertexBuffer/IndexBuffer::Create calls are byte/offset-faithful to the
//     b260 disassembly.
//   * fb00 STAGE-A DEVIATION (documented, sanctioned by
//     notes/water_mesh/fb00_contour_uv.md §7): the retail Stage A builds the two
//     offset bank contours through a 2D spatial-hash grid with rounded corner
//     fans (sub_40a0 / sub_3450 / sub_3680 / sub_ba30 / helper_31d0 — a ~1400
//     line cascade). We use the documented cheaper approximation: offset the
//     centerline by +/-radius along the per-node normal to get the two banks,
//     then feed them through the FAITHFUL Stage B (arc-length V) + Stage C
//     (4-column strip, U marches 0->1 in 0.25 steps, bilinear position,
//     arc-length V). This already yields the flow-following UV; it omits only
//     the rounded junctions/corner fans. Flagged for the lead's RenderDoc check.
//
// Constants (.rdata, verified via read_memory):
//   DAT_1401721e4 = 48.0     pos pack scale (pos.xy * 48 -> int16)
//   DAT_140172250 = 1024.0   uv  pack scale (uv     * 1024 -> int16)
//   DAT_1401722a4 = -192.0   phase-1 z base (z = -192 - bodyIdx)
//   _DAT_140172224 = 191.0   phase-2 z base (z = featureIdx - 191)
//   DAT_140172174 = 2.0      radius margin (radius = halfWidth + 2)
//   DAT_140172100 = 0.25     cross-strip U step
//   DAT_1401720bc = 1/256    heightmap int16 -> world height
//   DAT_14017227c = -0.75    phase-2 underwater test (corner.z <= -0.75)
//   DAT_14017222c = 255.0    phase-2 weight -> AO byte
//   DAT_14017228c = -16.0    streamwise V offset start
//   g_fUVScale    = 1.0
//
// Gated behind BANISHED_WATER_CONTOUR (default OFF). When OFF the flat-water /
// no-detail default is unchanged.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

namespace {

// ============================================================
// .rdata constants (embedded; the standalone PE has no exe .rdata)
// ============================================================
constexpr float kPosScale   = 48.0f;     // DAT_1401721e4
constexpr float kUVScale    = 1024.0f;   // DAT_140172250
constexpr float kZBaseP1    = -192.0f;   // DAT_1401722a4  (phase-1 z = -192 - bodyIdx)
constexpr float kZBaseP2    = 191.0f;    // _DAT_140172224 (phase-2 z = featureIdx - 191)
constexpr float kRadMargin  = 2.0f;      // DAT_140172174
constexpr float kUStep      = 0.25f;     // DAT_140172100
constexpr float kHeightStep = 1.0f / 256.0f; // DAT_1401720bc
constexpr float kUnderwater = -0.75f;    // DAT_14017227c
constexpr float kAOScale    = 255.0f;    // DAT_14017222c
constexpr float kVOffset    = -16.0f;    // DAT_14017228c

// ============================================================
// VertexBuffer / IndexBuffer::Create (Runtime DX wiring) — same lazy resolve
// pattern as serialize_heightmap_mesh.cpp.
// ============================================================
typedef bool (*VertexBufferCreate_t)(void* self, int sizeBytes, bool dynamic, void* initialData);
typedef bool (*IndexBufferCreate_t )(void* self, int sizeBytes, bool dynamic, void* initialData);

VertexBufferCreate_t s_VertexBufferCreate = nullptr;
IndexBufferCreate_t  s_IndexBufferCreate  = nullptr;
bool s_rtResolved = false;

void ResolveRuntimePtrs() {
    if (s_rtResolved) return;
    auto loadSlot = [](uintptr_t slotAddr) -> void* {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, slotAddr));
        return slot ? *slot : nullptr;
    };
    s_VertexBufferCreate = reinterpret_cast<VertexBufferCreate_t>(loadSlot(0x140152668));
    s_IndexBufferCreate  = reinterpret_cast<IndexBufferCreate_t >(loadSlot(0x140152660));
    if (!s_VertexBufferCreate)
        s_VertexBufferCreate = reinterpret_cast<VertexBufferCreate_t>(
            ResolveExport("?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    if (!s_IndexBufferCreate)
        s_IndexBufferCreate = reinterpret_cast<IndexBufferCreate_t>(
            ResolveExport("?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    s_rtResolved = true;
}

// ============================================================
// Growable-vector idiom (the engine's {data,count,cap} 16-byte header, with the
// "+1 append over-allocs 3x" growth, matching the b260/fb00 inlined resizes).
// ============================================================
struct Vec { void* data; int32_t count; int32_t cap; };

void* MemAllocA(uint64_t bytes, int align) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, bytes, align) : nullptr;
}

// Append one element of `stride` bytes; mirrors the inlined resize (count+1).
void VecPush(Vec* v, const void* elem, int stride, int align) {
    int n = v->count + 1;
    if (v->cap < n) {
        int newCap = (v->count != 0) ? n * 3 : n;   // 3x over-alloc on non-empty append
        void* nb = MemAllocA((uint64_t)newCap * stride, align);
        if (!nb) return;
        if (v->data && v->count > 0)
            std::memcpy(nb, v->data, (size_t)v->count * stride);
        if (v->data) fn_MemFreeStatic(v->data);
        v->data = nb;
        v->cap = newCap;
    }
    std::memcpy((uint8_t*)v->data + (size_t)v->count * stride, elem, stride);
    v->count = n;
}

// Resize a range-array (stride 0x10, align 4) to N entries, copy-only (sub_efd0).
void RangeResize(Vec* v, int n) {
    if (n == v->count) return;
    void* oldData = v->data;
    int oldCount = v->count;
    v->count = n;
    if (v->cap < n) {
        v->cap = n;
        void* nb = MemAllocA((uint64_t)n * 0x10, 4);
        if (nb && oldData && oldCount > 0) {
            int keep = (oldCount < n) ? oldCount : n;
            std::memcpy(nb, oldData, (size_t)keep * 0x10);
        }
        if (oldData && oldData != nb) fn_MemFreeStatic(oldData);
        v->data = nb;
    }
}

// ============================================================
// Vertex weld (f3e0 equivalent): full 16-byte key dedup -> monotonic index.
// The retail f3e0 uses an RB-tree keyed on the packed vertex; a hash with exact
// key compare is behaviourally identical (same dedup set, same index order).
// ============================================================
struct WeldSet {
    Vec     verts;       // OutVtx/16-byte GPU verts
    int32_t* buckets;    // hash head index (-1 empty)
    int32_t* chain;      // per-vertex next-in-bucket
    int32_t  mask;
    int32_t  chainCap;
    int32_t  stride;     // key/element size in bytes
};

uint32_t WeldHash(const void* key, int stride) {
    const uint8_t* k = (const uint8_t*)key;
    uint32_t h = 2166136261u;            // FNV-1a
    for (int i = 0; i < stride; ++i) { h ^= k[i]; h *= 16777619u; }
    return h;
}

void WeldInit(WeldSet* s, int stride, int estimate) {
    s->verts.data = nullptr; s->verts.count = 0; s->verts.cap = 0;
    s->stride = stride;
    int cap = 16;
    while (cap < estimate * 2) cap <<= 1;
    s->mask = cap - 1;
    s->buckets = (int32_t*)std::malloc((size_t)cap * sizeof(int32_t));
    for (int i = 0; i < cap; ++i) s->buckets[i] = -1;
    s->chainCap = estimate > 0 ? estimate : 16;
    s->chain = (int32_t*)std::malloc((size_t)s->chainCap * sizeof(int32_t));
}

void WeldFree(WeldSet* s) {
    if (s->buckets) std::free(s->buckets);
    if (s->chain) std::free(s->chain);
    if (s->verts.data) fn_MemFreeStatic(s->verts.data);
    s->buckets = nullptr; s->chain = nullptr; s->verts.data = nullptr;
}

// Returns the index of `key` in the weld set (existing on a hit, freshly
// appended on a miss). Mirrors f3e0's "return existing index or insert".
uint16_t WeldAdd(WeldSet* s, const void* key) {
    uint32_t b = WeldHash(key, s->stride) & s->mask;
    for (int32_t i = s->buckets[b]; i >= 0; i = s->chain[i]) {
        const uint8_t* v = (const uint8_t*)s->verts.data + (size_t)i * s->stride;
        if (std::memcmp(v, key, s->stride) == 0) return (uint16_t)i;
    }
    int32_t idx = s->verts.count;
    VecPush(&s->verts, key, s->stride, 2);
    if (idx >= s->chainCap) {
        int32_t nc = s->chainCap * 2;
        s->chain = (int32_t*)std::realloc(s->chain, (size_t)nc * sizeof(int32_t));
        s->chainCap = nc;
    }
    s->chain[idx] = s->buckets[b];
    s->buckets[b] = idx;
    return (uint16_t)idx;
}

// ============================================================
// fb00 output vertex {posX, posY, uvU, uvV} (4 floats = 16 bytes)
// ============================================================
struct OutVtx { float posX, posY, uvU, uvV; };
struct V2 { float x, y; };
inline V2 Lerp(V2 a, V2 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t }; }

// ============================================================
// fb00 — per-body contour vertex/UV generator (Stage A approximated, B/C faithful)
//
//   param_1 = hf (TradeData), param_2 = bodyIdx, out = vtxOut(OutVtx)+idxOut(u16).
// ============================================================
void fb00_BuildBody(void* hf, int bodyIdx, Vec* vtxOut, Vec* idxOut) {
    uint8_t* h = (uint8_t*)hf;
    // record = (hf+0x18) array, stride 0x10: {centerlinePtr@+0, ptCount@+8}
    uint8_t* recBase = *(uint8_t**)(h + 0x18);
    if (!recBase) return;
    uint8_t* rec = recBase + (size_t)bodyIdx * 0x10;
    const uint8_t* cl = *(const uint8_t**)(rec + 0x00);   // centerline array
    int ptCount = *(const int32_t*)(rec + 0x08);
    if (!cl || ptCount < 2) return;

    // Centerline point accessor (stride 0x1c): x@+0, y@+4, halfWidth@+0x14.
    auto CP = [&](int i) -> V2 {
        const float* f = (const float*)(cl + (size_t)i * 0x1c);
        return { f[0], f[1] };
    };
    // radius = centerline[0].halfWidth + 2.0   (fb00: *(*plVar38+0x14) + DAT_140172174)
    float radius = ((const float*)cl)[5] + kRadMargin;   // +0x14 == float index 5
    float channelWidth = 2.0f * radius;
    if (channelWidth < 1.0f) channelWidth = 1.0f;

    // --- Stage A (approx): banks = centerline +/- radius along per-node normal.
    // --- Stage B (faithful): arc-length V along the centerline (shared across
    //     the width band so the texture flows straight along the spine).
    V2*    bankL = (V2*)std::malloc((size_t)ptCount * sizeof(V2));
    V2*    bankR = (V2*)std::malloc((size_t)ptCount * sizeof(V2));
    float* vCoord = (float*)std::malloc((size_t)ptCount * sizeof(float));
    if (!bankL || !bankR || !vCoord) {
        if (bankL) std::free(bankL);
        if (bankR) std::free(bankR);
        if (vCoord) std::free(vCoord);
        return;
    }

    float arc = 0.0f;
    V2 prev = CP(0);
    for (int i = 0; i < ptCount; ++i) {
        V2 c = CP(i);
        // tangent via central difference (clamped at the ends)
        V2 a = CP(i > 0 ? i - 1 : i);
        V2 b = CP(i < ptCount - 1 ? i + 1 : i);
        float tx = b.x - a.x, ty = b.y - a.y;
        float tl = std::sqrt(tx * tx + ty * ty);
        if (tl < 1e-6f) { tx = 1.0f; ty = 0.0f; tl = 1.0f; }
        tx /= tl; ty /= tl;
        // normal = perpendicular (-ty, tx)
        float nx = -ty, ny = tx;
        bankL[i] = { c.x + nx * radius, c.y + ny * radius };
        bankR[i] = { c.x - nx * radius, c.y - ny * radius };
        // arc length along the centerline
        if (i > 0) {
            float dx = c.x - prev.x, dy = c.y - prev.y;
            arc += std::sqrt(dx * dx + dy * dy);
        }
        prev = c;
        // sub_b360 streamwise V (approx): isometric to U — one V-tile per channel
        // width of flow, offset by DAT_14017228c (-16). int16 wrap = seamless tile.
        vCoord[i] = (arc / channelWidth) + kVOffset;
    }

    // --- Stage C (faithful): 4-column quad strip per centerline segment.
    //     U marches 0 -> 1 in 0.25 steps; position bilinear across the banks;
    //     V is the (shared) centerline arc-length coordinate.
    int estimate = ptCount * 4 * 2;
    WeldSet weld; WeldInit(&weld, (int)sizeof(OutVtx), estimate);

    auto emitIdx = [&](uint16_t v) { VecPush(idxOut, &v, 2, 2); };

    for (int i = 0; i + 1 < ptCount; ++i) {
        V2 p0 = bankL[i], p1 = bankL[i + 1];
        float v0 = vCoord[i], v1 = vCoord[i + 1];
        float uLo = 0.0f, uHi = kUStep;
        for (int col = 0; col < 4; ++col) {
            float t = uHi;
            V2 pLo = Lerp(bankL[i],     bankR[i],     t);
            V2 pHi = Lerp(bankL[i + 1], bankR[i + 1], t);
            float vLo = v0;   // vA == vB (shared centerline V) -> lerp == v0/v1
            float vHi = v1;
            OutVtx V0 = { p0.x,  p0.y,  uLo, v0 };
            OutVtx V1 = { pLo.x, pLo.y, uHi, vLo };
            OutVtx V2v= { pHi.x, pHi.y, uHi, vHi };
            OutVtx V3 = { p1.x,  p1.y,  uLo, v1 };
            uint16_t i0 = WeldAdd(&weld, &V0);
            uint16_t i1 = WeldAdd(&weld, &V1);
            uint16_t i2 = WeldAdd(&weld, &V2v);
            uint16_t i3 = WeldAdd(&weld, &V3);
            // winding matches fb00 Stage C: (V0,V2,V1),(V0,V3,V2)
            emitIdx(i0); emitIdx(i2); emitIdx(i1);
            emitIdx(i0); emitIdx(i3); emitIdx(i2);
            uLo = uHi; uHi += kUStep; p0 = pLo; p1 = pHi; v0 = vLo; v1 = vHi;
        }
    }

    // Move the welded vertex set into vtxOut (transfer ownership of the buffer).
    vtxOut->data  = weld.verts.data;
    vtxOut->count = weld.verts.count;
    vtxOut->cap   = weld.verts.cap;
    weld.verts.data = nullptr;   // don't free below
    WeldFree(&weld);

    std::free(bankL); std::free(bankR); std::free(vCoord);
}

// ============================================================
// b260 PHASE 1 — contour water bodies.
// ============================================================
void b260_Phase1(uint8_t* geom, uint8_t* hf) {
    Vec* rangeA = (Vec*)(geom + 0x1c0);
    Vec* vtxAcc = (Vec*)(geom + 0x1a0);   // 16-byte GPU verts
    Vec* idxAcc = (Vec*)(geom + 0x1b0);   // uint16 indices

    int bodyCount = *(int32_t*)(hf + 0x20);
    RangeResize(rangeA, bodyCount);       // sub_efd0; sets rangeA.count (geom+0x1c8)

    for (int body = 0; body < bodyCount; ++body) {
        Vec vtxOut{ nullptr, 0, 0 };
        Vec idxOut{ nullptr, 0, 0 };
        fb00_BuildBody(hf, body, &vtxOut, &idxOut);

        // Range entry: {idxStart, vtxStart, vtxCount, idxCount}.
        int32_t* re = (int32_t*)((uint8_t*)rangeA->data + (size_t)body * 0x10);
        re[0] = idxAcc->count;     // idxStart (geom+0x1b8)
        re[1] = vtxAcc->count;     // vtxStart (geom+0x1a8)
        re[2] = vtxOut.count;      // vtxCount
        re[3] = idxOut.count;      // idxCount

        // Pack + append each fb00 vertex into the GPU staging buffer.
        int zBody = (int)(kZBaseP1 - (float)body);   // z = -192 - bodyIdx
        const OutVtx* ov = (const OutVtx*)vtxOut.data;
        for (int i = 0; i < vtxOut.count; ++i) {
            int16_t gv[8];
            gv[0] = (int16_t)(int)(ov[i].posX * kPosScale);
            gv[1] = (int16_t)(int)(ov[i].posY * kPosScale);
            gv[2] = (int16_t)zBody;
            gv[3] = 0;
            gv[4] = (int16_t)(int)(ov[i].uvU * kUVScale);
            gv[5] = (int16_t)(int)(ov[i].uvV * kUVScale);
            uint8_t* c = (uint8_t*)&gv[6];
            c[0] = 0x7f; c[1] = 0x7f; c[2] = 0xff; c[3] = 0x00;
            VecPush(vtxAcc, gv, 0x10, 2);
        }
        // Append the body-local indices verbatim (GPU adds baseVertex=vtxStart).
        const uint16_t* oi = (const uint16_t*)idxOut.data;
        for (int i = 0; i < idxOut.count; ++i)
            VecPush(idxAcc, &oi[i], 2, 2);

        if (vtxOut.data) fn_MemFreeStatic(vtxOut.data);
        if (idxOut.data) fn_MemFreeStatic(idxOut.data);
    }
}

// ============================================================
// b260 PHASE 2 — heightmap shore ring around the features at hf+0x48.
// Faithful port: for each feature, walk the bounding box [center +/- (radius+2)],
// emit a heightmap-conformal quad per cell that straddles the water boundary
// (some corner inside radius AND some corner underwater <= -0.75). Per-vertex
// planar UV (= pos.xy * 48) and an AO byte (= weight * 255). Welded via f3e0.
// ============================================================
void b260_Phase2(uint8_t* geom, uint8_t* hf) {
    Vec* rangeB = (Vec*)(geom + 0x1d0);
    Vec* vtxAcc = (Vec*)(geom + 0x1a0);
    Vec* idxAcc = (Vec*)(geom + 0x1b0);

    int featureCount = *(int32_t*)(hf + 0x50);
    RangeResize(rangeB, featureCount);

    int   N        = *(int32_t*)(geom + 0xd0);   // heightmap edge
    int16_t* hmap  = *(int16_t**)(geom + 0xd8);  // int16 heightmap
    int   halfGrid = *(int32_t*)(geom + 0xc0) >> 1;
    uint8_t* featBase = *(uint8_t**)(hf + 0x48);

    // [WATER-DIAG] Phase-2 tarn instrumentation. Entry state first so we can see
    // which early-return gate (if any) fires when nothing emits.
    const bool diag = std::getenv("BANISHED_WATER_DIAG") != nullptr;
    long dCells = 0, dUnder = 0, dInterior = 0, dEmitCells = 0;
    if (diag) {
        fprintf(stderr, "[WATER-DIAG] b260 Phase2 ENTER: features=%d N(+0xd0)=%d "
                "hmap(+0xd8)=%p halfGrid=%d featBase(+0x48)=%p\n",
                featureCount, N, (void*)hmap, halfGrid, (void*)featBase);
        fflush(stderr);
    }

    if (!hmap || N <= 0) return;
    if (!featBase) return;

    auto sampleH = [&](int worldCol, int worldRow) -> float {
        int ci = worldCol + halfGrid; if (ci < 0) ci = 0; if (ci >= N) ci = N - 1;
        int ri = worldRow + halfGrid; if (ri < 0) ri = 0; if (ri >= N) ri = N - 1;
        return (float)(int)hmap[(size_t)ri * N + ci] * kHeightStep;
    };

    for (int f = 0; f < featureCount; ++f) {
        const float* feat = (const float*)(featBase + (size_t)f * 0xc);
        float cx = feat[0], cy = feat[1], radius = feat[2];
        float r2 = radius + kRadMargin;

        int xmin = (int)(cx - r2), xmax = (int)(cx + r2);
        int ymin = (int)(cy - r2), ymax = (int)(cy + r2);

        int32_t* re = (int32_t*)((uint8_t*)rangeB->data + (size_t)f * 0x10);
        re[0] = idxAcc->count;     // idxStart
        re[1] = vtxAcc->count;     // vtxStart
        re[2] = 0;                 // vtxCount  (filled after)
        re[3] = 0;                 // idxCount  (filled after)

        int zFeat = (int)((float)f - kZBaseP2);   // z = featureIdx - 191

        // Per-feature local weld set (matches the b260 per-feature local_1c8).
        int est = (xmax - xmin + 1) * (ymax - ymin + 1) * 4;
        if (est < 16) est = 16;
        WeldSet weld; WeldInit(&weld, 0x10, est);
        int idxAdded = 0;

        for (int row = ymin; row < ymax; ++row) {
            for (int col = xmin; col < xmax; ++col) {
                // 4 cell corners {x, y, z=heightmap*1/256}
                float corner[4][3];
                corner[0][0] = (float)col;     corner[0][1] = (float)row;     corner[0][2] = sampleH(col,     row);
                corner[1][0] = (float)(col+1); corner[1][1] = (float)row;     corner[1][2] = sampleH(col + 1, row);
                corner[2][0] = (float)(col+1); corner[2][1] = (float)(row+1); corner[2][2] = sampleH(col + 1, row + 1);
                corner[3][0] = (float)col;     corner[3][1] = (float)(row+1); corner[3][2] = sampleH(col,     row + 1);

                bool someInterior = false;   // !bVar6
                bool someUnderwater = false;  // !bVar7
                float weight[4];
                for (int k = 0; k < 4; ++k) {
                    if (corner[k][2] <= kUnderwater) someUnderwater = true;
                    float dx = corner[k][0] - cx;
                    float dy = corner[k][1] - cy;
                    float dz = corner[k][2] - 0.0f;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    float w = 1.0f - (dist - radius) / 2.0f;   // fVar38 == 2.0
                    if (w >= 0.0f) {
                        if (w > 1.0f) w = 1.0f;
                        if (w > 0.0f) someInterior = true;     // (==0 does not mark interior)
                    } else {
                        w = 0.0f;
                    }
                    weight[k] = w;
                }

                if (diag) { ++dCells; if (someUnderwater) ++dUnder; if (someInterior) ++dInterior; }
                if (!(someInterior && someUnderwater)) continue;
                if (diag) ++dEmitCells;

                // Weld the 4 corners, then emit two triangles (i0,i1,i2),(i0,i2,i3).
                uint16_t ci[4];
                for (int k = 0; k < 4; ++k) {
                    int16_t gv[8];
                    gv[0] = (int16_t)(int)(corner[k][0] * kPosScale);
                    gv[1] = (int16_t)(int)(corner[k][1] * kPosScale);
                    gv[2] = (int16_t)zFeat;
                    gv[3] = 0;
                    gv[4] = (int16_t)(int)(corner[k][0] * kPosScale);  // planar UV = pos.xy
                    gv[5] = (int16_t)(int)(corner[k][1] * kPosScale);
                    uint8_t* c = (uint8_t*)&gv[6];
                    c[0] = 0x7f; c[1] = 0x7f;
                    c[2] = (uint8_t)(int64_t)(weight[k] * kAOScale);
                    c[3] = 0x00;
                    ci[k] = WeldAdd(&weld, gv);
                }
                uint16_t quad[6] = { ci[0], ci[1], ci[2], ci[0], ci[2], ci[3] };
                for (int k = 0; k < 6; ++k) VecPush(idxAcc, &quad[k], 2, 2);
                idxAdded += 6;
            }
        }

        // Bulk-append the welded verts to the GPU staging buffer.
        const uint8_t* wv = (const uint8_t*)weld.verts.data;
        for (int i = 0; i < weld.verts.count; ++i)
            VecPush(vtxAcc, wv + (size_t)i * 0x10, 0x10, 2);

        re[2] = weld.verts.count;
        re[3] = idxAdded;
        WeldFree(&weld);
    }

    if (diag) {
        // Global heightmap min + count of underwater cells across the whole map.
        long globalUnder = 0; float hmin = 1e9f;
        for (size_t i = 0; i < (size_t)N * N; ++i) {
            float z = (float)(int)hmap[i] * kHeightStep;
            if (z < hmin) hmin = z;
            if (z <= kUnderwater) ++globalUnder;
        }
        fprintf(stderr,
            "[WATER-DIAG] b260 Phase2: features=%d  cellsTested=%ld under(<=%.2f)=%ld "
            "interior=%ld emit=%ld | map hmin=%.3f globalUnderCells=%ld/%d\n",
            featureCount, dCells, (double)kUnderwater, dUnder, dInterior, dEmitCells,
            (double)hmin, globalUnder, N * N);
        fflush(stderr);
    }
}

} // anonymous namespace

// ============================================================
// Public entry — build the contour water mesh into the WorldGeometry object.
//   self = WorldGeometry (geom, the b260 param_1; VB+0x160 / IB+0x180)
//   hf   = heightfield TradeData (the b260 param_2; centerlines at +0x18)
// Mirrors TradeWork_impl_b260(param_1=self, param_2=hf).
// ============================================================
extern "C" void WorldGen_BuildContourWaterMesh(void* self, void* hf) {
    if (!self || !hf) return;
    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_MemFreeStatic) {
        fprintf(stderr, "[water_contour] MemAlloc/Free unavailable — skipping\n");
        fflush(stderr);
        return;
    }
    ResolveRuntimePtrs();

    uint8_t* geom = (uint8_t*)self;
    uint8_t* h    = (uint8_t*)hf;

    // Phase 2 can be gated separately (it builds shore rings around hf+0x48
    // features, which on the first-light map are mountain ranges — may look odd;
    // default ON to match retail, opt out via BANISHED_WATER_CONTOUR_NOPHASE2).
    const char* noP2 = std::getenv("BANISHED_WATER_CONTOUR_NOPHASE2");
    bool doPhase2 = !(noP2 && noP2[0] && noP2[0] != '0');

    b260_Phase1(geom, h);
    if (doPhase2) b260_Phase2(geom, h);

    // Finalize the GPU buffers (b260 tail). IndexBuffer first, then VertexBuffer.
    Vec* vtxAcc = (Vec*)(geom + 0x1a0);
    Vec* idxAcc = (Vec*)(geom + 0x1b0);
    if (s_IndexBufferCreate)
        s_IndexBufferCreate(geom + 0x180, idxAcc->count * 2, false, idxAcc->data);
    if (s_VertexBufferCreate)
        s_VertexBufferCreate(geom + 0x160, vtxAcc->count << 4, false, vtxAcc->data);

    fprintf(stderr, "[water_contour] built: bodies(+0x1c8)=%d featRanges(+0x1d8)=%d "
            "verts(+0x1a8)=%d idx(+0x1b8)=%d VB=%p IB=%p (phase2=%d)\n",
            *(int32_t*)(geom + 0x1c8), *(int32_t*)(geom + 0x1d8),
            vtxAcc->count, idxAcc->count,
            *(void**)(geom + 0x160 + 8), *(void**)(geom + 0x180 + 8), (int)doPhase2);
    fflush(stderr);
}
