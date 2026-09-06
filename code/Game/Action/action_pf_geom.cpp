// code/Game/Action/action_pf_geom.cpp
// Pathfinding / region-graph / geometry / heuristic leaf sub-helpers (PF porter wave).
//
// Faithful 1:1 ports of nine exe sub-helpers that are pure leaves (no NEEDS-PORT
// children — only Math::Vector3 engine exports, System::MemoryAllocator/Random
// exports, and bit/array/index math). In standalone these resolve to 0 (silently
// skipped) unless registered; this file provides the reimplementations and
// standalone/reg_pf_geom.cpp registers them.
//
// Transcribed literally from the Ghidra decompile/disassembly. Do NOT "improve",
// simplify, or reorder. None of these are exercised on the autostart path, so a
// wrong port is undetectable silent corruption — strictly worse than unregistered.
//
//   0x1400a5ac0  region-pair BST lookup (key0,key1) -> value index             [loop]
//   0x1400a5ec0  neighbour tile index by 8-direction code
//   0x1400a6030  movement-cost octile/diagonal heuristic
//   0x1400aa810  ray/triangle (Moller-Trumbore) barycentric intersect (float)
//   0x1400ae180  edge-mask -> tile-shape LUT index (DAT_1401c22d0)
//   0x14009d0a0  expanding-ring spiral search for nearest valid region cell     [loop]
//   0x140018880  quadtree node subdivide / push 4 children
//   0x140018dd0  quadtree/box-walker init (zeroes ~0xFF*0x24 stack records)
//   0x140053ca0  pasture/migration target picker (collect+random-pick cells)    [loop]

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {

// ---- Math::Vector3 engine exports (Runtime-x64.dll), MSVC member ABI ----------
// 3-float ctor:  ??0Vector3@Math@@QEAA@MMM@Z          (this, x, y, z)
// single-Scalar ctor (splat): ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z (this, &s)
// Cross:  ?Cross@Vector3@Math@@QEBA?AV12@AEBV12@@Z    (this, retbuf, &other) -> retbuf
// Dot:    ?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z (this, retbuf, &other) -> retbuf(float)
// Normalize: ?Normalize@Vector3@Math@@QEBA?AV12@XZ    (this, retbuf) -> retbuf
typedef void  (*fn_V3Ctor3_t)(void* self, float x, float y, float z);
typedef void  (*fn_V3CtorSplat_t)(void* self, const float* s);
typedef void* (*fn_V3Cross_t)(const void* self, void* retbuf, const void* other);
typedef void* (*fn_V3Dot_t)(const void* self, void* retbuf, const void* other);
typedef void* (*fn_V3Normalize_t)(const void* self, void* retbuf);

static fn_V3Ctor3_t     s_v3ctor3     = nullptr;  // slot 0x140152a30
static fn_V3CtorSplat_t s_v3ctorsplat = nullptr;  // slot 0x140151320
static fn_V3Cross_t     s_v3cross     = nullptr;  // slot 0x1401526a8
static fn_V3Dot_t       s_v3dot       = nullptr;  // slot 0x140151298
static fn_V3Normalize_t s_v3norm      = nullptr;  // slot 0x1401512c0

// System::MemoryAllocator static MemAlloc/MemFree + System::Random::Rand.
typedef void* (*fn_MemAllocStatic_t)(uint64_t size, uint32_t align);  // real export is SAPEAX_KH (uint64 size, int align); SAPEAXII does not exist
typedef void  (*fn_MemFreeStatic2_t)(void* ptr);                      // SAXPEAX
typedef uint32_t (*fn_RandMax_t)(void* random, uint32_t max);         // QEAAII
static fn_MemAllocStatic_t s_memAlloc = nullptr;  // slot 0x140152b60
static fn_MemFreeStatic2_t s_memFree  = nullptr;  // slot 0x140152b68
static fn_RandMax_t        s_rand     = nullptr;  // slot 0x1401512e8

static bool s_init = false;
static void EnsureFns() {
    if (s_init) return;
    // Prefer the registered exe dispatch slot (it stores the Runtime fn-ptr); fall
    // back to ResolveExport for the unregistered ones.
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_v3ctor3     = reinterpret_cast<fn_V3Ctor3_t>(slot(0x140152a30));
    if (!s_v3ctor3) s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    s_v3ctorsplat = reinterpret_cast<fn_V3CtorSplat_t>(slot(0x140151320));
    if (!s_v3ctorsplat) s_v3ctorsplat = reinterpret_cast<fn_V3CtorSplat_t>(
        ResolveExport("??0Vector3@Math@@QEAA@AEBVScalar@1@@Z"));
    s_v3cross = reinterpret_cast<fn_V3Cross_t>(slot(0x1401526a8));
    if (!s_v3cross) s_v3cross = reinterpret_cast<fn_V3Cross_t>(
        ResolveExport("?Cross@Vector3@Math@@QEBA?AV12@AEBV12@@Z"));
    s_v3dot = reinterpret_cast<fn_V3Dot_t>(slot(0x140151298));
    if (!s_v3dot) s_v3dot = reinterpret_cast<fn_V3Dot_t>(
        ResolveExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z"));
    s_v3norm = reinterpret_cast<fn_V3Normalize_t>(slot(0x1401512c0));
    if (!s_v3norm) s_v3norm = reinterpret_cast<fn_V3Normalize_t>(
        ResolveExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ"));
    s_memAlloc = reinterpret_cast<fn_MemAllocStatic_t>(slot(0x140152b60));
    if (!s_memAlloc) s_memAlloc = reinterpret_cast<fn_MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    s_memFree = reinterpret_cast<fn_MemFreeStatic2_t>(slot(0x140152b68));
    if (!s_memFree) s_memFree = reinterpret_cast<fn_MemFreeStatic2_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_rand = reinterpret_cast<fn_RandMax_t>(slot(0x1401512e8));
    if (!s_rand) s_rand = reinterpret_cast<fn_RandMax_t>(
        ResolveExport("?Rand@Random@System@@QEAAII@Z"));
    s_init = true;
}

// Float constants (read from exe .rdata in the original; inlined here as bit-exact
// values — verified via read_memory):
//   DAT_14017226c = 0xb8d1b717 = -1.0e-4f   (barycentric epsilon)
//   DAT_140172114 = 0x3f000000 =  0.5f
//   DAT_140172280 = 0xbf800000 = -1.0f
//   DAT_14017214c = 0x3f800000 =  1.0f      (g_fUVScale)
static inline float C_eps()    { uint32_t b = 0xb8d1b717u; float f; __builtin_memcpy(&f, &b, 4); return f; }
static inline float C_half()   { return 0.5f; }
static inline float C_negone() { return -1.0f; }
static inline float C_one()    { return 1.0f; }

} // namespace

// ============================================================
// 0x1400a5ac0 — region-pair BST lookup: (a,b) ordered -> value index
// ============================================================
// Self+0x28 = BST root; nodes: +0x00 key0(int,hi), +0x04 key1(int,lo),
// +0x08 valueIdx(int), +0x18 left, +0x20 right. Self+0x18 = value array base
// (stride 0x18); result = *(int*)(arr + valueIdx*0x18 + 8). Orders (a,b) so
// hi >= lo before the descent. Returns 0xffffffff if not found.
uint32_t Pf_RegionPairLookup_5ac0(uintptr_t self, int a, int b) {
    int hi = a, lo = b;
    if (a < b) { hi = b; lo = a; }
    uint8_t* node = *reinterpret_cast<uint8_t**>(self + 0x28);
    while (true) {
        if (node == nullptr) return 0xffffffffu;
        int key0 = *reinterpret_cast<int*>(node + 0x00);
        if (hi < key0) {                       // go left
            node = *reinterpret_cast<uint8_t**>(node + 0x18);
            continue;
        }
        if (hi > key0) {                       // go right
            node = *reinterpret_cast<uint8_t**>(node + 0x20);
            continue;
        }
        // hi == key0: compare lo against key1
        int key1 = *reinterpret_cast<int*>(node + 0x04);
        if (lo < key1) {                       // go left
            node = *reinterpret_cast<uint8_t**>(node + 0x18);
            continue;
        }
        if (lo > key1) {                       // go right
            node = *reinterpret_cast<uint8_t**>(node + 0x20);
            continue;
        }
        // exact match (hi==key0 && lo==key1)
        if (node == nullptr) return 0xffffffffu;
        int idx = *reinterpret_cast<int*>(node + 0x08);
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(self + 0x18);
        return *reinterpret_cast<uint32_t*>(arr + (int64_t)idx * 0x18 + 8);
    }
}

// ============================================================
// 0x1400a5ec0 — neighbour tile index by 8-direction code
// ============================================================
// self+0xd8 = grid width (square). Decompose linear index into (row,col);
// apply the direction delta (case 0..7); bounds-check; optional blocked-bit
// gate at self+0xd6 (mask 0x80 in the byte map at self+0xe0); finally only
// return the cell if its passable bit (self+0xd0 >> (cellByte&0x1f) & 1) is set.
int Pf_NeighbourTile_5ec0(uintptr_t self, int index, uint32_t dir) {
    int w = *reinterpret_cast<int*>(self + 0xd8);
    int row = index / w;
    int col = index % w;
    switch (dir) {
    case 0: col = col + 1; break;
    case 1: col = col - 1; break;
    case 2: row = row + 1; break;            // case 2: +row
    case 3: row = row - 1; break;            // case 3: -row
    case 4: col = col + 1; row = row + 1; break;
    case 5: col = col + 1; row = row - 1; break;
    case 6: col = col - 1; row = row - 1; break;
    case 7: col = col - 1; row = row + 1; break;
    default: break;
    }
    if ((-1 < col) && (col < w) && (-1 < row) && (row < w)) {
        int cell = w * row + col;
        char gateFlag = *reinterpret_cast<char*>(self + 0xd6);
        uint8_t* byteMap = *reinterpret_cast<uint8_t**>(self + 0xe0);
        if (gateFlag == 0 || (byteMap[cell] & 0x80) == 0) {
            uint32_t passMask = *reinterpret_cast<uint32_t*>(self + 0xd0);
            uint8_t cb = byteMap[cell];
            if (((passMask >> (cb & 0x1f)) & 1) != 0)
                return cell;
            return -1;
        }
    }
    return -1;
}

// ============================================================
// 0x1400a6030 — movement-cost octile / diagonal heuristic
// ============================================================
// self+0xd8 = grid width. dx,dy = |col delta|,|row delta|. If self+0xd4 set:
// octile (10*(dx+dy) - 6*min). If self+0xd5 set: Manhattan*10 ((dx+dy)*10).
// Else: Chebyshev*10 (max*10).
int Pf_MoveHeuristic_6030(uintptr_t self, int from, int to) {
    int w = *reinterpret_cast<int*>(self + 0xd8);
    uint32_t udx = (uint32_t)(from % w - to % w);
    uint32_t udy = (uint32_t)(from / w - to / w);
    uint32_t s;
    s = (uint32_t)((int)udx >> 0x1f);
    int dx = (int)((udx ^ s) - s);          // abs(dx)
    s = (uint32_t)((int)udy >> 0x1f);
    int dy = (int)((udy ^ s) - s);          // abs(dy)
    if (*reinterpret_cast<char*>(self + 0xd4) != 0) {
        int mn = dy;
        if (dx < dy) mn = dx;
        return (dy + dx) * 10 + mn * -6;
    }
    if (*reinterpret_cast<char*>(self + 0xd5) != 0) {
        return (dy + dx) * 10;
    }
    if (dy < dx) dy = dx;
    return dy * 10;
}

// ============================================================
// 0x1400aa810 — ray/triangle (Moller-Trumbore) barycentric intersect
// ============================================================
// Args: (unused this), rayOrigin(float*P), rayDir(Vector3*D), maxT(float*),
//   V0,V1,V2 (Vector3*), outT(float*), outNormal(Vector3* or null).
// edge1=V1-V0, edge2=V2-V0, pvec=D x edge2, det=edge1.pvec; cull det<=0;
// u=tvec.pvec (tvec=P-V0); cull u<-eps or u>det; qvec=tvec x edge1; v=D.qvec;
// cull v<-eps or u+v>det; t=(1/det)*edge2.qvec; cull t<0 or t>*maxT; *outT=t;
// if outNormal: *outNormal = normalize(edge1 x edge2). Returns 1 on hit, 0 else.
// Transcribed from disasm 0x1400aa810 (decompiler retbuf-aliasing is misleading).
int Pf_RayTriIntersect_aa810(void* /*self*/, const float* P, const float* D,
                             const float* maxT, const float* V0, const float* V1,
                             const float* V2, float* outT, float* outNormal) {
    EnsureFns();
    float edge1[3], edge2[3], pvec[3], tvec[3], qvec[3];
    // edge1 = V1 - V0
    s_v3ctor3(edge1, V1[0] - V0[0], V1[1] - V0[1], V1[2] - V0[2]);
    // edge2 = V2 - V0
    s_v3ctor3(edge2, V2[0] - V0[0], V2[1] - V0[1], V2[2] - V0[2]);
    // pvec = D.Cross(edge2)
    s_v3cross(D, pvec, edge2);
    // det = edge1.Dot(pvec)
    float det; s_v3dot(edge1, &det, pvec);
    // tvec = P - V0
    s_v3ctor3(tvec, P[0] - V0[0], P[1] - V0[1], P[2] - V0[2]);
    // qvec = tvec.Cross(edge1)
    s_v3cross(tvec, qvec, edge1);
    // u = tvec.Dot(pvec)
    float u; s_v3dot(tvec, &u, pvec);
    // v = D.Dot(qvec)
    float v; s_v3dot(D, &v, qvec);

    const float eps = C_eps();
    if (det <= 0.0f) return 0;
    if (eps > u) return 0;
    if (u > det) return 0;
    if (eps > v) return 0;
    if (v + u > det) return 0;

    // t = (1.0/det) * edge2.Dot(qvec)
    float tdot; s_v3dot(edge2, &tdot, qvec);
    float t = (C_one() / det) * tdot;
    *outT = t;
    if (t < 0.0f) return 0;
    if (*maxT < t) return 0;
    if (outNormal != nullptr) {
        // outNormal = Normalize(edge1.Cross(edge2))
        float ncross[3], nnorm[3];
        s_v3cross(edge1, ncross, edge2);
        s_v3norm(ncross, nnorm);
        outNormal[0] = nnorm[0];
        outNormal[1] = nnorm[1];
        outNormal[2] = nnorm[2];
    }
    return 1;
}

// ============================================================
// 0x1400ae180 — edge-mask -> tile-shape LUT index (DAT_1401c22d0)
// ============================================================
// Eight corner/edge booleans (p2..p9). First, four diagonal flags (p6..p9) are
// cleared unless both adjacent orthogonal edges are present. Then an 8-bit mask
// is assembled (bit weights: p2=1, p3=2, p4=4, p5=8, p6=0x10, p7=0x20, p8=0x40,
// p9=0x80) and used to index the signed-byte table at DAT_1401c22d0.
int Pf_TileShapeLut_ae180(void* /*self*/, char p2, char p3, char p4, char p5,
                          char p6, char p7, char p8, char p9) {
    if ((p6 != 0) && ((p2 == 0) || (p4 == 0))) p6 = 0;
    if ((p7 != 0) && ((p3 == 0) || (p4 == 0))) p7 = 0;
    if ((p8 != 0) && ((p2 == 0) || (p5 == 0))) p8 = 0;
    if ((p9 != 0) && ((p3 == 0) || (p5 == 0))) p9 = 0;

    uint64_t idx =
        (uint64_t)((uint32_t)(-(int)(p9 != 0)) & 0x80) |
        (uint64_t)(p2 != 0 ? 1u : 0u) |
        (uint64_t)((uint32_t)(-(int)(p3 != 0)) & 2) |
        (uint64_t)((uint32_t)(-(int)(p4 != 0)) & 4) |
        (uint64_t)((uint32_t)(-(int)(p8 != 0)) & 0x40) |
        (uint64_t)((uint32_t)(-(int)(p5 != 0)) & 8) |
        (uint64_t)((uint32_t)(-(int)(p6 != 0)) & 0x10) |
        (uint64_t)((uint32_t)(-(int)(p7 != 0)) & 0x20);

    // Tile-shape variant LUT (DAT_1401c22d0, 256 x int8). In standalone ResolveAddr on a
    // raw .rdata table returns NULL (reference_standalone_rdata_null_fallback), so
    // dereferencing it read address `idx` (e.g. 0x8a) and faulted. Embed the retail bytes
    // as the fallback; 0xFF (== -1) marks an invalid edge combination.
    static const uint8_t kTileShapeLut[256] = {
        0,1,2,5,3,6,7,11,4,8,9,12,10,13,14,19,
        255,255,255,255,255,15,255,20,255,255,255,255,255,21,255,28,
        255,255,255,255,255,255,16,22,255,255,255,255,255,255,23,29,
        255,255,255,255,255,255,255,30,255,255,255,255,255,255,255,36,
        255,255,255,255,255,255,255,255,255,17,255,24,255,25,255,31,
        255,255,255,255,255,255,255,255,255,255,255,255,255,32,255,37,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,38,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,42,
        255,255,255,255,255,255,255,255,255,255,18,26,255,255,27,33,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,39,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,34,40,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,43,
        255,255,255,255,255,255,255,255,255,255,255,35,255,255,255,41,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,44,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,45,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,46,
    };
    const int8_t* lut = reinterpret_cast<const int8_t*>(ResolveAddr(Module::Exe, 0x1401c22d0));
    if (!lut) lut = reinterpret_cast<const int8_t*>(kTileShapeLut);
    return (int)lut[idx];
}

// ============================================================
// 0x14009d0a0 — expanding-ring spiral search for nearest valid region cell
// ============================================================
// param_1[0] = grid width (square). param_1+0x20 (int* arithmetic, = BYTE 0x80) =
// cell-record array base — the SAME array Pf_IsDstReachable_f810 reads at (int*)param_1+0x20.
// (stride 0x30; +0x18 = regionId int). Starting from cell param_2, if that cell's
// regionId < 0, spiral outward in rings of growing radius (1..3) testing the four
// "side" walks of each ring; the first neighbour with regionId >= 0 returns that
// neighbour's regionId. Returns the start cell's regionId if it was already >= 0,
// else -1 after exhausting radius 3.
// Literal transcription of the Ghidra decompile (index aliasing preserved).
int Pf_RegionSpiral_9d0a0(int* param_1, int param_2) {
    int iVar5 = *param_1;
    int64_t lVar16 = (int64_t)iVar5;
    // FIX (was `param_1 + 0x20/4` = byte 0x20, a NULL field → the documented "null
    // Game+0x20 region-cell array" crash). Ghidra `param_1 + 0x20` on int* = byte 0x80;
    // this is the populated cell array. Matches Pf_IsDstReachable_f810's read exactly.
    int64_t lVar6 = *reinterpret_cast<int64_t*>(param_1 + 0x20);
    uint32_t uVar8 = (uint32_t)(param_2 / iVar5);
    int iVar11 = param_2 % iVar5;
    uint64_t uVar18 = (uint64_t)(int)uVar8;
    int iVar9 = *reinterpret_cast<int*>(lVar6 + 0x18 + (int64_t)param_2 * 0x30);
    if (iVar9 < 0) {
        int64_t lVar21 = (int64_t)iVar11;
        iVar9 = 1;
        int64_t lVar14 = 1;
        uint64_t uVar19 = uVar18;
        uint64_t uVar13 = uVar18;
        uint64_t uVar15 = uVar18;
        uint32_t uVar23 = uVar8;
        uint32_t uVar22 = uVar8;
        do {
            uVar22 = uVar22 + 1;
            uVar23 = uVar23 - 1;
            uVar15 = uVar15 - 1;
            uVar13 = uVar13 + 1;
            if ((int64_t)(uVar15 - uVar18) <= lVar14) {
                int64_t lVar4 = (lVar21 - uVar18) + uVar15;
                int iVar1 = (iVar11 - uVar8) + uVar22;
                int iVar2 = (iVar11 - uVar8) + uVar23;
                int64_t lVar10 = lVar4;
                int iVar20 = iVar2;
                do {
                    int64_t lVar17 = (uVar18 - lVar21) + lVar10;
                    int iVar3 = ((int)uVar19 - iVar11) + iVar20;
                    if ((-1 < lVar17) && (iVar3 < iVar5)) {
                        if ((-1 < lVar4) &&
                            ((iVar2 < iVar5 &&
                              (-1 < *reinterpret_cast<int*>(
                                        lVar6 + 0x18 + (lVar16 * lVar17 + lVar4) * 0x30))))) {
                            return *reinterpret_cast<int*>(
                                lVar6 + 0x18 + (lVar16 * iVar3 + (int64_t)iVar2) * 0x30);
                        }
                        int64_t lVar12 = (lVar21 - uVar18) + uVar13;
                        if (((-1 < lVar12) && (iVar1 < iVar5)) &&
                            (-1 < *reinterpret_cast<int*>(
                                      lVar6 + 0x18 + (lVar16 * lVar17 + lVar12) * 0x30))) {
                            return *reinterpret_cast<int*>(
                                lVar6 + 0x18 + (lVar16 * iVar3 + (int64_t)iVar1) * 0x30);
                        }
                    }
                    uint32_t uVar7 = 0;
                    if (((-1 < lVar10) && (iVar20 < iVar5)) &&
                        (((-1 < (int64_t)uVar15 &&
                           (((int)uVar23 < iVar5 &&
                             (uVar7 = uVar23,
                              -1 < *reinterpret_cast<int*>(
                                       lVar6 + 0x18 + (lVar16 * (int64_t)uVar15 + lVar10) * 0x30))))) ||
                          ((-1 < (int64_t)uVar13 &&
                            (((int)uVar22 < iVar5 &&
                              (uVar7 = uVar22,
                               -1 < *reinterpret_cast<int*>(
                                        lVar6 + 0x18 + (lVar16 * (int64_t)uVar13 + lVar10) * 0x30))))))))) {
                        return *reinterpret_cast<int*>(
                            lVar6 + 0x18 + ((int)uVar7 * lVar16 + (int64_t)iVar20) * 0x30);
                    }
                    lVar10 = lVar10 + 1;
                    iVar20 = iVar20 + 1;
                } while (lVar10 - lVar21 <= lVar14);
                uVar19 = (uint64_t)uVar8;
            }
            iVar9 = iVar9 + 1;
            lVar14 = lVar14 + 1;
        } while (iVar9 < 4);
        iVar9 = -1;
    }
    return iVar9;
}

// ============================================================
// 0x140018880 — quadtree node subdivide / push 4 children
// ============================================================
// Box-walker stack at param_1: +0x08 = top index (int). Each record (stride 0x24)
// holds a 3-float min(+0x00), 3-float ... and at +0x30/+0x34/+0x38 a 3-float
// extent + at +0x3c childId, +0x40 depth, +0x44 flag. Pops the top record, and if
// the next depth < tree maxDepth (*(int*)(*(param_1+0x2428)+0xc)), pushes the four
// quadrant children (childId = parentId*4 + {1,2,3,4}). DAT_140172114 = 0.5,
// DAT_140172280 = -1.0, g_fUVScale = 1.0. Faithful transcription of the decompile.
void Pf_QuadtreeSubdivide_18880(uintptr_t param_1) {
    EnsureFns();
    const float c_half   = C_half();    // DAT_140172114
    const float c_negone = C_negone();  // DAT_140172280

    int iVar5 = *reinterpret_cast<int*>(param_1 + 8);
    int64_t lVar8 = (int64_t)iVar5 + 1;
    *reinterpret_cast<int*>(param_1 + 8) = iVar5 - 1;
    int iVar9 = *reinterpret_cast<int*>(param_1 + 0x1c + lVar8 * 0x24) + 1;
    if (iVar9 < *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x2428) + 0xc)) {
        int iVar6   = *reinterpret_cast<int*>(param_1 + 0x18 + lVar8 * 0x24);
        float fVar1 = *reinterpret_cast<float*>(param_1 + 0x08 + lVar8 * 0x24);
        float fVar2 = *reinterpret_cast<float*>(param_1 + 0x14 + lVar8 * 0x24);
        float fVar3 = *reinterpret_cast<float*>(param_1 + 0x00 + lVar8 * 0x24);
        float fVar4 = *reinterpret_cast<float*>(param_1 + 0x04 + lVar8 * 0x24);
        float fVar13 = *reinterpret_cast<float*>(param_1 + 0x0c + lVar8 * 0x24);
        float fVar12 = *reinterpret_cast<float*>(param_1 + 0x10 + lVar8 * 0x24) * c_half;
        *reinterpret_cast<int*>(param_1 + 8) = iVar5;
        lVar8 = (int64_t)iVar5 + 1;
        fVar13 = fVar13 * c_half;
        float fVar10 = fVar12 * c_negone;
        float fVar11 = fVar13 * c_negone;
        *reinterpret_cast<float*>(param_1 + 0x00 + lVar8 * 0x24) = fVar3 + fVar11;
        *reinterpret_cast<float*>(param_1 + 0x04 + lVar8 * 0x24) = fVar4 + fVar10;
        *reinterpret_cast<float*>(param_1 + 0x08 + lVar8 * 0x24) = fVar1 + fVar2;
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8);
        *reinterpret_cast<float*>(param_1 + 0x30 + lVar8 * 0x24) = fVar13;
        *reinterpret_cast<float*>(param_1 + 0x34 + lVar8 * 0x24) = fVar12;
        *reinterpret_cast<float*>(param_1 + 0x38 + lVar8 * 0x24) = fVar2;
        *reinterpret_cast<int*>(param_1 + 0x3c + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar6 * 4 + 1;
        *reinterpret_cast<int*>(param_1 + 0x40 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar9;
        *reinterpret_cast<uint32_t*>(param_1 + 0x44 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;

        // Vector3 stack slots local_a8 (A) and local_9c (B) — each a contiguous
        // 3-float record the ctor writes through. Mapping: a8->A[0],a4->A[1],a0->A[2],
        // 9c->B[0],98->B[1],94->B[2].
        float A[3], B[3];
        s_v3ctor3(A, fVar13, fVar10, fVar2);
        s_v3ctor3(B, A[0] + fVar3, A[1] + fVar4, A[2] + fVar1);
        *reinterpret_cast<int*>(param_1 + 8) = *reinterpret_cast<int*>(param_1 + 8) + 1;
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8) + 1;
        *reinterpret_cast<float*>(param_1 + 0x00 + lVar8 * 0x24) = B[0];
        *reinterpret_cast<float*>(param_1 + 0x04 + lVar8 * 0x24) = B[1];
        *reinterpret_cast<float*>(param_1 + 0x08 + lVar8 * 0x24) = B[2];
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8);
        *reinterpret_cast<float*>(param_1 + 0x30 + lVar8 * 0x24) = fVar13;
        *reinterpret_cast<float*>(param_1 + 0x34 + lVar8 * 0x24) = fVar12;
        *reinterpret_cast<float*>(param_1 + 0x38 + lVar8 * 0x24) = fVar2;
        *reinterpret_cast<int*>(param_1 + 0x3c + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar6 * 4 + 2;
        *reinterpret_cast<int*>(param_1 + 0x40 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar9;
        *reinterpret_cast<uint32_t*>(param_1 + 0x44 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;

        s_v3ctor3(B, fVar11, fVar12, fVar2);
        s_v3ctor3(A, B[0] + fVar3, B[1] + fVar4, B[2] + fVar1);
        *reinterpret_cast<int*>(param_1 + 8) = *reinterpret_cast<int*>(param_1 + 8) + 1;
        float fVar10b = *g_fUVScale;  // g_fUVScale loaded here in the original
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8) + 1;
        *reinterpret_cast<float*>(param_1 + 0x00 + lVar8 * 0x24) = A[0];
        *reinterpret_cast<float*>(param_1 + 0x04 + lVar8 * 0x24) = A[1];
        *reinterpret_cast<float*>(param_1 + 0x08 + lVar8 * 0x24) = A[2];
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8);
        *reinterpret_cast<float*>(param_1 + 0x30 + lVar8 * 0x24) = fVar13;
        *reinterpret_cast<float*>(param_1 + 0x34 + lVar8 * 0x24) = fVar12;
        *reinterpret_cast<float*>(param_1 + 0x38 + lVar8 * 0x24) = fVar2;
        *reinterpret_cast<int*>(param_1 + 0x3c + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar6 * 4 + 3;
        *reinterpret_cast<int*>(param_1 + 0x40 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar9;
        *reinterpret_cast<uint32_t*>(param_1 + 0x44 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;

        float scaleVec[3];
        s_v3ctor3(scaleVec, fVar10b, fVar10b, fVar10b);
        s_v3ctor3(B, fVar13 * scaleVec[0], fVar12 * scaleVec[1], fVar2 * scaleVec[2]);
        s_v3ctor3(A, B[0] + fVar3, B[1] + fVar4, B[2] + fVar1);
        *reinterpret_cast<int*>(param_1 + 8) = *reinterpret_cast<int*>(param_1 + 8) + 1;
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8) + 1;
        *reinterpret_cast<float*>(param_1 + 0x00 + lVar8 * 0x24) = A[0];
        *reinterpret_cast<float*>(param_1 + 0x04 + lVar8 * 0x24) = A[1];
        *reinterpret_cast<float*>(param_1 + 0x08 + lVar8 * 0x24) = A[2];
        lVar8 = (int64_t)*reinterpret_cast<int*>(param_1 + 8);
        *reinterpret_cast<float*>(param_1 + 0x30 + lVar8 * 0x24) = fVar13;
        *reinterpret_cast<float*>(param_1 + 0x34 + lVar8 * 0x24) = fVar12;
        *reinterpret_cast<float*>(param_1 + 0x38 + lVar8 * 0x24) = fVar2;
        *reinterpret_cast<int*>(param_1 + 0x3c + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar6 * 4 + 4;
        *reinterpret_cast<int*>(param_1 + 0x40 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = iVar9;
        *reinterpret_cast<uint32_t*>(param_1 + 0x44 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;
    }
}

// ============================================================
// 0x140018dd0 — quadtree / box-walker init (zeroes ~0xFF*0x24 stack records)
// ============================================================
// Initialises the walker struct at param_1: +0x08 top=-1, +0x14=param_3,
// +0x0c..0x14 zero, +0x18..0x20 zero, +0x20=param_4. Then zeroes the record array
// (256 records of stride 0x24, three 8-byte clears at -0x14/-0xc/-4 of each).
// param_1+0x2428 = root box (param_2). Pushes the root record (childId=0, depth=0)
// with extents (0.5*param_2[0]) on x/y. Returns param_1.
uintptr_t Pf_QuadtreeInit_18dd0(uintptr_t param_1, float* param_2, uint32_t param_3, uint32_t param_4) {
    EnsureFns();
    *reinterpret_cast<uint32_t*>(param_1 + 0x08) = 0xffffffffu;
    *reinterpret_cast<uint32_t*>(param_1 + 0x14) = param_3;
    *reinterpret_cast<uint64_t*>(param_1 + 0x0c) = 0;
    uintptr_t lVar2 = param_1 + 0x38;
    *reinterpret_cast<uint64_t*>(param_1 + 0x18) = 0;
    *reinterpret_cast<uint32_t*>(param_1 + 0x20) = param_4;
    int iVar3 = 0xff;
    do {
        iVar3 = iVar3 - 1;
        *reinterpret_cast<uint64_t*>(lVar2 - 0x14) = 0;
        *reinterpret_cast<uint64_t*>(lVar2 - 0x0c) = 0;
        *reinterpret_cast<uint64_t*>(lVar2 - 0x04) = 0;
        lVar2 = lVar2 + 0x24;
    } while (-1 < iVar3);
    *reinterpret_cast<float**>(param_1 + 0x2428) = param_2;

    float local_res18 = 0.0f;
    float fVar4 = *param_2 * C_half();   // DAT_140172114 = 0.5
    float local_38[3];
    s_v3ctorsplat(local_38, &local_res18);   // splat 0 -> (0,0,0)
    *reinterpret_cast<int*>(param_1 + 8) = *reinterpret_cast<int*>(param_1 + 8) + 1;
    int64_t lVar2b = (int64_t)*reinterpret_cast<int*>(param_1 + 8) + 1;
    *reinterpret_cast<float*>(param_1 + 0x00 + lVar2b * 0x24) = local_38[0];
    *reinterpret_cast<float*>(param_1 + 0x04 + lVar2b * 0x24) = local_38[1];
    *reinterpret_cast<float*>(param_1 + 0x08 + lVar2b * 0x24) = local_38[2];
    lVar2b = (int64_t)*reinterpret_cast<int*>(param_1 + 8);
    *reinterpret_cast<float*>(param_1 + 0x30 + lVar2b * 0x24) = fVar4;
    *reinterpret_cast<float*>(param_1 + 0x34 + lVar2b * 0x24) = fVar4;
    *reinterpret_cast<uint32_t*>(param_1 + 0x38 + lVar2b * 0x24) = 0;
    *reinterpret_cast<uint32_t*>(param_1 + 0x3c + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;
    *reinterpret_cast<uint32_t*>(param_1 + 0x40 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;
    *reinterpret_cast<uint32_t*>(param_1 + 0x44 + (int64_t)*reinterpret_cast<int*>(param_1 + 8) * 0x24) = 0;
    return param_1;
}

// ============================================================
// 0x140053ca0 — pasture/migration target picker (collect + random-pick cells)
// ============================================================
// param_1+0x30 = grid width; param_1+0x48 = sub-grid divisor; param_1+0x38 = cell
// array base (stride 0xc; +0x00 = first int, a >0 = "valid" flag). Walks the
// (iVar2 x iVar2) block at the (param_2)-th sub-tile, collecting valid cell indices
// into a dynamically-grown int array (System::MemoryAllocator::MemAlloc, growth *3
// past the first). If any collected, picks one uniformly (System::Random::Rand on
// g_pGameWorldSingleton+0xd8) and maps it to a world coordinate via the +0x28 cell
// size from param_1+0x28 and the world stride at g_pGameWorldSingleton+0xf8.
// Returns the world index or -1. Frees the temp array. 1:1 transcription.
int Pf_PastureTargetPick_53ca0(uintptr_t param_1, int param_2) {
    EnsureFns();
    int iVar2 = *reinterpret_cast<int*>(param_1 + 0x30) / *reinterpret_cast<int*>(param_1 + 0x48);
    int iVar13 = iVar2 * (param_2 % *reinterpret_cast<int*>(param_1 + 0x48));
    int iVar12 = iVar2 * (param_2 / *reinterpret_cast<int*>(param_1 + 0x48));
    void* pvVar5 = nullptr;
    uint32_t uVar16 = 0;
    uint32_t uVar4 = 0;
    int64_t lVar8 = (int64_t)iVar12;
    int64_t local_88 = 0;
    int iVar2_result = -1;
    if (0 < iVar2) {
        do {
            int64_t lVar15 = 0;
            void* pvVar11 = pvVar5;
            int iVar14 = iVar13;
            do {
                int iVar1 = *reinterpret_cast<int*>(param_1 + 0x30);
                pvVar5 = pvVar11;
                if (0 < *reinterpret_cast<int*>(
                            *reinterpret_cast<int64_t*>(param_1 + 0x38) +
                            ((int64_t)iVar1 * (lVar8 + local_88) + iVar13 + lVar15) * 0xc)) {
                    uint32_t uVar10 = uVar16 + 1;
                    if (uVar10 != uVar16) {
                        if ((int)uVar4 < (int)uVar10) {
                            uVar4 = uVar10;
                            if (uVar16 != 0) {
                                uVar4 = uVar10 * 3;
                            }
                            pvVar5 = s_memAlloc((uint64_t)(int)uVar4 << 2, 4);
                        }
                        uint32_t uVar6 = 0;
                        if (pvVar11 == pvVar5) {
                            uVar6 = uVar16;
                        }
                        int64_t lVar9 = (int64_t)(int)uVar6;
                        if (lVar9 < (int)uVar10) {
                            int* piVar7 = reinterpret_cast<int*>((int64_t)pvVar5 + lVar9 * 4);
                            do {
                                if (piVar7 != nullptr) {
                                    int iVar3 = iVar12 * iVar1 + iVar14;
                                    if (lVar9 < (int)uVar16) {
                                        iVar3 = *reinterpret_cast<int*>(
                                            ((int64_t)pvVar11 - (int64_t)pvVar5) + (int64_t)piVar7);
                                    }
                                    *piVar7 = iVar3;
                                }
                                lVar9 = lVar9 + 1;
                                piVar7 = piVar7 + 1;
                            } while (lVar9 < (int)uVar10);
                        }
                        uVar16 = uVar10;
                        if (pvVar11 != pvVar5) {
                            s_memFree(pvVar11);
                        }
                    }
                }
                iVar14 = iVar14 + 1;
                lVar15 = lVar15 + 1;
                pvVar11 = pvVar5;
            } while (lVar15 < iVar2);
            iVar12 = iVar12 + 1;
            local_88 = local_88 + 1;
        } while (local_88 < iVar2);
        if (uVar16 != 0) {
            uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;
            void* randomObj = *reinterpret_cast<void**>(game + 0xd8);
            uint32_t uVar4r = s_rand(randomObj, uVar16);
            int picked = *reinterpret_cast<int*>((int64_t)pvVar5 + (int64_t)(int)uVar4r * 4);
            int64_t lVar8b = *reinterpret_cast<int64_t*>(param_1 + 0x28);
            int cellSize = *reinterpret_cast<int*>(lVar8b + 0x28);
            int worldStride = **reinterpret_cast<int**>(game + 0xf8);
            iVar2_result =
                worldStride *
                    (cellSize * (picked / *reinterpret_cast<int*>(param_1 + 0x30)) + (cellSize >> 1)) +
                cellSize * (picked % *reinterpret_cast<int*>(param_1 + 0x30)) + (cellSize >> 1);
        }
    }
    s_memFree(pvVar5);
    return iVar2_result;
}
