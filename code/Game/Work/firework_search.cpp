// code/Game/Work/firework_search.cpp
// FireWork burning-entity spiral search (FireWork action / building-on-fire propagation).
//
//   FireWork_helper_81d0          @ 0x1400881d0  -> FireWork_SpiralSearch_81d0
//   FireWork_helper_81d0_sub_8730 @ 0x140088730  -> FireWork_SpiralCell_8730
//
// FireWork_SpiralSearch_81d0 scans the TownManager's coarse cell index in an
// expanding ring spiral around a centre cell (param_2). For every in-bounds cell it
// invokes the per-cell helper (8730), which walks that cell's entity list, filters by
// flag/type, validates reachability (a00f0 + f810 path queries) and tracks the closest
// burnable target (best path cost in *param_7, its list id in *param_8, an IBP to it in
// *param_9). When the ring radius exceeds the search limit (param_3) the scan stops.
//
// After the scan:
//   - if nothing was found (local_88 == 0): sample two terrain positions for the best
//     fallback cell (local_b8) via the map terrain sampler and write them to param_4/param_5.
//   - if a target IBP was found: locate an InteractComponent (type DAT_1401c7900) sub-entity
//     of the found entity, random-index it (Random::Rand over the recursive child count),
//     resolve its world position (Entity_impl_2e20) and anchor cell (Entity_impl_7fb0), then
//     write both terrain positions.
//
// ⚠ param_1 IS NOT THE GameWorld. Slot 4 passes `*(GameWorld+0xF0)` -- the
// TownManager -- and that is faithful: retail 0x1400e977c does `mov rsi,rcx` with rsi
// loaded from `0xf0(g_GameWorld)`. The three fields this pair walks are TownManager
// fields, and the GameWorld's same-numbered fields are unrelated (GW+0xE0 = camera
// holder, GW+0x130 = weather sim, GW+0x140 = save/load), so reading them off the wrong
// object yields garbage rather than a clean null:
//
//   TownManager+0xE0/+0xE8   water-source entity refs (registered by 0x140091cb0)
//   TownManager+0x130/+0x138 water cell-id pairs
//   TownManager+0x140/+0x148 COARSE CELL INDEX over both: (W>>3)^2 buckets of stride
//                            0x10 = {int* ids, int count}. Sized by the GameWorld
//                            world-init at 0x1400975c7 -- see gameupdateloop_load.cpp
//                            Step 31. It is NOT sized by the TownManager ctor
//                            (0x140090a30 only zeroes it), so a port that skips that
//                            call site leaves the base null and the first ring probe
//                            faults at coarseCell*0x10.
//
// Faithful, offset-by-offset port of the decompiled exe helpers; do not "improve" behavior.
// Every numeric constant, the spiral index arithmetic and the per-cell call argument order
// were confirmed against disassemble_function.
//
// All exe sub-callees are already ported + registered elsewhere (closure-safe):
//   0x140122e20 Entity_impl_2e20    (reg_pf_road.cpp  Pf_EntityResolvePos_2e20)
//   0x140123070 FUN_140123070       (reg_pf_ctors.cpp Pf_RecursiveChildSum_123070)
//   0x140087fb0 Entity_impl_7fb0    (reg_pf_road.cpp  Pf_EntityAnchorCell_7fb0)
//   0x14009f3e0 CharacterComponent_impl_f3e0 == fn_MapGetTerrainPos
//   0x1400a00f0 FUN_1400a00f0       (reg_work_base.cpp WorkBase_ValidateCell_a00f0)
//   0x14009f810 BuildStructure_impl_f810 (reg_pf_roots_route.cpp Pf_IsDstReachable_f810)
//   0x140132010 FUN_140132010       (reg_work_impl2.cpp WorkImpl_Sqrtf_132010)
//   0x140002ed0 Entity_FindComponent_Position (reg_work_base.cpp WorkBase_FindComponent_2ed0)
//   0x1401512e8 System::Random::Rand (dispatch slot, registered in address_registry.cpp)
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ---------------------------------------------------------------------------
// Lazily-resolved sub-helpers (resolve once; in standalone -> our registered ports). Mirrors action_pf_road.cpp's style.
// ---------------------------------------------------------------------------

