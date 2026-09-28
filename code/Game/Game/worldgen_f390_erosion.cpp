// code/Game/Game/worldgen_f390_erosion.cpp
// ============================================================================
// M5 5b — worldgen f390 LATE STAGES: droplet erosion + feature carve + smoothing.
// ============================================================================
//
// Faithful clean-C++ reimplementation of the trailing f390 heightmap stages
// (blueprint §0 order — these run AFTER e5d0 base + the path/river/stamp build
// pass, operating in-place on the float heightmap at hf+0x08):
//
//   gworld+0x24 = 4 ; cc080(hf)   ; droplet erosion        -> wg_f390_erosion
//   gworld+0x24 = 5 ; ec50(hf)    ; feature stamp finalize -> wg_f390_carve
//                     c350(hf)    ; weighted box blur      -> wg_f390_smooth (pass 1)
//                     c350(hf)    ; second smoothing pass  -> wg_f390_smooth (pass 2)
//
// The orchestrator (wg_HeightmapPipeline_f390 in worldgen.cpp, kFullPipeline path)
// calls these in that order; wg_f390_smooth is ONE pass — the orchestrator invokes
// it twice in a row, matching the two consecutive c350 calls in f390.
//
// Verified against live Ghidra disassembly of Application-x64.exe:
//   FUN_1400cc080                  (droplet erosion)            @ 0x1400cc080
//   ConsumeProduceComponent_impl_ec50 (feature finalize/carve)  @ 0x1400cec50
//   ConsumeProduceComponent_impl_c350 (box-blur smoothing)      @ 0x1400cc350
//   ConsumeProduceComponent_impl_sub_eee0 (radial falloff brush)@ 0x1400ceee0  (shared callee)
//   ConsumeProduceComponent_impl_sub_1460 == CRT powf           @ 0x140131460  (-> std::pow)
//   FUN_140132010                  == CRT sqrtf                 @ 0x140132010  (-> std::sqrt)
//
// Heightfield scratch-struct field offsets (blueprint §1.1) — Ghidra's int* indexing:
//   param_1[0]     +0x00  gridSize (dim = mapDim*2; (dim+1) verts/side)
//   param_1[2]     +0x08  float* heightmap data
//   param_1[4]     +0x10  heightmap cell count = (dim+1)^2 (count slot of +0x08 header)
//   param_1[6]     +0x18  patchList.data  (vector of {void* recs, int count} 16B groups)
//   param_1[8]     +0x20  patchList group count (detailCountA)
//   param_1[0xe]   +0x38  listC.data (mountain satellites/centers, 0xC-stride {x,y,radius})
//   param_1[0x10]  +0x40  listC.count
//   param_1[0x28a] +0xa28 sizeRow (int*) ; sizeRow[+0x24] = erodeIterations
//   (Random @ hf+0x58 == param_1+0x16)
//
// ADD-ONLY: this file does not modify worldgen.cpp. The orchestrator wires these
// three extern "C" entry points into the kFullPipeline branch separately.

#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "resolve.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ----------------------------------------------------------------------------
// Shared resize helper (sub_3a00 == 4B-stride {data,count,cap} resize).
// Defined in serialize_resize_helpers.cpp; declared extern (do NOT define here).
// ec50 + c350 use it to allocate temp float masks/buffers sized param_1[4].
// ----------------------------------------------------------------------------
extern "C" void Reimpl_Array_Resize_Stride04(void* hdr, int n);   // sub_3a00 (float arrays)

// ----------------------------------------------------------------------------
// Heightfield field offsets (int* index -> byte offset). Verified §1.1.
// ----------------------------------------------------------------------------
static constexpr int HF_GRIDSIZE   = 0x00;   // param_1[0]
static constexpr int HF_HEIGHTMAP  = 0x08;   // param_1[2]  float* data
static constexpr int HF_HM_COUNT   = 0x10;   // param_1[4]  cell count (dim+1)^2
static constexpr int HF_PATCH_DATA = 0x18;   // param_1[6]
static constexpr int HF_PATCH_CNT  = 0x20;   // param_1[8]
static constexpr int HF_LISTC_DATA = 0x38;   // param_1[0xe]
static constexpr int HF_LISTC_CNT  = 0x40;   // param_1[0x10]
static constexpr int HF_RANDOM     = 0x58;   // System::Random (param_1+0x16)
static constexpr int HF_SIZEROW    = 0xa28;  // param_1[0x28a]  int*

