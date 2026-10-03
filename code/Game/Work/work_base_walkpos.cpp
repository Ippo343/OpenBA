// code/Game/Work/work_base_walkpos.cpp
// Wave 2 (Work) shared-helper cluster — walkable-position search + build-progress step.
//
// Faithful 1:1 ports of 3 Work-referenced exe helpers (decompiled + disasm-verified in Ghidra):
//   0x140122be0  FUN_140122be0             — walkable-position search over one use-node array
//   0x140122b00  FUN_140122b00             — recursive walkable-position search across the parts
//   0x1400f6ef0  Work::StepProgress        — build-progress / time-budget step
#include "Work/exe_bodies.h"
//
// IMPORTANT correction over notes/wave2_drafts/work_base_impl2_full.cpp.draft:
//   The draft's 0x140122be0 dropped the Transform::operator* INPUT vector (R8). Disasm at
//   0x140122d08 `CALL [0x140151470]` shows the member-ABI value-return call is
//     RCX = &transform (the ModelComponent's, +0x148..+0x160)
//     RDX = &outVec3   (hidden return)
//     R8  = &node[i]   (the per-node input Vector3; node array data = *(param_2),
//                       count = *(param_2+8), element stride 0x18)
//   i.e. out = model.transform * node[i].offset. The draft applied the transform to garbage.
//

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/Map.h"
#include "Game/ProfessionList.h"
#include "Game/WeatherSimulation.h"
#include "math/Math.h"
#include "Action/exe_bodies.h"   // B3R: ActCNR_FindComponent_3f60, Pf_IsDstReachable_f810
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cmath>
#include "Component/InteractDescription.h"   // ENDGAME B.3.9 D5
#include "Component/InteractComponent.h"
#include "Component/CitizenDescription.h"    // ENDGAME B.3.9 D5
#include "Component/CitizenComponent.h"
#include "Component/ModelComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"
#include "Work/Work.h"

// ============================================================
// Exe sub-helpers, called by name (B3R)
// ============================================================
//   0x140003f60 FindComponent<ModelComponent>
//   0x14009f810 MapPathCost    = Pf_IsDstReachable_f810      (Action/exe_bodies.h)
//   0x1400a00f0 ValidateCell   = WorkBase_ValidateCell_a00f0 (Work/exe_bodies.h)
// Math::Transform::operator*(Vector3 const&) const -> Vector3: dispatch slot [0x140151470]
// in retail, a direct Math::Transform member call since B.3.1 step 2.

// Clamp helper mirroring the inline (x<0 -> 0; x>=w -> w-1) the decompiler emits everywhere.
static inline int WalkClampCell(int v, int w) {
    if (v < 0) v = 0;
    if (v >= w) v = w - 1;
    return v;
}