typedef void* (*fw_EntityResolvePos_t)(void* self, float* retVec3, int p3, int p4, float* p5);  // 0x140122e20
typedef int   (*fw_RecursiveChildSum_t)(int64_t self, int idx);                                  // 0x140123070
typedef int   (*fw_EntityAnchorCell_t)(void* selfIBP, float* p2);                                // 0x140087fb0
typedef void* (*fw_FindCompPos_t)(uintptr_t entity, void* retIBP);                               // 0x140002ed0
typedef uint64_t (*fw_ValidateCell_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell); // 0x1400a00f0
typedef int   (*fw_IsDstReachable_t)(int* map, uint32_t srcCell, uint32_t dstCell, char useCache);      // 0x14009f810
typedef float (*fw_Sqrtf_t)(float v);                                                            // 0x140132010
typedef uint32_t (*fw_RandomRand_t)(void* rng, uint32_t range);                                  // [0x1401512e8]

static bool                   s_init = false;
static fw_EntityResolvePos_t  s_resolvePos   = nullptr;
static fw_RecursiveChildSum_t s_childSum     = nullptr;
static fw_EntityAnchorCell_t  s_anchorCell   = nullptr;
static fw_FindCompPos_t       s_findCompPos  = nullptr;
static fw_ValidateCell_t      s_validateCell = nullptr;
static fw_IsDstReachable_t    s_isReachable  = nullptr;
static fw_Sqrtf_t             s_sqrtf        = nullptr;
static fw_RandomRand_t        s_rand         = nullptr;
static const float*           s_pHalfCell    = nullptr;  // DAT_140172114 = 0.5f
static const uint32_t*        s_pTid_c7900   = nullptr;  // DAT_1401c7900 (InteractComponent)

static void EnsureInit() {
    if (s_init) return;
    s_resolvePos   = reinterpret_cast<fw_EntityResolvePos_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_childSum     = reinterpret_cast<fw_RecursiveChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_anchorCell   = reinterpret_cast<fw_EntityAnchorCell_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_findCompPos  = reinterpret_cast<fw_FindCompPos_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_validateCell = reinterpret_cast<fw_ValidateCell_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_isReachable  = reinterpret_cast<fw_IsDstReachable_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_sqrtf        = reinterpret_cast<fw_Sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    uintptr_t slot = ResolveAddr(Module::Exe, 0x1401512e8);
    if (slot) s_rand = *reinterpret_cast<fw_RandomRand_t*>(slot);
    s_pHalfCell    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pTid_c7900   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7900));
    s_init = true;
}

static inline float HalfCell() { return s_pHalfCell ? *s_pHalfCell : 0.5f; }

