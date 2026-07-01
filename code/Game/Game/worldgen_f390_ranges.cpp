// code/Game/Game/worldgen_f390_ranges.cpp
// ============================================================================
// M5 5b worldgen — f390 MOUNTAIN-RANGES + SECONDARY-PATHS stages.
// ============================================================================
//
// Reimplements two of the feature stages that run inside the heightmap pipeline
// f390 (notes/WORLDGEN_TERRAIN_BLUEPRINT.md §0), in the verified order:
//
//   e5d0  base perlin heightmap        (already reimpl'd in worldgen.cpp)
//   cd090 coastline ring
//   cd150 mountain ranges   <-- wg_f390_ranges  (THIS FILE)
//   ccf60 secondary paths   <-- wg_f390_paths   (THIS FILE)
//   ...
//
//   FUN_1400cd150  EmitMountainRanges    -> wg_f390_ranges
//   FUN_1400ccf60  EmitSecondaryPaths    -> wg_f390_paths
//   sub-workers (all static here):
//     FUN_1400245d0  AppendVec3   (0xC-stride array resize-and-fill)
//     FUN_1400cc850  TryPlacePath (build+validate a candidate polyline)
//     FUN_1400cc510  SubdivideSeg (recursive fractal midpoint displacement)
//     FUN_140023b70  ResizeVec1c  (0x1C-stride array resize)
//     FUN_140024460  DeepAppendAoA(array-of-arrays deep append)
//     FUN_140024ac0  CopyVec1c    (deep-copy of one 0x1C-stride vector)
//
// ----------------------------------------------------------------------------
// HEIGHT EFFECT (these two stages do NOT touch the float heightmap directly):
//   Neither cd150 nor ccf60 writes heightfield+0x08. They only POPULATE the
//   scratch feature lists that the LATER stamp stages (e910/ec50) read to carve
//   the heightmap:
//     * cd150 pushes mountain "satellite" + "center" points into listC (+0x38)
//       and one record per range into detailRecs (+0x48). e910/ec50 later stamp
//       these as positive radial bumps into maskC (a MAX/additive mask) using
//       exp 2.0 / scale 1.778 — i.e. they RAISE terrain into mountain ranges.
//     * ccf60 pushes validated polylines (river/coast branch paths) into the
//       array-of-arrays at hf+0x18 (via cc850's deep-append at param_1+0x18).
//       The river network (cd460) + stamp stages later carve these DOWN toward
//       sea level (maskA, a MIN mask, exp 1.33) — channels/valleys.
//   So in isolation these stages change NO height values; their magnitude shows
//   up only after e910/ec50 run. Recorded magnitudes (for the orchestrator):
//     - mountain center "radius" field = bandX - radiusMin  (in grid units)
//     - mountain satellite "radius" field = RandFloat*(radiusMin-radiusMax)+radiusMax
//     - both later become additive height bumps (peaks), strength scaled 1.778.
//     - secondary paths are zero-height polylines; they become carve channels.
// ----------------------------------------------------------------------------
//
// All offsets verified against live Ghidra disassembly of Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ============================================================
// Heightfield scratch-struct field offsets (blueprint §1.1).
// (mirrors worldgen.cpp; kept local to this TU)
// ============================================================
static constexpr int HF_GRIDSIZE   = 0x00;   // int  dim (== sizeRow[0]*2)
static constexpr int HF_PATHLIST   = 0x18;   // array-of-arrays of polylines (cc850 deep-appends here)
static constexpr int HF_PATHCOUNT  = 0x20;   // int  count of paths in HF_PATHLIST (branch-select bound)
static constexpr int HF_LISTC      = 0x38;   // {void*,int,int,int} mountain satellites+centers (0xC elem {x,y,radius})
static constexpr int HF_LISTC_CNT  = 0x40;   // int  count of LISTC
static constexpr int HF_DETAILRECS = 0x48;   // {void*,int,int,int} per-range records (0xC elem {x,y,bandX})
static constexpr int HF_DETAIL_CNT = 0x50;   // int  count of DETAILRECS
static constexpr int HF_RANDOM     = 0x58;   // System::Random
static constexpr int HF_SIZEROW    = 0xa28;  // int* size-table row

// SizeRow word indices used here (blueprint §1.2). sizeRow is int*, so [k] == byte k*4.
//   [0]    = +0x00 dim
//   +0x20  = numSecondaryPaths
//   [0x0a] = +0x28 minRangeCount
//   [0x0b] = +0x2c maxRangeCount
//   [0x0c] = +0x30 minBandX
//   [0x0d] = +0x34 maxBandX
//   +0x38  = satellitesPerRange
//   +0x3c  = radiusMax
//   +0x40  = radiusMin

