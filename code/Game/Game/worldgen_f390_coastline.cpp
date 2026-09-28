// code/Game/Game/worldgen_f390_coastline.cpp
// ============================================================================
// f390 worldgen pipeline — COASTLINE-RING stage.
// ============================================================================
//
//   FUN_1400cd090  (EmitCoastlineRing)   -> wg_f390_coastline   [stage entry]
//     -> FUN_1400cc850 (TryPlacePath)    -> coastline_TryPlacePath_cc850
//        -> FUN_140023b70 (resize 0x1c)  -> coastline_Resize_Stride1C_23b70
//        -> FUN_1400cc510 (Subdivide)    -> coastline_Subdivide_cc510
//        -> FUN_140024460 (AoA append)   -> coastline_AppendPath_24460
//           -> FUN_140024ac0 (deep copy) -> coastline_DeepCopyPath_24ac0
//
// PURPOSE OF THIS STAGE (verified from disassembly):
//   cd090 builds ONE closed coastline ring polyline (a 2D point loop in the
//   horizontal plane, stored as a polygon in the heightfield "patchList" vector
//   at hf+0x18). It is a PATH GENERATOR: it produces (x,y) polyline geometry +
//   per-vertex {param_4..param_8} attributes. It does NOT touch the float
//   heightmap at hf+0x08 at all (no reads, no writes). The ring it emits is
//   consumed LATER by the feature-stamp stages (e910 / ec50, via the patchList),
//   which carve/raise the heightmap along it. So the HEIGHT EFFECT of THIS stage
//   in isolation is ZERO (see header RETURN item 3).
//
//   The whole stage is a rejection-sampling loop: keep generating a candidate
//   closed ring (fractal midpoint-displaced loop) until one is produced that has
//   no self-intersections and no crossings with previously-placed paths, then
//   deep-append it into the patchList. cd090 loops cc850 until the patchList
//   count (hf+0x20) becomes nonzero — i.e. until exactly one ring lands.
//
// This file ADDS a self-contained reimpl of the coastline stage. All sub-workers
// are file-local `static`. Shared engine functions go through fn_* pointers.
//
// Field offsets verified against Ghidra disassembly of Application-x64.exe.

#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Heightfield scratch-struct offsets (blueprint §1.1; matches worldgen.cpp).
// ============================================================
static constexpr int HF_GRIDSIZE   = 0x00;   // int gridSize (= mapDim*2), read by cd090 as *param_1
static constexpr int HF_PATCHLIST  = 0x18;   // {void* data; int count; int cap} vector<vector<PathPt>>
static constexpr int HF_DETAILCNTA = 0x20;   // int — cd090's loop guard (== patchList.count)
static constexpr int HF_RANDOM     = 0x58;   // System::Random, seeded

// ============================================================
// PathPoint record: 0x1C bytes.
//   +0x00 float x        (point position X)
//   +0x04 float y        (point position Y)
//   +0x08 float a4       (param_4 attribute — propagated to every vertex)
//   +0x0C float a5       (param_5)
//   +0x10 float a6       (param_6)
//   +0x14 float a7       (param_7)
//   +0x18 float a8       (param_8)
// The patchList element is a vector<PathPoint> = {void* data; int count; int cap}
// (16 bytes per outer element).
// ============================================================
static constexpr int PATHPT_STRIDE = 0x1c;

// ============================================================
// .rdata float constants (all verified via read_memory; address = value).
// ============================================================
// cd090 (the ring-stage args):
static constexpr float kRing_a4_2_0   = 2.0f;   // 0x140172174 = 0x40000000
static constexpr float kRing_a5_10_0  = 10.0f;  // 0x1401721b0 = 0x41200000
static constexpr float kRing_a6_1_25  = 1.25f;  // 0x140172158 = 0x3fa00000
static constexpr float kRing_a78_4_0  = 4.0f;   // 0x140172194 = 0x40800000
// cc850 / cc510:
static constexpr float kHalf          = 0.5f;       // 0x140172114 = 0x3f000000
static constexpr float kNegHalf       = -0.5f;      // 0x140172278 = 0xbf000000
static constexpr float kJitter_0_125  = 0.125f;     // 0x1401720ec = 0x3e000000  (closed-ring midpoint jitter amp)
static constexpr float kQuarter       = 0.25f;      // 0x140172100 = 0x3e800000  (subdiv coord amplitude seed)
static constexpr float kBigDist       = 1000000.0f; // 0x140172268 = 0x49742400  (nearest-neighbour init = INF)
static constexpr float kMidAmp_0_666  = 0.66602f;   // 0x14017211c = 0x3f2a7efa  (midpoint displacement amp factor)
static constexpr float kOne           = 1.0f;       // 0x14017214c = 0x3f800000  (g_fUVScale; intersection (0,1) bound)
static constexpr uint32_t kSignMask   = 0x80000000u;// 0x1401722c0  (float-negate XOR mask)

