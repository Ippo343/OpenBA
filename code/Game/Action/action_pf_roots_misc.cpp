// code/Game/Action/action_pf_roots_misc.cpp
// PF porter wave 3 — three Cluster-C "root" sub-helpers (large, all sub-callees
// already registered). Faithful 1:1 ports from the Ghidra decompile, cross-checked
// against disassemble_function for the intricate index math.
//
// None are exercised on the autostart path, so a wrong port is undetectable silent
// corruption — strictly worse than leaving the addr unregistered. Transcribed
// literally; no "improvements". Registration in standalone/reg_pf_roots_misc.cpp.
#include "Action/exe_bodies.h"
//
//   0x14009bd40  FindPlacementCell — expanding Chebyshev-ring spiral cell search
//                (calls f3e0 grid->world + Math::Vector3 ctor + Math::Scalar::Abs)
//   0x140092260  spatial entity proximity query via quadtree walk (uses an ~0x2438
//                local walker built by 18dd0 / advanced by 18880; Box::Intersect +
//                Vector3::Dot + InternalBasePtr)
//   0x140122340  HerdComponent::StartMigration (calls 053ca0 target picker, 003110
//                queue-push, 0c2ea0 mark-removal; see loop-exit note below)

#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "Game/exe_bodies.h"   // Reimpl_MapGetTerrainPos
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/HerdDescription.h"
#include "Component/AnimalComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/CharacterGrid.h"
#include "Component/HerdComponent.h"
#include "Component/ModelComponent.h"
#include "Component/WildAnimalComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/EntityList.h"
#include "Game/Map.h"
#include "Action/PathSearch.h"   // CharacterGridWalker