// ============================================================
// .rdata float constants (verified via read_memory; address=value).
// ============================================================
static constexpr float kHalf       = 0.5f;     // 0x140172114  amplitude seed / subdivide half
static constexpr float kEighth      = 0.125f;  // 0x1401720ec  cc850 closed-ring midpoint jitter
static constexpr float kQuarter     = 0.25f;   // 0x140172100  cc510 endpoint amplitude (cc850 param_6 seed)
static constexpr float kMidAmp      = 0.66602f;// 0x14017211c  cc510 midpoint displacement amplitude factor
static constexpr float kNegHalf     = -0.5f;   // 0x140172278  cc850 ring first-point Y
// ccf60 path params (constants, address=value):
static constexpr float kPathP46     = 1.25f;   // 0x140172158  ccf60 param_6
static constexpr float kPathP47     = 1.5f;    // 0x140172160  ccf60 param_4 & param_7
static constexpr float kPathP5      = 8.0f;    // 0x1401721a8  ccf60 param_5
static constexpr float kPathP8      = 16.0f;   // 0x1401721c0  ccf60 param_8
// float-negate sign mask: 0x1401722c0 = 0x80000000
static constexpr uint32_t kSignMask = 0x80000000u;
// g_fUVScale (0x14017214c) = 1.0f — intersection-parameter (0,1) bound.
static constexpr float kOne = 1.0f;

// ============================================================
// Late-bound Runtime resolves not yet in engine_funcs.h.
//   Random::Rand(min,max)   2-arg  ?Rand@Random@System@@QEAAHHH@Z       (slot 0x140151310)
//   Random::RandFloatTol()         ?RandFloatTol@Random@System@@QEAAMXZ (slot 0x140151400)
//   Vector2::Normalize(this)       ?Normalize@Vector2@Math@@QEAAAEAV12@XZ (slot 0x1401526b0)
// (Random::Rand(uint) [slot 0x1401512e8], RandFloat() [slot 0x1401512f8] and
//  MemAlloc/MemFree are already wired in engine_funcs.)
// ============================================================
typedef int    (*fn_Random_Rand2_t)(void* self, int lo, int hi);
typedef float  (*fn_Random_RandFloatTol_t)(void* self);
typedef float* (*fn_Vec2_Normalize_t)(float* self);   // normalizes in place, returns self

static fn_Random_Rand2_t        s_fn_Rand2        = nullptr;
static fn_Random_RandFloatTol_t s_fn_RandFloatTol = nullptr;
static fn_Vec2_Normalize_t      s_fn_Vec2Norm     = nullptr;
static bool s_rangesResolved = false;

// [WATER-DIAG] TryPlacePath reject-reason counters (BANISHED_WATER_DIAG dump in
// wg_f390_paths). Disambiguate which validation gate rejects candidates.
static long s_rejSelf  = 0;   // self-intersection reject
static long s_rejCross = 0;   // cross-path reject

static void rg_EnsureInit() {
    if (s_rangesResolved) return;
    s_rangesResolved = true;
    s_fn_Rand2        = (fn_Random_Rand2_t)ResolveExport("?Rand@Random@System@@QEAAHHH@Z");
    s_fn_RandFloatTol = (fn_Random_RandFloatTol_t)ResolveExport("?RandFloatTol@Random@System@@QEAAMXZ");
    s_fn_Vec2Norm     = (fn_Vec2_Normalize_t)ResolveExport("?Normalize@Vector2@Math@@QEAAAEAV12@XZ");
    fprintf(stderr, "[worldgen] ranges resolves: Rand2=%p RandFloatTol=%p Vec2Norm=%p "
            "Rand1=%p RandFloat=%p\n",
            (void*)s_fn_Rand2, (void*)s_fn_RandFloatTol, (void*)s_fn_Vec2Norm,
            (void*)fn_Random_Rand, (void*)fn_Random_RandFloat);
    fflush(stderr);
}

// float negate matching the exe's `(uint)x ^ 0x80000000`.
static inline float fneg(float x) {
    uint32_t u; __builtin_memcpy(&u, &x, 4); u ^= kSignMask;
    float r; __builtin_memcpy(&r, &u, 4); return r;
}
// Vector2::Normalize in place (engine fn if resolved; else a faithful fallback).
static inline void vec2_normalize2(float* v) {
    if (s_fn_Vec2Norm) { s_fn_Vec2Norm(v); return; }
    float len = sqrtf(v[0]*v[0] + v[1]*v[1]);
    if (len > 0.0f) { v[0] /= len; v[1] /= len; }
}
static inline float rftol(void* rng) {
    return s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
}
static inline float rfloat(void* rng) {
    return fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 0.0f;
}