// ============================================================
// Late-bound Runtime resolves (not in engine_funcs.h yet).
//   System::Random::RandFloatTol() -> float in [-1,1]
//   System::Random::Rand(int max)  -> [0,max)
//   System::Random::Rand(int min,int max) -> [min,max)
//   Math::Vector2::Length()        const -> float
//   Math::Vector2::LengthSquared() const -> float
//   Math::Vector2::Normalize()     -> Vector2& (returns self ptr; normalizes in place)
// ============================================================
// The three ResolveExport calls below used to look up mangled names that DO
// NOT EXIST in Runtime-x64.dll's export table (verified with pefile 2026-09-06):
//   "?Length@Vector2@Math@@QEBAMXZ"        (assumed a plain-float return)
//   "?LengthSquared@Vector2@Math@@QEBAMXZ" (assumed a plain-float return)
//   "?Normalize@Vector2@Math@@QEAAAEAV12@XZ" (assumed an in-place, non-const overload)
// So the three lookups always resolved null and every call silently ran the
// fallback instead of the DLL. The real exports exist
// under different signatures (Length/LengthSquared return Math::Scalar via a
// hidden RDX return-slot pointer, per the ABI note in
// runtime/types/math/Math.h; Normalize has NO in-place overload, only a
// by-value one). These adapters call the real members and preserve this file's
// existing call-site shape (float return / in-place self-mutation); they are
// called by name (B3R: the pointer cache and its init went).
static float coast_vec2len_real(void* self) {
    return reinterpret_cast<const Math::Vector2*>(self)->Length().v;
}
static float coast_vec2lensq_real(void* self) {
    return reinterpret_cast<const Math::Vector2*>(self)->LengthSquared().v;
}
static void* coast_vec2norm_real(void* self) {
    // Retail has no in-place Normalize, only the by-value one; copy back.
    Math::Vector2 out = reinterpret_cast<const Math::Vector2*>(self)->Normalize();
    *reinterpret_cast<Math::Vector2*>(self) = out;
    return self;
}

// Helper: bit-cast negate (float XOR 0x80000000), matching the original's
// integer sign-flip (avoids -0.0 / signalling-NaN ABI surprises).
static inline float fnegate(float v) {
    uint32_t b; __builtin_memcpy(&b, &v, 4); b ^= kSignMask;
    float r; __builtin_memcpy(&r, &b, 4); return r;
}

