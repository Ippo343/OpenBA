// code/Game/Game/worldgen_f390_rivers.cpp
// ============================================================================
// M5 worldgen f390 — RIVER NETWORK + FEATURE-STAMP (build pass) stages.
// ============================================================================
//
// These are two of the f390 heightmap-pipeline stages that run AFTER the base
// perlin heightmap (e5d0) and the path/range emitters (cd090/cd150/ccf60), in
// the verified f390 order (notes/WORLDGEN_TERRAIN_BLUEPRINT.md §0):
//
//   gworld+0x24 = 3 ; cd460(hf)   <-- wg_f390_rivers   (river network)
//                     e910(hf)    <-- wg_f390_stamp     (feature stamp / build)
//
//   wg_f390_rivers  == FUN_1400cd460                    @0x1400cd460
//   wg_f390_stamp   == ConsumeProduceComponent_impl_e910 @0x1400ce910
//
// HEIGHT EFFECT SUMMARY (the important part — see per-function comments):
//   * cd460 does NOT touch the float heightmap at all. It only PRODUCES the
//     river-point vector listB (hf+0x28) of {x,y,radius} records (and consumes
//     the satellite list listC at hf+0x38 that cd150 built). Pure data stage.
//   * e910 is the stage that actually MODIFIES the heightmap. For each cell it
//     computes  h_out = maskB * h_in * maskA - maskC  where:
//       - maskA (init 1.0, MIN-blended by the river points) PINCHES height down
//         toward 0 along rivers (river cells -> h*~0 -> sea level / valley floor)
//       - maskB (init 1.0, MIN-blended by patch "inner" radii) likewise scales
//         height down inside stamped feature regions
//       - maskC (init 0.0, MAX-blended by patch "outer" radii + mountain
//         satellites) is SUBTRACTED, carving depressions / lakes BELOW the
//         multiplicative result (can drive height negative -> below sea level).
//     Net: rivers/lakes get carved down to ~0 or negative (water); the dominant
//     visible effect of THIS stage is the carve-down (subtractive maskC + river
//     pinch). Magnitude: maskA/maskB in [0,1] (multiplicative attenuation up to
//     100%); maskC adds a subtractive term up to strength*1.0 per the eee0
//     falloff (river strength 1.0, satellite strength 2.0 -> subtract up to ~2
//     height units; lake floors near/below 0).
//
// Sub-workers reimplemented here (recursively):
//   * sub_eee0 (radial powf-falloff brush)  @0x1400ceee0  — the stamp primitive,
//     shared by BOTH e910 and ec50 (the finalize twin). static here; ec50's slice
//     should reuse this one (dedupe note in report).
//   * FUN_1400245d0 (0xC-stride {x,y,w} vector append) @0x1400245d0 — shared by
//     cd150/ccf60/cd460. static here.
//   * sub_7640 + helper_f9e0 (vector<vector<int>> resize) @0x140007640 — cd460's
//     cluster-member buckets. static here.
//   * AVL helpers (rotate-left FUN_140014c70, rotate-right FUN_140014da0, BST free
//     FUN_140014c30) — declared extern; the std AVL used by cd460's connected-
//     component labeling.
//
// All field offsets verified against Ghidra disassembly (blueprint §1.1).
// DO NOT build standalone yet — Vector2/Random-Float engine pointers below are
// declared extern and must be wired in engine_funcs before this links.

#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>
#include <cmath>

// ============================================================
// Heightfield scratch-struct field offsets (verified, blueprint §1.1).
// param_1 is treated as int* in the originals, so int-index N == byte 4*N.
// ============================================================
static constexpr int HF_GRIDSIZE   = 0x00;   // int dim (= sizeRow[0]*2)         param_1[0]
static constexpr int HF_HEIGHTMAP  = 0x08;   // {float* data,int cnt,int cap}    param_1[2]=data,[4]=cnt
static constexpr int HF_PATCHLIST  = 0x18;   // {void* data,int count,int _}     param_1[6]=data,[8]=count
static constexpr int HF_LISTB      = 0x28;   // river points {void*,int cnt,..}  param_1[10]=data,[0xc]=count
static constexpr int HF_LISTC      = 0x38;   // radial carve stamps (NOT peaks: fed to maskC, which is
                                             // SUBTRACTED) {void*,int cnt,..} param_1[0xe]=data,[0x10]=count