// ============================================================
// FUN_1400245d0  @0x1400245d0  —  AppendVec3 (0xC-stride resize-and-fill).
// Resizes the {data,count,cap} header at `hdr` to `newCount` (grow ×3 on the
// count+1 fast path), copying any tail-overrun slots from `src` (a 12-byte
// {float x,float y,float z} record). Callers always pass newCount = count+1 and
// src = the element to push, so the net effect is push_back(src). The original's
// 4-wide unrolled copy is purely a perf optimization of this element-wise fill.
// ============================================================
static void AppendVec3(void* hdr, int newCount, const float* src /*x,y,z*/) {
    uint8_t* h = (uint8_t*)hdr;
    void* oldData  = *(void**)(h + 0x00);
    int   oldCount = *(int*)(h + 0x08);
    int   cap      = *(int*)(h + 0x0c);
    if (newCount == oldCount) return;

    *(int*)(h + 0x08) = newCount;
    void* data = oldData;
    if (cap < newCount) {
        int newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) newCap = newCount * 3;
        *(int*)(h + 0x0c) = newCap;
        data = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 0x0c, 4) : nullptr;
        *(void**)(h + 0x00) = data;
    }
    if (!data) return;

    int start = (data == oldData) ? oldCount : 0;
    uint8_t* dst = (uint8_t*)data;
    uint8_t* old = (uint8_t*)oldData;
    for (int i = start; i < newCount; ++i) {
        const float* s = src;
        if (data != oldData && i < oldCount) s = (const float*)(old + (size_t)i * 0x0c);
        float* d = (float*)(dst + (size_t)i * 0x0c);
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
    }
    if (oldData && data != oldData && fn_MemFreeStatic) fn_MemFreeStatic(oldData);
}

// ============================================================
// FUN_140024ac0  @0x140024ac0  —  CopyVec1c: deep-copy one 0x1C-stride vector.
// dst{data,count,cap}@dst <- src{data,count,cap}@src (frees dst's old data,
// allocates count*0x1c, copies each 0x1C-byte element).
// ============================================================
static void CopyVec1c(void* dstHdr, void* srcHdr) {
    if (dstHdr == srcHdr) return;
    uint8_t* d = (uint8_t*)dstHdr;
    uint8_t* s = (uint8_t*)srcHdr;
    if (*(void**)(d + 0x00) && fn_MemFreeStatic) fn_MemFreeStatic(*(void**)(d + 0x00));
    *(void**)(d + 0x00) = nullptr;
    *(int*)(d + 0x08) = 0;
    int cnt = *(int*)(s + 0x08);
    *(int*)(d + 0x08) = cnt;
    void* nd = nullptr;
    if (cnt > 0) {
        *(int*)(d + 0x0c) = cnt;
        nd = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cnt * 0x1c, 4) : nullptr;
        *(void**)(d + 0x00) = nd;
    }
    if (!nd) return;
    uint8_t* sd = *(uint8_t**)(s + 0x00);
    for (int i = 0; i < cnt; ++i)
        __builtin_memcpy((uint8_t*)nd + (size_t)i*0x1c, sd + (size_t)i*0x1c, 0x1c);
}

// ============================================================
// FUN_140024460  @0x140024460  —  DeepAppendAoA: append `srcVec` (a 0x1C-stride
// {data,count,cap} header) to the array-of-arrays header `hdr` (16-byte elems,
// each a {data,count,cap}+pad inner vector), resizing to newCount = count+1 and
// deep-copying srcVec into the new tail slot via CopyVec1c.
// (Only the push_back-one fast path is exercised by cc850.)
// ============================================================
static void DeepAppendAoA(void* hdr, int newCount, void* srcVec) {
    uint8_t* h = (uint8_t*)hdr;
    int oldCount = *(int*)(h + 0x08);
    if (newCount == oldCount) return;
    void* oldData = *(void**)(h + 0x00);
    int cap = *(int*)(h + 0x0c);
    *(int*)(h + 0x08) = newCount;
    void* data = oldData;
    if (cap < newCount) {
        int newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) newCap = newCount * 3;
        *(int*)(h + 0x0c) = newCap;
        data = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap << 4, 8) : nullptr;
        *(void**)(h + 0x00) = data;
    }
    if (!data) return;

    int start = (data == oldData) ? oldCount : 0;
    uint8_t* dst = (uint8_t*)data;
    uint8_t* old = (uint8_t*)oldData;
    for (int i = start; i < newCount; ++i) {
        void* slot = dst + (size_t)i * 0x10;
        void* srcSlot = srcVec;
        if (data != oldData && i < oldCount) srcSlot = old + (size_t)i * 0x10;
        *(void**)slot = nullptr;
        *(uint64_t*)((uint8_t*)slot + 8) = 0;
        CopyVec1c(slot, srcSlot);
    }
    if (data != oldData && oldData && fn_MemFreeStatic) {
        uint8_t* o = (uint8_t*)oldData;
        for (int i = 0; i < oldCount; ++i) {
            void* inner = *(void**)(o + (size_t)i * 0x10);
            if (inner) fn_MemFreeStatic(inner);
        }
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// FUN_140023b70  @0x140023b70  —  ResizeVec1c: resize a 0x1C-stride
// {data,count,cap} vector to `newCount` (grow ×3 on count+1), copying retained
// elements and zeroing the new tail. cc850 calls this then overwrites the tail.
// ============================================================
static void ResizeVec1c(void* hdr, int newCount) {
    uint8_t* h = (uint8_t*)hdr;
    int oldCount = *(int*)(h + 0x08);
    if (newCount == oldCount) return;
    void* oldData = *(void**)(h + 0x00);
    int cap = *(int*)(h + 0x0c);
    *(int*)(h + 0x08) = newCount;
    void* data = oldData;
    if (cap < newCount) {
        int newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) newCap = newCount * 3;
        *(int*)(h + 0x0c) = newCap;
        data = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 0x1c, 4) : nullptr;
        *(void**)(h + 0x00) = data;
    }
    if (!data) return;
    int start = (data == oldData) ? oldCount : 0;
    uint8_t* dst = (uint8_t*)data;
    uint8_t* old = (uint8_t*)oldData;
    for (int i = start; i < newCount; ++i) {
        uint8_t* d = dst + (size_t)i * 0x1c;
        if (data != oldData && i < oldCount)
            __builtin_memcpy(d, old + (size_t)i * 0x1c, 0x1c);
        else
            __builtin_memset(d, 0, 0x1c);
    }
    if (oldData && data != oldData && fn_MemFreeStatic) fn_MemFreeStatic(oldData);
}