namespace {

// ---- exe sub-callees, called by name (B3R) ----
//   9f3e0 Reimpl_MapGetTerrainPos (f3e0)     18dd0 Pf_QuadtreeInit_18dd0
//   18880 Pf_QuadtreeSubdivide_18880          053ca0 Pf_PastureTargetPick_53ca0
//   003110 ActHunt_DynArraySetCount_3110      0c2ea0 ActHunt_MarkEntityRemoval_c2ea0

// ---- Math / Volume engine exports (Runtime-x64.dll) -------------------------
// Math::Vector3 ctor(x,y,z) / splat ctor / Dot and Math::Scalar::Abs are now direct
// members of the real classes (B.3.1). Box::Intersect (slot 0x140151e80,
// ?Intersect@Box@Volume@@QEBA_NAEBV12@@Z) was also resolved here but never called: the
// hot path uses InlBoxIsect below; the dead resolve is gone (B3R).

// PERF (2026-09-04): bit-identical inline copy of the trivial Runtime Volume export
// the hot path called through a dispatch slot (a DLL call per box test). Body
// transcribed from the Runtime decompile. (The Math::Vector3 / Math::Vector2 / Scalar
// inline copies that sat beside it — InlV3, InlV3Splat, InlV3Dot, InlV2Dot,
// InlScalarMax — were replaced by the real class members in B.3.1; Vector3::Dot keeps
// the same (y, x, z) operand ORDER, so the float result is the same to the bit.)
//   ?Intersect@Box@Volume@@ (0x180012160)               -> InlBoxIsect(a,b)
static inline bool InlBoxIsect(const float* p, const float* q) {
    return p[0] <= q[3] && q[0] <= p[3] && p[1] <= q[4] && q[1] <= p[4] && p[2] <= q[5] && q[2] <= p[5];
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
// 0x14009bd40 — Map::FindPlacementCell: expanding Chebyshev-ring spiral cell search
// ============================================================
// `param_2` = start cell index. `param_3` = char "use-mask" flag. `allow` = an entity the
// cell's blocker may be. `param_5` = uint walkability bitmask.
//
// Spirals outward in Chebyshev rings (iVar23 = 0..6). For each ring, only cells whose
// Chebyshev distance component equals the ring radius are tested (abs(dRow)==r ||
// abs(dCol)==r). A candidate cell must: be in-bounds; have no occupant; either no blocker
// OR (allow set AND the blocker is it); have a terrain type with neither bit 1 nor 2 set
// AND field_0x2A == 0xff; and NOT be present in the skip tree at field_0xE8 (key = cell
// index). If param_3==0: additionally validate the cell's terrain position against the
// terrain (the two f3e0 samples must agree within 1000.0 and the growth >= 0.97). If
// param_3!=0: require the walkability bit (param_5 >> (terrainType&0x1f) & 1). On success
// returns the matching cell index; else -1 after 7 rings.
//
// Literal transcription of the decompile, node/flag offsets disasm-verified.
int Map::FindPlacementCell(int param_2, char param_3, ComponentEntity* const* allow, uint32_t param_5) {
    const float fHalf = C_half();    // DAT_140172114
    const float fMinGrowth = C_097(); // DAT_14017213c
    const float fTol = C_1000();     // DAT_140172248

    int iVar20 = -1;
    int iVar23 = 0;
    int local_e8 = -1;
    uint64_t uVar13 = (uint64_t)((int64_t)param_2 / (int64_t)size);
    uint64_t uVar19 = (uint64_t)((int64_t)param_2 % (int64_t)size);
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
                            iVar7 = size;
                            inBounds = (iVar20 < iVar7) && (iVar18 < iVar7);
                        }
                        if (inBounds) {
                            iVar7 = iVar7 * iVar18 + iVar20;
                            int64_t lVar10 = (int64_t)iVar7;
                            if (cells[lVar10].occupant == nullptr) {
                                ComponentEntity* local_d8 = cells[lVar10].blocker;
                                bool occOk = (local_d8 == nullptr);
                                if (!occOk && *allow != nullptr) {
                                    occOk = (*allow == local_d8);
                                }
                                if (occOk) {
                                    const MapCell& cell = cells[lVar10];
                                    uint32_t uVar8 = 1u << (cell.terrainType & 0x1f);
                                    if (((uVar8 & 2) == 0) &&
                                        (((uVar8 & 1) == 0) && (cell.field_0x2A == 0xff))) {
                                        // the skip tree at field_0xE8 (key = cell index)
                                        const AvlNode28* piVar2 = field_0xE8;
                                        bool skip = false;
                                        while (piVar2 != nullptr) {
                                            if (iVar7 < piVar2->key) {
                                                piVar2 = piVar2->left;
                                            } else {
                                                if (iVar7 <= piVar2->key) { skip = true; break; }   // found in skip tree
                                                piVar2 = piVar2->right;
                                            }
                                        }
                                        if (!skip) {
                                            if (param_3 == 0) {
                                                iVar14 = size;
                                                iVar12 = iVar14 * iVar18 + iVar20;
                                                float fVar25 = (float)(iVar14 >> 1) - fHalf;
                                                Math::Vector3 v98((float)(iVar12 % iVar14) - fVar25,
                                                                  (float)(iVar12 / iVar14) - fVar25, 0.0f);
                                                float v88[3];
                                                Reimpl_MapGetTerrainPos(this, v88, &v98.x, '\0', nullptr);   // 0x14009f3e0
                                                iVar14 = size;
                                                if ((iVar20 < iVar14) && (iVar18 < iVar14)) {
                                                    iVar12 = iVar14 * iVar18 + iVar20;
                                                    const MapCell& at = cells[iVar12];
                                                    if (((1u << (at.terrainType & 0x1f) & 0xfffffffbU) == 0) &&
                                                        ((at.blocker == nullptr) &&
                                                         (at.footprintCount == 0))) {
                                                        // ext = (local_d0[2], local_c8) contiguous height-sample buffer.
                                                        float ext[3]; ext[2] = 0.0f;  // local_c8 = 0.0
                                                        fVar25 = (float)(iVar14 >> 1) - fHalf;
                                                        Math::Vector3 vB8((float)(iVar12 % iVar14) - fVar25,
                                                                          (float)(iVar12 / iVar14) - fVar25, 0.0f);
                                                        float vA8[3];
                                                        Reimpl_MapGetTerrainPos(this, vA8, &vB8.x, '\0', ext);   // 0x14009f3e0
                                                        if (fMinGrowth <= ext[2]) {   // local_c8
                                                            Math::Scalar local_e0(vA8[2] - v88[2]); // local_a0 - local_80
                                                            Math::Scalar absv = local_e0.Abs();
                                                            if (absv.v <= fTol) {
                                                                local_e8 = iVar7;
                                                                return local_e8;  // break -> success path
                                                            }
                                                        }
                                                    }
                                                }
                                            } else if ((param_5 >> (cell.terrainType & 0x1f) & 1) != 0) {
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
// 0x140092260 — the nearest character to a point, by a CharacterGrid walk
// ============================================================
// `best` = out nearest character, `param_3` = query point (xyz), `radius`, `param_5` =
// component type-id filter (0 = none), `exclude` = the character to skip.
//
// Seeds a CharacterGridWalker (0x140018dd0, pads -1 / +1 on z), the query AABB (point +/-
// radius), then DFS: skip nodes whose cell has no occupancy; test the node's box against
// the query AABB; for an intersecting node walk the cell's character list (cellHead, then
// each slot's +0x08 link), apply the optional component-type filter (exact Type() match on
// the character's entity), and keep the character whose squared distance (Vector3::Dot of
// (position - point)) is smallest within radius^2; then 0x140018880 pushes the children.
// Faithful transcription.
void QueryNearestCharacter(const CharacterGrid* grid, CharacterComponent** best, const float* param_3,
                           const Math::Scalar* radius, int param_5, CharacterComponent* const* exclude) {
    const float fUV = kRdata_14017214c;      // g_fUVScale (1.0)
    const float fNeg1 = C_neg1();       // DAT_140172280
    const float f2 = C_2();             // DAT_140172174

    CharacterGridWalker walker;
    walker.Init(grid, /*DAT_140172280*/ fNeg1, /*g_fUVScale*/ fUV);   // 0x140018dd0

    float fr = radius->v;
    *best = nullptr;
    float fr2 = fr * fr;                 // radius^2

    // Seed the query AABB: min = point - radius (splat), max = point + radius (splat).
    Math::Vector3 vmin(*radius);   // local_25d8 (splat radius)
    Math::Vector3 vmax(*radius);   // local_25b8 (splat radius)
    Math::Vector3 boxMax(vmin.x + param_3[0], vmin.y + param_3[1], vmin.z + param_3[2]); // local_25e8
    Math::Vector3 boxMin(param_3[0] - vmax.x, param_3[1] - vmax.y, param_3[2] - vmax.z); // local_25c8

    // queryBox layout: [0..2]=min (local_2560..), [3..5]=max (local_2554..) — a Volume::Box
    // (min xyz at +0x00, max xyz at +0xc).
    float queryBox[6];
    queryBox[0] = boxMin.x; queryBox[1] = boxMin.y; queryBox[2] = boxMin.z;
    queryBox[3] = boxMax.x; queryBox[4] = boxMax.y; queryBox[5] = boxMax.z;

    int top = walker.top;
    const CharacterGrid* region = walker.grid;

    while (true) {
        // Skip nodes with no population; pop down to the next populated node.
        int recIdx;
        int cellIdx;
        while (true) {
            if (top < 0) {
                return;
            }
            recIdx = top;
            // cellIdx is an INT32 (0x14009240b `MOVSXD RSI, dword ptr [RBP + R14*0x4 + 0x4c]`,
            // written by 18880 as parentCell*4 + 1..4).
            cellIdx = walker.nodes[recIdx].cell;
            if (0 < region->cellOccupancy[cellIdx]) break;
            top = top - 1;
        }
        walker.top = top;

        // Build the node's child-box and test intersection with the query AABB.
        const CharacterGridWalkerNode& node = walker.nodes[recIdx];
        Math::Vector3 scaleVec(f2, f2, fUV);   // local_25d8 = (2.0, 2.0, g_fUVScale)
        float e0 = node.extent[0]; // XMM11
        float e1 = node.extent[1]; // XMM10
        float e2 = node.extent[2]; // XMM9
        Math::Vector3 ext(scaleVec.x * e0, scaleVec.y * e1, scaleVec.z * e2); // local_258c
        float c0 = node.center[2]; // XMM6
        float c1 = node.center[1]; // XMM7
        float c2 = node.center[0]; // XMM8
        Math::Vector3 nmin(c2 - ext.x, c1 - ext.y, c0 - ext.z);   // local_2580
        float baseLo0 = walker.padLo[0]; // local_24fc
        float baseLo1 = walker.padLo[1]; // local_24f8
        float baseLo2 = walker.padLo[2]; // local_24f4
        Math::Vector3 nodeMin(baseLo0 + nmin.x, baseLo1 + nmin.y, baseLo2 + nmin.z); // local_2548
        Math::Vector3 ext2(scaleVec.x * e0, scaleVec.y * e1, scaleVec.z * e2); // local_25a4
        Math::Vector3 nmax(ext2.x + c2, ext2.y + c1, ext2.z + c0);            // local_2598
        float baseHi0 = walker.padHi[0]; // local_24f0
        float baseHi1 = walker.padHi[1]; // local_24ec
        float baseHi2 = walker.padHi[2]; // local_24e8
        Math::Vector3 nodeMax(baseHi0 + nmax.x, baseHi1 + nmax.y, baseHi2 + nmax.z); // local_2570

        float nodeBox[6];
        nodeBox[0] = nodeMin.x; nodeBox[1] = nodeMin.y; nodeBox[2] = nodeMin.z;
        nodeBox[3] = nodeMax.x; nodeBox[4] = nodeMax.y; nodeBox[5] = nodeMax.z;

        if (!InlBoxIsect(queryBox, nodeBox)) {
            top = top - 1;
            continue;
        }

        // Node intersects: walk its character list from the cell's head.
        int ent = region->cellHead[cellIdx];
        int headSentinel = ent;
        if (-1 < ent) {
            do {
                CharacterComponent* entObj = region->slots[ent].character.Get();   // local_25e8
                if (entObj != *exclude) {
                    bool dropped = false;
                    CharacterComponent* entRef = entObj;
                    if (param_5 != 0) {
                        // Exact Type() match on the character's entity, no IsDerivedFrom
                        // (the generic find 0x1400c3680's shape, inlined here).
                        Component* found = entObj->_entity->FindComponentByTypeId(static_cast<uint32_t>(param_5));
                        region = walker.grid; // lVar15 = local_e0 reload
                        if (found == nullptr) dropped = true;
                    }
                    if (!dropped) {
                        const Math::Vector3& pos = entRef->modelComponent->transform.translation;
                        Math::Vector3 diff(pos.x - param_3[0],
                                           pos.y - param_3[1],
                                           pos.z - param_3[2]);
                        float dsq = diff.Dot(diff).v;
                        if (dsq < fr2) {
                            *best = entRef;
                            fr2 = dsq;
                        }
                    }
                }
                // LAB_140092741: advance to the next character in the cell list (+0x08).
                ent = region->slots[ent].prev;
            } while (ent != headSentinel);
        }
        walker.Subdivide();   // 0x140018880
        top = walker.top;
        region = walker.grid;
    }
}

// ============================================================
// 0x140122340 — HerdComponent::StartMigration
// ============================================================
// Picks a migration target tile for the herd via 053ca0 (stored in `tile`).
//
//  * If the picker FAILS (tile < 0): the herd disperses — for each member (loop while
//    memberCount > 0) it marks the member entity for removal (0c2ea0) and appends it to
//    the world entity list's removalList via 003110.
//
//    LOOP-EXIT NOTE (verified by disassemble_function 0x140122340): the dispersal loop
//    counter R15 is set to 0 ONCE (0x14012237b XOR R15D,R15D) and is NEVER incremented
//    inside the body; the loop guard is `CMP [R14+0x40],R15D ; JG` (i.e. while herd
//    member-count > 0). Neither 0c2ea0 nor 003110 writes memberCount (0c2ea0 ORs flag
//    0x400 into the *animal* entity's _entityFlags, a different object; 003110 is a queue
//    push). So this is faithfully a `do { ... } while (0 < memberCount)` that relies on
//    the engine's deferred-removal machinery (the queue drained later) to shrink
//    memberCount. The real game does not hang here, so this faithful transcription will
//    not either. Transcribed exactly as the binary does it.
//
//  * If the picker SUCCEEDS (tile >= 0): computes a clamped target rectangle (from the
//    map size and the HerdDescription's _spread) and writes the 4 int16 wander fields
//    into each member's AnimalComponent.
void HerdStartMigration(HerdComponent* herd) {
    int iVar4 = Pf_PastureTargetPick_53ca0(herd->population, herd->populationCell);   // 0x140053ca0
    herd->tile = iVar4;

    if (iVar4 < 0) {
        if (0 < herd->memberCount) {
            do {
                ComponentEntity* local_res8 = herd->members[0]->_entity;
                EntityList* list = GameUpdateLoop::_singleton->m_entityList;
                if (local_res8 != nullptr) {
                    if ((local_res8->_entityFlags >> 10 & 1) == 0) {
                        ActHunt_MarkEntityRemoval_c2ea0(local_res8);   // 0x1400c2ea0
                        ActHunt_DynArraySetCount_3110(&list->removalList,   // 0x140003110
                                 list->removalList.count + 1, &local_res8);
                    }
                }
            } while (0 < herd->memberCount);
        }
        return;
    }

    // Positive branch: compute clamped target rectangle + write member move targets.
    Description* pvVar11 = herd->_description;
    bool bVar3 = ::Object::IsDerivedFromNullSafe(pvVar11, HerdDescription::TypeId());
    const HerdDescription* pvVar6 = bVar3 ? static_cast<const HerdDescription*>(pvVar11) : nullptr;
    int local_res10 = pvVar6->_spread;
    int worldW = GameUpdateLoop::_singleton->m_game->size;
    int local_res18 = herd->tile % worldW - (local_res10 >> 1);
    int local_res20 = herd->tile / worldW - (local_res10 >> 1);
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

    if (0 < herd->memberCount) {
        uint32_t memberIdx = 0;
        do {
            ComponentEntity* local_58 = herd->members[memberIdx]->_entity;
            AnimalComponent* foundComp = local_58->FindComponent<AnimalComponent>();
            // DEVIATION (guard): retail writes unconditionally — it can, because a
            // dead member is erased from the herd list by WildAnimalComponent's
            // OnRemove before migration ever walks it. A null foundComp here means
            // that cleanup didn't happen (or the member lost its AnimalComponent):
            // skipping the write beats the retail-faithful WRITE-null+0x38 crash,
            // and the trace makes the cleanup bug visible instead of silent.
            if (!foundComp) {
                static int s_nullMemberLogs = 0;
                if (s_nullMemberLogs < 8) {
                    ++s_nullMemberLogs;
                    fprintf(stderr, "[Herd] StartMigration: member %u of herd %p has no "
                            "AnimalComponent (dead entry not erased?) — skipping\n",
                            memberIdx, static_cast<void*>(herd));
                }
                memberIdx = memberIdx + 1;
                continue;
            }
            foundComp->wanderX = (int16_t)local_res18;
            foundComp->wanderY = (int16_t)local_res20;
            foundComp->wanderWidth = (int16_t)local_res8_lo;
            foundComp->wanderHeight = (int16_t)local_res10;

            memberIdx = memberIdx + 1;
        } while ((int)memberIdx < herd->memberCount);
    }
}

// ===========================================================================
// B4 FORWARD: the C spellings other blocks' files still call, one line each.
// ===========================================================================

// B4 FORWARD: 1 outside caller (Action/createrawmaterial.cpp, block ACT-b)
int Pf_FindPlacementCell_bd40(int* param_1, int param_2, char param_3, int64_t* param_4, uint32_t param_5) {
    return reinterpret_cast<Map*>(param_1)->FindPlacementCell(
        param_2, param_3, reinterpret_cast<ComponentEntity* const*>(param_4), param_5);
}

// B4 FORWARD: 2 outside callers (Action/huntanimal.cpp, Action/moveto.cpp, block ACT-b).
// Returns its fourth argument, as retail's RAX does.
int64_t* Pf_SpatialProximityQuery_92260(float* param_1, int64_t* param_2, float* param_3,
                                         void* param_4, int param_5, void* param_6) {
    QueryNearestCharacter(reinterpret_cast<const CharacterGrid*>(param_1),
                          reinterpret_cast<CharacterComponent**>(param_2), param_3,
                          reinterpret_cast<const Math::Scalar*>(param_4), param_5,
                          reinterpret_cast<CharacterComponent* const*>(param_6));
    return reinterpret_cast<int64_t*>(param_4);
}

// B4 FORWARD: outside callers Component/herdcomponent.cpp, Action/huntanimal.cpp
void Pf_HerdStartMigration_122340(void* param_1) {
    HerdStartMigration(reinterpret_cast<HerdComponent*>(param_1));
}
