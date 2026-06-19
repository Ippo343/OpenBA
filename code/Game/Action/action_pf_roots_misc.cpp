// code/Game/Action/action_pf_roots_misc.cpp
// PF porter wave 3 — three Cluster-C "root" sub-helpers (large, all sub-callees
// already registered). Faithful 1:1 ports from the Ghidra decompile, cross-checked
// against disassemble_function for the intricate index math.
//
// None are exercised on the autostart path, so a wrong port is undetectable silent
// corruption — strictly worse than leaving the addr unregistered. Transcribed
// literally; no "improvements". Registration in standalone/reg_pf_roots_misc.cpp.
//
//   0x14009bd40  FindPlacementCell — expanding Chebyshev-ring spiral cell search
//                (calls f3e0 grid->world + Math::Vector3 ctor + Math::Scalar::Abs)
//   0x140092260  spatial entity proximity query via quadtree walk (uses an ~0x2438
//                local walker built by 18dd0 / advanced by 18880; Box::Intersect +
//                Vector3::Dot + InternalBasePtr)
//   0x140122340  HerdComponent::StartMigration (calls 053ca0 target picker, 003110
//                queue-push, 0c2ea0 mark-removal; see loop-exit note below)

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {

// ---- exe sub-callees (all registered) resolved via ResolveAddr(Module::Exe) ----
//   0x14009f3e0  CharacterComponent_impl_f3e0: grid->world; writes Vector3 to outA.
//   0x140018dd0  quadtree/box-walker init.
//   0x140018880  quadtree node subdivide / push 4 children.
//   0x140053ca0  pasture/migration target picker.
//   0x140003110  dynamic 8-byte-element array set-count + insert.
//   0x1400c2ea0  mark entity (+ removable components) for removal.
typedef float* (*fn_f3e0_t)(int* mapGrid, float* outA, float* inCell, char flag, void* extra);
typedef void   (*fn_QInit_t)(void* walker, float* box, uint32_t a, uint32_t b);   // 18dd0
typedef void   (*fn_QSubdiv_t)(void* walker);                                     // 18880
typedef int    (*fn_TargetPick_t)(void* self, int idx);                           // 053ca0
typedef void   (*fn_DynArrSet_t)(void* arr, int newCount, void* valuePtr);        // 003110
typedef void   (*fn_MarkRemoval_t)(void* self);                                   // 0c2ea0

static fn_f3e0_t        s_f3e0      = nullptr;
static fn_QInit_t       s_qinit     = nullptr;
static fn_QSubdiv_t     s_qsubdiv   = nullptr;
static fn_TargetPick_t  s_pick      = nullptr;
static fn_DynArrSet_t   s_dynset    = nullptr;
static fn_MarkRemoval_t s_markrem   = nullptr;

// ---- Math / Volume engine exports (Runtime-x64.dll) -------------------------
// 3-float ctor:  ??0Vector3@Math@@QEAA@MMM@Z              (this, x, y, z)
// splat ctor:    ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z    (this, &s)
// Dot:           ?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z (this, retbuf, &other)->retbuf(float)
// Box::Intersect:?Intersect@Box@Volume@@QEBA_NAEBV12@@Z   (this, &other)->bool
// Scalar::Abs:   ?Abs@Scalar@Math@@SA?AV12@V12@@Z         (val, retbuf)->retbuf
//   (disasm shows RCX=val, RDX=retbuf — member-style retbuf order)
typedef void  (*fn_V3Ctor3_t)(void* self, float x, float y, float z);
typedef void  (*fn_V3CtorSplat_t)(void* self, const float* s);
typedef void* (*fn_V3Dot_t)(const void* self, void* retbuf, const void* other);
typedef bool  (*fn_BoxIntersect_t)(const void* self, const void* other);
typedef float* (*fn_ScalarAbs_t)(const float* val, float* retbuf);

static fn_V3Ctor3_t     s_v3ctor3     = nullptr;  // slot 0x140152a30
static fn_V3CtorSplat_t s_v3ctorsplat = nullptr;  // slot 0x140151320
static fn_V3Dot_t       s_v3dot       = nullptr;  // slot 0x140151298
static fn_BoxIntersect_t s_boxisect   = nullptr;  // slot 0x140151e80
static fn_ScalarAbs_t   s_scalarabs   = nullptr;  // slot 0x140152420

// Type-id / float globals (resolved from exe .rdata).
static uint32_t* s_pTid_c7858 = nullptr;  // DAT_1401c7858 (StartMigration positive branch)
static uint32_t* s_pTid_c6f98 = nullptr;  // DAT_1401c6f98 (model/character component type)

static bool s_init = false;
static void EnsureFns() {
    if (s_init) return;
    auto R = [](uintptr_t a) -> uintptr_t { return ResolveAddr(Module::Exe, a); };
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_f3e0    = reinterpret_cast<fn_f3e0_t>(R(0x14009f3e0));
    s_qinit   = reinterpret_cast<fn_QInit_t>(R(0x140018dd0));
    s_qsubdiv = reinterpret_cast<fn_QSubdiv_t>(R(0x140018880));
    s_pick    = reinterpret_cast<fn_TargetPick_t>(R(0x140053ca0));
    s_dynset  = reinterpret_cast<fn_DynArrSet_t>(R(0x140003110));
    s_markrem = reinterpret_cast<fn_MarkRemoval_t>(R(0x1400c2ea0));

    s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(slot(0x140152a30));
    if (!s_v3ctor3) s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    s_v3ctorsplat = reinterpret_cast<fn_V3CtorSplat_t>(slot(0x140151320));
    if (!s_v3ctorsplat) s_v3ctorsplat = reinterpret_cast<fn_V3CtorSplat_t>(
        ResolveExport("??0Vector3@Math@@QEAA@AEBVScalar@1@@Z"));
    s_v3dot = reinterpret_cast<fn_V3Dot_t>(slot(0x140151298));
    if (!s_v3dot) s_v3dot = reinterpret_cast<fn_V3Dot_t>(
        ResolveExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z"));
    s_boxisect = reinterpret_cast<fn_BoxIntersect_t>(slot(0x140151e80));
    if (!s_boxisect) s_boxisect = reinterpret_cast<fn_BoxIntersect_t>(
        ResolveExport("?Intersect@Box@Volume@@QEBA_NAEBV12@@Z"));
    s_scalarabs = reinterpret_cast<fn_ScalarAbs_t>(slot(0x140152420));
    if (!s_scalarabs) s_scalarabs = reinterpret_cast<fn_ScalarAbs_t>(
        ResolveExport("?Abs@Scalar@Math@@SA?AV12@V12@@Z"));

    s_pTid_c7858 = reinterpret_cast<uint32_t*>(R(0x1401c7858));
    s_pTid_c6f98 = reinterpret_cast<uint32_t*>(R(0x1401c6f98));
    s_init = true;
}

// Float constants (read_memory-verified, inlined bit-exact):
//   DAT_140172248 = 0x447a0000 = 1000.0f   (height-diff tolerance)
//   DAT_14017213c = 0x3f7851ec = 0.97f      (min growth / slope threshold)
//   DAT_140172114 = 0x3f000000 = 0.5f       (half-cell offset)
//   DAT_140172174 = 0x40000000 = 2.0f
//   DAT_140172280 = 0xbf800000 = -1.0f
//   g_fUVScale (0x14017214c) = 1.0f         (extern in engine_funcs.h)
static inline float C_1000() { uint32_t b = 0x447a0000u; float f; __builtin_memcpy(&f,&b,4); return f; }
static inline float C_097()  { uint32_t b = 0x3f7851ecu; float f; __builtin_memcpy(&f,&b,4); return f; }
static inline float C_half() { return 0.5f; }
static inline float C_2()    { return 2.0f; }
static inline float C_neg1() { return -1.0f; }

} // namespace

// ============================================================
// 0x14009bd40 — FindPlacementCell: expanding Chebyshev-ring spiral cell search
// ============================================================
// param_1 = map/grid object (param_1[0] = grid width). param_2 = start cell index.
// param_3 = char "use-mask" flag. param_4 = longlong* (a required entity ref to match
// at the cell's +0x00 occupant). param_5 = uint walkability bitmask.
//
// Spirals outward in Chebyshev rings (iVar23 = 0..6). For each ring, only cells whose
// Chebyshev distance component equals the ring radius are tested (abs(dRow)==r ||
// abs(dCol)==r). A candidate cell must: be in-bounds; have a null occupant at
// grid[idx].+0x08; either no occupant at +0x00 OR (param_4 set AND occupant matches);
// have flag byte (+0x29) with neither bit 1 nor 2 set AND its +0x2a == 0xff; and NOT be
// present in the skip AVL tree at param_1+0x1d0 (key = cell index; left +0x10, right
// +0x18). If param_3==0: additionally validate the cell's south neighbour grid->world
// height delta against terrain (the two f3e0 samples must agree within 1000.0 and the
// neighbour growth >= 0.97). If param_3!=0: require the walkability bit (param_5 >>
// (flag&0x1f) & 1). On success returns the matching cell index; else -1 after 7 rings.
//
// Literal transcription of the decompile, node/flag offsets disasm-verified.
int Pf_FindPlacementCell_bd40(int* param_1, int param_2, char param_3, int64_t* param_4, uint32_t param_5) {
    EnsureFns();
    const float fHalf = C_half();    // DAT_140172114
    const float fMinGrowth = C_097(); // DAT_14017213c
    const float fTol = C_1000();     // DAT_140172248

    int iVar20 = -1;
    int iVar23 = 0;
    int local_e8 = -1;
    uint64_t uVar13 = (uint64_t)((int64_t)param_2 / (int64_t)*param_1);
    uint64_t uVar19 = (uint64_t)((int64_t)param_2 % (int64_t)*param_1);
    uint64_t uVar15 = uVar19 & 0xffffffff;
    uint32_t uVar6 = (uint32_t)uVar13;
    uint32_t local_f8 = (uint32_t)uVar19;
    uVar13 = uVar13 & 0xffffffff;
    int iVar17 = (int)local_f8 - (int)uVar6;
    int iVar7 = (int)uVar6 - (int)local_f8;
    uint32_t local_res8 = uVar6;
    int local_dc = iVar7;
    int iVar14, iVar12;
    do {
        iVar14 = (int)uVar15;
        int iVar21 = iVar7 + iVar14;
        iVar12 = (int)uVar13;
        if (iVar12 <= iVar21) {
            uint32_t uVar22 = (uint32_t)(iVar12 - (int)uVar6);
            uVar19 = uVar13;
            uint32_t uVar9 = uVar22;
            int iVar18;
            do {
                iVar20 = iVar17 + (int)uVar13;
                iVar18 = (int)uVar19;
                if (iVar20 <= (int)uVar15) {
                    uint32_t uVar24 = uVar22;
                    do {
                        // ring-membership: abs(dRow)==r OR abs(dCol)==r (abs via shift trick)
                        int abs9  = (int)(((int)uVar9  ^ ((int)uVar9  >> 0x1f)) - ((int)uVar9  >> 0x1f));
                        int abs24 = (int)(((int)uVar24 ^ ((int)uVar24 >> 0x1f)) - ((int)uVar24 >> 0x1f));
                        bool onRing = (abs9 == iVar23) || (abs24 == iVar23);
                        bool inBounds = false;
                        if (onRing && (-1 < iVar20) && (-1 < iVar18)) {
                            iVar7 = *param_1;
                            inBounds = (iVar20 < iVar7) && (iVar18 < iVar7);
                        }
                        if (inBounds) {
                            iVar7 = iVar7 * iVar18 + iVar20;
                            int64_t lVar10 = (int64_t)iVar7;
                            uintptr_t p1 = reinterpret_cast<uintptr_t>(param_1);
                            int64_t grid = *reinterpret_cast<int64_t*>(p1 + 0x80); // *(longlong*)(param_1+0x20)
                            if ((*reinterpret_cast<int64_t*>(grid + 8 + lVar10 * 0x30) == 0)) {
                                int64_t local_d8 = *reinterpret_cast<int64_t*>(grid + lVar10 * 0x30);
                                bool occOk = (local_d8 == 0);
                                if (!occOk && *param_4 != 0) {
                                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_d8);
                                    occOk = (*param_4 == local_d8);
                                }
                                if (occOk) {
                                    int64_t lVar1 = *reinterpret_cast<int64_t*>(p1 + 0x80); // *(longlong*)(param_1+0x20)
                                    uint32_t uVar8 = 1u << (*reinterpret_cast<uint8_t*>(lVar1 + 0x29 + lVar10 * 0x30) & 0x1f);
                                    if (((uVar8 & 2) == 0) &&
                                        (((uVar8 & 1) == 0) && (*reinterpret_cast<int8_t*>(lVar1 + 0x2a + lVar10 * 0x30) == -1))) {
                                        // skip AVL tree root at param_1+0xe8 (= *(int**)(param_1+0x3a) int* units)
                                        int* piVar2 = *reinterpret_cast<int**>(p1 + 0xe8);
                                        bool skip = false;
                                        while (piVar2 != nullptr) {
                                            if (iVar7 < *piVar2) {
                                                piVar2 = *reinterpret_cast<int**>(piVar2 + 4);   // left +0x10
                                            } else {
                                                if (iVar7 <= *piVar2) { skip = true; break; }   // found in skip tree
                                                piVar2 = *reinterpret_cast<int**>(piVar2 + 6);   // right +0x18
                                            }
                                        }
                                        if (!skip) {
                                            if (param_3 == 0) {
                                                iVar14 = *param_1;
                                                iVar12 = iVar14 * iVar18 + iVar20;
                                                float fVar25 = (float)(iVar14 >> 1) - fHalf;
                                                float v98[3];
                                                s_v3ctor3(v98, (float)(iVar12 % iVar14) - fVar25,
                                                          (float)(iVar12 / iVar14) - fVar25, 0.0f);
                                                float v88[3];
                                                s_f3e0(param_1, v88, v98, '\0', nullptr);
                                                iVar14 = *param_1;
                                                if ((iVar20 < iVar14) && (iVar18 < iVar14)) {
                                                    iVar12 = iVar14 * iVar18 + iVar20;
                                                    int64_t plVar16 = (int64_t)iVar12 * 0x30 +
                                                        *reinterpret_cast<int64_t*>(p1 + 0x80); // *(longlong*)(param_1+0x20)
                                                    if (((1u << (*reinterpret_cast<uint8_t*>(plVar16 + 0x29) & 0x1f) & 0xfffffffbU) == 0) &&
                                                        ((*reinterpret_cast<int64_t*>(plVar16) == 0) &&
                                                         (*reinterpret_cast<int8_t*>(plVar16 + 0x28) == 0))) {
                                                        // ext = (local_d0[2], local_c8) contiguous height-sample buffer.
                                                        float ext[3]; ext[2] = 0.0f;  // local_c8 = 0.0
                                                        fVar25 = (float)(iVar14 >> 1) - fHalf;
                                                        float vB8[3];
                                                        s_v3ctor3(vB8, (float)(iVar12 % iVar14) - fVar25,
                                                                  (float)(iVar12 / iVar14) - fVar25, 0.0f);
                                                        float vA8[3];
                                                        s_f3e0(param_1, vA8, vB8, '\0', ext);
                                                        if (fMinGrowth <= ext[2]) {   // local_c8
                                                            float local_e0 = vA8[2] - v88[2]; // local_a0 - local_80
                                                            float absbuf[1];
                                                            float* pfVar11 = s_scalarabs(&local_e0, absbuf);
                                                            if (*pfVar11 <= fTol) {
                                                                local_e8 = iVar7;
                                                                return local_e8;  // break -> success path
                                                            }
                                                        }
                                                    }
                                                }
                                            } else if ((param_5 >> (*reinterpret_cast<uint8_t*>(lVar1 + 0x29 + lVar10 * 0x30) & 0x1f) & 1) != 0) {
                                                return iVar7;  // break -> success
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // LAB_14009c134
                        iVar20 = iVar20 + 1;
                        uVar24 = uVar24 + 1;
                        iVar7 = local_e8;
                    } while (iVar20 <= (int)local_f8);
                    local_e8 = iVar7;
                    uVar15 = (uint64_t)local_f8;
                    uVar13 = (uint64_t)local_res8;
                }
                iVar14 = (int)uVar15;
                iVar12 = (int)uVar13;
                if (-1 < local_e8) {
                    return local_e8;
                }
                uVar9 = uVar9 + 1;
                uVar19 = (uint64_t)(uint32_t)(iVar18 + 1);
                iVar20 = local_e8;
                iVar7 = local_dc;
            } while ((int)(iVar18 + 1) <= iVar21);
        }
        if (-1 < iVar20) {
            return iVar20;
        }
        local_res8 = (uint32_t)(iVar12 - 1);
        uVar13 = (uint64_t)local_res8;
        local_f8 = (uint32_t)(iVar14 + 1);
        uVar15 = (uint64_t)local_f8;
        iVar23 = iVar23 + 1;
        if (6 < iVar23) {
            return iVar20;
        }
        if (iVar20 != -1) {
            return iVar20;
        }
    } while (true);
}

// ============================================================
// 0x140092260 — spatial entity proximity query via quadtree walk
// ============================================================
// Args: param_1 = map/region object (param_1[0] = quadtree root box float[]; +4 (=+0x10)
//   = entity index array base), param_2 = out (longlong* best-entity), param_3 = query
//   point (float* xyz), param_4 = Scalar* radius, param_5 = component type-id filter (0=
//   none), param_6 = InternalBasePtr* self-to-exclude. Returns param_4 (Box*) — the
//   decompile's return is the scratch; the real output is *param_2 (nearest entity).
//
// Builds a quadtree box-walker on a ~0x2438-byte local buffer via 18dd0, seeds the
// query AABB (point +/- radius), then DFS: pop top node (18880 pushes 4 children when a
// node's cell has population and its box intersects the query AABB). For each node with
// population, walks the entity linked-list at region+0x30/+0x10, applies the optional
// component-type filter (vtable +0x10 GetTypeId == param_5, else via InternalBasePtr at
// entity+0x10 -> +0x30 list), and keeps the entity whose squared distance (Vector3::Dot
// of (entityPos - point)) is smallest within radius^2. Faithful transcription; walker
// buffer size derived from 18dd0 (256 records * 0x24 from +0x38, backref at +0x2428 ->
// max touched +0x2438).
int64_t* Pf_SpatialProximityQuery_92260(float* param_1, int64_t* param_2, float* param_3,
                                         void* param_4, int param_5, void* param_6) {
    EnsureFns();
    const float fUV = *g_fUVScale;      // g_fUVScale (1.0)
    const float fNeg1 = C_neg1();       // DAT_140172280
    const float f2 = C_2();             // DAT_140172174

    // Walker buffer: 18dd0 writes records[0..255] (stride 0x24 from +0x38) and the
    // param backref at +0x2428 -> highest touched byte = +0x2438. Match that size.
    alignas(16) uint8_t walker[0x2438];
    s_qinit(walker, param_1, /*DAT_140172280*/ *reinterpret_cast<const uint32_t*>(&fNeg1),
            /*g_fUVScale*/ *reinterpret_cast<const uint32_t*>(&fUV));

    float fr = *reinterpret_cast<float*>(param_4);
    *param_2 = 0;
    float fr2 = fr * fr;                 // radius^2

    // Seed the query AABB: min = point - radius (splat), max = point + radius (splat).
    float vmin[3], vmax[3];
    s_v3ctorsplat(vmin, reinterpret_cast<const float*>(param_4));   // local_25d8 (splat radius)
    s_v3ctorsplat(vmax, reinterpret_cast<const float*>(param_4));   // local_25b8 (splat radius)
    float boxMax[3], boxMin[3];
    s_v3ctor3(boxMax, vmin[0] + param_3[0], vmin[1] + param_3[1], vmin[2] + param_3[2]); // local_25e8
    s_v3ctor3(boxMin, param_3[0] - vmax[0], param_3[1] - vmax[1], param_3[2] - vmax[2]); // local_25c8

    // queryBox layout: [0..2]=min (local_2560..), [3..5]=max (local_2554..) — a Volume::Box
    // (min xyz at +0x00, max xyz at +0xc).
    float queryBox[6];
    queryBox[0] = boxMin[0]; queryBox[1] = boxMin[1]; queryBox[2] = boxMin[2];
    queryBox[3] = boxMax[0]; queryBox[4] = boxMax[1]; queryBox[5] = boxMax[2];

    // Walker fields: top index at walker+0x08, region backref at walker+0x2428.
    int top = *reinterpret_cast<int*>(walker + 0x08);
    int64_t region = *reinterpret_cast<int64_t*>(walker + 0x2428);

    while (true) {
        // Skip nodes with no population; pop down to the next populated node.
        int recIdx;
        float cellF;
        while (true) {
            if (top < 0) {
                return reinterpret_cast<int64_t*>(param_4);
            }
            recIdx = top;
            // Record fields are addressed as walker + recIdx*0x24 + off (records are a
            // ping-pong stack: stride 0x24 but each spans +0x24..+0x3c, so they overlap).
            // Offsets disasm-verified at 0x140092400 (R14=recIdx*9, field = [RBP+R14*4+X],
            // walker base = RBP+0x10, so walker_off = recIdx*0x24 + (X-0x10)):
            //   cellIdx X=0x4c -> +0x3c ; e0 X=0x40 -> +0x30 ; e1 X=0x44 -> +0x34 ;
            //   e2 X=0x48 -> +0x38 ; c0 X=0x3c -> +0x2c ; c1 X=0x38 -> +0x28 ; c2 X=0x34 -> +0x24
            int64_t recBase = (int64_t)recIdx * 0x24;
            cellF = *reinterpret_cast<float*>(walker + recBase + 0x3c);   // afStack_24cc[rec*9]
            if (0 < *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(region + 0x50) +
                                            (int64_t)(int)cellF * 4)) break;
            top = top - 1;
        }
        *reinterpret_cast<int*>(walker + 0x08) = top;

        // Build the node's child-box and test intersection with the query AABB.
        int64_t rb = (int64_t)recIdx * 0x24;
        float scaleVec[3];
        s_v3ctor3(scaleVec, f2, f2, fUV);   // local_25d8 = (2.0, 2.0, g_fUVScale)
        float e0 = *reinterpret_cast<float*>(walker + rb + 0x30); // XMM11
        float e1 = *reinterpret_cast<float*>(walker + rb + 0x34); // XMM10
        float e2 = *reinterpret_cast<float*>(walker + rb + 0x38); // XMM9
        float ext[3];
        s_v3ctor3(ext, scaleVec[0] * e0, scaleVec[1] * e1, scaleVec[2] * e2); // local_258c
        float c0 = *reinterpret_cast<float*>(walker + rb + 0x2c); // XMM6
        float c1 = *reinterpret_cast<float*>(walker + rb + 0x28); // XMM7
        float c2 = *reinterpret_cast<float*>(walker + rb + 0x24); // XMM8
        float nmin[3];
        s_v3ctor3(nmin, c2 - ext[0], c1 - ext[1], c0 - ext[2]);   // local_2580
        // base point (record world-origin lo) at walker+0x0c/0x10/0x14 ([RBP+0x1c/0x20/0x24]).
        float baseLo0 = *reinterpret_cast<float*>(walker + 0x0c); // local_24fc
        float baseLo1 = *reinterpret_cast<float*>(walker + 0x10); // local_24f8
        float baseLo2 = *reinterpret_cast<float*>(walker + 0x14); // local_24f4
        float nodeMin[3];
        s_v3ctor3(nodeMin, baseLo0 + nmin[0], baseLo1 + nmin[1], baseLo2 + nmin[2]); // local_2548
        float ext2[3];
        s_v3ctor3(ext2, scaleVec[0] * e0, scaleVec[1] * e1, scaleVec[2] * e2); // local_25a4
        float nmax[3];
        s_v3ctor3(nmax, ext2[0] + c2, ext2[1] + c1, ext2[2] + c0);            // local_2598
        // base point hi at walker+0x18/0x1c/0x20 ([RBP+0x28/0x2c/0x30]).
        float baseHi0 = *reinterpret_cast<float*>(walker + 0x18); // local_24f0
        float baseHi1 = *reinterpret_cast<float*>(walker + 0x1c); // local_24ec
        float baseHi2 = *reinterpret_cast<float*>(walker + 0x20); // local_24e8
        float nodeMax[3];
        s_v3ctor3(nodeMax, baseHi0 + nmax[0], baseHi1 + nmax[1], baseHi2 + nmax[2]); // local_2570

        float nodeBox[6];
        nodeBox[0] = nodeMin[0]; nodeBox[1] = nodeMin[1]; nodeBox[2] = nodeMin[2];
        nodeBox[3] = nodeMax[0]; nodeBox[4] = nodeMax[1]; nodeBox[5] = nodeMax[2];

        if (!s_boxisect(queryBox, nodeBox)) {
            top = top - 1;
            continue;
        }

        // Node intersects: walk its entity list. iVar11 = head index at region+0x30[cellF].
        int ent = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(region + 0x30) + (int64_t)(int)cellF * 4);
        int headSentinel = ent;
        if (-1 < ent) {
            do {
                int64_t entObj = *reinterpret_cast<int64_t*>(
                    *reinterpret_cast<int64_t*>(param_1 + 4) + (int64_t)ent * 0x18);  // local_25e8
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_6);
                if (entObj != *reinterpret_cast<int64_t*>(param_6)) {
                    bool dropped = false;
                    int64_t entRef = entObj;
                    if (param_5 != 0) {
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entObj);
                        int64_t compList = *reinterpret_cast<int64_t*>(entObj + 0x10);  // local_25c8
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&compList);
                        int64_t lv = compList;
                        int64_t found = 0;
                        if (0 < *reinterpret_cast<int*>(compList + 0x38)) {
                            uint64_t uVar14 = 0;
                            int uVar17 = 0;
                            while (true) {
                                void** self = reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(lv + 0x30) + uVar14 * 8);
                                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                                int iVar12 = (*reinterpret_cast<int(**)(void*)>(*reinterpret_cast<int64_t*>(*self) + 0x10))(*self);
                                if (iVar12 == param_5) {
                                    found = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lv + 0x30) + uVar14 * 8);
                                    break;
                                }
                                uVar17 = uVar17 + 1;
                                uVar14 = uVar14 + 1;
                                if (!((int)uVar17 < *reinterpret_cast<int*>(lv + 0x38))) break;
                            }
                        }
                        if (found == 0) {
                            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&found, nullptr);
                        }
                        region = *reinterpret_cast<int64_t*>(walker + 0x2428); // lVar15 = local_e0 reload
                        if (found == 0) dropped = true;
                    }
                    if (!dropped) {
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
                        int64_t lVar13 = entRef;
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entRef + 0x68));
                        lVar13 = *reinterpret_cast<int64_t*>(lVar13 + 0x68);
                        float diff[3];
                        s_v3ctor3(diff,
                                  *reinterpret_cast<float*>(lVar13 + 0x158) - param_3[0],
                                  *reinterpret_cast<float*>(lVar13 + 0x15c) - param_3[1],
                                  *reinterpret_cast<float*>(lVar13 + 0x160) - param_3[2]);
                        float dsq;
                        s_v3dot(diff, &dsq, diff);
                        if (dsq < fr2) {
                            *param_2 = entRef;
                            fr2 = dsq;
                        }
                    }
                }
                // LAB_140092741: advance to next entity in the cell list.
                ent = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(region + 0x10) + 8 + (int64_t)ent * 0x18);
            } while (ent != headSentinel);
        }
        s_qsubdiv(walker);
        top = *reinterpret_cast<int*>(walker + 0x08);
        region = *reinterpret_cast<int64_t*>(walker + 0x2428);
    }
}

