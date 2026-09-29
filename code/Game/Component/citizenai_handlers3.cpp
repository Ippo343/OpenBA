// code/Game/Component/citizenai_handlers3.cpp
//
// Brick 3b handler wave 3 — faithful 1:1 ports of the three CitizenAIComponent state
// handlers that the deep find-helpers (citizenai_findhelpers.cpp) just unblocked:
//
//   0x14010d790  CitizenAI_State_Work            (state id 5)  -> db20 + NavigateTo/WalkTowards
//                                                  + 122be0/122b00 walkable-pos search + 59b0
//   0x14010cd80  CitizenAI_State_AdultTransition (state id 0/14) -> e370/dd30 + 2ac30 + Ctor6
//   0x14010d2c0  CitizenAI_State_ElderTransition (state id 1/15) -> ec70 + 2ac30 + Ctor8
//
// Each makes its state ENTERABLE by building + pushing an Action/Work sequence onto the
// citizen action stack (via the registered builder ctors). ValidateAndEnterState
// (citizenai_candidatelist.cpp) dispatches them by ResolveAddr; returning 1 ("pushed")
// commits the state. Transcribed from the Ghidra decompile, offset-by-offset.
//
// B.4 W1 COMP-b: the handlers are CitizenAIComponent members; `citizen` is the sibling
//   CitizenComponent, its fields read by name (home, careBuilding, hunger, coldness,
//   description->_fcc / _fec / _f124, character->modelComponent whose transform
//   +0x158/+0x15c/+0x160 = world pos, raw). Success epilogue writes field_0x30 = -1,
//   statusFlags |= 4, field_0xB0 = 0 and returns 1. Cooldown-fail stamps Time(3.75) into
//   timers[0] & timers[14] (Adult) or timers[1] & timers[15] (Elder) and returns 0.
//
// Registered exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x14010db20 CitizenAI_helper_db20  (citizenai_findhelpers.cpp)   [Work]
//   0x14008e370 CitizenAI_helper_e370  (citizenai_findhelpers.cpp)   [Adult]
//   0x14008ec70 Entity_impl_ec70       (citizenai_findhelpers.cpp)   [Elder]
//   0x14008dd30 CitizenAI_FindBestTarget_dd30 (citizenai_findtarget.cpp) [Adult]
//   0x14012ac30 ResidenceCheck_2ac30   (citizenai_findhelpers.cpp)   [Adult, Elder]
//   0x1401158d0 CitizenComp_CheckWorkplaceValid (reg_component_gap_A.cpp) [Adult]
//   0x14012a9d0 CitizenComp_Storage_GetResourceCount (reg_component_people.cpp) [Adult]
//   0x140002ed0 WorkBase_FindComponent_Position (reg_work_base.cpp)  [all]
//   0x140008090 CitizenComp_Entity_FindComponent_Storage (reg_component_people.cpp) [Adult, Elder]
//   0x140122e20 Pf_EntityResolvePos_2e20 (reg_pf_road.cpp)           [all]
//   0x140087fb0 Pf_EntityAnchorCell_7fb0  (reg_pf_road.cpp)          [Adult]
//   0x140122be0 WorkImpl_WalkablePos_122be0 (reg_work_walkpos.cpp)   [Work]
//   0x140122b00 WorkImpl_WalkablePosRecursive_122b00 (reg_work_walkpos.cpp) [Work]
//   0x1401159b0 AttendanceComponent_ReleaseLink59b0 (reg_component_gap_A.cpp) [Work]
//   0x1400bd420 PerformWork_Action_Ctor6  (citizenai_actionctors.cpp) [Adult]
//   0x1400bd790 PerformWork_Action_Ctor8  (citizenai_actionctors.cpp) [Elder]
//   0x1400bda50 Pathfinding_NavigateTo    (citizenai_actionctors.cpp) [Work]
//   0x1400bb490 Pathfinding_WalkTowards   (citizenai_actionctors.cpp) [Work]
//
// SELF-CONTAINED-BRICK type-ids (UNFILLED 0xFFFFFFFF in standalone; resolved via
//   Object::GetTypeIdByName, scan-result derefs null-guarded):
//     DAT_1401c7938  -> "InteractDescription" [Work transform-sub IsDerivedFrom]
//     DWORD_1401c7190 -> "CitizenDescription" [Work walk-time IsDerivedFrom]
//
// .rdata float const: DAT_140172190 = 3.75 (Adult/Elder transition cooldown seconds; read
//   from the exe at 0x140172190 = 0x40700000). Hardcoded here (pure scalar; no indirection).
//