// ============================================================
// FUN_1400cc510  @0x1400cc510  —  SubdivideSeg: recursive fractal midpoint
// displacement of the polyline in *vec between indices [param3, param4] using a
// half-plane reject sampler.
//
//   pts are 0x1C-stride; the leading 8 bytes of each = {float x, float y}.
//   a = pts[param3], b = pts[param4]; mid = (a+b)/2; dir = (b-a).
//   normal = normalize( (-dir.y, dir.x) ).                 [perpendicular]
//   limit = -param5  (the half-plane bound).
//   Repeatedly draw t = RandFloatTol()*param6*0.66602 ; candidate = mid+normal*t,
//   until the candidate passes the reject test (lies within the [limit, param5]
//   band and the two edge directions from a/b are not back-facing — dot>0 reject).
//   Store candidate at the midpoint index, then recurse on both halves with the
//   displacement amplitude halved (param6*0.5). Terminates when span <= 1.
// ============================================================
static void SubdivideSeg(int32_t* param_1, int32_t** vec, int param_3, int param_4,
                         float param_5, float param_6) {
    int span = param_4 - param_3;
    if (span <= 1) return;

    void* rng = (uint8_t*)param_1 + HF_RANDOM;
    uint8_t* base = *(uint8_t**)vec;
    float* a = (float*)(base + (size_t)param_3 * 0x1c);   // endpoint A {x,y}
    float* b = (float*)(base + (size_t)param_4 * 0x1c);   // endpoint B {x,y}
    float ax = a[0], ay = a[1];
    float bx = b[0], by = b[1];
    float dirX = bx - ax;                 // local_120
    float dirY = by - ay;                 // local_11c
    float midX = (bx + ax) * kHalf;       // fVar11
    float midY = (by + ay) * kHalf;       // fVar12

    // half-plane normal: (-dir.y, dir.x), normalized.  (local_128 = dir.y ^ sign)
    float perp[2] = { fneg(dirY), dirX };
    vec2_normalize2(perp);

    float limit = fneg(param_5);           // fVar9 = -param5

    float candX = 0.0f, candY = 0.0f;
    int safety = 0;
    do {
        float t = rftol(rng) * param_6 * kMidAmp;   // displacement magnitude
        candX = perp[0] * t + midX;        // fVar8
        candY = perp[1] * t + midY;        // fVar7

        float dB[2] = { bx - candX, by - candY };   // local_f8/local_f4
        float dA[2] = { ax - candX, ay - candY };   // local_f0/local_ec
        vec2_normalize2(dB);
        vec2_normalize2(dA);
        float facing = dA[0]*dB[0] + dA[1]*dB[1];    // dot (pfVar4 . pfVar5)

        // reject conditions transcribed verbatim:
        //   while (cand.x < limit);
        //   while ( (cand.x<=limit && cand.y<limit) || param5<cand.x
        //           || (param5<=cand.x && param5<cand.y) || facing>0 );
        bool retry = false;
        if (candX < limit) retry = true;
        else if (((candX <= limit) && (candY < limit)) ||
                 (param_5 < candX) ||
                 ((param_5 <= candX) && (param_5 < candY)) ||
                 (0.0f < facing)) retry = true;
        if (!retry) break;
        if (++safety > 1000000) return;    // guard (original is unbounded)
    } while (true);

    int mid = (span >> 1) + param_3;
    float* m = (float*)(base + (size_t)mid * 0x1c);
    m[0] = candX; m[1] = candY;

    float halfAmp = param_6 * kHalf;
    SubdivideSeg(param_1, vec, param_3, mid, param_5, halfAmp);
    SubdivideSeg(param_1, vec, mid, param_4, param_5, halfAmp);
}