// ===========================================================================
// FireWork_SpiralCell_8730  (FUN_140088730)
//   Per-cell check: walk the entity-id list of coarse cell `param_3`, and for each
//   burnable + reachable entity update the closest-target trackers (param_7/8/9).
// param_1 = TownManager (= *(GameWorld+0xF0)), param_2 = WorldGrid (int*), param_3 = coarse cell index,
// param_4/param_5 = ring offsets (used for the radius-vs-limit early reject),
// param_6 = source cell, param_7 = &bestCost, param_8 = &bestId, param_9 = &bestIBP.
// ===========================================================================
void FireWork_SpiralCell_8730(int64_t param_1, int* param_2, int param_3,
                              int param_4, int param_5, uint32_t param_6,
                              int* param_7, int* param_8, uint64_t* param_9) {
    EnsureInit();

    // plVar8 = *(param_1 + 0x140) + param_3 * 0x10 ; entry: {ptr ids, int count, ...}
    int64_t* plVar8 = reinterpret_cast<int64_t*>(
        *reinterpret_cast<int64_t*>(param_1 + 0x140) + static_cast<int64_t>(param_3) * 0x10);
    if (*reinterpret_cast<int*>(reinterpret_cast<int64_t>(plVar8) + 8) <= 0)
        return;

    int  iVar10 = 0;
    int64_t lVar9 = 0;
    do {
        // Ring radius (in fine units, *8) vs current best/limit squared distance.
        if (param_5 * 8 * param_5 * 8 + param_4 * 8 * param_4 * 8 <= (*param_7) * (*param_7)) {
            int iVar1 = *reinterpret_cast<int*>(lVar9 + *plVar8);   // entity list id
            uint32_t uVar7;
            if (iVar1 < 0) {
                // Negative id -> indirect entity ref at GameWorld+0xe0[-1-iVar1].
                int64_t* refSlot = reinterpret_cast<int64_t*>(
                    *reinterpret_cast<int64_t*>(param_1 + 0xe0) +
                    static_cast<int64_t>(-1 - iVar1) * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(refSlot);

                int64_t entIBP = *reinterpret_cast<int64_t*>(*refSlot + 0x10);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entIBP);

                int64_t posIBP = 0;
                if (s_findCompPos) s_findCompPos(static_cast<uintptr_t>(entIBP), &posIBP);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);

                float pos[3] = {0.0f, 0.0f, 0.0f};
                if (s_resolvePos) s_resolvePos(reinterpret_cast<void*>(posIBP), pos, 0, 0, nullptr);

                int iVar6 = *param_2;                  // grid width W
                int iVar2 = static_cast<int>(pos[0] + static_cast<float>(iVar6 >> 1));
                int iVar3 = static_cast<int>(pos[1] + static_cast<float>(iVar6 >> 1));
                if (iVar2 < 0)       iVar2 = 0;
                if (iVar6 <= iVar2)  iVar2 = iVar6 - 1;
                if (iVar3 < 0)       iVar3 = 0;
                if (iVar6 <= iVar3)  iVar3 = iVar6 - 1;
                uVar7 = static_cast<uint32_t>(iVar6 * iVar3 + iVar2);
            } else {
                uVar7 = *reinterpret_cast<uint32_t*>(
                    *reinterpret_cast<int64_t*>(param_1 + 0x130) + static_cast<int64_t>(iVar1) * 8);
                // Cell-type gate: bit (cellType & 0x1f) of mask 0xe07 must be set.
                // param_2 is int*; Ghidra's "param_2 + 0x20" is byte offset 0x80
                // (disasm: MOV RAX,[R14 + 0x80]). cellType = (*(param_2+0x80))[uVar7*0x30 + 0x29].
                uint8_t cellType = *reinterpret_cast<uint8_t*>(
                    *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(param_2) + 0x80) +
                    0x29 + static_cast<int64_t>(static_cast<int>(uVar7)) * 0x30);
                if (((1u << (cellType & 0x1f)) & 0xe07u) == 0)
                    goto next;
            }

            // Reachability: path-type 0xe07 query then explicit length query.
            if (s_validateCell) {
                uint64_t ok = s_validateCell(param_2, 0xe07, param_6, static_cast<int>(uVar7));
                if (static_cast<char>(ok) != '\0') {
                    int iVar2;
                    if (s_isReachable)
                        iVar2 = s_isReachable(param_2, param_6, uVar7, '\x01');
                    else
                        iVar2 = 0;
                    if (iVar2 < 0) {
                        // Unreachable by road -> fall back to euclidean (in cells) + bias.
                        int W2  = *param_2;
                        int dr  = static_cast<int>(param_6) / W2 - static_cast<int>(uVar7) / W2;
                        int dc  = static_cast<int>(param_6) % W2 - static_cast<int>(uVar7) % W2;
                        float d = s_sqrtf ? s_sqrtf(static_cast<float>(dr * dr + dc * dc)) : 0.0f;
                        iVar2 = static_cast<int>(d) + W2;
                    }
                    if (iVar2 < *param_7) {
                        *param_7 = iVar2;
                        *param_8 = iVar1;
                        uint64_t bestRef;
                        if (iVar1 < 0) {
                            // The wrapped object one slot below the ref entry.
                            bestRef = *reinterpret_cast<uint64_t*>(
                                *reinterpret_cast<int64_t*>(param_1 + 0xe0) -
                                (static_cast<int64_t>(iVar1) * 8 + 8));
                        } else {
                            uint64_t emptyIBP = 0;
                            if (fn_InternalBasePtr_Ctor) new (&emptyIBP) ::Object::InternalBasePtr(
                                nullptr);   // B.3.3 bridge
                            bestRef = emptyIBP;
                        }
                        *param_9 = bestRef;
                    }
                }
            }
        }
    next:
        iVar10 = iVar10 + 1;
        lVar9 = lVar9 + 4;
        plVar8 = reinterpret_cast<int64_t*>(
            *reinterpret_cast<int64_t*>(param_1 + 0x140) + static_cast<int64_t>(param_3) * 0x10);
    } while (iVar10 < *reinterpret_cast<int*>(reinterpret_cast<int64_t>(plVar8) + 8));
}