static constexpr int SIZEROW_ERODE_ITERS = 0x24;   // sizeRow[+0x24]

// ----------------------------------------------------------------------------
// .rdata float constants (verified via read_memory; address = raw LE dword = value)
// ----------------------------------------------------------------------------
static constexpr float kOne             = 1.0f;     // 0x14017214c (g_fUVScale) = 0x3f800000
static constexpr float kErodeFactor     = 0.0625f;  // 0x1401720dc = 0x3d800000 (slope->transfer, 1/16)
static constexpr float kErodeMaxXfer    = 0.8f;     // 0x140172128 = 0x3f4ccccd (max-transfer clamp)
                                                    //   NB: blueprint mislabeled this as 0.5; the raw
                                                    //   dword 0x3f4ccccd is 0.8, used live in cc080.
static constexpr float kCenterWeight    = 2.0f;     // 0x140172174 = 0x40000000 (c350 center / eee0 mul)
static constexpr float kRadiusThreshold = 0.99f;    // 0x140172144 = 0x3f7d70a4 (eee0 radius-growth)
static constexpr float kListCExp        = 2.25f;    // 0x14017217c = 0x40100000 (ec50 listC powf base arg = DAT_14017217c)
static constexpr float kListCStrength   = 2.0f;     // 0x140172174 = 0x40000000 (ec50 listC strength   = DAT_140172174)
static const     uint32_t kSignBitMask  = 0x80000000u; // 0x1401722c0 (float-negate XOR mask)

// ============================================================================
// powf / sqrtf — the exe's sub_1460 / FUN_140132010 are the CRT libm routines.
// Worldgen never feeds inf/NaN, so the special-case branches are dead. Map to
// std:: equivalents (blueprint §3). sub_1460(base, exp) == powf(base, exp).
// ============================================================================
static inline float wg_powf(float base, float exp) { return std::pow(base, exp); }
static inline float wg_sqrtf(float v)               { return std::sqrt(v); }

// XOR a float's bit pattern with the sign bit (== negate). Matches the exe's
// `(uint)x ^ 0x80000000` idiom in eee0 (used to compute -(d - cutoff)).
static inline float wg_negate_bits(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    bits ^= kSignBitMask;
    float out;
    std::memcpy(&out, &bits, 4);
    return out;
}