// ============================================================
// FUN_1400cc850  @0x1400cc850  —  TryPlacePath: build a candidate polyline of
// `param_2` points (closed ring if branchSel<0, else a branch off the existing
// path[branchSel]) via SubdivideSeg, validate it has no self-intersections and
// no crossings with existing paths (segment intersection params both in (0,1)),
// and on success deep-append it to the array-of-arrays at param_1+0x18.
// Returns 1 on success (placed), 0 on reject.
// ============================================================
static int TryPlacePath(int32_t* param_1, int param_2 /*nptCount*/, float param_3 /*scale*/,
                        uint32_t param_4, uint32_t param_5, uint32_t param_6,
                        uint32_t param_7, uint32_t param_8, float param_9 /*branchSel*/) {
    void* rng = (uint8_t*)param_1 + HF_RANDOM;

    struct Vec1c { void* data; int count; int cap; int _p; } pts = {nullptr, 0, 0, 0};
    ResizeVec1c(&pts, param_2);
    if (!pts.data) return 0;

    uint8_t* pd = (uint8_t*)pts.data;
    // seed band params into bytes [8..0x1c) of every element
    for (int i = 0; i < param_2; ++i) {
        uint8_t* e = pd + (size_t)i * 0x1c;
        *(uint32_t*)(e + 0x08) = param_4;
        *(uint32_t*)(e + 0x0c) = param_5;
        *(uint32_t*)(e + 0x10) = param_6;
        *(uint32_t*)(e + 0x14) = param_7;
        *(uint32_t*)(e + 0x18) = param_8;
    }

    int lo, hi;
    float subAmp, subAmp2;   // cc510 param_5 (fVar24), param_6 (fVar22)

    if ((int)param_9 < 0) {
        // ---- CLOSED RING ----
        int last = pts.count - 1;
        float* p0 = (float*)pd;
        p0[0] = 0.0f; p0[1] = param_3 * kNegHalf;     // first: {0, scale*-0.5}

        uint32_t r = fn_Random_Rand ? fn_Random_Rand(rng, 3) : 0;
        float* pl = (float*)(pd + (size_t)last * 0x1c);
        if (r == 0) {
            pl[0] = 0.0f; pl[1] = param_3 * kHalf;    // {0, scale*0.5}
        } else {
            float v = (r == 1) ? (param_3 * kHalf)    // {scale*0.5, 0}
                               : (param_3 * kNegHalf);// r==2: {scale*-0.5, 0}
            pl[0] = v; pl[1] = 0.0f;
        }
        // midpoint jitter: pts[count/2] = (RandFloatTol*scale*0.125, RandFloatTol*scale*0.125)
        float jy = rftol(rng) * param_3 * kEighth;
        float jx = rftol(rng) * param_3 * kEighth;
        float* pm = (float*)(pd + (size_t)(pts.count >> 1) * 0x1c);
        pm[0] = jx; pm[1] = jy;

        subAmp2 = param_3 * kQuarter;                 // fVar22
        subAmp  = param_3 * kHalf;                    // fVar24
        SubdivideSeg(param_1, (int32_t**)&pts, 0, pts.count >> 1, subAmp, subAmp2);
        lo = pts.count >> 1;
        hi = pts.count - 1;
    } else {
        // ---- BRANCH off existing path[branchSel] ----
        void* aoa = *(void**)((uint8_t*)param_1 + HF_PATHLIST);   // hf+0x18 data
        uint8_t* selHdr = (uint8_t*)aoa + (size_t)(int)param_9 * 0x10;
        int selCount = *(int*)(selHdr + 0x08);
        int q = (selCount + ((selCount >> 31) & 3)) >> 2;          // selCount/4 (toward 0)
        int startIdx = s_fn_Rand2 ? s_fn_Rand2(rng, q, q * 3) : q;
        uint8_t* selData = *(uint8_t**)(selHdr + 0x00);
        *(uint64_t*)pd = *(uint64_t*)(selData + (size_t)startIdx * 0x1c);  // first pt = selected pt {x,y}

        float jitter = rftol(rng) * param_3 * kHalf;  // fVar17
        int last = pts.count - 1;
        float* pl = (float*)(pd + (size_t)last * 0x1c);
        uint32_t r = fn_Random_Rand ? fn_Random_Rand(rng, 4) : 0;
        float half = param_3 * kHalf;                 // fVar24 = scale*0.5
        // CONCAT44(hi,lo) -> pl[0]=lo, pl[1]=hi
        if (r == 0)      { pl[0] = jitter;            pl[1] = half;             } // {jitter, scale*0.5}
        else if (r == 1) { pl[0] = half;             pl[1] = jitter;           } // {scale*0.5, jitter}
        else if (r == 2) { pl[0] = param_3*kNegHalf; pl[1] = jitter;           } // {scale*-0.5, jitter}
        else /* r==3 */  { pl[0] = jitter;           pl[1] = param_3*kNegHalf; } // {jitter, scale*-0.5}

        subAmp2 = param_3 * kQuarter;
        subAmp  = param_3 * kHalf;
        lo = 0;
        hi = pts.count - 1;
    }
    SubdivideSeg(param_1, (int32_t**)&pts, lo, hi, subAmp, subAmp2);

    // ---- VALIDATION ----
    // Then: self-intersection + cross-path intersection; reject if both
    // intersection parameters are strictly in (0,1).
    int n = pts.count;
    uint8_t* P = (uint8_t*)pts.data;
    bool reject = false;

    // Nearest-neighbour adjacency pre-pass (retail FUN_1400cc850 first loop, the
    // DAT_140172268=1e6 running-min block). For each vertex i, find the closest
    // OTHER vertex by LengthSquared and reject the whole candidate unless that
    // closest index is i's immediate neighbour (i-1 or i+1). Draws NO RNG, so it
    // does not perturb the MT stream — it only rejects self-approaching polylines
    // that retail retries away (prevents merged/over-wide troughs & spurious pits).
    {
        int prevPick = -1;
        for (int i = 0; i < n && !reject; ++i) {
            int   bestIdx = -2;
            float best    = 1e6f;                     // DAT_140172268
            float* pa = (float*)(P + (size_t)i * 0x1c);
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                float* pb = (float*)(P + (size_t)j * 0x1c);
                float dvx = pa[0] - pb[0];
                float dvy = pa[1] - pb[1];
                float lensq = dvx * dvx + dvy * dvy;  // Math::Vector2::LengthSquared
                if (lensq < best) { best = lensq; bestIdx = j; }
            }
            if (bestIdx != prevPick + 2 && bestIdx != prevPick) reject = true;
            prevPick++;
        }
    }

    for (int i = 0; i < n - 1 && !reject; ++i) {
        float* a0 = (float*)(P + (size_t)i * 0x1c);
        float* a1 = (float*)(P + (size_t)(i + 1) * 0x1c);
        float ax = a0[0], ay = a0[1];
        float dAx = a1[0] - ax, dAy = a1[1] - ay;

        // self vs other (non-adjacent) segments of this path
        for (int j = 0; j < n - 1 && !reject; ++j) {
            if (j == i || j + 1 == i || j == i + 1 || j + 1 == i + 1) continue;
            float* b0 = (float*)(P + (size_t)j * 0x1c);
            float* b1 = (float*)(P + (size_t)(j + 1) * 0x1c);
            float dBx = b1[0] - b0[0], dBy = b1[1] - b0[1];
            // denom/t/u transcribed byte-faithfully from FUN_1400cc850's self-
            // intersection block (the puVar12 loop): denom = dAx*dBy - dBx*dAy,
            // t = (dBx*wy - wx*dBy)/denom, u = (dAx*wy - wx*dAy)/denom. The prior
            // port flipped denom's sign AND used a garbage u numerator
            // (dAy*wy - wx*dBy), which landed u in (0,1) for most segment pairs and
            // rejected nearly every candidate -> ccf60 placed 0 of numSecondary.
            float denom = dAx * dBy - dBx * dAy;       // fVar18
            if (denom != 0.0f) {
                float wx = ax - b0[0], wy = ay - b0[1];
                float t = (dBx * wy - wx * dBy) / denom;   // fVar20
                float u = (dAx * wy - wx * dAy) / denom;   // fVar18
                if (t > 0.0f && t < kOne && u > 0.0f && u < kOne) { reject = true; ++s_rejSelf; }
            }
        }
        if (reject) break;

        // vs existing paths (hf+0x18, count at hf+0x20)
        int pathCount = *(int*)((uint8_t*)param_1 + HF_PATHCOUNT);
        if (pathCount > 0) {
            void* aoa = *(void**)((uint8_t*)param_1 + HF_PATHLIST);
            for (int pi = 0; pi < pathCount && !reject; ++pi) {
                uint8_t* hdr = (uint8_t*)aoa + (size_t)pi * 0x10;
                int pc = *(int*)(hdr + 0x08);
                uint8_t* pdat = *(uint8_t**)(hdr + 0x00);
                for (int k = 0; k < pc - 1 && !reject; ++k) {
                    float* q0 = (float*)(pdat + (size_t)k * 0x1c);
                    float* q1 = (float*)(pdat + (size_t)(k + 1) * 0x1c);
                    float dBx = q1[0] - q0[0], dBy = q1[1] - q0[1];
                    // Same canonical crossing test as FUN_1400cc850's puVar13 loop
                    // (cross-path): denom = dAx*dBy - dBx*dAy, t = (dBx*wy - wx*dBy)/denom,
                    // u = (dAx*wy - wx*dAy)/denom. (Was sign-flipped denom + wrong u.)
                    float denom = dAx * dBy - dBx * dAy;   // fVar20
                    if (denom != 0.0f) {
                        float wx = ax - q0[0], wy = ay - q0[1];
                        float t = (dBx * wy - wx * dBy) / denom;   // fVar21
                        float u = (dAx * wy - wx * dAy) / denom;   // fVar20
                        if (t > 0.0f && t < kOne && u > 0.0f && u < kOne) { reject = true; ++s_rejCross; }
                    }
                }
            }
        }
    }

    int result;
    if (reject) {
        result = 0;
    } else {
        DeepAppendAoA((uint8_t*)param_1 + HF_PATHLIST,
                      *(int*)((uint8_t*)param_1 + HF_PATHCOUNT) + 1, &pts);
        result = 1;
    }
    if (pts.data && fn_MemFreeStatic) fn_MemFreeStatic(pts.data);
    return result;
}