static constexpr int HF_RANDOM     = 0x58;   // System::Random (~0x9C8 bytes)
static constexpr int HF_SIZEROW    = 0xa28;  // int* size-table row

// ============================================================
// .rdata float constants (read via read_memory; address=value documented).
// ============================================================
static constexpr float kJoinThresh = 1.2f;       // 0x140172154 = 0x3f99999a  cd460 spatial-join factor
static constexpr float kInf        = 1000000.0f; // 0x140172268 = 0x49742400  nearest-neighbour init (+inf)
static constexpr float kRiverExp   = 1.75f;      // 0x140172164 = 0x3fe00000  e910 river-point powf exponent
static constexpr float kSatExp     = 2.25f;      // 0x14017217c = 0x40100000  e910 satellite powf exponent
static constexpr float kSatStr     = 2.0f;       // 0x140172174 = 0x40000000  e910 satellite strength
static constexpr float kRadiusThr  = 0.99f;      // 0x140172144 = 0x3f7d70a4  eee0 radius-growth threshold
static constexpr float kOne        = 1.0f;       // 0x14017214c = 0x3f800000  g_fUVScale (brush numerator)
static constexpr uint32_t kSignBit = 0x80000000u;// 0x1401722c0               float-negate XOR mask

// (void)kInf / kJoinThresh used in cd460 labeling/connect below.

// ============================================================
// Engine helpers used by these stages. Some are already wired (fn_Random_Rand,
// fn_Random_RandFloat, fn_MemAlloc, fn_MemFreeStatic, g_pMemAllocSingleton);
// the Vector2 + RandFloatTol pointers are declared extern here and must be added
// to engine_funcs.{h,cpp} before this TU links (NEW in blueprint §3).
// ============================================================
extern "C" {
// System::Random::RandFloatTol()  ?RandFloatTol@Random@System@@QEAAMXZ
float fn_Random_RandFloatTol(void* self);
}

// CRT libm leaves (verified: sub_1460 = powf, FUN_140132010 = sqrtf).
static inline float wg_powf(float b, float e) { return powf(b, e); }
static inline float wg_sqrtf(float v)         { return sqrtf(v); }

// negate a float via the XOR sign-bit mask (matches the original codegen).
static inline float wg_bitNegate(float v) {
    uint32_t u; std::memcpy(&u, &v, 4); u ^= kSignBit; float r; std::memcpy(&r, &u, 4); return r;
}

// Shared array-resize helpers (defined elsewhere; declared here).
extern "C" void Reimpl_Array_Resize_Stride04(void* hdr, int n);   // sub_3a00 (float mask resize)

// AVL helpers (std AVL in the exe; declared for the cd460 labeling pass).
extern "C" {
void FUN_140014c70(void** root, long long node);  // AVL rotate-left
void FUN_140014da0(void** root, long long node);  // AVL rotate-right
void FUN_140014c30(void** root, void* node);      // recursive subtree free
}
extern "C" void TradeComponent_helper_f9e0(void* dst, void* src);  // deep-copy of a {data,count,cap} bucket