// ===========================================================================
// FireWork_SpiralSearch_81d0  (FUN_1400881d0)
//   Expanding-ring spiral over the coarse cell grid around centre cell param_2,
//   bounded by half-search-extent param_3. Returns true (low byte) when it wrote
//   the two terrain positions to param_4/param_5.
// param_1 = TownManager (= *(GameWorld+0xF0)), param_2 = centre cell, param_3 = search extent (fine units),
// param_4 / param_5 = out Vector3 (two terrain positions).
// ===========================================================================
int64_t FireWork_SpiralSearch_81d0(int64_t param_1, uint32_t param_2, int param_3,
                                   float* param_4, float* param_5) {
    EnsureInit();

    // WorldGrid pointer at GameWorld(+0xf8). piVar1[0] = grid width W.
    int* piVar1 = *reinterpret_cast<int**>(
        reinterpret_cast<int64_t>(*g_pGameWorldSingleton) + 0xf8);

    uint64_t local_88 = 0;          // found-target IBP (0 = none)
    int W      = *piVar1;
    int cellW  = W >> 3;            // coarse grid dimension
    int local_b8 = -1;             // best fallback coarse cell id
    int iVar14 = static_cast<int>(param_2) % W >> 3;   // coarse col
    int local_b4 = static_cast<int>(param_2) / W >> 3; // coarse row

    int neg_cellW = -cellW;
    int local_a8  = cellW * local_b4;
    int local_54  = 1 - iVar14;
    int local_58  = iVar14 + 1;
    int local_80  = -iVar14;
    int local_68row = local_b4 + 1;

    int iVar6 = -1;                 // ring radius (grows each outer iteration)
    int local_b0 = local_a8;
    int local_70 = iVar14;          // iVar14 captured at top of each ring (decompile local_70)

    while (true) {
        int iVar3 = (iVar6 < 0) ? 0 : iVar6;
        local_70 = iVar14;          // capture entry iVar14 (restored at 2nd-for body end)
        if (param_3 <= iVar3 * 8) break;   // search-extent exit

        iVar3 = iVar6 + 1;
        int iVar8 = local_80 + iVar14;
        int local_ac = iVar8;
        int iVar10 = local_a8;
        int iVar4  = local_b0;
        // --- vertical edges of the ring (columns iVar14 and local_58+iVar6) ---
        for (; iVar8 <= iVar3; iVar8 = iVar8 + 1) {
            local_b0 = iVar4;
            if ((-1 < iVar8 + local_b4) && (iVar8 + local_b4 < cellW)) {
                if ((-1 < iVar14) && (iVar14 < cellW)) {
                    FireWork_SpiralCell_8730(param_1, piVar1, iVar14 + iVar10, local_ac, iVar8,
                                             param_2, &param_3, &local_b8, &local_88);
                }
                iVar4 = local_58 + iVar6;
                if ((-1 < iVar4) && (iVar4 < cellW)) {
                    FireWork_SpiralCell_8730(param_1, piVar1, iVar4 + iVar10, iVar3, iVar8,
                                             param_2, &param_3, &local_b8, &local_88);
                }
            }
            iVar10 = iVar10 + cellW;
            iVar4 = local_b0;
        }
        // --- horizontal edges of the ring (rows local_ac and local_68row) ---
        iVar10 = iVar14;
        for (iVar8 = local_54 + iVar14; iVar8 <= iVar6; iVar8 = iVar8 + 1) {
            iVar14 = iVar14 + 1;
            if ((-1 < iVar14) && (iVar14 < cellW)) {
                int row = local_ac + local_b4;
                if ((-1 < row) && (row < cellW)) {
                    FireWork_SpiralCell_8730(param_1, piVar1, row * cellW + iVar14, iVar8, local_ac,
                                             param_2, &param_3, &local_b8, &local_88);
                }
                if ((-1 < local_68row + iVar6) && (local_68row + iVar6 < cellW)) {
                    FireWork_SpiralCell_8730(param_1, piVar1, iVar4 + iVar14, iVar8, iVar3,
                                             param_2, &param_3, &local_b8, &local_88);
                }
            }
            iVar10 = local_70;   // decompile restores iVar10 = local_70 (entry iVar14), NOT the incremented iVar14
        }
        local_a8 = local_a8 + neg_cellW;
        local_b0 = local_b0 + cellW;
        iVar14 = iVar10 - 1;
        iVar6 = iVar3;
    }

    if (local_88 == 0) {
        // No target found: emit terrain positions for the best fallback cell, if any.
        if (local_b8 < 0)
            return 0;   // low byte 0 (CONCAT garbage upper bytes; callers test the bool)

        int64_t lVar7 = static_cast<int64_t>(local_b8);
        float half = static_cast<float>(*piVar1 >> 1) - HalfCell();

        // First terrain position: cell id at TownManager+0x130[best].
        int cellA = *reinterpret_cast<int*>(
            *reinterpret_cast<int64_t*>(param_1 + 0x130) + lVar7 * 8);
        float inXY_a[3];
        inXY_a[0] = static_cast<float>(cellA % *piVar1) - half;
        inXY_a[1] = static_cast<float>(cellA / *piVar1) - half;
        inXY_a[2] = 0.0f;
        float outA[3];
        if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(piVar1, outA, inXY_a, '\0', nullptr);
        param_4[0] = outA[0];
        param_4[1] = outA[1];
        param_4[2] = outA[2];

        // Second terrain position: cell id at TownManager+0x130[best] +4.
        int cellB = *reinterpret_cast<int*>(
            *reinterpret_cast<int64_t*>(param_1 + 0x130) + 4 + lVar7 * 8);
        float half2 = static_cast<float>(*piVar1 >> 1) - HalfCell();
        float inXY_b[3];
        inXY_b[0] = static_cast<float>(cellB % *piVar1) - half2;
        inXY_b[1] = static_cast<float>(cellB / *piVar1) - half2;
        inXY_b[2] = 0.0f;
        float outB[3];
        if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(piVar1, outB, inXY_b, '\0', nullptr);
        param_5[0] = outB[0];
        param_5[1] = outB[1];
        param_5[2] = outB[2];
        return 1;
    }

    // A target was found: dig into its InteractComponent sub-entity tree.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_88);
    int64_t entIBP = *reinterpret_cast<int64_t*>(local_88 + 0x10);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entIBP);

    // Scan the entity's children for the first one whose type == DAT_1401c7900,
    // then verify it via IsDerivedFrom; chosen entity -> `chosen`.
    void* chosen = nullptr;
    uint32_t typeId = s_pTid_c7900 ? *s_pTid_c7900 : 0;
    int  childCount = *reinterpret_cast<int*>(entIBP + 0x38);
    if (childCount > 0) {
        int   idx = 0;
        int64_t off = 0;
        do {
            uint64_t* childSlot = reinterpret_cast<uint64_t*>(
                *reinterpret_cast<int64_t*>(entIBP + 0x30) + off);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(childSlot);
            void* child = reinterpret_cast<void*>(*childSlot);
            // GetTypeId via vtable +0x10.
            uint32_t (*getTypeId)(void*) =
                *reinterpret_cast<uint32_t (**)(void*)>(*reinterpret_cast<int64_t*>(child) + 0x10);
            uint32_t t = getTypeId(child);
            if (t == typeId) {
                uint64_t* sel = reinterpret_cast<uint64_t*>(
                    *reinterpret_cast<int64_t*>(entIBP + 0x30) + static_cast<int64_t>(idx) * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(sel);
                void* candidate = reinterpret_cast<void*>(*sel);
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(candidate, typeId))
                    chosen = candidate;
                break;
            }
            idx = idx + 1;
            off = off + 8;
        } while (idx < *reinterpret_cast<int*>(entIBP + 0x38));
    }

    // Wrap the chosen entity in an IBP, random-index a sub-entity, resolve its position.
    uint64_t chosenIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&chosenIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(chosen));   // B.3.3 bridge
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&chosenIBP);

    uint32_t count = s_childSum ? static_cast<uint32_t>(s_childSum(static_cast<int64_t>(chosenIBP), 0)) : 0;
    void* rng = *reinterpret_cast<void**>(
        reinterpret_cast<int64_t>(*g_pGameWorldSingleton) + 0xd8);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&chosenIBP);
    uint32_t randIdx = s_rand ? s_rand(rng, count) : 0;

    float resolved[3] = {0.0f, 0.0f, 0.0f};
    if (s_resolvePos)
        s_resolvePos(reinterpret_cast<void*>(chosenIBP), resolved, 0,
                     static_cast<int>(randIdx), nullptr);

    // Anchor cell of the found entity (entIBP), then its terrain position into param_5.
    uint64_t anchorIBP = entIBP;
    int anchorCell = s_anchorCell ? s_anchorCell(&anchorIBP, nullptr) : 0;
    float half = static_cast<float>(*piVar1 >> 1) - HalfCell();
    float inXY[3];
    inXY[0] = static_cast<float>(anchorCell % *piVar1) - half;
    inXY[1] = static_cast<float>(anchorCell / *piVar1) - half;
    inXY[2] = 0.0f;
    float outP[3];
    if (fn_MapGetTerrainPos) fn_MapGetTerrainPos(piVar1, outP, inXY, '\0', nullptr);
    param_5[0] = outP[0];
    param_5[1] = outP[1];
    param_5[2] = outP[2];

    // The random-resolved sub-entity position goes to param_4.
    param_4[0] = resolved[0];
    param_4[1] = resolved[1];
    param_4[2] = resolved[2];
    return 1;
}