// ============================================================================
// sub_eee0 — RADIAL FALLOFF BRUSH (shared callee of ec50; also used by e910).
//   @ 0x1400ceee0  ConsumeProduceComponent_impl_sub_eee0
//
// Stamps a circular falloff into a target float mask. The falloff at a cell at
// Euclidean distance d from the brush centre is
//        f(d) = 1 / ( powf(base, -(d - cutoff)) + 1 )      [logistic-ish curve]
// where base = `falloffBase` (arg), cutoff = `cutoff` (arg, param_3).
//
// Step 1 — grow the integer radius `r` (float, +1.0 each step) starting at 1.0
//   while  1/(powf(base, -(r - cutoff)) + 1) < 0.99  (kRadiusThreshold).
//   i.e. expand until the on-axis falloff reaches ~0.99 of full. `r` becomes the
//   half-extent of the square scan window (+1).
//
// Step 2 — scan the square window centred on (cx,cy), bounds-checked to [0, dim],
//   computing per-cell falloff and blending into the target mask:
//     mode 0 (MIN):  v = f(d) * strength ;          if (v < mask)  mask = v;
//     mode 1 (MAX):  v = (1 - f(d)) * strength ;    if (v > mask)  mask = v;  (strict >)
//
// Params (matching the disasm):
//   hf          = heightfield (only *hf == dim is read, for row stride dim+1)
//   center      = float[2] {x, y} brush centre (fractional)
//   cutoff      = param_3 (falloff midpoint distance)
//   falloffBase = param_4 (powf base)
//   strength    = param_5
//   mode        = param_6 (0 = MIN blend, 1 = MAX blend)
//   maskHdr     = param_7 ({float* data, ...} target mask header; *maskHdr == data ptr)
// ============================================================================
static void wg_eee0_radial_brush(int32_t* hf, const float* center, float cutoff,
                                 float falloffBase, float strength, char mode,
                                 int64_t* maskHdr) {
    int dim  = *(int32_t*)((uint8_t*)hf + HF_GRIDSIZE);
    int edge = dim + 1;   // row stride (iVar1 = iVar9 + 1)

    // Step 1: grow radius until on-axis falloff >= 0.99.
    //   exponent argument = negate_bits(g_fUVScale - cutoff) on first eval, then
    //   negate_bits(r - cutoff) as r grows. (g_fUVScale == 1.0 == the start r.)
    float r = kOne;
    float fall = wg_powf(falloffBase, wg_negate_bits(kOne - cutoff));
    while (kOne / (fall + kOne) < kRadiusThreshold) {
        r += kOne;
        fall = wg_powf(falloffBase, wg_negate_bits(r - cutoff));
    }

    int half  = dim >> 1;                  // iVar9 = iVar9 >> 1
    int cxBase = (int)center[0] + half;    // (int)param_2[0] + half  (col)  -- see note
    int cyBase = (int)center[1] + half;    // (int)param_2[1] + half  (row)
    int span   = (int)r + 1;               // iVar10 = (int)fVar17 + 1

    float* mask = (float*)(*maskHdr);

    // Disasm: outer var iVar15 = dy in [-span, span] (row), inner uVar13/iVar12 =
    // dx in [-span, span] (col). Distance uses (dx^2 + dy^2). The exe maps
    // param_2[1] (center.y) to the ROW offset (cyBase) and param_2[0] (center.x)
    // to the COL offset (cxBase); fVar2 = *param_2 (center.x) seeds the col base.
    for (int dy = -span; dy <= span; ++dy) {
        int row = dy + cyBase;
        if (row < 0 || row >= edge) continue;
        for (int dx = -span; dx <= span; ++dx) {
            int col = dx + cxBase;
            if (col < 0 || col >= edge) continue;

            float d = wg_sqrtf((float)(dx * dx + dy * dy));
            float f = wg_powf(falloffBase, wg_negate_bits(d - cutoff));
            f = kOne / (f + kOne);

            float* cell = &mask[(int64_t)row * edge + col];
            if (mode == 0) {
                float v = f * strength;
                if (v < *cell) *cell = v;                   // MIN blend
            } else {
                float v = (kOne - f) * strength;
                if (v >= *cell && v != *cell) *cell = v;    // MAX blend (strict >)
            }
        }
    }
}

