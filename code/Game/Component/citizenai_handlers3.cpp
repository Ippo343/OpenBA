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
// SELF CHAIN: param_1 = CitizenAIComponent. self = param_1+0x38 (sibling CitizenComponent
//   IBP). citizen = *self. citizen+0x238 = workplace entity, +0x228 = active path/target,
//   +0x288 = CitizenDescription (+0xcc adult-age thresh, +0xec elder-age thresh, +0x124
//   walk time), +0x44/+0x48 = age fields, +0x278 = CharacterComponent (+0x68 = ModelComponent,
//   whose transform +0x158/+0x15c/+0x160 = world pos). Success epilogue writes the state flags
//   to *self (+0x30 = -1, +0x54 |= 4, +0xb0 = 0) and returns 1. Cooldown-fail stamps Time(3.75)
//   into the CitizenAIComponent cooldown arrays (Adult: param_1+0x40 & +0xb0; Elder: +0x48 & +0xb8)
//   and returns 0.
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


#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/InteractDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "resolve.h"
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

// citizen ModelComponent transform (+0x68 of CharacterComponent at citizen+0x278).
static int64_t CitizenModel(int64_t citizen) {
    int64_t ch = *reinterpret_cast<int64_t*>(citizen + 0x278);
    return *reinterpret_cast<int64_t*>(ch + 0x68);
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

// success epilogue: set the action-state flags on the CitizenComponent (*self) and return 1.
static uint64_t Pushed(int64_t* self) {
    int64_t cz = *self;
    *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
    *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
    *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
    return (static_cast<uint64_t>(cz) & 0xffffffffffffff00ULL) | 1;  // CONCAT71(cz>>8, 1)
}

} // namespace

// ============================================================
// 0x14010d2c0 — CitizenAI_State_ElderTransition  (state id 1 / 15)
// ============================================================
extern "C" uint64_t CitizenAI_State_ElderTransition(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    int64_t citizen = *self;
    float age = *reinterpret_cast<float*>(citizen + 0x48);
    int64_t desc = *reinterpret_cast<int64_t*>(citizen + 0x288);

    float dest[4] = {0, 0, 0, 0};
    int64_t found = 0;        // local_res8
    int64_t holderEntity = 0; // entity passed to Ctor8
    bool doPush = false;

    // is-elder: age > elder threshold (desc+0xec)
    if (desc != 0 && *reinterpret_cast<float*>(desc + 0xec) <= age && age != *reinterpret_cast<float*>(desc + 0xec)) {
        int* mg = MapGrid();
        int64_t listEnt = *reinterpret_cast<int64_t*>(GW() + 0xf0);
        int64_t model = CitizenModel(citizen);
        uint32_t cell = ModelCell(model, mg);
        Entity_impl_ec70(listEnt, &found, cell, dest);   // find nearest valid residence
        if (found != 0) { holderEntity = found; doPush = true; }
    }

    if (!doPush) {
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            citizen = *self;
            if (*reinterpret_cast<int64_t*>(citizen + 0x238) != 0) {
                int64_t storBuf[2] = {0, 0};
                CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(citizen + 0x238), storBuf);
                bool ok = (storBuf[0]) ? ResidenceCheck_2ac30(storBuf[0]) : false;
                if (ok) {
                    int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
                    int64_t posBuf[2] = {0, 0};
                    void* pp = WorkBase_FindComponent_2ed0(wp, posBuf);
                    if (pp) Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(pp), dest, 0, 0, nullptr);
                    holderEntity = *reinterpret_cast<int64_t*>(*self + 0x238);
                    doPush = true;
                }
            }
        }
    }

    if (doPush) {
        int64_t holderCitizen = *self;
        CitizenAI_PerformWorkCtor8_bd790(&holderCitizen, reinterpret_cast<uint64_t*>(&holderEntity), dest);
        return Pushed(self);
    }

    // cooldown stamp -> CitizenAIComponent +0x48 & +0xb8
    int64_t stamp = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
    *reinterpret_cast<int64_t*>(param_1 + 0x48) = stamp;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
    *reinterpret_cast<int64_t*>(param_1 + 0xb8) = stamp;
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