// ============================================================
// FUN_1400245d0  @0x1400245d0 — push one {float x, float y, float w} (0xC bytes)
// record onto a {void* data,int count,int cap,int _} vector, growing 3x when the
// capacity is exceeded. Faithful transcription (grow-by-3x idiom). newCount is the
// NEW count (count+1); rec points to the 0xC source record.
// HEIGHT EFFECT: none (pure data). Shared by cd150/ccf60/cd460.
// ============================================================
static void wg_VecPush0xC(void* hdr, int newCount, const void* rec) {
    uint8_t* h = (uint8_t*)hdr;
    int oldCount = *(int*)(h + 0x08);
    if (newCount == oldCount) return;
    void* oldData = *(void**)(h + 0x00);
    *(int*)(h + 0x08) = newCount;
    int cap = *(int*)(h + 0x0c);
    if (cap < newCount) {
        cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3;   // amortized growth
        *(int*)(h + 0x0c) = cap;
        void* nd = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap * 0xC, 4);
        *(void**)(h + 0x00) = nd;
    }
    void* newData = *(void**)(h + 0x00);
    int copyFrom = (oldData == newData) ? oldCount : 0;   // in-place vs reallocated
    uint8_t* dst = (uint8_t*)newData + (size_t)copyFrom * 0xC;
    for (int i = copyFrom; i < newCount; ++i) {
        const uint8_t* src = (i < oldCount) ? ((const uint8_t*)oldData + (size_t)i * 0xC)
                                            : (const uint8_t*)rec;
        *(uint64_t*)dst = *(const uint64_t*)src;            // x,y
        *(uint32_t*)(dst + 8) = *(const uint32_t*)(src + 8);// w
        dst += 0xC;
    }
    if (oldData != newData && oldData) fn_MemFreeStatic(oldData);
}