// ============================================================================
// wg_f390_erosion == FUN_1400cc080 — DROPLET / THERMAL EROSION.
//   @ 0x1400cc080
//
// Runs `erodeIterations` (sizeRow[+0x24]) independent droplet passes. Each pass:
//   * pick a random start cell (col,row) in [1, dim-1] via Random::Rand(dim-1)+1.
//   * repeatedly: examine the cell's full 3x3 Moore neighbourhood (9 cells incl.
//     itself, scanned in a fixed tie-break order: top row L->R, mid row L->R,
//     bottom row L->R), find the LOWEST cell (strict <, first wins).
//   * if no cell is lower than the current cell (best == self), or current cell
//     height <= 0, or we stepped to the [0]/[dim] border, stop this droplet.
//   * otherwise transfer material between current and lowest:
//        slope  = (h_cur - h_low) * 0.0625          (kErodeFactor)
//        t      = min(slope, 0.8)                    (kErodeMaxXfer clamp)
//        h_cur' = (1 - t) * h_cur + t * h_low
//        h_low' = (1 - t) * h_low + t * h_cur
//     i.e. fraction t of the gap is exchanged, pulling the pair toward its mean.
//     Then walk to the lowest cell and repeat.
//
// HEIGHT EFFECT — *conservative*: each transfer is a symmetric exchange about the
// pair mean (h_cur' + h_low' == h_cur + h_low EXACTLY). Erosion therefore does NOT
// change the global SUM or MEAN of the heightmap (mass-conserving). It only
// REDISTRIBUTES: peaks get shaved, adjacent lower cells rise, slopes relax toward
// local flatness. No global lowering toward sea level comes from this stage.
// Net signed change of the whole field ~ 0 (the start cell never starts on the
// border ring, so the outermost row/col is left untouched). Local amplitude
// (max-min over a slope) shrinks; valley-floor mean is essentially unchanged.
// => NOT the source of the ~8-unit normalization.
// ============================================================================
extern "C" void wg_f390_erosion(void* hfv) {
    uint8_t* p = (uint8_t*)hfv;

    int dim  = *(int32_t*)(p + HF_GRIDSIZE);    // *param_1
    int edge = dim + 1;                         // iVar19 = iVar2 + 1 (row stride)

    int32_t* sizeRow = *(int32_t**)(p + HF_SIZEROW);
    if (!sizeRow) return;
    int iters = *(int32_t*)((uint8_t*)sizeRow + SIZEROW_ERODE_ITERS);

    if (iters <= 0) {
        return;
    }

    void* rng = p + HF_RANDOM;

    for (int it = 0; it < iters; ++it) {
        // Start cell in [1, dim-1] (Rand(dim-1) -> [0, dim-2], +1).
        int col = (int)reinterpret_cast<System::Random*>(rng)->Rand((uint32_t)(dim - 1)) + 1;   // B.3.4b bridge
        int row = (int)reinterpret_cast<System::Random*>(rng)->Rand((uint32_t)(dim - 1)) + 1;   // B.3.4b bridge

        while (true) {
            float* hm = *(float**)(p + HF_HEIGHTMAP);   // re-read base each step (matches disasm)
            int curIdx = row * edge + col;
            float hCur = hm[curIdx];
            if (hCur <= 0.0f) break;            // dead/sea cell — stop droplet

            // Scan 3x3 Moore neighbourhood for the strictly-lowest cell.
            // Tie-break: first-encountered wins (scan order fixed).
            int bestCol = -1, bestRow = -1;     // iVar9 / iVar14 (start -1,-1)
            int bestIdx = curIdx;               // iVar15 (start self)
            float bestH = hCur;                 // fVar20

            int rowTop = (row - 1) * edge;
            {
                float h = hm[rowTop + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowTop; bestRow = row - 1; bestH = h; }
                h = hm[rowTop + col];
                if (h < bestH) { bestIdx = rowTop + col; bestRow = row - 1; bestH = h; bestCol = col; }
                h = hm[rowTop + col + 1];
                if (h < bestH) { bestCol = col + 1; bestIdx = col + 1 + rowTop; bestRow = row - 1; bestH = h; }
            }
            int rowMid = row * edge;
            {
                float h = hm[rowMid + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowMid; bestH = h; bestRow = row; }
                h = hm[rowMid + col];               // self (never < bestH on first pass)
                if (h < bestH) { bestIdx = rowMid + col; bestH = h; bestCol = col; bestRow = row; }
                h = hm[rowMid + col + 1];
                if (h < bestH) { bestCol = col + 1; bestIdx = col + 1 + rowMid; bestH = h; bestRow = row; }
            }
            int rowBot = (row + 1) * edge;
            {
                float h = hm[rowBot + col - 1];
                if (h < bestH) { bestCol = col - 1; bestIdx = col - 1 + rowBot; bestH = h; bestRow = row + 1; }
                h = hm[rowBot + col];
                if (h < bestH) { bestIdx = rowBot + col; bestH = h; bestCol = col; bestRow = row + 1; }
                h = hm[rowBot + col + 1];
                if (h < bestH) { bestIdx = col + 1 + rowBot; bestH = h; bestCol = col + 1; bestRow = row + 1; }
            }

            // Conservative symmetric transfer toward the pair mean.
            float t = (hCur - bestH) * kErodeFactor;     // slope * 1/16
            if (t > kErodeMaxXfer) t = kErodeMaxXfer;    // clamp to 0.8
            hm[curIdx] = (kOne - t) * hCur + t * bestH;
            (*(float**)(p + HF_HEIGHTMAP))[bestIdx] = (kOne - t) * bestH + t * hCur;

            // Step to the lowest cell; stop if we didn't move (best == self),
            // or stepped onto the [0]/[dim] border ring.
            col = bestCol;
            row = bestRow;
            if (bestIdx == curIdx || col < 1 || row < 1 || col >= dim || row >= dim)
                break;
        }
    }

}