// ============================================================
// 0x14010cd80 — CitizenAI_State_AdultTransition  (state id 0 / 14)
// ============================================================
extern "C" uint64_t CitizenAI_State_AdultTransition(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    int64_t citizen = *self;
    float age = *reinterpret_cast<float*>(citizen + 0x44);
    int64_t desc = *reinterpret_cast<int64_t*>(citizen + 0x288);
    bool isAdult = (desc != 0) && (age > *reinterpret_cast<float*>(desc + 0xcc));

    float dest[4] = {0, 0, 0, 0};
    int64_t found = 0;     // local_res8 (chosen target entity)
    char flag = 0;         // Ctor6 flag (cVar9)
    bool ctorDirect = false;  // reached the Ctor6 push with holders already set (LAB_14010d004)
    int64_t holderEntity = 0; // local_res10
    int64_t holderCitizen = 0;// local_res18

    bool retryWorkPath = !isAdult;  // age <= thresh -> LAB_14010d03c

    if (isAdult) {
        if (*reinterpret_cast<int64_t*>(citizen + 0x238) != 0) {
            int64_t storBuf[2] = {0, 0};
            void* p13 = CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(citizen + 0x238), storBuf);
            int cnt = (p13) ? CitizenComp_Storage_GetResourceCount(*reinterpret_cast<int64_t*>(p13)) : 0;
            if (cnt > 0) {
                found = *reinterpret_cast<int64_t*>(citizen + 0x238);  // workplace
                int64_t model = CitizenModel(*self);
                dest[0] = *reinterpret_cast<float*>(model + 0x158);
                dest[1] = *reinterpret_cast<float*>(model + 0x15c);
                dest[2] = *reinterpret_cast<float*>(model + 0x160);
            }
        }
        if (found == 0) {
            int* mg = MapGrid();
            int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
            int64_t wp = *reinterpret_cast<int64_t*>(citizen + 0x238);
            int64_t model = CitizenModel(*self);
            uint32_t cell = ModelCell(model, mg);
            int64_t e370arg = wp;          // param_7 (workplace preferred candidate)
            int64_t outComp = 0;           // local_res20
            int64_t outBest = 0;           // local_res18
            CitizenAI_helper_e370(listpair, &outBest, cell, &outComp, dest, 0, &e370arg);
            found = outBest;
            if (outBest == 0) retryWorkPath = true;  // goto LAB_14010d03c
        }
    }

    if (retryWorkPath) {
        // LAB_14010d03c: prefer an existing valid workplace with resources
        bool handled = false;
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            int64_t wpv = *reinterpret_cast<int64_t*>(*self + 0x238);
            char ok = CitizenComp_CheckWorkplaceValid(reinterpret_cast<CitizenComponent*>(*self), &wpv);
            if (ok != 0) {
                int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
                int64_t storBuf[2] = {0, 0};
                void* p13 = CitizenComp_Entity_FindComponent_Storage(wp, storBuf);
                int cnt = (p13) ? CitizenComp_Storage_GetResourceCount(*reinterpret_cast<int64_t*>(p13)) : 0;
                if (cnt > 0) {
                    int64_t wp2 = *reinterpret_cast<int64_t*>(*self + 0x238);
                    int64_t posBuf[2] = {0, 0};
                    void* pp = WorkBase_FindComponent_2ed0(wp2, posBuf);
                    if (pp) Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(pp), dest, 0, 0, nullptr);
                    holderCitizen = *self;
                    flag = 1;
                    holderEntity = *reinterpret_cast<int64_t*>(holderCitizen + 0x238);
                    ctorDirect = true;
                    handled = true;
                }
            }
        }
        if (!handled) {
            // dd30 find-work target
            dest[0] = dest[1] = dest[2] = 0.0f;
            int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
            int* mg = MapGrid();
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            uint32_t bGoal;
            if (wp == 0) bGoal = 0xffffffff;
            else { int64_t wpIBP = wp; bGoal = Pf_EntityAnchorCell_7fb0(&wpIBP, nullptr); }
            int64_t model = CitizenModel(*self);
            uint32_t cell = ModelCell(model, mg);
            int64_t outComp = 0;  // local_res18
            CitizenAI_FindBestTarget_dd30(listpair, &found, cell, bGoal, &outComp, dest, 0);
            if (found == 0) {
                // no target -> cooldown stamp (param_1+0x40 & +0xb0) + return 0
                int64_t stamp = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
                *reinterpret_cast<int64_t*>(param_1 + 0x40) = stamp;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&stamp), kCooldown);
                *reinterpret_cast<int64_t*>(param_1 + 0xb0) = stamp;
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
        holderCitizen = *self;
    }

    CitizenAI_PerformWorkCtor6_bd420(&holderCitizen, &holderEntity, dest, flag);
    return Pushed(self);
}