// ============================================================
// FUN_140023b70 — resize a {void* data; int count; int cap} vector whose
// element stride is 0x1C bytes, copying existing records on grow.
// Exe addr: 0x140023b70.  (Same grow-3x idiom as the shared Stride04/16 helpers,
// but stride 0x1C is unique to PathPoint, so we keep a private copy.)
//
// Faithful transcription: on grow it MemAllocs cap*0x1C, copies the old [0,count)
// records (each 0x1C bytes = x/y + the 6 attribute dwords... 0x1C/4 = 7 dwords)
// forward, frees the old buffer. We simplify the decompiler's manually-unrolled
// 4-at-a-time copy into a straight per-record loop — byte-identical result.
//
// ENDGAME B.3.2 step 2: the hand-written grow below is now one
// `Collection::Array<>::SetSize` over the `kSpec_WgPathPoint` row. Compared arm
// for arm against rtcoll::SetSizeCore and against the retail body (0x140023b70,
// re-decompiled 2026-09-13): exact-equal early return, `_size` stored before the
// capacity test, treble-on-single-append-from-non-empty, cap*0x1C at align 4,
// copy the preserved prefix only when the block moved, free the old block only
// then, no destroy.
//
// THE NEW-TAIL ARM IS SITE-EXACT, NOT SHARED. One retail body (0x140023b70) has
// two transcriptions in this tree and they disagree about the new tail: THIS one
// leaves the record untouched, and the twin in worldgen_f390_ranges.cpp
// (ResizeVec1c) zeroes all 0x1C bytes. Retail's own arm is a single QWORD store
// of 0 (x,y zeroed, [8,0x1C) untouched), so neither transcription matches it.
// Both are believed unobservable — each caller writes [8,0x1C) of every record
// immediately after the resize and every x,y before any read — but "believed
// unobservable" is not a licence to change world generation inside a refactor
// whose contract is no behaviour change (B32_RULES / B31_RULES step 2). So each
// site keeps ITS OWN arm: `kSpec_WgPathPointNoInit` here, and for ranges the
// zeroing element `Collection::Array<GameColl::Bytes<0x1C,4>>` it now spells
// itself (ENDGAME B.3.2 closing pass — that arm's row was a whole-stride byte
// copy plus a whole-stride zero tail with no destroy and no untouched extent,
// i.e. exactly what `SpecFor<Bytes<0x1C,4>>()` derives, so the shared row object
// this file used to export for it is gone). Collapsing the two arms onto
// retail's single-QWORD arm is a separate, separately-verified change.
// ============================================================
// The rows this stage instantiates. EXTERNAL linkage on purpose:
// worldgen.cpp (wg_FreePatchList, the hf+0x18 teardown) names the PathVec row
// through an `extern` declaration — the same row instance.
static void AssignWgPathPoint(uint8_t* d, const uint8_t* s) {
    __builtin_memcpy(d, s, PATHPT_STRIDE);          // 7 dwords, the whole stride
}
// This site's arm: NOTHING. `init == nullptr` is how an ArraySpec spells "the
// new tail keeps whatever the allocation contained", which is exactly what the
// hand-written loop here did (it only ran for i < oldCount).
// stays explicit: the whole stride is a deliberately-untouched extent
// (`init unwritten` = [0, 0x1C)) — the probe contract for "this site's new tail
// keeps whatever the allocation held", which no element type can state.
extern const rtcoll::ArraySpec kSpec_WgPathPointNoInit;
const rtcoll::ArraySpec kSpec_WgPathPointNoInit = {  // exe 0x140023b70, coastline arm
    "Array<PathPoint> (tail untouched)", PATHPT_STRIDE, 4,
    &AssignWgPathPoint, nullptr, nullptr, /*byteCopy*/true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, PATHPT_STRIDE,
    /*seed zero       */ 0, 0 };
using WgPathPointArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                           Collection::ExplicitSpec<&kSpec_WgPathPointNoInit>>;

static void coastline_Resize_Stride1C_23b70(void* hdr, int newCount) {
    // B.3.2 bridge: the caller's header is still a raw {data,count,cap} triple.
    reinterpret_cast<WgPathPointArray*>(hdr)->SetSize(newCount);
}

// ============================================================
// FUN_140024ac0 — deep-copy a vector<PathPoint> (0x1C-stride) into a destination
// vector header, freeing the destination's prior buffer first.
// Exe addr: 0x140024ac0.
//   dst, src are {void* data; int count; int cap} headers (16 bytes).
// ============================================================
static void* coastline_DeepCopyPath_24ac0(void* dst, void* src) {
    if (dst == src) return dst;
    uint8_t* d = (uint8_t*)dst;
    uint8_t* s = (uint8_t*)src;

    if (*(void**)(d + 0x00)) System::MemoryAllocator::MemFree(*(void**)(d + 0x00));
    *(int*)(d + 0x08) = 0;          // count
    *(void**)(d + 0x00) = nullptr;  // data

    int n = *(int*)(s + 0x08);      // src count
    *(int*)(d + 0x08) = n;
    if (n > 0) {
        *(int*)(d + 0x0c) = n;      // cap
        void* nb = System::MemoryAllocator::MemAlloc((uint64_t)n * PATHPT_STRIDE, 4);
        *(void**)(d + 0x00) = nb;

        uint8_t* dd = (uint8_t*)nb;
        uint8_t* ss = *(uint8_t**)(s + 0x00);
        for (int i = 0; i < n; ++i) {
            __builtin_memcpy(dd + (size_t)i * PATHPT_STRIDE,
                             ss + (size_t)i * PATHPT_STRIDE, PATHPT_STRIDE);
        }
    }
    return dst;
}