#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/InteractDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenAIComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/AttendanceComponent.h"
#include "Component/InteractComponent.h"
#include "Component/ResidenceComponent.h"
#include "Entity/ComponentEntity.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "system/Value.h"

namespace {

// --- registered exe sub-callees: called by name (Component/Action/Work exe_bodies.h) ---

// EnsureInit resolves no door: it only caches the type ids.
static uint32_t s_tid_InteractDescription = 0;  // DAT_1401c7938
static uint32_t s_tid_CitizenDescription  = 0;  // DWORD_1401c7190

static const float kCooldown = 3.75f;  // DAT_140172190

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    if (s_tid_InteractDescription == 0) s_tid_InteractDescription = InteractDescription::TypeId();
    if (s_tid_CitizenDescription == 0)  s_tid_CitizenDescription  = CitizenDescription::TypeId();
    s_init = true;
}

static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }

// the citizen's ModelComponent; its transform (+0x158/+0x15c/+0x160 = world pos) is raw.
static int64_t CitizenModel(CitizenComponent* citizen) {
    return reinterpret_cast<int64_t>(citizen->character->modelComponent);
}

// model world pos (+0x158/+0x15c) -> clamped grid cell (row*w + col).
static uint32_t ModelCell(int64_t model, int* mapGrid) {
    int w = *mapGrid;
    int col = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(w >> 1));
    int row = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(w >> 1));
    if (col < 0) col = 0; if (w <= col) col = w - 1;
    if (row < 0) row = 0; if (w <= row) row = w - 1;
    return static_cast<uint32_t>(w * row + col);
}

// success epilogue: set the action-state flags on the CitizenComponent and return 1.
static uint64_t Pushed(CitizenComponent* cz) {
    cz->field_0x30 = -1;
    cz->statusFlags |= 4;
    cz->field_0xB0 = 0;
    return 1;  // CONCAT71(cz>>8, 1): the caller reads the low byte
}

} // namespace