// ============================================================
// FUN_1400cd150  @0x1400cd150  —  wg_f390_ranges (EmitMountainRanges).
//
//   rangeCount = minRangeCount + Rand( maxRangeCount - minRangeCount + 1 )
//   bandX      = minBandX + Rand( maxBandX - minBandX )
//   yExtent    = (dim/2 - 2) - bandX            (vertical span from map center)
//   for each range (rangeCount):
//     dx = RandFloatTol() * yExtent             (range center jitter, x)
//     dy = RandFloatTol() * yExtent             (range center jitter, y)
//     amp = 0.5                                  (fVar19, grows per satellite)
//     for each satellite (satellitesPerRange):
//       radius = RandFloat()*(radiusMin - radiusMax) + radiusMax
//       g      = RandFloat()
//       _t     = RandFloatTol()                  (feeds the normalize input pair)
//       amp    = g*amp + amp
//       n = normalize( RandFloatTol(), RandFloatTol() )   (random unit dir)
//       reach = bandX - radius
//       satX = reach * n.x * amp + dy
//       satY = reach * n.y * amp + dx
//       listC.push_back( { satX, satY, radius } )            // +0x38
//     listC.push_back(   { dy, dx, bandX - radiusMin } )     // +0x38  (range center)
//     detailRecs.push_back( { dy, dx, bandX } )              // +0x48  (per-range record)
//
//   HEIGHT EFFECT: writes NO heights. Populates listC/detailRecs that the later
//   stamp stages (e910/ec50) read to RAISE radial mountain bumps (additive MAX
//   mask, exp 2.0, strength 1.778). Bump magnitude = the "radius" fields above.
// ============================================================
extern "C" void wg_f390_ranges(void* hf) {
    rg_EnsureInit();
    void* rng = (uint8_t*)hf + HF_RANDOM;

    int32_t* sizeRow = *(int32_t**)((uint8_t*)hf + HF_SIZEROW);
    if (!sizeRow) return;
    int dim          = sizeRow[0];
    int minRangeCnt  = sizeRow[0x0a];                       // +0x28
    int maxRangeCnt  = sizeRow[0x0b];                       // +0x2c
    int minBandX     = sizeRow[0x0c];                       // +0x30
    int maxBandX     = sizeRow[0x0d];                       // +0x34
    int satPerRange  = *(int*)((uint8_t*)sizeRow + 0x38);
    int radiusMax    = *(int*)((uint8_t*)sizeRow + 0x3c);
    int radiusMin    = *(int*)((uint8_t*)sizeRow + 0x40);

    uint32_t rRange = fn_Random_Rand ? fn_Random_Rand(rng, (uint32_t)(maxRangeCnt - minRangeCnt + 1)) : 0;
    uint32_t rBand  = fn_Random_Rand ? fn_Random_Rand(rng, (uint32_t)(maxBandX - minBandX)) : 0;
    int bandX       = (int)rBand + minBandX;                // iVar10
    int rangeCount  = (int)rRange + minRangeCnt;            // uVar8 + iVar2
    if (rangeCount <= 0) return;

    float fBandX  = (float)bandX;                           // fVar21
    float yExtent = (float)((dim >> 1) - 2) - fBandX;       // fVar20

    for (int r = 0; r < rangeCount; ++r) {
        float dx = rftol(rng) * yExtent;                    // fVar14
        float dy = rftol(rng) * yExtent;                    // fVar15
        float amp = kHalf;                                  // fVar19

        for (int s = 0; s < satPerRange; ++s) {
            float radius = rfloat(rng) * (float)(radiusMin - radiusMax) + (float)radiusMax;  // fVar18
            float g      = rfloat(rng);                     // fVar16
            float t3     = rftol(rng);                      // fVar17 (3rd RandFloatTol)
            amp = g * amp + amp;                            // fVar19 = fVar16*fVar19 + fVar19

            // random unit direction. The original packs {local_res18, fVar17}:
            //   local_res18 = RandFloatTol() (drawn AFTER fVar17), local_res1c = fVar17.
            float dir[2];
            dir[1] = t3;                                    // local_res1c = fVar17
            dir[0] = rftol(rng);                            // local_res18 (4th RandFloatTol)
            vec2_normalize2(dir);

            float reach = fBandX - radius;                  // fVar18 = fVar21 - radius
            float satX = reach * dir[0] * amp + dy;         // local_res20
            float satY = reach * dir[1] * amp + dx;         // fStackX_24
            float rec[3] = { satX, satY, radius };
            AppendVec3((uint8_t*)hf + HF_LISTC, *(int*)((uint8_t*)hf + HF_LISTC_CNT) + 1, rec);
        }

        // range center -> listC (+0x38): {dy, dx, bandX - radiusMin}
        float center[3] = { dy, dx, (float)(bandX - radiusMin) };
        AppendVec3((uint8_t*)hf + HF_LISTC, *(int*)((uint8_t*)hf + HF_LISTC_CNT) + 1, center);

        // per-range record -> detailRecs (+0x48): {dy, dx, bandX}
        float detail[3] = { dy, dx, fBandX };
        AppendVec3((uint8_t*)hf + HF_DETAILRECS, *(int*)((uint8_t*)hf + HF_DETAIL_CNT) + 1, detail);
    }
}