// ============================================================================
// wg_f390_carve == ConsumeProduceComponent_impl_ec50 — FEATURE STAMP FINALIZE/CARVE.
//   @ 0x1400cec50
//
// Builds two temp float masks sized param_1[4] (= (dim+1)^2 cells):
//     mulMask (local_a0)  — init 1.0 everywhere ; MIN-blended  (mode 0)
//     subMask (local_90)  — init 0.0 everywhere ; MAX-blended  (mode 1)
// Stamps features into them via the shared radial brush eee0:
//
//   Pass A — patch groups (param_1[6]/[8]): for each group g (count param_1[8]),
//     for each 0x1C-stride record in the group's recs[] (count at grp+8):
//       rec = {float pt[2]@0, float a@8, float b@0xc, float c@0x10, float d@0x14, float e@0x18}
//       eee0(hf, pt, cutoff=c, base=b, strength=1.0, mode 0 -> mulMask)   // MIN
//       eee0(hf, pt, cutoff=d, base=e, strength=a,   mode 1 -> subMask)   // MAX
//
//   Pass B — listC points (param_1[0xe]/[0x10]): for each 0xC-stride record
//       {float pt[2]@0, float radius@8}:
//       eee0(hf, pt, cutoff=radius, base=2.25, strength=2.0, mode 1 -> subMask)  // MAX
//
// Finalize (the carve) — for every heightmap cell i in [0, param_1[4]):
//       h[i] = mulMask[i] * h[i] - subMask[i]
//
// HEIGHT EFFECT — a MULTIPLY-then-SUBTRACT carve, NOT mass-conserving:
//   * mulMask in (0, 1]: where features are stamped it DROPS below 1.0, SCALING the
//     height DOWN toward 0 (lowering). Untouched cells keep mul=1.0 (NO change).
//   * subMask in [0, ~2.25]: where features are stamped it RISES above 0.0,
//     SUBTRACTING an absolute amount AFTER the scale — pulling heights toward / below
//     sea level (0). Untouched cells keep sub=0.0 (NO change).
//   Net: h' = mul*h - sub. Stamped regions are lowered both multiplicatively and
//   additively (rivers/lakes/resource cuts carve channels DOWN toward 0 = sea level),
//   and CAN go negative (below sea). This is a LOCAL operation: only cells under a
//   stamp move; bare land (mul=1, sub=0) is left EXACTLY as-is. Globally it lowers
//   the mean only by the total stamped mass, NOT by a uniform field-wide offset.
//   => ec50 is NOT the global ~8-unit lowerer; it carves localized water/feature beds.
// ============================================================================
extern "C" void wg_f390_carve(void* hfv) {
    int32_t* param_1 = (int32_t*)hfv;
    uint8_t* p = (uint8_t*)hfv;

    int cellCount = *(int32_t*)(p + HF_HM_COUNT);    // param_1[4]

    // Two temp masks (sub_3a00 resize: {data,count,cap,pad} headers on the stack).
    struct { void* data; int count; int cap; int pad; } mulHdr = {nullptr, 0, 0, 0};
    struct { void* data; int count; int cap; int pad; } subHdr = {nullptr, 0, 0, 0};
    Reimpl_Array_Resize_Stride04(&mulHdr, cellCount);   // local_a0 (MIN mask)
    Reimpl_Array_Resize_Stride04(&subHdr, cellCount);   // local_90 (MAX mask)

    float* mulMask = (float*)mulHdr.data;
    float* subMask = (float*)subHdr.data;
    if (mulMask && subMask) {
        for (int i = 0; i < cellCount; ++i) {
            mulMask[i] = kOne;   // init 1.0 (0x3f800000 in disasm)
            subMask[i] = 0.0f;   // init 0.0
        }
    }

    int64_t* mulMaskHdr = (int64_t*)&mulHdr;   // *hdr == data ptr (eee0 reads *param_7)
    int64_t* subMaskHdr = (int64_t*)&subHdr;

    // Pass A — patch groups (0x1C-stride records).
    int groupCount = *(int32_t*)(p + HF_PATCH_CNT);    // param_1[8]
    if (groupCount > 0) {
        uint8_t* groupBase = *(uint8_t**)(p + HF_PATCH_DATA);   // param_1[6]
        for (int g = 0; g < groupCount; ++g) {
            // Each group elem (16B) = {void* recs, int count, ...} at groupBase + g*0x10.
            uint8_t* grp  = groupBase + (int64_t)g * 0x10;
            int      recN = *(int32_t*)(grp + 8);
            uint8_t* recs = *(uint8_t**)(grp + 0);
            for (int r = 0; r < recN; ++r) {
                uint8_t* rec = recs + (int64_t)r * 0x1c;
                float center[2];
                center[0] = *(float*)(rec + 0);
                center[1] = *(float*)(rec + 4);
                float a = *(float*)(rec + 0x08);   // pass-2 strength
                float b = *(float*)(rec + 0x0c);   // pass-1 cutoff (inner radius)
                float c = *(float*)(rec + 0x10);   // pass-1 falloff base
                float d = *(float*)(rec + 0x14);   // pass-2 cutoff (outer radius)
                float e = *(float*)(rec + 0x18);   // pass-2 falloff base
                // MIN into mulMask: eee0(center, cutoff=b, base=c, strength=1.0, mode 0)
                wg_eee0_radial_brush(param_1, center, b, c, kOne, 0, mulMaskHdr);
                // MAX into subMask: eee0(center, cutoff=d, base=e, strength=a, mode 1)
                wg_eee0_radial_brush(param_1, center, d, e, a, 1, subMaskHdr);
            }
        }
    }

    // Pass B — listC points (0xC-stride {pt[2], radius}). MAX into subMask.
    int listcCount = *(int32_t*)(p + HF_LISTC_CNT);    // param_1[0x10]
    if (listcCount > 0) {
        uint8_t* base = *(uint8_t**)(p + HF_LISTC_DATA);   // param_1[0xe]
        for (int i = 0; i < listcCount; ++i) {
            uint8_t* rec = base + (int64_t)i * 0xc;
            float center[2];
            center[0] = *(float*)(rec + 0);
            center[1] = *(float*)(rec + 4);
            float radius = *(float*)(rec + 8);
            // eee0(center, cutoff=radius, base=2.0, strength=2.25, mode 1 -> subMask)
            wg_eee0_radial_brush(param_1, center, radius, kListCExp, kListCStrength, 1, subMaskHdr);
        }
    }

    // Finalize carve: h[i] = mulMask[i] * h[i] - subMask[i].
    float* hm = *(float**)(p + HF_HEIGHTMAP);          // param_1[2]
    if (hm && mulMask && subMask) {
        for (int i = 0; i < cellCount; ++i) {
            hm[i] = mulMask[i] * hm[i] - subMask[i];
        }
    }

    {
        System::MemoryAllocator::MemFree(subHdr.data);   // frees local_90 first (matches disasm order)
        System::MemoryAllocator::MemFree(mulHdr.data);   // then local_a0
    }
}