// ============================================================
// 0x14010d2c0 — CitizenAIComponent::State_ElderTransition  (state id 1 / 15)
// ============================================================
uint64_t CitizenAIComponent::State_ElderTransition() {
    EnsureInit();
    float age = citizen->coldness;
    CitizenDescription* desc = citizen->description;

    float dest[4] = {0, 0, 0, 0};
    int64_t found = 0;        // local_res8
    int64_t holderEntity = 0; // entity passed to Ctor8
    bool doPush = false;

    // coldness above the description's _fec threshold
    if (desc != nullptr && desc->_fec <= age && age != desc->_fec) {
        int* mg = MapGrid();
        int64_t listEnt = *reinterpret_cast<int64_t*>(GW() + 0xf0);
        int64_t model = CitizenModel(citizen);
        uint32_t cell = ModelCell(model, mg);
        Entity_impl_ec70(listEnt, &found, cell, dest);   // find nearest valid residence
        if (found != 0) { holderEntity = found; doPush = true; }
    }

    if (!doPush) {
        if (citizen->home != nullptr) {
            if (citizen->home != nullptr) {
                ResidenceComponent* stor = citizen->home->FindComponent<ResidenceComponent>();
                bool ok = (stor) ? ResidenceCheck_2ac30(reinterpret_cast<int64_t>(stor)) : false;
                if (ok) {
                    InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
                    Pf_EntityResolvePos_2e20(pp, dest, 0, 0, nullptr);
                    holderEntity = reinterpret_cast<int64_t>(citizen->home);
                    doPush = true;
                }
            }
        }
    }

    if (doPush) {
        CitizenComponent* holderCitizen = citizen;
        CitizenAI_PerformWorkCtor8_bd790(&holderCitizen, reinterpret_cast<uint64_t*>(&holderEntity), dest);
        return Pushed(citizen);
    }

    // cooldown stamp -> timers[1] & timers[15]
    int64_t stamp = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
    timers[1] = stamp;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
    timers[15] = stamp;
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

// ============================================================
// 0x14010cd80 — CitizenAIComponent::State_AdultTransition  (state id 0 / 14)
// ============================================================
uint64_t CitizenAIComponent::State_AdultTransition() {
    EnsureInit();
    float age = citizen->hunger;
    CitizenDescription* desc = citizen->description;
    bool isAdult = (desc != nullptr) && (age > desc->_fcc);

    float dest[4] = {0, 0, 0, 0};
    int64_t found = 0;     // local_res8 (chosen target entity)
    char flag = 0;         // Ctor6 flag (cVar9)
    bool ctorDirect = false;  // reached the Ctor6 push with holders already set (LAB_14010d004)
    int64_t holderEntity = 0; // local_res10
    CitizenComponent* holderCitizen = nullptr;// local_res18

    bool retryWorkPath = !isAdult;  // hunger <= thresh -> LAB_14010d03c

    if (isAdult) {
        if (citizen->home != nullptr) {
            ResidenceComponent* stor = citizen->home->FindComponent<ResidenceComponent>();
            int cnt = CitizenComp_Storage_GetResourceCount(reinterpret_cast<int64_t>(stor));
            if (cnt > 0) {
                found = reinterpret_cast<int64_t>(citizen->home);  // the home
                int64_t model = CitizenModel(citizen);
                dest[0] = *reinterpret_cast<float*>(model + 0x158);
                dest[1] = *reinterpret_cast<float*>(model + 0x15c);
                dest[2] = *reinterpret_cast<float*>(model + 0x160);
            }
        }
        if (found == 0) {
            int* mg = MapGrid();
            int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
            int64_t model = CitizenModel(citizen);
            uint32_t cell = ModelCell(model, mg);
            ComponentEntity* e370arg = citizen->home;   // param_7 (home preferred candidate)
            int64_t outComp = 0;           // local_res20
            int64_t outBest = 0;           // local_res18
            CitizenAI_helper_e370(listpair, &outBest, cell, &outComp, dest, 0, reinterpret_cast<int64_t*>(&e370arg));
            found = outBest;
            if (outBest == 0) retryWorkPath = true;  // goto LAB_14010d03c
        }
    }

    if (retryWorkPath) {
        // LAB_14010d03c: prefer the reachable home when it has food
        bool handled = false;
        if (citizen->home != nullptr) {
            char ok = citizen->CanReach(citizen->home);
            if (ok != 0) {
                ResidenceComponent* stor = citizen->home->FindComponent<ResidenceComponent>();
                int cnt = CitizenComp_Storage_GetResourceCount(reinterpret_cast<int64_t>(stor));
                if (cnt > 0) {
                    InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
                    Pf_EntityResolvePos_2e20(pp, dest, 0, 0, nullptr);
                    holderCitizen = citizen;
                    flag = 1;
                    holderEntity = reinterpret_cast<int64_t>(holderCitizen->home);
                    ctorDirect = true;
                    handled = true;
                }
            }
        }
        if (!handled) {
            // dd30 find-food target
            dest[0] = dest[1] = dest[2] = 0.0f;
            int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
            int* mg = MapGrid();
            uint32_t bGoal;
            if (citizen->home == nullptr) bGoal = 0xffffffff;
            else { ComponentEntity* wpIBP = citizen->home; bGoal = Pf_EntityAnchorCell_7fb0(&wpIBP, nullptr); }
            int64_t model = CitizenModel(citizen);
            uint32_t cell = ModelCell(model, mg);
            int64_t outComp = 0;  // local_res18
            CitizenAI_FindBestTarget_dd30(listpair, &found, cell, bGoal, &outComp, dest, 0);
            if (found == 0) {
                // no target -> cooldown stamp (timers[0] & timers[14]) + return 0
                int64_t stamp = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
                timers[0] = stamp;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
                timers[14] = stamp;
                return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
            }
        }
    }

    if (!ctorDirect) {
        // common tail: validate found entity's Storage with ResidenceCheck_2ac30
        int64_t sbuf20[2] = {0, 0};
        void* p13 = CitizenComp_Entity_FindComponent_Storage(found, sbuf20);
        int64_t storComp = p13 ? *reinterpret_cast<int64_t*>(p13) : 0;
        if (storComp == 0) {
            flag = 0;
        } else {
            int64_t sbuf68[2] = {0, 0};
            void* p13b = CitizenComp_Entity_FindComponent_Storage(found, sbuf68);
            int64_t storComp2 = p13b ? *reinterpret_cast<int64_t*>(p13b) : 0;
            flag = (storComp2 && ResidenceCheck_2ac30(storComp2)) ? 1 : 0;
        }
        holderEntity = found;
        holderCitizen = citizen;
    }

    CitizenAI_PerformWorkCtor6_bd420(&holderCitizen, &holderEntity, dest, flag);
    return Pushed(citizen);
}

// ============================================================
// 0x14010d790 — CitizenAIComponent::State_Work  (state id 5)
//
// VERIFIED IN-GAME 2026-09-06.  Despite the name, this is the SICK-CITIZEN state:
// BuildCandidateStateList only ever appends id 5 when the citizen's disease
// (set by Infect 0x140116b90) is non-null, so a healthy town never offers it.
// A/B in the BANISHED_STRESS town, 200 citizens, 1600 frames:
//   control            -> id 5 absent from the census, 0 [Work] entries
//   BANISHED_FORCE_SICK=25 -> "Work (5) offered=55 top=55 by prio: p6=55", handler entered
// careBuilding (filled by FindCareBuilding 0x140111590) picks the arm:
// set -> walk to that building's entity; clear -> NavigateTo the home.
// ============================================================
uint64_t CitizenAIComponent::State_Work() {
    EnsureInit();
    AttendanceComponent* active = citizen->careBuilding;

    if (active == nullptr) {
        uint64_t valid = CanReachHome();
        if (static_cast<char>(valid) != '\0') {
            // NavigateTo the home
            InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
            uint32_t navDest[4] = {0, 0, 0, 0};
            Pf_EntityResolvePos_2e20(pp, reinterpret_cast<float*>(navDest), 0, 0, nullptr);
            CitizenComponent* holderCitizen = citizen;
            ComponentEntity* holderEntity = holderCitizen->home;
            CitizenAI_Pathfinding_NavigateTo_bda50(&holderCitizen, reinterpret_cast<int64_t*>(&holderEntity), navDest, 0);
            return Pushed(citizen);
        }
        return valid & 0xffffffffffffff00ULL;  // db20 false -> return 0
    }

    // active != 0: a care building is set -> search for a walkable adjacent position
    InteractComponent* posComp = active->_entity->FindComponent<InteractComponent>();

    float walkOut[4] = {0, 0, 0, 0};    // local_70 (out walkable pos)
    // citizen model pos -> pos vec (local_80/local_7c/local_78)
    int64_t model = CitizenModel(citizen);
    float pos[4];
    pos[0] = *reinterpret_cast<float*>(model + 0x158);
    pos[1] = *reinterpret_cast<float*>(model + 0x15c);
    pos[2] = *reinterpret_cast<float*>(model + 0x160);
    pos[3] = 0.0f;

    void* sub = posComp ? posComp->_description : nullptr;
    int bestCost = 0x7fffffff;
    void* transformSub = (sub && Object::IsDerivedFromNullSafe(sub, s_tid_InteractDescription)) ? sub : nullptr;
    uint8_t any = WorkImpl_WalkablePos_122be0(reinterpret_cast<uintptr_t>(posComp), reinterpret_cast<int64_t>(transformSub) + 0x28, pos, walkOut, &bestCost);
    if (posComp != nullptr) {
        // the use locations: field28 array, the count in field30's low half (untyped: raw)
        int count = *reinterpret_cast<int*>(&posComp->field30);
        int i = 0;
        int64_t byteOff = 0;
        while (i < count) {
            int64_t* childSlot = reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(posComp->field28) + byteOff);
            uint8_t got = WorkImpl_WalkablePosRecursive_122b00(*childSlot, 0, pos, walkOut, &bestCost);
            any = static_cast<uint8_t>(any | got);
            byteOff += 8;
            ++i;
        }
    }

    if (any == 0) {
        // no walkable pos: release the attendance link, then NavigateTo the home
        AttendanceComponent_ReleaseLink59b0(citizen);
        if (citizen->home != nullptr) {
            InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
            uint32_t navDest[4] = {0, 0, 0, 0};
            Pf_EntityResolvePos_2e20(pp, reinterpret_cast<float*>(navDest), 0, 0, nullptr);
            CitizenComponent* holderCitizen = citizen;
            ComponentEntity* holderEntity = holderCitizen->home;
            CitizenAI_Pathfinding_NavigateTo_bda50(&holderCitizen, reinterpret_cast<int64_t*>(&holderEntity), navDest, 0);
            return Pushed(citizen);
        }
    }

    // WalkTowards the found walkable position (or home-less fallback)
    void* cdesc = citizen->_description;
    void* citDesc = (cdesc && Object::IsDerivedFromNullSafe(cdesc, s_tid_CitizenDescription)) ? cdesc : nullptr;
    CitizenComponent* holderCitizen = citizen;
    // [STANDALONE GUARD] the citDesc null-check is ours; the original derefs +0x124
    // unconditionally (would fault if the description type-check failed).
    float walkTime = citDesc ? reinterpret_cast<CitizenDescription*>(citDesc)->_f124 : 0.0f;
    CitizenAI_Pathfinding_WalkTowards_bb490(&holderCitizen, 0xffffffff, reinterpret_cast<uint32_t*>(walkOut), 5, walkTime, 1);

    citizen->statusFlags |= 4;
    citizen->field_0x30 = -1;
    citizen->field_0xB0 = 0;
    return 0;  // low byte 0 (retail's RAX carries the citizen pointer above it)
}