// ============================================================
// FUN_1400ccf60  @0x1400ccf60  —  wg_f390_paths (EmitSecondaryPaths).
//
//   numSecondaryPaths = sizeRow[+0x20]
//   for each of numSecondaryPaths:
//     retry up to 1000 times:
//       branchSel = (float)Rand( pathCount )   ; pathCount = hf+0x20 (live count)
//       ok = TryPlacePath(hf, dim+1, (float)dim, 1.5, 8.0, 1.25, 1.5, 16.0, branchSel)
//       if ok: break
//
//   params (address=value): param_4=1.5(0x140172160), param_5=8.0(0x1401721a8),
//   param_6=1.25(0x140172158), param_7=1.5(0x140172160), param_8=16.0(0x1401721c0).
//   branchSel >= 0 here (Rand returns >=0), so every secondary path BRANCHES off
//   an existing path (tributaries of the coastline/river ring placed by cd090).
//
//   HEIGHT EFFECT: writes NO heights. Appends validated polylines to hf+0x18,
//   later carved DOWN toward sea level by the river/stamp stages (maskA, MIN
//   mask) — secondary channels/valleys branching off the main paths.
// ============================================================
extern "C" void wg_f390_paths(void* hf) {
    rg_EnsureInit();
    int32_t* p = (int32_t*)hf;

    int32_t* sizeRow = *(int32_t**)((uint8_t*)hf + HF_SIZEROW);
    if (!sizeRow) return;
    int numSecondary = *(int*)((uint8_t*)sizeRow + 0x20);
    if (numSecondary <= 0) return;

    void* rng = (uint8_t*)hf + HF_RANDOM;
    int dim = *(int*)((uint8_t*)hf + HF_GRIDSIZE);   // *param_1 (== mapDim*2, set by f390)

    // float path params, packed to the uint32 ABI cc850 expects (it stores them
    // verbatim into the per-point band fields).
    const float P4 = kPathP47;   // 1.5  (0x140172160)
    const float P5 = kPathP5;    // 8.0  (0x1401721a8)
    const float P6 = kPathP46;   // 1.25 (0x140172158)
    const float P7 = kPathP47;   // 1.5  (0x140172160)
    const float P8 = kPathP8;    // 16.0 (0x1401721c0)
    uint32_t u4, u5, u6, u7, u8;
    __builtin_memcpy(&u4, &P4, 4); __builtin_memcpy(&u5, &P5, 4);
    __builtin_memcpy(&u6, &P6, 4); __builtin_memcpy(&u7, &P7, 4);
    __builtin_memcpy(&u8, &P8, 4);

    int placed = 0;                        // [WATER-DIAG]
    int startCount = *(int*)((uint8_t*)hf + HF_PATHCOUNT);
    for (int i = 0; i < numSecondary; ++i) {
        for (int attempt = 0; attempt < 1000; ++attempt) {
            int pathCount = *(int*)((uint8_t*)hf + HF_PATHCOUNT);
            // Defensive: secondary paths branch off an existing path. If none exists
            // (coastline ring not placed), there is nothing to branch from — skip
            // rather than null-deref the empty patchList in TryPlacePath's branch path.
            if (pathCount <= 0) break;
            float branchSel = (float)(fn_Random_Rand ? fn_Random_Rand(rng, (uint32_t)pathCount) : 0u);
            int ok = TryPlacePath(p, dim + 1, (float)dim, u4, u5, u6, u7, u8, branchSel);
            if (ok) { ++placed; break; }
        }
        // re-read the loop bound each outer iteration (matches the original).
        numSecondary = *(int*)((uint8_t*)sizeRow + 0x20);
    }

    if (getenv("BANISHED_WATER_DIAG")) {
        fprintf(stderr,
            "[WATER-DIAG] ccf60 secondary paths: requested=%d placed=%d "
            "patchList %d->%d  rejects(self=%ld cross=%ld)\n",
            numSecondary, placed, startCount,
            *(int*)((uint8_t*)hf + HF_PATHCOUNT), s_rejSelf, s_rejCross);
        fflush(stderr);
    }
}