// ============================================================
// 0x140122be0 — walkable-position search over one use-node array
// ============================================================
// param_1 = the InteractComponent (its entity's ModelComponent holds the Transform).
// param_2 = one of its description's node arrays (InteractDescription::NodeArray),
// param_3 = source world pos (float[2]+), param_4 = best-pos out (float[3]),
// param_5 = best-distance out (int). An InteractComponent member (its header is
// COMP's, so this stays free).
// Returns true if *param_5 < 0xffff (a reachable node was found).
// LEFT: the outside callers hand the component and the node array as integers.
bool WorkImpl_WalkablePos_122be0(uintptr_t param_1, uintptr_t param_2,
                                 float* param_3, float* param_4, int* param_5) {
    InteractComponent* interact = reinterpret_cast<InteractComponent*>(param_1);
    const InteractDescription::NodeArray* nodes = reinterpret_cast<const InteractDescription::NodeArray*>(param_2);
    ModelComponent* model = interact->_entity->FindComponent<ModelComponent>();   // 0x140003f60

    Map* map = GameUpdateLoop::_singleton->m_game;
    int* mapGrid = reinterpret_cast<int*>(map);   // LEFT: the grid callees take the Map as its leading int
    int w = map->size;
    int srcCol = WalkClampCell(static_cast<int>(param_3[0] + static_cast<float>(w >> 1)), w);
    int srcRow = WalkClampCell(static_cast<int>(param_3[1] + static_cast<float>(w >> 1)), w);
    uint32_t srcCell = static_cast<uint32_t>(w * srcRow + srcCol);

    InteractDescription::Node* recBase = nodes->data;   // *(param_2)
    int recCount = nodes->count;                        // *(param_2+8)
    for (int i = 0; i < recCount; i++) {
        const Math::Transform& xform = model->transform;
        const Math::Vector3& inVec = recBase[i].offset;
        Math::Vector3 pos = xform * inVec;

        int w2 = map->size;
        int col = WalkClampCell(static_cast<int>(pos.x + static_cast<float>(w2 >> 1)), w2);
        int row = WalkClampCell(static_cast<int>(pos.y + static_cast<float>(w2 >> 1)), w2);
        uint32_t destCell = static_cast<uint32_t>(w2 * row + col);

        uint64_t reach = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, srcCell, static_cast<int>(destCell));
        if (static_cast<char>(reach) != '\0') {
            int dist = Pf_IsDstReachable_f810(mapGrid, srcCell, destCell, '\x01');
            if (dist < 0) {
                int gw = map->size;
                int dRow = static_cast<int>(srcCell) / gw - static_cast<int>(destCell) / gw;
                int dCol = static_cast<int>(srcCell) % gw - static_cast<int>(destCell) % gw;
                dist = static_cast<int>(std::sqrt(static_cast<float>(dRow * dRow + dCol * dCol))) + gw;
            }
            if (dist < *param_5) {
                *param_5 = dist;
                param_4[0] = pos.x;
                param_4[1] = pos.y;
                param_4[2] = pos.z;
            }
        }
    }
    return static_cast<uint32_t>(*param_5) < 0xffff;
}

// ============================================================
// 0x140122b00 — recursive walkable-position search across the parts
// ============================================================
// param_1 = the InteractComponent. Resolves its InteractDescription (+0x18,
// IsDerivedFrom DAT_1401c7938), runs WalkablePos_122be0 on node array `param_2`
// (0 use, 1 build, 2 create: desc + 0x28 + param_2*0x10), then recurses into each
// part (useLocations +0x28, count +0x30). Returns the OR of all sub-results.
// LEFT: the outside callers hand the component as an integer.
uint8_t WorkImpl_WalkablePosRecursive_122b00(uintptr_t param_1, int param_2,
                                             float* param_3, float* param_4, int* param_5) {
    InteractComponent* interact = reinterpret_cast<InteractComponent*>(param_1);
    Description* buildRaw = interact->_description;
    InteractDescription* build = nullptr;
    if (Object::IsDerivedFromNullSafe(buildRaw, InteractDescription::TypeId()))
        build = static_cast<InteractDescription*>(buildRaw);

    // The three node arrays are consecutive (+0x28 / +0x38 / +0x48): retail indexes them.
    const InteractDescription::NodeArray* nodes = &build->_useNodes + param_2;
    uint8_t result = WorkImpl_WalkablePos_122be0(
        param_1,
        reinterpret_cast<uintptr_t>(nodes),   // LEFT: 0x140122be0's C signature
        param_3, param_4, param_5) ? 1 : 0;

    int childCount = interact->useLocationCount;
    InteractComponent** childArr = interact->useLocations;
    for (int i = 0; i < childCount; i++) {
        uint8_t sub = WorkImpl_WalkablePosRecursive_122b00(reinterpret_cast<uintptr_t>(childArr[i]),
                                                           param_2, param_3, param_4, param_5);
        result = static_cast<uint8_t>(result | sub);
    }
    return result;
}