// ============================================================
// FUN_140024460 — append one path (deep-copied) to the array-of-vectors at
// hf+0x18 (an outer vector whose elements are 16-byte vector<PathPoint> headers).
// Exe addr: 0x140024460.
//   outer  = hf+0x18  ({void* data; int count; int cap}, element = 16B header)
//   newLen = patchList.count + 1
//   srcVec = candidate path header (&local_e0 in cc850)
// On grow (cap*16, align 8) it deep-copies the surviving headers via 24ac0; the
// new tail slot is zero-initialized then 24ac0-filled from srcVec.
//
// ENDGAME B.3.2 step 2: the body is now `Collection::Array<>::SetSizeFill` (the
// 3-argument form: srcVec is the fill) over the `kSpec_WgPathVec` row. Compared
// arm for arm against rtcoll::SetSizeCore and the retail body (0x140024460,
// re-decompiled 2026-09-13): exact-equal early return, `_size` first, treble
// rule, cap*16 at align 8; per element `assign` = zero the header then
// 24ac0-deep-copy (prefix from the old block only when it moved, otherwise the
// fill); `destroy` = free the inner buffer and zero the header, applied to the
// truncated tail in place or to every old element before the old block is
// freed. Every arm matched, nothing changed. The row's `init` (the 2-argument
// form's new-tail arm) is not exercised by any site in this tree; it is the
// three zero stores of Collection::Array's constructor.
// ============================================================
static void AssignWgPathVec(uint8_t* d, const uint8_t* s) {
    *(void**)(d + 0x00) = nullptr;                  // zero the dst header first
    *(int64_t*)(d + 0x08) = 0;
    coastline_DeepCopyPath_24ac0(d, const_cast<uint8_t*>(s));
}
static void InitWgPathVec(uint8_t* d) {
    *(uint64_t*)(d + 0x00) = 0;
    *(uint64_t*)(d + 0x08) = 0;
}
static void DestroyWgPathVec(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i) {
        uint8_t* slot = base + (size_t)i * 16;
        rtcoll::MemFree(*(void**)(slot + 0x00));    // null-guarded inside
        *(void**)(slot + 0x00) = nullptr;
        *(int64_t*)(slot + 0x08) = 0;
    }
}
// stays explicit: `assign` is a deep copy (24ac0), not a byte copy, and
// `destroy` frees each element's inner buffer — byteCopy=false with a destroy is
// a combination C++ traits cannot produce.
extern const rtcoll::ArraySpec kSpec_WgPathVec;
const rtcoll::ArraySpec kSpec_WgPathVec = {         // exe 0x140024460 (grow), 0x1400175d0 (dtor)
    "Array<Array<PathPoint>>", 0x10, 8,
    &AssignWgPathVec, &InitWgPathVec, &DestroyWgPathVec, /*byteCopy*/false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using WgPathVecArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                         Collection::ExplicitSpec<&kSpec_WgPathVec>>;

static void coastline_AppendPath_24460(void* outer, int newLen, void* srcVec) {
    // B.3.2 bridge: hf+0x18 is still a raw {data,count,cap} triple in the
    // heightfield scratch struct.
    reinterpret_cast<WgPathVecArray*>(outer)->SetSizeFill(newLen, srcVec);
}

// ============================================================
// FUN_1400cc510 — recursive midpoint subdivision (fractal displacement) of a
// path segment [lo,hi] in the point array, with a half-plane rejection sampler.
// Exe addr: 0x1400cc510.
//   self  = hf base   (RNG at +0x58)
//   arrPtr= &pointArrayPtr  (*arrPtr -> base of 0x1C-stride records)
//   lo,hi = endpoint indices in the array
//   limA  = param_5  (an axis bound; original: fVar9 = negate(limA))
//   limB  = param_6  (amplitude scale; halved each recursion)
//
// Algorithm (faithful):
//   span = hi - lo;  if (span <= 1) return.
//   p_lo = arr[lo], p_hi = arr[hi]
//   d   = p_hi - p_lo                                 (segment vector)
//   mid = (p_hi + p_lo) * 0.5                          (midpoint)
//   Length(d)   (original computes; result not numerically reused but called)
//   n = normalize( (-d.y, d.x) )                       (left normal, via XOR sign)
//   negLimA = negate(limA)
//   do {
//     do {
//       t = RandFloatTol(rng) * limB * 0.66602         (displacement magnitude)
//       cand = mid + n*t
//       e_hi = normalize(p_hi - cand);  e_lo = normalize(p_lo - cand)
//     } while (cand.x < negLimA);
//   } while ( reject:
//       (cand.x <= negLimA && cand.y < negLimA) || limA < cand.x
//       || (limA <= cand.x && limA < cand.y)
//       || dot(e_lo, e_hi) > 0 );
//   arr[(span>>1)+lo] = cand
//   recurse(lo, midIdx, limA, limB*0.5)
//   recurse(midIdx, hi, limA, limB*0.5)
// ============================================================
static void coastline_Subdivide_cc510(void* self, void** arrPtr, int lo, int hi,
                                       float limA, float limB) {
    int span = hi - lo;
    if (span <= 1) return;

    // RNG is the hf-embedded System::Random at self+0x58 — NOT self (hf base). Passing
    // `self` made the MT core read/write a window shifted -0x58, and its twist clobbered
    // the real Random's index at hf+0x58 with a state value -> later Rand() did state[~1.7e9]
    // -> OOB crash. (cc850 / SubdivideSeg already pass +0x58; only this call was wrong.)
    void* rng = (uint8_t*)self + HF_RANDOM;

    uint8_t* base = (uint8_t*)*arrPtr;
    float* pLo = (float*)(base + (size_t)lo * PATHPT_STRIDE);
    float* pHi = (float*)(base + (size_t)hi * PATHPT_STRIDE);

    float loX = pLo[0], loY = pLo[1];
    float hiX = pHi[0], hiY = pHi[1];

    float dX = hiX - loX;       // segment delta
    float dY = hiY - loY;
    float midX = (hiX + loX) * kHalf;
    float midY = (hiY + loY) * kHalf;

    // Length(d) is computed by the original (call it to preserve parity / state).
    { float dvec[2] = { dX, dY }; (void)coast_vec2len_real(dvec); }

    // Perpendicular (left normal): n = normalize( (-dY, dX) )  via integer XOR.
    float nvec[2];
    nvec[0] = fnegate(dY);
    nvec[1] = dX;
    coast_vec2norm_real(nvec);
    float nX = nvec[0], nY = nvec[1];

    float negLimA = fnegate(limA);

    float candX = 0.0f, candY = 0.0f;
    for (;;) {
        bool innerOK = false;
        float eHi[2], eLo[2];
        while (!innerOK) {
            float t = reinterpret_cast<System::Random*>(rng)->RandFloatTol() * limB * kMidAmp_0_666;   // B.3.4b bridge
            candX = nX * t + midX;
            candY = nY * t + midY;

            eHi[0] = hiX - candX;  eHi[1] = hiY - candY;
            eLo[0] = loX - candX;  eLo[1] = loY - candY;
            coast_vec2norm_real(eHi); coast_vec2norm_real(eLo);

            // inner do/while continues while (cand.x < negLimA)
            innerOK = !(candX < negLimA);
        }
        // outer reject condition (loop again while TRUE):
        float dot = eLo[0] * eHi[0] + eLo[1] * eHi[1];
        bool reject =
            ((candX <= negLimA) && (candY < negLimA)) ||
            (limA < candX) ||
            ((limA <= candX) && (limA < candY)) ||
            (dot > 0.0f);
        if (!reject) break;
    }

    int midIdx = (span >> 1) + lo;
    float* pMid = (float*)(base + (size_t)midIdx * PATHPT_STRIDE);
    pMid[0] = candX;
    pMid[1] = candY;

    float half = limB * kHalf;
    coastline_Subdivide_cc510(self, arrPtr, lo, midIdx, limA, half);
    coastline_Subdivide_cc510(self, arrPtr, midIdx, hi, limA, half);
}

// ============================================================
// FUN_1400cc850 — TryPlacePath: build one candidate polyline, validate it has no
// self-intersection and no crossing with previously-placed paths, and on success
// deep-append it into the patchList (hf+0x18). Returns 1 on success, 0 on reject.
// Exe addr: 0x1400cc850.
//
//   self      = hf base
//   nPts      = number of points in the path (cd090 passes gridSize+1)
//   span      = (float)gridSize           (the ring's coordinate extent)
//   a4..a8    = per-vertex attributes propagated to every record (+0x08..+0x18)
//   branchSel = param_9  ( <0 -> closed coastline ring;  >=0 -> branch off path[branchSel] )
//
// IMPORTANT: this stage operates ENTIRELY in the 2D point plane. It never reads
// or writes the float heightmap at hf+0x08.
// ============================================================
static int coastline_TryPlacePath_cc850(void* self, int nPts, float span,
                                          float a4, float a5, float a6, float a7, float a8,
                                          float branchSel) {
    uint8_t* hf = (uint8_t*)self;
    void* rng = hf + HF_RANDOM;

    // candidate path header {void* data; int count; int cap}.
    // ENDGAME B.3.2 step 2: a real Collection::Array with RAII. Exactly one
    // destruction per path: the three reject-path frees and the success-path
    // free that used to sit at each `return` are the destructor now.
    WgPathPointArray cand;

    coastline_Resize_Stride1C_23b70(&cand, nPts);

    // Fill attributes for all nPts records (disasm store order: a6,a5,a8,a7,a4).
    {
        uint8_t* e = (uint8_t*)cand._data;
        for (int i = 0; i < nPts; ++i) {
            float* r = (float*)(e + (size_t)i * PATHPT_STRIDE);
            r[2] = a4; // +0x08
            r[3] = a5; // +0x0c
            r[4] = a6; // +0x10
            r[5] = a7; // +0x14
            r[6] = a8; // +0x18
        }
    }

    int midIdx;
    if ((int)branchSel < 0) {
        // ---- closed coastline ring branch (cd090's path) ----
        uint8_t* base = (uint8_t*)cand._data;
        float* p0 = (float*)(base + 0);

        // point[0]: original packs (uint)(span*-0.5) << 0x20 -> low dword 0,
        // high dword = span*-0.5. i.e. x=0, y=span*-0.5.
        p0[0] = 0.0f;
        p0[1] = span * kNegHalf;

        int lastIdx = cand._size - 1;
        float* pLast = (float*)(base + (size_t)lastIdx * PATHPT_STRIDE);

        uint32_t which = reinterpret_cast<System::Random*>(rng)->Rand(3);   // B.3.4b bridge
        float spanHalf = span * kHalf;
        // Disasm cases (CONCAT44 packing):
        //  case 0: local_f8 = (ull)(uint)(span*0.5) << 0x20  -> x=0, y=span*0.5
        //  case 1: fVar22 = span*0.5; local_f8 = (ull)(uint)fVar22 -> x=span*0.5, y=0
        //  case 2: local_f8 = (ull)(uint)fVar22 where fVar22 = span*-0.5 (initial)
        //          -> x=span*-0.5, y=0
        if (which == 0) {
            p0[1] = span * kNegHalf;  // (point[0] already set above; harmless)
            pLast[0] = 0.0f;
            pLast[1] = spanHalf;
        } else if (which == 1) {
            pLast[0] = spanHalf;
            pLast[1] = 0.0f;
        } else { // which == 2
            pLast[0] = span * kNegHalf;
            pLast[1] = 0.0f;
        }

        // midpoint: random jitter, both lanes RandFloatTol * span * 0.125.
        // disasm: CONCAT44(jx, jy) -> low dword=jy (x lane), high dword=jx (y lane).
        midIdx = cand._size >> 1;
        float jx = reinterpret_cast<System::Random*>(rng)->RandFloatTol() * span * kJitter_0_125;   // B.3.4b bridge
        float jy = reinterpret_cast<System::Random*>(rng)->RandFloatTol() * span * kJitter_0_125;   // B.3.4b bridge
        float* pMid = (float*)(base + (size_t)midIdx * PATHPT_STRIDE);
        pMid[0] = jy;
        pMid[1] = jx;

        float amp  = span * kQuarter;   // fVar22 = span*0.25
        float limA = span * kHalf;      // fVar24 = span*0.5
        coastline_Subdivide_cc510(self, reinterpret_cast<void**>(&cand._data), 0, midIdx, limA, amp);
    } else {
        // ---- branch-off-existing-path branch (NOT used by cd090; ccf60 uses it) ----
        uint8_t* outerData = *(uint8_t**)(hf + HF_PATCHLIST);
        int bi = (int)branchSel;
        int srcCount = *(int*)(outerData + (size_t)bi * 16 + 8);
        int q = (srcCount + ((srcCount >> 31) & 3)) >> 2;
        int pick = reinterpret_cast<System::Random*>(rng)->Rand(q, q * 3);   // B.3.4b bridge
        uint8_t* srcVec = *(uint8_t**)(outerData + (size_t)bi * 16);
        __builtin_memcpy((uint8_t*)cand._data, srcVec + (size_t)pick * PATHPT_STRIDE, 8); // point[0] = src[pick].xy

        float rf = reinterpret_cast<System::Random*>(rng)->RandFloatTol() * span * kHalf;   // B.3.4b bridge
        int lastIdx = cand._size - 1;
        float* pLast = (float*)((uint8_t*)cand._data + (size_t)lastIdx * PATHPT_STRIDE);
        uint32_t which = reinterpret_cast<System::Random*>(rng)->Rand(4);   // B.3.4b bridge
        // Retail CONCAT44 packing (verified 0x1400cc850, param_9>=0 block):
        //  which0: (x=rf,        y=span*0.5)   which1: (x=span*0.5,  y=rf)
        //  which2: (x=span*-0.5, y=rf)         which3: (x=rf,        y=span*-0.5)
        if (which == 0)      { pLast[0] = rf;              pLast[1] = span * kHalf; }
        else if (which == 1) { pLast[0] = span * kHalf;    pLast[1] = rf; }
        else if (which == 2) { pLast[0] = span * kNegHalf; pLast[1] = rf; }
        else                 { pLast[0] = rf;              pLast[1] = span * kNegHalf; }

        midIdx = 0;
        float amp  = span * kQuarter;
        float limA = span * kHalf;
        (void)amp; (void)limA;
        midIdx = 0;
    }

    // Subdivide the second half [midIdx, count-1].
    {
        float amp  = span * kQuarter;
        float limA = span * kHalf;
        coastline_Subdivide_cc510(self, reinterpret_cast<void**>(&cand._data), midIdx, cand._size - 1, limA, amp);
    }

    int count = cand._size;

    // ---- nearest-neighbour adjacency validation pass (self-consistency) ----
    // Walk along the polyline; for each point find the nearest OTHER point via
    // LengthSquared. If the nearest is not the previous-pick or previous-pick+2,
    // reject. (The original seeds the running min with branchSel(param_9); since
    // branchSel is hugely negative for the ring, the first comparison degenerates
    // to "always replace", i.e. plain nearest-neighbour search.)
    {
        int prevPick = -1;
        int row = 0;
        uint8_t* data = (uint8_t*)cand._data;
        bool rejected = false;
        for (int li = 0; li < count && !rejected; ++li) {
            int bestIdx = -2;
            float best = kBigDist;
            float* pa = (float*)(data + (size_t)row * PATHPT_STRIDE);
            for (int j = 0; j < count; ++j) {
                if (row == j) continue;
                float* pb = (float*)(data + (size_t)j * PATHPT_STRIDE);
                float dvx = pa[0] - pb[0];
                float dvy = pa[1] - pb[1];
                float lensq;
                { float v[2] = { dvx, dvy }; lensq = coast_vec2lensq_real(v); }
                if (lensq < best) { best = lensq; bestIdx = j; }
            }
            if (bestIdx != prevPick + 2 && bestIdx != prevPick) { rejected = true; break; }
            row++;
            prevPick++;
        }
        if (rejected) {
            return 0;
        }
    }

    // ---- self-intersection: segment vs non-adjacent segment in the SAME path ----
    {
        int lim = count - 1;
        uint8_t* data = (uint8_t*)cand._data;
        for (int i = 0; i < lim; ++i) {
            float* s0 = (float*)(data + (size_t)i * PATHPT_STRIDE);
            float* s1 = (float*)(data + (size_t)(i + 1) * PATHPT_STRIDE);
            float ax = s0[0], ay = s0[1];
            float bx = s1[0], by = s1[1];
            float dx = bx - ax;
            float dy = by - ay;
            for (int j = 0; j < lim; ++j) {
                // skip adjacent / shared-vertex segments
                if (j == i || j + 1 == i || j == i + 1 || j + 1 == i + 1) continue;
                float* t0 = (float*)(data + (size_t)j * PATHPT_STRIDE);
                float* t1 = (float*)(data + (size_t)(j + 1) * PATHPT_STRIDE);
                float cx = t1[0] - t0[0];
                float cy = t1[1] - t0[1];
                float ex = ax - t0[0];   // (seg-i start) - (seg-j start)
                float ey = ay - t0[1];
                // denom must be dA x dB = dx*cy - dy*cx to match FUN_1400cc850. The
                // numerators below are already canonical (u->t, v->u); the prior
                // sign-flipped denom negated both params, degenerating the crossing
                // test to the backward region (harmless for the first ring, but wrong).
                float denom = dx * cy - dy * cx;
                if (denom != 0.0f) {
                    float u = (cx * ey - cy * ex) / denom;
                    float v = (dx * ey - dy * ex) / denom;
                    if (u > 0.0f && u < kOne && v > 0.0f && v < kOne) {
                        return 0;
                    }
                }
            }

            // ---- cross-path: this segment vs every segment of every existing path ----
            int nPaths = *(int*)(hf + HF_DETAILCNTA);   // patchList count
            if (nPaths > 0) {
                uint8_t* outer = *(uint8_t**)(hf + HF_PATCHLIST);
                for (int pIdx = 0; pIdx < nPaths; ++pIdx) {
                    uint8_t* hdr = outer + (size_t)pIdx * 16;
                    int oc = *(int*)(hdr + 8);
                    uint8_t* od = *(uint8_t**)(hdr + 0);
                    for (int k = 0; k < oc - 1; ++k) {
                        float* q0 = (float*)(od + (size_t)k * PATHPT_STRIDE);
                        float* q1 = (float*)(od + (size_t)(k + 1) * PATHPT_STRIDE);
                        float cx = q1[0] - q0[0];
                        float cy = q1[1] - q0[1];
                        float ex = ax - q0[0];
                        float ey = ay - q0[1];
                        // canonical dA x dB (was sign-flipped); see self-intersection note above.
                        float denom = dx * cy - dy * cx;
                        if (denom != 0.0f) {
                            float u = (cx * ey - cy * ex) / denom;
                            float v = (dx * ey - dy * ex) / denom;
                            if (u > 0.0f && u < kOne && v > 0.0f && v < kOne) {
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- success: deep-append the candidate into patchList (hf+0x18) ----
    coastline_AppendPath_24460(hf + HF_PATCHLIST, *(int*)(hf + HF_DETAILCNTA) + 1, &cand);

    return 1;
}

// ============================================================
// FUN_1400cd090 — EmitCoastlineRing (STAGE ENTRY).
// Exe addr: 0x1400cd090.
//   while (patchList.count == 0)               // hf+0x20
//     cc850(hf, gridSize+1, (float)gridSize,
//           2.0, 10.0, 1.25, 4.0, 4.0, -NAN);   // closed ring (branchSel < 0)
// Loops until exactly one valid closed coastline ring is appended.
//
// HEIGHT EFFECT: NONE. cd090 emits only 2D polyline geometry into the patchList
// (hf+0x18); the heightmap at hf+0x08 is untouched. Later stamp stages (e910/ec50)
// consume the ring to carve the heightmap; the height change happens THERE.
// ============================================================
extern "C" void wg_f390_coastline(void* hf) {
    if (!hf) return;

    uint8_t* h = (uint8_t*)hf;
    int gridSize = *(int*)(h + HF_GRIDSIZE);

    // Guard: without the allocator the rejection sampler can't make progress;
    // bail rather than spin forever.
    if (!System::MemoryAllocator::_singleton) return;

    // branchSel = -NAN in the original (negative-as-int). Use -1.0f:
    // (int)(-1.0f) = -1 < 0 selects the closed-ring branch.
    const float branchSel = -1.0f;

    int guard = 0;
    while (*(int*)(h + HF_DETAILCNTA) == 0) {
        coastline_TryPlacePath_cc850(hf, gridSize + 1, (float)gridSize,
                                     kRing_a4_2_0, kRing_a5_10_0, kRing_a6_1_25,
                                     kRing_a78_4_0, kRing_a78_4_0, branchSel);
        if (++guard > 100000) break; // safety: never hang the load thread
    }
}