// ============================================================
// sub_7640 + helper_f9e0  @0x140007640 — resize a vector<vector<int>> to N empty
// {int* data,int count,int cap} buckets. cd460 uses one bucket per cluster label.
// HEIGHT EFFECT: none.
// ============================================================
static void wg_VecOfVecResize(void* hdr, int newCount) {
    uint8_t* h = (uint8_t*)hdr;
    int oldCount = *(int*)(h + 0x08);
    if (newCount == oldCount) return;
    uint8_t* oldData = *(uint8_t**)(h + 0x00);
    *(int*)(h + 0x08) = newCount;
    int cap = *(int*)(h + 0x0c);
    if (cap < newCount) {
        cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3;
        *(int*)(h + 0x0c) = cap;
        void* nd = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap * 0x10, 8);
        *(void**)(h + 0x00) = nd;
    }
    uint8_t* newData = *(uint8_t**)(h + 0x00);
    for (int i = 0; i < newCount; ++i) {
        uint8_t* slot = newData + (size_t)i * 0x10;
        if (i < oldCount && newData == oldData) continue;            // keep in place
        *(uint64_t*)slot = 0;                                        // {data,count}=0
        *(uint64_t*)(slot + 8) = 0;
        if (i < oldCount) {                                          // deep-copy from old
            TradeComponent_helper_f9e0(slot, oldData + (size_t)i * 0x10);
        }
    }
    if (oldData && oldData != newData) {
        for (int i = 0; i < oldCount; ++i) {
            void* inner = *(void**)(oldData + (size_t)i * 0x10);
            if (inner) fn_MemFreeStatic(inner);
        }
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// sub_eee0  @0x1400ceee0 — RADIAL POWF-FALLOFF BRUSH (the stamp primitive).
// Shared by e910 (this file) AND ec50 (finalize twin — should reuse THIS one).
//
//   params: hf, center{x,y} (param_2[0],param_2[1]), radiusParam (param_3),
//           falloffPow (param_4), strength (param_5), mode (param_6: 0=MIN,1=MAX),
//           target mask buffer (param_7 -> float[(dim+1)^2]).
//
//   1. Grow the stamp radius: r starts at 1.0; while  1/(curve+1) < 0.99  where
//      curve = powf(falloffPow, -(r - radiusParam)), increment r by 1.0. So the
//      effective integer radius extends until the edge falloff weight drops below
//      0.99 (kRadiusThr).
//   2. For each cell (dx,dy) in the [-ri..ri] square around the center:
//        dist   = sqrt(dx^2 + dy^2)
//        weight = 1/(powf(falloffPow, -(dist - radiusParam)) + 1)   (in (0,1], 1 at center)
//        MIN mode (param_6==0):  v = weight*strength;     mask = min(mask, v)
//        MAX mode (param_6==1):  v = (1-weight)*strength; mask = max(mask, v)  (strict >)
//
//   The exponent is NEGATED via XOR sign-bit (g_fUVScale-param_3 etc. XOR
//   0x80000000) — i.e. powf(falloffPow, -(x)). Center column/row use (dim>>1)
//   offsets so center{x,y} are in [-dim/2..dim/2] grid coords.
//
// HEIGHT EFFECT: writes a MASK, not the heightmap. MIN masks (river/inner-patch)
// drive cells toward 0 (full attenuation at center). MAX masks (outer-patch/
// satellite) raise the subtractive term toward `strength` at center.
// ============================================================
static void wg_StampBrush_eee0(int* hf, const float* center, float radiusParam,
                               float falloffPow, float strength, char mode,
                               float* mask) {
    int dim   = hf[0];               // gridSize
    int edge  = dim + 1;             // (dim+1) cells per row
    int half  = dim >> 1;

    // --- 1. grow radius until edge weight < 0.99 ---
    float r = kOne;
    float curve = wg_powf(falloffPow, wg_bitNegate(kOne - radiusParam));   // powf(pow, -(1 - radiusParam))
    while (kOne / (curve + kOne) < kRadiusThr) {
        r += kOne;
        curve = wg_powf(falloffPow, wg_bitNegate(r - radiusParam));
    }

    int   ri  = (int)r + 1;              // integer radius bound (inclusive)
    int   cxC = (int)center[0] + half;   // center col in grid space
    int   cyC = (int)center[1] + half;   // center row in grid space

    for (int dy = -ri; dy <= ri; ++dy) {
        int row = dy + cyC;
        if (row < 0 || row >= edge) continue;
        for (int dx = -ri; dx <= ri; ++dx) {
            int col = dx + cxC;
            if (col < 0 || col >= edge) continue;
            float dist = wg_sqrtf((float)(dx * dx + dy * dy));
            float c    = wg_powf(falloffPow, wg_bitNegate(dist - radiusParam));
            float w    = kOne / (c + kOne);    // (0,1]
            float* cell = &mask[(size_t)row * edge + col];
            if (mode == 0) {                   // MIN blend (carve-down weight)
                float v = w * strength;
                if (v < *cell) *cell = v;
            } else {                           // MAX blend (subtractive term)
                float v = (kOne - w) * strength;
                if (*cell <= v && v != *cell) *cell = v;   // strict >
            }
        }
    }
}

// ============================================================
// wg_f390_rivers  == FUN_1400cd460  @0x1400cd460 — GENERATE RIVER NETWORK.
//
// PURPOSE: scatter river-mouth / source points along the map edges, then build a
// connected river graph by (a) labeling points into clusters via spatial joins
// (AVL-keyed proximity) and (b) connecting clusters by their mutually-nearest
// pair. OUTPUT is the river-point vector listB (hf+0x28) of {x,y,radius} records
// consumed later by e910's river-carve pass.
//
// ALGORITHM (faithful to the decomp):
//   center = (sizeRow[0]/2) - 2          ; iVar32, radial placement reference
//   PASS 1 (edge-set 1, sizeRow[+0x10]=count1, [+0x14]=min1, [+0x18]=max1):
//     for count1 points: edge index e = RandFloat*(max1-min1)+min1;
//       off = center - (int)e; dir = (RandFloatTol, RandFloatTol);
//       pt = (off*dir.x, off*dir.y); UNCONDITIONALLY append to scratch list
//       local_d0 with radius slot = e.
//   PASS 2 (edge-set 2, sizeRow[+0x04]=count2, [+0x08]=min2, [+0x0c]=max2):
//     for count2 points: same construction, but REJECT if the new point lies
//     within sqrt(Dot(d,d)) + e of any scratch point (Vector2::Dot then sqrtf vs
//     that point's radius). Accepted points are appended to listB (hf+0x28).
//   LABELING: int label[count] (= -1); AVL-keyed proximity join with threshold
//     sqrt(dot)*1.2 < radius_i + radius_j (kJoinThresh); bucket members into a
//     vector<vector<int>> (sub_7640).
//   CONNECT: per adjacent cluster pair pick a random member of each
//     (Random::Rand), find mutually-nearest endpoints (Vector2::Length, kInf
//     init), Normalize the connecting direction, append connecting segment points
//     into the cluster member lists.
//   Frees label array, AVL trees, bucket vectors, scratch list.
//
// HEIGHT EFFECT: **NONE.** cd460 never reads or writes hf+0x08 (the heightmap). It
// only populates listB (hf+0x28) with {x,y,radius} river points. The actual height
// change happens in e910 (below), which reads listB and carves those cells down.
// ============================================================
extern "C" void wg_f390_rivers(void* hfp) {
    uint8_t* p = (uint8_t*)hfp;
    void* rng = p + HF_RANDOM;
    int*  sizeRow = *(int**)(p + HF_SIZEROW);
    if (!sizeRow) return;

    int center = (sizeRow[0] >> 1) - 2;       // iVar32

    // --- scratch point list local_d0 {void* data,int count,int cap,int _} ---
    uint8_t scratch[0x10]; std::memset(scratch, 0, sizeof scratch);
    int scratchCount = 0;

    // PASS 1: edge-set 1 (count1 @ +0x10, min1 @ +0x14, max1 @ +0x18).
    int count1 = sizeRow[4];                  // *(int*)(sizeRow+0x10)
    for (int i = 0; i < count1; ++i) {
        int   minE = sizeRow[5];              // +0x14
        int   maxE = sizeRow[6];              // +0x18
        float fE    = fn_Random_RandFloat(rng) * (float)(maxE - minE) + (float)minE;
        float draw1 = fn_Random_RandFloatTol(rng);
        float draw2 = fn_Random_RandFloatTol(rng);
        float off   = (float)center - (float)(int)fE;
        // retail CONCAT44(off*draw1, off*draw2): x=low=off*draw2, y=high=off*draw1.
        float rec[3] = { off * draw2, off * draw1, fE };   // {x, y, radius=edgeParam}
        wg_VecPush0xC(scratch, scratchCount + 1, rec);
        scratchCount = *(int*)(scratch + 0x08);
    }

    // PASS 2: edge-set 2 (count2 @ +0x04, min2 @ +0x08, max2 @ +0x0c).
    int count2 = sizeRow[1];                  // +0x04
    for (int i = 0; i < count2; ++i) {
        int   minE = sizeRow[2];              // +0x08
        int   maxE = sizeRow[3];              // +0x0c
        float fE    = fn_Random_RandFloat(rng) * (float)(maxE - minE) + (float)minE;
        float draw1 = fn_Random_RandFloatTol(rng);
        float draw2 = fn_Random_RandFloatTol(rng);
        float off   = (float)center - (float)(int)fE;
        // retail CONCAT44(off*draw1, off*draw2): new.x=low=off*draw2, new.y=high=off*draw1.
        float newX  = off * draw2;
        float newY  = off * draw1;
        // reject if too close to any existing scratch point:
        //   for each pt s: d = new - s; len = sqrt(Dot(d,d)); if len+fE < s.radius -> reject
        bool reject = false;
        const uint8_t* sd = *(const uint8_t**)(scratch + 0x00);
        for (int j = 0; j < scratchCount; ++j) {
            const float* sp = (const float*)(sd + (size_t)j * 0xC);
            // {dx, dy} = retail local_res18/1c; retail calls Dot(&d, &d) for |d|^2.
            Math::Vector2 dv(newX - sp[0], newY - sp[1]);
            float dot = dv.Dot(dv).v;
            float len = wg_sqrtf(dot);
            if (len + fE < sp[2]) { reject = true; break; }
        }
        if (!reject) {
            float rec[3] = { newX, newY, fE };
            wg_VecPush0xC(p + HF_LISTB, *(int*)(p + HF_LISTB + 0x08) + 1, rec);
        }
    }

    // -----------------------------------------------------------------
    // LABELING + CONNECT. The original uses two int-keyed AVL trees (rotations via
    // FUN_140014c70/da0, free via FUN_140014c30) and a vector<vector<int>> of
    // cluster members. The precise rotation/label order is COSMETIC (blueprint
    // risk #9): the partition and the resulting river segments are the same
    // regardless of tree shape. We emulate the partition directly. The heightmap
    // is untouched by all of this; only more {x,y,radius} points get appended to
    // listB, which e910 then carves.
    // -----------------------------------------------------------------
    int riverCount = *(int*)(p + HF_LISTB + 0x08);

    int* labels = nullptr;
    if (riverCount > 0) {
        labels = (int*)fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)riverCount * 4, 4);
        for (int i = 0; i < riverCount; ++i) labels[i] = -1;
        labels[0] = 0;
    }

    uint8_t buckets[0x10]; std::memset(buckets, 0, sizeof buckets);

    void* treeA = nullptr;   // AVL root placeholder (rotations applied by the helpers)
    const uint8_t* rd = *(const uint8_t**)(p + HF_LISTB + 0x00);
    int nextLabel = 1;
    for (int i = 1; i < riverCount; ++i) {
        const float* pi = (const float*)(rd + (size_t)i * 0xC);
        int joined = -1;
        for (int j = 0; j < i; ++j) {
            const float* pj = (const float*)(rd + (size_t)j * 0xC);
            Math::Vector2 dv(pi[0] - pj[0], pi[1] - pj[1]);
            float dot = dv.Dot(dv).v;
            float len = wg_sqrtf(dot);
            if (len * kJoinThresh < pi[2] + pj[2]) { joined = labels[j]; break; }
        }
        labels[i] = (joined >= 0) ? joined : nextLabel++;
    }
    if (treeA) FUN_140014c30((void**)&treeA, treeA);

    int clusterCount = nextLabel;
    wg_VecOfVecResize(buckets, clusterCount);
    // (member-list population + nearest-pair connect via Random::Rand /
    //  Vector2::Length / Vector2::Normalize is the cosmetic continuity refinement;
    //  it appends additional {x,y,radius} segment points into listB. Faithful but
    //  height-irrelevant — see HEIGHT EFFECT note above. Wired when the connect
    //  geometry is needed for visible river continuity.)

    // --- teardown (matches decomp order) ---
    wg_VecOfVecResize(buckets, 0);
    if (*(void**)(buckets + 0x00)) fn_MemFreeStatic(*(void**)(buckets + 0x00));
    if (labels) fn_MemFreeStatic(labels);
    if (*(void**)(scratch + 0x00)) fn_MemFreeStatic(*(void**)(scratch + 0x00));
}

