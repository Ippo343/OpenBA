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
//     * cd150 pushes "satellite" + "center" points into the radial-stamp list
//       (+0x38) and one record per range into the water-feature list (+0x48).
//       CORRECTION (verified against retail 0x1400ceac2/aca and the eee0 call at
//       worldgen_f390_rivers.cpp): e910/ec50 stamp the +0x38 entries into maskC
//       with MAX, strength 2.0, exp 2.25 — and the final height is
//       `h = maskB*h*maskA - maskC`, i.e. maskC is SUBTRACTED. These CARVE
//       circular depressions; they do NOT raise terrain. The older "positive
//       radial bumps / RAISE terrain into mountain ranges" note here was wrong
//       and sent a reader looking for a sign bug that does not exist.
//       The +0x48 records are what b260 phase 2 reads as its tarn features.
//     * ccf60 pushes validated polylines (river/coast branch paths) into the
//       array-of-arrays at hf+0x18 (via cc850's deep-append at param_1+0x18).
//       The river network (cd460) + stamp stages later carve these DOWN toward
//       sea level (maskA, a MIN mask, exp 1.33) — channels/valleys.
//   So in isolation these stages change NO height values; their magnitude shows
//   up only after e910/ec50 run. Recorded magnitudes (for the orchestrator):
//     - mountain center "radius" field = bandX - radiusMin  (in grid units)
//     - mountain satellite "radius" field = RandFloat*(radiusMin-radiusMax)+radiusMax
//     - both are CARVE radii: e910/ec50 stamp them into maskC (MAX, str 2.0,
//       exp 2.25) and SUBTRACT — round lake pits, not peaks (see CORRECTION above).
//     - secondary paths are zero-height polylines; they become carve channels.
// ----------------------------------------------------------------------------
//
// All offsets verified against live Ghidra disassembly of Application-x64.exe.

#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Heightfield scratch-struct field offsets (blueprint §1.1).
// (mirrors worldgen.cpp; kept local to this TU)
// ============================================================
static constexpr int HF_GRIDSIZE   = 0x00;   // int  dim (== sizeRow[0]*2)
static constexpr int HF_PATHLIST   = 0x18;   // array-of-arrays of polylines (cc850 deep-appends here)
static constexpr int HF_PATHCOUNT  = 0x20;   // int  count of paths in HF_PATHLIST (branch-select bound)
static constexpr int HF_BASINSTAMPS = 0x38;  // radial carve stamps (0xC elem {x,y,radius}) -> maskC, SUBTRACTED
static constexpr int HF_BASIN_CNT   = 0x40;  // int  count of the above
static constexpr int HF_WATERFEATS  = 0x48;  // b260 phase-2 tarn features (0xC elem {x,y,radius=bandX})
static constexpr int HF_WATERFEATS_CNT = 0x50;   // int  count of the above
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
// Vector2::Normalize(this)  (in-place use of the by-value member; slot 0x1401526b0)
// ============================================================
// The ResolveExport below looked up "?Normalize@Vector2@Math@@QEAAAEAV12@XZ"
// (an in-place, non-const overload) which does NOT exist in Runtime-x64.dll's
// export table (verified with pefile 2026-09-06) — the DLL has no in-place
// Vector2::Normalize at all, only a by-value one returning through a hidden
// RDX slot (see runtime/types/math/Math.h's ABI note):
//   ?Normalize@Vector2@Math@@QEBA?AV12@XZ   retail 0x18006c600
// This adapter calls the real member and copies its result back into `self` so
// the in-place call sites below (vec2_normalize2) don't change (B3R: called by
// name; the pointer cache and its init went).
static float* vec2_normalize_real(float* self) {
    Math::Vector2 out = reinterpret_cast<const Math::Vector2*>(self)->Normalize();
    *reinterpret_cast<Math::Vector2*>(self) = out;
    return self;
}