// ============================================================
// 0x1400f6ef0 — Work::StepProgress: build-progress / time-budget step
// ============================================================
// RCX = the Work (+0x30 / +0x38 Times, +0x40 counter: Work's own fields), RDX = the
// citizen reference, R8 / R9 = grid cells, [RSP+0x28] = the current Time. Returns a
// ulonglong whose low byte is a bool "should continue"; the upper bytes are what the
// register held (the tree's callers read the whole value, so it is kept bit-exact).
uint64_t Work::StepProgress(CitizenComponent** citizen, uint32_t cellA, uint32_t cellB, const System::Time* now) {

    // +0x30 += (*now - +0x38)
    field_30._ticks += (now->_ticks - field_38._ticks);

    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    Profession* gateA = gw->m_professionList->field_0x30;

    CitizenComponent* target = *citizen;
    if (target->profession != gateA) {
        target = *citizen;
        if (target->workplace != nullptr) {
            WeatherSimulation* weather = gw->m_weather;
            // LEFT: the fall-through return carries the weather pointer's upper bytes.
            uint64_t resDefault = reinterpret_cast<uint64_t>(weather);
            CitizenBehavior* mapInfo = gw->m_citizenList;
            int budget = gw->m_game->PathDistance(cellA, cellB, '\x01');   // 0x14009f810

            Description* obj = (*citizen)->_description;
            CitizenDescription* stage = (Object::IsDerivedFromNullSafe(obj, CitizenDescription::TypeId()))
                                            ? static_cast<CitizenDescription*>(obj) : nullptr;

            int   field108 = stage->_i108;
            int   field0fc = stage->_ifc;
            int   field100 = stage->_i100;
            float field10c = stage->_f10c;
            int   field104 = stage->_i104;

            // Time cap = field104 * 300.0f seconds.
            System::Time cap = {0};
            RtSystem_Time_Ctor(&cap, static_cast<float>(field104) * 300.0f);

            if ((mapInfo->citizenCount / 2 < field_40) &&
                (field_30._ticks < cap._ticks)) {
                field_30._ticks *= 2;
            }
            if ((field_30._ticks < 0) ||
                (cap._ticks < field_30._ticks)) {
                field_30._ticks = cap._ticks;
            }

            float accSec = field_30.AsSeconds();
            float nowSec = now->AsSeconds();
            uint32_t progress = static_cast<uint32_t>(((accSec / nowSec) / static_cast<float>(field104 * 300))
                                * static_cast<float>(field100 - field0fc)) + static_cast<uint32_t>(field0fc);

            uint64_t local_res10_v = resDefault;
            if (static_cast<int>(progress) < budget) {
                field_40 += 1;
                local_res10_v = static_cast<uint64_t>(progress);
            } else {
                uint64_t uVar8 = static_cast<uint64_t>(progress);
                if (budget <= field108) {
                    return (uVar8 & 0xffffffffffffff00ull) | 1u;
                }
                uVar8 = resDefault;
                float cmp = weather->temperature;
                if (field10c < cmp || field10c == cmp) {
                    return (uVar8 & 0xffffffffffffff00ull) | 1u;
                }
            }
            return local_res10_v & 0xffffffffffffff00ull;
        }
    }
    return static_cast<uint64_t>(1);
}

// B4 FORWARD: 5 outside callers (ClearArea, ClearStorage, FireWork, GatherRawMaterials,
// Pickup in WORK-a / WORK-b files) hand the receiver as an integer and the citizen
// reference and the Time as void*.
uint64_t WorkImpl_BuildStructureStep_6ef0(uintptr_t param_1, void* param_2IBP,
                                          uint32_t param_3, uint32_t param_4, void* param_5Time) {
    return reinterpret_cast<Work*>(param_1)->StepProgress(reinterpret_cast<CitizenComponent**>(param_2IBP),
                                                          param_3, param_4,
                                                          reinterpret_cast<const System::Time*>(param_5Time));
}