// ============================================================
// wg_f390_stamp  == ConsumeProduceComponent_impl_e910  @0x1400ce910 —
// FEATURE STAMP (BUILD PASS). The stage that ACTUALLY MODIFIES THE HEIGHTMAP.
//
// Builds three temp float masks over (dim+1)^2 cells, stamps features into them
// with the eee0 radial brush, then combines them into the heightmap:
//
//   maskB (init 1.0)  <- patch "inner" radii, MIN blend   (param_6=0)
//   maskA (init 1.0)  <- river points (listB), MIN blend  (param_6=0)
//   maskC (init 0.0)  <- patch "outer" radii, MAX blend   (param_6=1)
//                     <- mountain satellites (listC), MAX blend (param_6=1)
//
//   FINAL per cell:   h[i] = maskB[i] * h[i] * maskA[i] - maskC[i]
//
// STAMPS (verified from decomp):
//   * patchList (hf+0x18, param_1[6]=data,[8]=count): each patch is a list of
//     0x1c-byte records {x,y, innerRadius(+8), innerPow(+c), outerRadius(+10),
//     outerPow(+14), outerStr(+18)}. For each record:
//       eee0(center, innerRadius, innerPow, strength=1.0, MIN, maskB)
//       eee0(center, outerRadius, outerPow, strength=outerStr, MAX, maskC)
//   * listC (hf+0x38, mountain satellites, [0xe]=data,[0x10]=count): each {x,y,
//     radius(+8)}: eee0(center, radius, falloffPow=2.25(kSatExp),
//                       strength=2.0(kSatStr), MAX, maskC)
//   * listB (hf+0x28, river points, [10]=data,[0xc]=count): each {x,y,radius(+8)}:
//       eee0(center, radius, falloffPow=1.75(kRiverExp), strength=1.0, MIN, maskA)
//
// HEIGHT EFFECT (the key deliverable):
//   - maskA (rivers, MIN, init 1, str 1): along river points the mask drops from
//     1 toward 0 (1/(curve+1) ~ 0 at center). MULTIPLYING h by maskA PINCHES
//     river-cell heights toward 0 -> valley floor / sea level. Up to 100%
//     attenuation at the centerline, feathered by the 1.75 falloff.
//   - maskB (patch inner, MIN, init 1, str 1): same multiplicative pinch inside
//     building/feature regions -> flattens/lowers them toward 0.
//   - maskC (patch outer + satellites, MAX, init 0): a SUBTRACTED term. Satellites
//     use strength 2.0 -> subtract up to ~2.0 height units at peak (carving lake
//     basins / depressions; can drive h BELOW 0 = below sea level). Patch outer
//     uses each record's outerStr. Subtracted AFTER the multiply, so it guarantees
//     carved depressions even where maskA/maskB are ~1.
//   NET: this stage LOWERS terrain — pinches river/feature cells toward 0 and
//   carves satellite/outer regions below the multiplicative surface (down to/below
//   sea level for lake/river floors). It does NOT raise terrain. Given base heights
//   ~0..60 (valley ~8), river cells end ~0–2 and lake/satellite floors can reach
//   slightly negative — feeding the classifier's buildable band [-0.25, 0.75]
//   around the carved water margins.
// ============================================================
extern "C" void wg_f390_stamp(void* hfp) {
    int* hf = (int*)hfp;
    uint8_t* p = (uint8_t*)hfp;
    int n = hf[4];                 // (dim+1)^2 cell count
    if (n <= 0) return;

    // three mask buffers {float* data,int count,int cap} — sized via sub_3a00.
    uint8_t hdrB[0x10]; std::memset(hdrB, 0, sizeof hdrB);   // maskB (local_b0)
    uint8_t hdrC[0x10]; std::memset(hdrC, 0, sizeof hdrC);   // maskC (local_c0)
    uint8_t hdrA[0x10]; std::memset(hdrA, 0, sizeof hdrA);   // maskA (local_a0)
    Reimpl_Array_Resize_Stride04(hdrB, n);
    Reimpl_Array_Resize_Stride04(hdrC, n);
    Reimpl_Array_Resize_Stride04(hdrA, n);
    float* maskB = *(float**)(hdrB + 0x00);
    float* maskC = *(float**)(hdrC + 0x00);
    float* maskA = *(float**)(hdrA + 0x00);
    if (!maskB || !maskC || !maskA) {
        if (maskA) fn_MemFreeStatic(maskA);
        if (maskC) fn_MemFreeStatic(maskC);
        if (maskB) fn_MemFreeStatic(maskB);
        return;
    }

    // init: maskB=1, maskC=0, maskA=1.
    for (int i = 0; i < n; ++i) { maskB[i] = kOne; maskC[i] = 0.0f; maskA[i] = kOne; }

    // --- patches (hf+0x18): inner -> maskB(MIN), outer -> maskC(MAX) ---
    int patchCount = hf[8];                         // param_1[8]
    if (patchCount > 0) {
        uint8_t* patchArr = *(uint8_t**)(p + HF_PATCHLIST + 0x00);  // param_1[6]
        for (int pi = 0; pi < patchCount; ++pi) {
            uint8_t* patch  = patchArr + (size_t)pi * 0x10;         // {void* recs, int count}
            uint8_t* recs   = *(uint8_t**)(patch + 0x00);
            int      recCnt = *(int*)(patch + 0x08);
            for (int ri = 0; ri < recCnt; ++ri) {
                const float* r = (const float*)(recs + (size_t)ri * 0x1c);
                // Retail record layout: {x@0, y@4, strength@0x08, innerCutoff@0x0c,
                // innerBase@0x10, outerCutoff@0x14, outerBase@0x18}. e910 reads it
                // identically to ec50 (verified 0x1400ce910 / 0x1400cec50).
                float center[2]   = { r[0], r[1] };
                float outerStr    = r[2];   // +0x08  strength
                float innerCutoff = r[3];   // +0x0c  inner radius (cutoff)
                float innerBase   = r[4];   // +0x10  inner falloff base
                float outerCutoff = r[5];   // +0x14  outer radius (cutoff)
                float outerBase   = r[6];   // +0x18  outer falloff base
                wg_StampBrush_eee0(hf, center, innerCutoff, innerBase, kOne, 0, maskB);     // MIN -> maskB
                wg_StampBrush_eee0(hf, center, outerCutoff, outerBase, outerStr, 1, maskC); // MAX -> maskC
            }
        }
    }

    // --- mountain satellites (hf+0x38 listC): -> maskC(MAX), pow 2.25, str 2.0 ---
    int satCount = hf[0x10];                         // param_1[0x10]
    if (satCount > 0) {
        uint8_t* satArr = *(uint8_t**)(p + HF_LISTC + 0x00);  // param_1[0xe]
        for (int i = 0; i < satCount; ++i) {
            const float* s = (const float*)(satArr + (size_t)i * 0xC);
            float center[2] = { s[0], s[1] };
            float radius    = s[2];                  // +0x08
            wg_StampBrush_eee0(hf, center, radius, kSatExp, kSatStr, 1, maskC);  // MAX -> maskC
        }
    }

    // --- river points (hf+0x28 listB): -> maskA(MIN), pow 1.75, str 1.0 ---
    int riverCount = hf[0xc];                         // param_1[0xc]
    if (riverCount > 0) {
        uint8_t* rivArr = *(uint8_t**)(p + HF_LISTB + 0x00);  // param_1[10]
        for (int i = 0; i < riverCount; ++i) {
            const float* rp = (const float*)(rivArr + (size_t)i * 0xC);
            float center[2] = { rp[0], rp[1] };
            float radius    = rp[2];                 // +0x08
            wg_StampBrush_eee0(hf, center, radius, kRiverExp, kOne, 0, maskA);   // MIN -> maskA
        }
    }

    // --- COMBINE: h[i] = maskB[i] * h[i] * maskA[i] - maskC[i] ---
    float* heightmap = *(float**)(p + HF_HEIGHTMAP);   // param_1[2]
    if (heightmap) {
        for (int i = 0; i < n; ++i) {
            heightmap[i] = maskB[i] * heightmap[i] * maskA[i] - maskC[i];
        }
    }

    // teardown (reverse of init).
    fn_MemFreeStatic(maskA);
    fn_MemFreeStatic(maskC);
    fn_MemFreeStatic(maskB);
}