// ============================================================
// 0x14010d790 — CitizenAI_State_Work  (state id 5)
//
// VERIFIED IN-GAME 2026-09-06.  Despite the name, this is the SICK-CITIZEN state:
// BuildCandidateStateList only ever appends id 5 when CitizenComponent+0x68 (the
// Disease*, set by Infect 0x140116b90) is non-null, so a healthy town never offers it.
// A/B in the BANISHED_STRESS town, 200 citizens, 1600 frames:
//   control            -> id 5 absent from the census, 0 [Work] entries
//   BANISHED_FORCE_SICK=25 -> "Work (5) offered=55 top=55 by prio: p6=55", handler entered
// +0x228 (care building, filled by FindCareBuilding 0x140111590) picks the arm:
// set -> walk to that building's entity (+0x228 -> +0x10); clear -> NavigateTo +0x238.
// This closes the lane-C open that read +0x68 as a "housing link".
// ============================================================
extern "C" uint64_t CitizenAI_State_Work(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    int64_t active = *reinterpret_cast<int64_t*>(*self + 0x228);

    if (active == 0) {
        uint64_t valid = CitizenAI_helper_db20(param_1);
        if (static_cast<char>(valid) != '\0') {
            // NavigateTo the workplace
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            int64_t posBuf[2] = {0, 0};
            void* pp = WorkBase_FindComponent_2ed0(wp, posBuf);
            uint32_t navDest[4] = {0, 0, 0, 0};
            if (pp) Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(pp), reinterpret_cast<float*>(navDest), 0, 0, nullptr);
            int64_t holderCitizen = *self;
            int64_t holderEntity = *reinterpret_cast<int64_t*>(holderCitizen + 0x238);
            CitizenAI_Pathfinding_NavigateTo_bda50(&holderCitizen, &holderEntity, navDest, 0);
            return Pushed(self);
        }
        return valid & 0xffffffffffffff00ULL;  // db20 false -> return 0
    }

    // active != 0: a work target is set -> search for a walkable adjacent position
    int64_t target = *reinterpret_cast<int64_t*>(active + 0x10);
    int64_t posCompBuf[2] = {0, 0};
    WorkBase_FindComponent_2ed0(target, posCompBuf);

    float walkOut[4] = {0, 0, 0, 0};    // local_70 (out walkable pos)
    // citizen model pos -> pos vec (local_80/local_7c/local_78)
    int64_t model = CitizenModel(*self);
    float pos[4];
    pos[0] = *reinterpret_cast<float*>(model + 0x158);
    pos[1] = *reinterpret_cast<float*>(model + 0x15c);
    pos[2] = *reinterpret_cast<float*>(model + 0x160);
    pos[3] = 0.0f;

    int64_t posComp = posCompBuf[0];
    void* sub = posComp ? *reinterpret_cast<void**>(posComp + 0x18) : nullptr;
    int bestCost = 0x7fffffff;
    void* transformSub = (sub && Object::IsDerivedFromNullSafe(sub, s_tid_InteractDescription)) ? sub : nullptr;
    uint8_t any = WorkImpl_WalkablePos_122be0(posComp, reinterpret_cast<int64_t>(transformSub) + 0x28, pos, walkOut, &bestCost);
    if (posComp != 0) {
        int count = *reinterpret_cast<int*>(posComp + 0x30);
        int i = 0;
        int64_t byteOff = 0;
        while (i < count) {
            int64_t* childSlot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(posComp + 0x28) + byteOff);
            uint8_t got = WorkImpl_WalkablePosRecursive_122b00(*childSlot, 0, pos, walkOut, &bestCost);
            any = static_cast<uint8_t>(any | got);
            byteOff += 8;
            ++i;
        }
    }

    if (any == 0) {
        // no walkable pos: release the attendance link, then NavigateTo the workplace
        AttendanceComponent_ReleaseLink59b0(reinterpret_cast<void*>(*self));
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            int64_t posBuf[2] = {0, 0};
            void* pp = WorkBase_FindComponent_2ed0(wp, posBuf);
            uint32_t navDest[4] = {0, 0, 0, 0};
            if (pp) Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(pp), reinterpret_cast<float*>(navDest), 0, 0, nullptr);
            int64_t holderCitizen = *self;
            int64_t holderEntity = *reinterpret_cast<int64_t*>(holderCitizen + 0x238);
            CitizenAI_Pathfinding_NavigateTo_bda50(&holderCitizen, &holderEntity, navDest, 0);
            return Pushed(self);
        }
    }

    // WalkTowards the found walkable position (or workplace-less fallback)
    void* cdesc = *reinterpret_cast<void**>(*self + 0x18);
    void* citDesc = (cdesc && Object::IsDerivedFromNullSafe(cdesc, s_tid_CitizenDescription)) ? cdesc : nullptr;
    int64_t holderCitizen = *self;
    // [STANDALONE GUARD] the citDesc null-check is ours; the original derefs +0x124
    // unconditionally (would fault if the description type-check failed).
    float walkTime = citDesc ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(citDesc) + 0x124) : 0.0f;
    CitizenAI_Pathfinding_WalkTowards_bb490(&holderCitizen, 0xffffffff, reinterpret_cast<uint32_t*>(walkOut), 5, walkTime, 1);

    int64_t cz = *self;
    *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
    *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
    *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
    return static_cast<uint64_t>(cz) & 0xffffffffffffff00ULL;
}