// ============================================================
// 0x140122340 — HerdComponent::StartMigration
// ============================================================
// Picks a migration target cell for the herd via 053ca0. self+0x4c = chosen cell index.
//
//  * If the picker FAILS (cell < 0): the herd disperses — for each member (loop while
//    self+0x40 > 0) it marks the member entity for removal (0c2ea0) and enqueues it on
//    the world removal queue at *(gameWorld+0xe8)+0xd0 via 003110.
//
//    LOOP-EXIT NOTE (verified by disassemble_function 0x140122340): the dispersal loop
//    counter R15 is set to 0 ONCE (0x14012237b XOR R15D,R15D) and is NEVER incremented
//    inside the body; the loop guard is `CMP [R14+0x40],R15D ; JG` (i.e. while herd
//    member-count > 0). Neither 0c2ea0 nor 003110 writes [R14+0x40] (0c2ea0 ORs flag
//    0x400 into the *animal* entity's +0x40, a different object; 003110 is a queue push).
//    So this is faithfully a `do { ... } while (0 < self->memberCount)` that relies on
//    the engine's deferred-removal machinery (the queue drained later) to shrink
//    self+0x40. The real game does not hang here, so this faithful transcription will
//    not either — the count is decremented out-of-band by removal processing, which is
//    outside this function. It is a genuine guarded loop, NOT dead code, and NOT an
//    unbounded spin in context. Transcribed exactly as the binary does it.
//
//  * If the picker SUCCEEDS (cell >= 0): computes a clamped target rectangle (from the
//    world stride at *(gameWorld+0xf8) and the herd-template footprint at self+0x18 ->
//    +0x38 when derived from DAT_1401c7858) and writes the 4 int16 target fields
//    (+0x38..+0x3e) into each member's movement component (found by type DAT_1401c6f98).
void Pf_HerdStartMigration_122340(void* param_1) {
    EnsureFns();
    uintptr_t self = reinterpret_cast<uintptr_t>(param_1);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x28));
    int iVar4 = s_pick(*reinterpret_cast<void**>(self + 0x28), *reinterpret_cast<int*>(self + 0x30));
    *reinterpret_cast<int*>(self + 0x4c) = iVar4;

    if (iVar4 < 0) {
        if (0 < *reinterpret_cast<int*>(self + 0x40)) {
            do {
                int64_t* selfMember = *reinterpret_cast<int64_t**>(self + 0x38);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(selfMember);
                int64_t local_res8 = *reinterpret_cast<int64_t*>(*selfMember + 0x10);
                int64_t lVar2 = *reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0xe8);
                if (local_res8 != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
                    if ((*reinterpret_cast<uint32_t*>(local_res8 + 0x40) >> 10 & 1) == 0) {
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
                        s_markrem(reinterpret_cast<void*>(local_res8));
                        s_dynset(reinterpret_cast<void*>(lVar2 + 0xd0),
                                 *reinterpret_cast<int*>(lVar2 + 0xd8) + 1, &local_res8);
                    }
                }
            } while (0 < *reinterpret_cast<int*>(self + 0x40));
        }
        return;
    }

    // Positive branch: compute clamped target rectangle + write member move targets.
    void* pvVar11 = *reinterpret_cast<void**>(self + 0x18);
    bool bVar3 = fn_IsDerivedFrom && fn_IsDerivedFrom(pvVar11, *s_pTid_c7858);
    void* pvVar6 = bVar3 ? pvVar11 : nullptr;
    int local_res10 = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pvVar6) + 0x38);
    int worldW = **reinterpret_cast<int**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int local_res18 = *reinterpret_cast<int*>(self + 0x4c) % worldW - (local_res10 >> 1);
    int local_res20 = *reinterpret_cast<int*>(self + 0x4c) / worldW - (local_res10 >> 1);
    if (local_res18 < 1) local_res18 = 1;
    if (local_res20 < 1) local_res20 = 1;
    int iVar10 = local_res10;
    if (worldW - 1 <= local_res18 + local_res10) {
        iVar10 = (worldW - local_res18) - 2;
    }
    int local_res8_lo = iVar10;                 // CONCAT44(.., iVar10) low dword
    if (worldW - 1 <= local_res20 + iVar10) {
        local_res10 = (worldW - local_res20) - 2;
    }

    if (0 < *reinterpret_cast<int*>(self + 0x40)) {
        int64_t pvVar6_off = 0;     // byte offset into member array (pvVar6 in decompile)
        uint32_t memberIdx = 0;     // pvVar11 counter
        do {
            int64_t lVar2 = *reinterpret_cast<int64_t*>(self + 0x38);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar2 + pvVar6_off));
            int64_t local_58 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar2 + pvVar6_off) + 0x10);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_58);
            int64_t lv = local_58;

            // Find the movement component (type DAT_1401c6f98) in the member's comp list.
            void* foundComp = nullptr;
            if (0 < *reinterpret_cast<int*>(local_58 + 0x38)) {
                int64_t slotOff = 0;
                int idx = 0;
                while (true) {
                    void** puVar1 = reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(lv + 0x30) + slotOff * 8);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(puVar1);
                    uint32_t wantId = *s_pTid_c6f98;
                    uint32_t gotId = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(*puVar1) + 0x10))(*puVar1);
                    if (gotId == wantId) {
                        // LAB_1401225ef: re-read the slot, IsDerivedFrom-confirm.
                        void** p2 = reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(lv + 0x30) + slotOff * 8);
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p2);
                        void* comp = *p2;
                        bool d = fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *s_pTid_c6f98);
                        foundComp = d ? comp : nullptr;
                        break;
                    }
                    idx = idx + 1;
                    slotOff = slotOff + 1;
                    if (!((int)idx < *reinterpret_cast<int*>(lv + 0x38))) { foundComp = nullptr; break; }
                }
            }
            // LAB_140122574: ctor an IBP from foundComp, write the 4 int16 target fields.
            int64_t local_50[2];
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(local_50, foundComp);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(local_50);
            *reinterpret_cast<int16_t*>(local_50[0] + 0x38) = (int16_t)local_res18;
            *reinterpret_cast<int16_t*>(local_50[0] + 0x3a) = (int16_t)local_res20;
            *reinterpret_cast<int16_t*>(local_50[0] + 0x3c) = (int16_t)local_res8_lo;
            *reinterpret_cast<int16_t*>(local_50[0] + 0x3e) = (int16_t)local_res10;

            memberIdx = memberIdx + 1;
            pvVar6_off = pvVar6_off + 8;
        } while ((int)memberIdx < *reinterpret_cast<int*>(self + 0x40));
    }
}