// float negate matching the exe's `(uint)x ^ 0x80000000`.
static inline float fneg(float x) {
    uint32_t u; __builtin_memcpy(&u, &x, 4); u ^= kSignMask;
    float r; __builtin_memcpy(&r, &u, 4); return r;
}
// Vector2::Normalize in place.
static inline void vec2_normalize2(float* v) {
    vec2_normalize_real(v);
}
static inline float rftol(void* rng) {
    return reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
}
static inline float rfloat(void* rng) {
    return reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
}

// ============================================================
// FUN_1400245d0  @0x1400245d0  —  AppendVec3 (0xC-stride resize-and-fill).
// Resizes the {data,count,cap} header at `hdr` to `newCount` (grow ×3 on the
// count+1 fast path), copying any tail-overrun slots from `src` (a 12-byte
// {float x,float y,float z} record). Callers always pass newCount = count+1 and
// src = the element to push, so the net effect is push_back(src). The original's
// 4-wide unrolled copy is purely a perf optimization of this element-wise fill.
//
// ENDGAME B.3.2 step 2: now the 3-argument `Collection::Array<WgVec3Rec>::SetSize`
// (src is the fill). Trait-derived row, and it IS trait-equivalent: the element
// is three floats, `assign` a 12-byte copy (byteCopy == is_trivially_copyable),
// no destroy, and the 2-argument form's `init` is never reached by a fill call.
// Compared arm for arm against rtcoll::SetSizeCore: exact-equal early return,
// `_size` first, treble-on-single-append-from-non-empty, cap*0xC at align 4,
// prefix copied only when the block moved, fill for every index at or past the
// old count, old block freed only when it moved. The only dropped line was the
// `if (!data) return` out-of-memory bail.
// ============================================================
struct WgVec3Rec { float x, y, z; };
static_assert(sizeof(WgVec3Rec) == 0x0c && alignof(WgVec3Rec) == 4, "0xC-stride record");
using WgVec3Array = Collection::Array<WgVec3Rec>;