// ============================================================================
// wg_f390_smooth == ConsumeProduceComponent_impl_c350 — WEIGHTED BOX BLUR (one pass).
//   @ 0x1400cc350   (f390 calls it twice — orchestrator repeats it)
//
// Disassembly-exact kernel (re-derived from 0x1400cc350 asm, NOT the flattened
// decompiler output). Outer loop y = EBP in [0, dim); inner loop x = EBX in [0, dim).
// Indices into the heightmap use row stride edge = dim+1, with every index CLAMPED
// to [0, dim] (edge-replicate). For each (x, y):
//
//   centre term:  accum = h[ edge*clamp(y) + clamp(x) ] * 2.0   (kCenterWeight)
//                 wsum  = 2.0
//   neighbour loop (R10 = -1, 0, +1; SKIPS R10 == 0):  column is FIXED at clamp(x-1),
//                 row = clamp(y + R10)  ->  contributes (x-1, y-1) and (x-1, y+1):
//                 accum += h[ edge*clamp(y-1) + clamp(x-1) ]
//                 accum += h[ edge*clamp(y+1) + clamp(x-1) ]
//                 wsum  += 1.0  (twice)   -> wsum total = 4.0
//   out[ edge*y + x ] = accum / wsum
//
// So the kernel is the ASYMMETRIC 3-tap stencil
//       { (x,y):2.0,  (x-1,y-1):1.0,  (x-1,y+1):1.0 } / 4.0
// (the neighbour column is x-1 only — verified in asm: `LEA R8D,[RBX-1]` is the
// only column ever formed in the inner loop; the centre uses x). Results go to a
// temp buffer (sub_3a00, sized param_1[4]) laid out edge-wide, then the first
// param_1[4] dwords are copied back over the heightmap.
//
// HEIGHT EFFECT — a normalized weighted average (Σw·h / Σw, all w > 0): it is
// approximately mean-PRESERVING for a locally-flat field. Because the kernel is
// biased toward the (x-1) column, it shifts/diffuses height very slightly toward
// +x and rounds local curvature, but the divisor (4.0) exactly matches the weight
// sum (2+1+1) so a constant field maps to itself (no offset). It shrinks local
// extrema (peaks down, pits up); the global mean drifts only at second order near
// curved/edge regions (edge-replicate clamping barely pulls border cells inward).
// Running twice doubles the diffusion but not any bias.
// => smoothing is NOT the source of the ~8-unit global lowering.
// ============================================================================
extern "C" void wg_f390_smooth(void* hfv) {
    uint8_t* p = (uint8_t*)hfv;

    int cellCount = *(int32_t*)(p + HF_HM_COUNT);    // param_1[4]

    // Temp output buffer (sub_3a00).
    struct { void* data; int count; int cap; int pad; } tmpHdr = {nullptr, 0, 0, 0};
    Reimpl_Array_Resize_Stride04(&tmpHdr, cellCount);
    float* tmp = (float*)tmpHdr.data;

    int dim  = *(int32_t*)(p + HF_GRIDSIZE);   // *param_1
    int edge = dim + 1;                        // iVar3 = iVar6 + 1 (row stride)
    float* hm = *(float**)(p + HF_HEIGHTMAP);  // *(longlong*)(param_1+2)

    auto clamp = [&](int v) { return v < 0 ? 0 : (v > dim ? dim : v); };

    if (tmp && hm) {
        for (int y = 0; y < dim; ++y) {
            // out index advances 1 per col, `edge` per row (tmp is edge-wide;
            // only [0,dim) per row is written, matching the disasm RSI walk).
            float* outRow = tmp + (int64_t)y * edge;
            for (int x = 0; x < dim; ++x) {
                int cyC = clamp(y);
                int cxC = clamp(x);
                float accum = hm[(int64_t)edge * cyC + cxC] * kCenterWeight;   // centre * 2.0
                float wsum  = kCenterWeight;

                int colL = clamp(x - 1);                  // fixed neighbour column = x-1
                // R10 = -1 -> row = y-1 ; R10 = +1 -> row = y+1 (R10 = 0 skipped)
                accum += hm[(int64_t)edge * clamp(y - 1) + colL];
                wsum  += kOne;
                accum += hm[(int64_t)edge * clamp(y + 1) + colL];
                wsum  += kOne;

                outRow[x] = accum / wsum;
            }
        }

        // Copy temp back over the heightmap (cellCount floats).
        for (int i = 0; i < cellCount; ++i) hm[i] = tmp[i];
    }

    System::MemoryAllocator::MemFree(tmpHdr.data);
}