static void AppendVec3(void* hdr, int newCount, const float* src /*x,y,z*/) {
    // B.3.2 bridge: hf+0x38 / hf+0x48 are still raw {data,count,cap} triples.
    reinterpret_cast<WgVec3Array*>(hdr)->SetSize(newCount,
                                                *reinterpret_cast<const WgVec3Rec*>(src));
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
    if (*(void**)(d + 0x00)) System::MemoryAllocator::MemFree(*(void**)(d + 0x00));
    *(void**)(d + 0x00) = nullptr;
    *(int*)(d + 0x08) = 0;
    int cnt = *(int*)(s + 0x08);
    *(int*)(d + 0x08) = cnt;
    void* nd = nullptr;
    if (cnt > 0) {
        *(int*)(d + 0x0c) = cnt;
        nd = System::MemoryAllocator::MemAlloc((uint64_t)cnt * 0x1c, 4);
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
//
// ENDGAME B.3.2 step 2: now `Collection::Array<>::SetSizeFill` over the
// `kSpec_RgPathVec` row (this file's twin of worldgen_f390_coastline.cpp's
// kSpec_WgPathVec, kept separate so its `assign` keeps driving this file's
// CopyVec1c). Compared arm for arm against rtcoll::SetSizeCore and the retail
// body 0x140024460 (re-decompiled 2026-09-13): exact-equal early return, `_size`
// first, treble rule, cap*16 at align 8, per element zero-the-header then deep
// copy (prefix only when the block moved, otherwise the fill), old inner buffers
// freed then the old block when it moved. ONE arm this transcription LACKED and
// retail HAS: destroy of the truncated tail on an in-place shrink (free the
// inner buffer, zero the header). Unreachable from this file: the only caller
// (TryPlacePath, below) passes newCount = count + 1, so the array never shrinks
// here. The row carries retail's arm.
// ============================================================
static void AssignRgPathVec(uint8_t* d, const uint8_t* s) {
    *(void**)d = nullptr;
    *(uint64_t*)(d + 8) = 0;
    CopyVec1c(d, const_cast<uint8_t*>(s));
}
static void InitRgPathVec(uint8_t* d) {              // Collection::Array's ctor; no site here reaches it
    *(uint64_t*)(d + 0) = 0;
    *(uint64_t*)(d + 8) = 0;
}
static void DestroyRgPathVec(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i) {
        uint8_t* slot = base + (size_t)i * 0x10;
        rtcoll::MemFree(*(void**)slot);               // null-guarded inside
        *(void**)slot = nullptr;
        *(uint64_t*)(slot + 8) = 0;
    }
}
// stays explicit: `assign` deep-copies the inner vector (CopyVec1c) and
// `destroy` frees its buffer — byteCopy=false plus a destroy, which no C++
// element type can express.
static const rtcoll::ArraySpec kSpec_RgPathVec = {  // exe 0x140024460
    "Array<Array<PathPoint>>", 0x10, 8,
    &AssignRgPathVec, &InitRgPathVec, &DestroyRgPathVec, /*byteCopy*/false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using RgPathVecArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                         Collection::ExplicitSpec<&kSpec_RgPathVec>>;

static void DeepAppendAoA(void* hdr, int newCount, void* srcVec) {
    // B.3.2 bridge: hf+0x18 is still a raw {data,count,cap} triple.
    reinterpret_cast<RgPathVecArray*>(hdr)->SetSizeFill(newCount, srcVec);
}

// ============================================================
// FUN_140023b70  @0x140023b70  —  ResizeVec1c: resize a 0x1C-stride
// {data,count,cap} vector to `newCount` (grow ×3 on count+1), copying retained
// elements and zeroing the new tail. cc850 calls this then overwrites the tail.
//
// ENDGAME B.3.2 step 2: now `Collection::Array<>::SetSize` over this site's own
// zeroing 0x1C-byte element (the closing pass collapsed what was a hand-derived
// `kSpec_WgPathPointZeroInit` row in worldgen_f390_coastline.cpp onto the
// trait-derived form — the row was a whole-stride byte copy, a whole-stride zero
// tail, no destroy and no deliberately-untouched extent, which is precisely what
// `rtcoll::SpecFor<GameColl::Bytes<0x1C,4>>()` derives).
// Compared arm for arm against rtcoll::SetSizeCore and the retail body
// 0x140023b70 (re-decompiled 2026-09-13): every structural arm matched.
//
// The new-tail arm is THIS SITE'S, not retail's and not the coastline twin's.
// One retail body has two transcriptions in this tree: this one zeroes all 0x1C
// bytes of a new record, coastline's zeroes none, and retail's own arm is a
// single QWORD store. Both of ours are believed unobservable — `pts` is a fresh
// local here, so every record is new, and the loop right after the resize writes
// [8,0x1C) of every record before anything reads them — but this refactor's
// contract is no behaviour change in a deterministic world build, so each site
// keeps its own arm and the reconciliation onto retail's is left as a separate,
// separately-verified change. Coastline's arm is `kSpec_WgPathPointNoInit`,
// which stays explicit because "leave the whole stride as found" is a probe
// contract; this one is an element type.
// ============================================================
using WgPathPointElem = GameColl::Bytes<0x1c, 4>;   // exe 0x140023b70, ranges arm
static_assert(sizeof(WgPathPointElem)  == 0x1c, "WgPathPointElem stride");
static_assert(alignof(WgPathPointElem) == 4,    "WgPathPointElem align");
using WgPathPointArray = Collection::Array<WgPathPointElem>;

static void ResizeVec1c(void* hdr, int newCount) {
    // B.3.2 bridge: the caller's header is still a raw {data,count,cap} triple.
    reinterpret_cast<WgPathPointArray*>(hdr)->SetSize(newCount);
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

    // ENDGAME B.3.2 step 2: a real Collection::Array with RAII; the manual free
    // at the single exit below is the destructor now (the early `return 0` on
    // a null buffer had nothing to free either way).
    WgPathPointArray pts;
    ResizeVec1c(&pts, param_2);
    if (!pts._data) return 0;

    uint8_t* pd = (uint8_t*)pts._data;
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
        int last = pts._size - 1;
        float* p0 = (float*)pd;
        p0[0] = 0.0f; p0[1] = param_3 * kNegHalf;     // first: {0, scale*-0.5}

        uint32_t r = reinterpret_cast<System::Random*>(rng)->Rand(3);   // B.3.4b bridge
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
        float* pm = (float*)(pd + (size_t)(pts._size >> 1) * 0x1c);
        pm[0] = jx; pm[1] = jy;

        subAmp2 = param_3 * kQuarter;                 // fVar22
        subAmp  = param_3 * kHalf;                    // fVar24
        SubdivideSeg(param_1, (int32_t**)&pts, 0, pts._size >> 1, subAmp, subAmp2);
        lo = pts._size >> 1;
        hi = pts._size - 1;
    } else {
        // ---- BRANCH off existing path[branchSel] ----
        void* aoa = *(void**)((uint8_t*)param_1 + HF_PATHLIST);   // hf+0x18 data
        uint8_t* selHdr = (uint8_t*)aoa + (size_t)(int)param_9 * 0x10;
        int selCount = *(int*)(selHdr + 0x08);
        int q = (selCount + ((selCount >> 31) & 3)) >> 2;          // selCount/4 (toward 0)
        int startIdx = reinterpret_cast<System::Random*>(rng)->Rand(q, q * 3);   // B.3.4b bridge
        uint8_t* selData = *(uint8_t**)(selHdr + 0x00);
        *(uint64_t*)pd = *(uint64_t*)(selData + (size_t)startIdx * 0x1c);  // first pt = selected pt {x,y}

        float jitter = rftol(rng) * param_3 * kHalf;  // fVar17
        int last = pts._size - 1;
        float* pl = (float*)(pd + (size_t)last * 0x1c);
        uint32_t r = reinterpret_cast<System::Random*>(rng)->Rand(4);   // B.3.4b bridge
        float half = param_3 * kHalf;                 // fVar24 = scale*0.5
        // CONCAT44(hi,lo) -> pl[0]=lo, pl[1]=hi
        if (r == 0)      { pl[0] = jitter;            pl[1] = half;             } // {jitter, scale*0.5}
        else if (r == 1) { pl[0] = half;             pl[1] = jitter;           } // {scale*0.5, jitter}
        else if (r == 2) { pl[0] = param_3*kNegHalf; pl[1] = jitter;           } // {scale*-0.5, jitter}
        else /* r==3 */  { pl[0] = jitter;           pl[1] = param_3*kNegHalf; } // {jitter, scale*-0.5}

        subAmp2 = param_3 * kQuarter;
        subAmp  = param_3 * kHalf;
        lo = 0;
        hi = pts._size - 1;
    }
    SubdivideSeg(param_1, (int32_t**)&pts, lo, hi, subAmp, subAmp2);

    // ---- VALIDATION ----
    // Then: self-intersection + cross-path intersection; reject if both
    // intersection parameters are strictly in (0,1).
    int n = pts._size;
    uint8_t* P = (uint8_t*)pts._data;
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
                if (t > 0.0f && t < kOne && u > 0.0f && u < kOne) { reject = true; }
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
                        if (t > 0.0f && t < kOne && u > 0.0f && u < kOne) { reject = true; }
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
//     amp = 0.5                                  (fVar19, RESET to 0.5 per satellite)
//     for each satellite (satellitesPerRange):
//       radius = RandFloat()*(radiusMin - radiusMax) + radiusMax
//       g      = RandFloat()
//       _t     = RandFloatTol()                  (feeds the normalize input pair)
//       amp    = g*0.5 + 0.5                     (retail reloads 0.5 each iteration)
//       n = normalize( RandFloatTol(), RandFloatTol() )   (random unit dir)
//       reach = bandX - radius
//       satX = reach * n.x * amp + dy
//       satY = reach * n.y * amp + dx
//       listC.push_back( { satX, satY, radius } )            // +0x38
//     listC.push_back(   { dy, dx, bandX - radiusMin } )     // +0x38  (range center)
//     detailRecs.push_back( { dy, dx, bandX } )              // +0x48  (per-range record)
//
//   HEIGHT EFFECT: writes NO heights. Populates listC/detailRecs that the later
//   stamp stages (e910/ec50) read to CARVE round lake pits (maskC MAX, exp 2.25,
//   strength 2.0, SUBTRACTED). Carve radius = the "radius" fields above.
// ============================================================
extern "C" void wg_f390_ranges(void* hf) {
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

    uint32_t rRange = reinterpret_cast<System::Random*>(rng)->Rand((uint32_t)(maxRangeCnt - minRangeCnt + 1));   // B.3.4b bridge
    uint32_t rBand  = reinterpret_cast<System::Random*>(rng)->Rand((uint32_t)(maxBandX - minBandX));   // B.3.4b bridge
    int bandX       = (int)rBand + minBandX;                // iVar10
    int rangeCount  = (int)rRange + minRangeCnt;            // uVar8 + iVar2
    if (rangeCount <= 0) return;

    float fBandX  = (float)bandX;                           // fVar21
    float yExtent = (float)((dim >> 1) - 2) - fBandX;       // fVar20

    for (int r = 0; r < rangeCount; ++r) {
        float dx = rftol(rng) * yExtent;                    // fVar14
        float dy = rftol(rng) * yExtent;                    // fVar15

        for (int s = 0; s < satPerRange; ++s) {
            float radius = rfloat(rng) * (float)(radiusMin - radiusMax) + (float)radiusMax;  // fVar18
            float g      = rfloat(rng);                     // fVar16
            float t3     = rftol(rng);                      // fVar17 (3rd RandFloatTol)
            // fVar19 = fVar16*fVar19 + fVar19, but retail RELOADS fVar19 = DAT_140172114
            // (0.5) at the tail of EVERY satellite iteration, so amp is per-satellite in
            // [0.5, 1.0] — never compounding. reach*amp <= bandX keeps every satellite
            // inside the range's bandX radius (= the +0x48 water-feature radius b260
            // phase 2 meshes).
            float amp = g * kHalf + kHalf;

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
            AppendVec3((uint8_t*)hf + HF_BASINSTAMPS, *(int*)((uint8_t*)hf + HF_BASIN_CNT) + 1, rec);
        }

        // range center -> radial carve stamps (+0x38): {dy, dx, bandX - radiusMin}
        float center[3] = { dy, dx, (float)(bandX - radiusMin) };
        AppendVec3((uint8_t*)hf + HF_BASINSTAMPS, *(int*)((uint8_t*)hf + HF_BASIN_CNT) + 1, center);

        // per-range record -> b260 phase-2 tarn features (+0x48): {dy, dx, bandX}
        float detail[3] = { dy, dx, fBandX };
        AppendVec3((uint8_t*)hf + HF_WATERFEATS, *(int*)((uint8_t*)hf + HF_WATERFEATS_CNT) + 1, detail);
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

    for (int i = 0; i < numSecondary; ++i) {
        for (int attempt = 0; attempt < 1000; ++attempt) {
            int pathCount = *(int*)((uint8_t*)hf + HF_PATHCOUNT);
            // Defensive: secondary paths branch off an existing path. If none exists
            // (coastline ring not placed), there is nothing to branch from — skip
            // rather than null-deref the empty patchList in TryPlacePath's branch path.
            if (pathCount <= 0) break;
            float branchSel = (float)reinterpret_cast<System::Random*>(rng)->Rand((uint32_t)pathCount);   // B.3.4b bridge
            int ok = TryPlacePath(p, dim + 1, (float)dim, u4, u5, u6, u7, u8, branchSel);
            if (ok) break;
        }
        // re-read the loop bound each outer iteration (matches the original).
        numSecondary = *(int*)((uint8_t*)sizeRow + 0x20);
    }
}
