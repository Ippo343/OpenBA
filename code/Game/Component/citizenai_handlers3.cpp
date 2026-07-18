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
// This file must NOT depend on hooks.h — it is portable for the standalone exe.


#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// --- registered exe sub-callees ---
typedef uint64_t (*fn_db20_t)(int64_t);                                              // 0x14010db20
typedef int64_t* (*fn_e370_t)(int64_t*, int64_t*, uint32_t, int64_t*, float*, uint64_t, int64_t*); // 0x14008e370
typedef int64_t* (*fn_ec70_t)(int64_t, int64_t*, uint32_t, float*);                  // 0x14008ec70
typedef int64_t* (*fn_dd30_t)(int64_t*, int64_t*, uint32_t, uint32_t, int64_t*, float*, uint32_t); // 0x14008dd30
typedef bool     (*fn_2ac30_t)(int64_t);                                             // 0x14012ac30
typedef char     (*fn_checkWork_t)(void* citizen, void* arg);                        // 0x1401158d0
typedef int      (*fn_getCount_t)(int64_t storageWrap);                              // 0x14012a9d0
typedef void*    (*fn_findComp_t)(int64_t entity, void* outIBP);                     // 0x140002ed0 / 0x140008090
typedef void     (*fn_2e20_t)(int64_t comp, void* outVec3, int, int, void*);         // 0x140122e20
typedef uint32_t (*fn_7fb0_t)(void* ibp, float* outVec3orNull);                      // 0x140087fb0
typedef uint8_t  (*fn_122be0_t)(int64_t entity, int64_t transformSub, float* pos, float* out, int* best); // 0x140122be0
typedef uint8_t  (*fn_122b00_t)(int64_t child, int p2, float* pos, float* out, int* best); // 0x140122b00
typedef void     (*fn_59b0_t)(int64_t citizen);                                      // 0x1401159b0
typedef void     (*fn_ctor6_t)(void* holder, int64_t* entityIBP, float* dest, char flag); // 0x1400bd420
typedef void     (*fn_ctor8_t)(void* holder, uint64_t* entityIBP, float* dest);      // 0x1400bd790
typedef void     (*fn_navTo_t)(void* holder, int64_t* entityIBP, uint32_t* dest, uint8_t flag); // 0x1400bda50
typedef void     (*fn_walk_t)(void* holder, uint32_t p2, uint32_t* dest, uint32_t p4, float time, uint32_t flag); // 0x1400bb490

static fn_db20_t      s_db20      = nullptr;
static fn_e370_t      s_e370      = nullptr;
static fn_ec70_t      s_ec70      = nullptr;
static fn_dd30_t      s_dd30      = nullptr;
static fn_2ac30_t     s_2ac30     = nullptr;
static fn_checkWork_t s_checkWork = nullptr;
static fn_getCount_t  s_getCount  = nullptr;
static fn_findComp_t  s_findPos   = nullptr;  // 0x140002ed0
static fn_findComp_t  s_findStor  = nullptr;  // 0x140008090
static fn_2e20_t      s_2e20      = nullptr;
static fn_7fb0_t      s_7fb0      = nullptr;
static fn_122be0_t    s_122be0    = nullptr;
static fn_122b00_t    s_122b00    = nullptr;
static fn_59b0_t      s_59b0      = nullptr;
static fn_ctor6_t     s_ctor6     = nullptr;
static fn_ctor8_t     s_ctor8     = nullptr;
static fn_navTo_t     s_navTo     = nullptr;
static fn_walk_t      s_walk      = nullptr;

static uint32_t s_tid_InteractDescription = 0;  // DAT_1401c7938
static uint32_t s_tid_CitizenDescription  = 0;  // DWORD_1401c7190

static const float kCooldown = 3.75f;  // DAT_140172190

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_db20      = reinterpret_cast<fn_db20_t>(ResolveAddr(Module::Exe, 0x14010db20));
    s_e370      = reinterpret_cast<fn_e370_t>(ResolveAddr(Module::Exe, 0x14008e370));
    s_ec70      = reinterpret_cast<fn_ec70_t>(ResolveAddr(Module::Exe, 0x14008ec70));
    s_dd30      = reinterpret_cast<fn_dd30_t>(ResolveAddr(Module::Exe, 0x14008dd30));
    s_2ac30     = reinterpret_cast<fn_2ac30_t>(ResolveAddr(Module::Exe, 0x14012ac30));
    s_checkWork = reinterpret_cast<fn_checkWork_t>(ResolveAddr(Module::Exe, 0x1401158d0));
    s_getCount  = reinterpret_cast<fn_getCount_t>(ResolveAddr(Module::Exe, 0x14012a9d0));
    s_findPos   = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_findStor  = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140008090));
    s_2e20      = reinterpret_cast<fn_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_7fb0      = reinterpret_cast<fn_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_122be0    = reinterpret_cast<fn_122be0_t>(ResolveAddr(Module::Exe, 0x140122be0));
    s_122b00    = reinterpret_cast<fn_122b00_t>(ResolveAddr(Module::Exe, 0x140122b00));
    s_59b0      = reinterpret_cast<fn_59b0_t>(ResolveAddr(Module::Exe, 0x1401159b0));
    s_ctor6     = reinterpret_cast<fn_ctor6_t>(ResolveAddr(Module::Exe, 0x1400bd420));
    s_ctor8     = reinterpret_cast<fn_ctor8_t>(ResolveAddr(Module::Exe, 0x1400bd790));
    s_navTo     = reinterpret_cast<fn_navTo_t>(ResolveAddr(Module::Exe, 0x1400bda50));
    s_walk      = reinterpret_cast<fn_walk_t>(ResolveAddr(Module::Exe, 0x1400bb490));
    if (s_tid_InteractDescription == 0) s_tid_InteractDescription = ::Object::GetTypeIdByName("InteractDescription");
    if (s_tid_CitizenDescription == 0)  s_tid_CitizenDescription  = ::Object::GetTypeIdByName("CitizenDescription");
    s_init = true;
}

static inline void CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }

static inline bool TraceOn() {
    static int s_ts = -1;
    if (s_ts < 0) { const char* e = getenv("BANISHED_CITIZENAI_TRACE"); s_ts = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s_ts == 1;
}

// citizen ModelComponent transform (+0x68 of CharacterComponent at citizen+0x278).
static int64_t CitizenModel(int64_t citizen) {
    CheckPtr(reinterpret_cast<void*>(citizen + 0x278));
    int64_t ch = *reinterpret_cast<int64_t*>(citizen + 0x278);
    CheckPtr(reinterpret_cast<void*>(ch + 0x68));
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
    CheckPtr(self);
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
    CheckPtr(self);
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
        if (s_ec70) s_ec70(listEnt, &found, cell, dest);   // find nearest valid residence
        if (found != 0) { holderEntity = found; doPush = true; }
    }

    if (!doPush) {
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            CheckPtr(self);
            citizen = *self;
            if (*reinterpret_cast<int64_t*>(citizen + 0x238) != 0) {
                CheckPtr(reinterpret_cast<void*>(citizen + 0x238));
                int64_t storBuf[2] = {0, 0};
                if (s_findStor) s_findStor(*reinterpret_cast<int64_t*>(citizen + 0x238), storBuf);
                CheckPtr(storBuf);
                bool ok = (storBuf[0] && s_2ac30) ? s_2ac30(storBuf[0]) : false;
                if (ok) {
                    CheckPtr(self);
                    int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
                    CheckPtr(&wp);
                    int64_t posBuf[2] = {0, 0};
                    void* pp = s_findPos ? s_findPos(wp, posBuf) : nullptr;
                    CheckPtr(pp);
                    if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), dest, 0, 0, nullptr);
                    holderEntity = *reinterpret_cast<int64_t*>(*self + 0x238);
                    doPush = true;
                }
            }
        }
    }

    if (doPush) {
        int64_t holderCitizen = *self;
        if (s_ctor8) s_ctor8(&holderCitizen, reinterpret_cast<uint64_t*>(&holderEntity), dest);
        if (TraceOn()) {
            static int n = 0;
            if (n < 6) { fprintf(stderr, "[ElderTrans] self=%p ENTERED entity=%p\n",
                                 reinterpret_cast<void*>(param_1), reinterpret_cast<void*>(holderEntity)); fflush(stderr); n++; }
        }
        return Pushed(self);
    }

    // cooldown stamp -> CitizenAIComponent +0x48 & +0xb8
    int64_t stamp = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&stamp, kCooldown);
    *reinterpret_cast<int64_t*>(param_1 + 0x48) = stamp;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&stamp, kCooldown);
    *reinterpret_cast<int64_t*>(param_1 + 0xb8) = stamp;
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

// ============================================================
// 0x14010cd80 — CitizenAI_State_AdultTransition  (state id 0 / 14)
// ============================================================
extern "C" uint64_t CitizenAI_State_AdultTransition(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    CheckPtr(self);
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
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(citizen + 0x238) != 0) {
            int64_t storBuf[2] = {0, 0};
            void* p13 = s_findStor ? s_findStor(*reinterpret_cast<int64_t*>(citizen + 0x238), storBuf) : nullptr;
            CheckPtr(p13);
            int cnt = (p13 && s_getCount) ? s_getCount(*reinterpret_cast<int64_t*>(p13)) : 0;
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
            if (s_e370) s_e370(listpair, &outBest, cell, &outComp, dest, 0, &e370arg);
            found = outBest;
            if (outBest == 0) retryWorkPath = true;  // goto LAB_14010d03c
        }
    }

    if (retryWorkPath) {
        // LAB_14010d03c: prefer an existing valid workplace with resources
        bool handled = false;
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            CheckPtr(self);
            int64_t wpv = *reinterpret_cast<int64_t*>(*self + 0x238);
            CheckPtr(self);
            char ok = s_checkWork ? s_checkWork(reinterpret_cast<void*>(*self), &wpv) : 0;
            if (ok != 0) {
                CheckPtr(self);
                int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
                CheckPtr(&wp);
                int64_t storBuf[2] = {0, 0};
                void* p13 = s_findStor ? s_findStor(wp, storBuf) : nullptr;
                CheckPtr(p13);
                int cnt = (p13 && s_getCount) ? s_getCount(*reinterpret_cast<int64_t*>(p13)) : 0;
                if (cnt > 0) {
                    CheckPtr(self);
                    int64_t wp2 = *reinterpret_cast<int64_t*>(*self + 0x238);
                    CheckPtr(&wp2);
                    int64_t posBuf[2] = {0, 0};
                    void* pp = s_findPos ? s_findPos(wp2, posBuf) : nullptr;
                    CheckPtr(pp);
                    if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), dest, 0, 0, nullptr);
                    CheckPtr(self);
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
            CheckPtr(self);
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            uint32_t bGoal;
            if (wp == 0) bGoal = 0xffffffff;
            else { int64_t wpIBP = wp; bGoal = s_7fb0 ? s_7fb0(&wpIBP, nullptr) : 0xffffffff; }
            int64_t model = CitizenModel(*self);
            uint32_t cell = ModelCell(model, mg);
            int64_t outComp = 0;  // local_res18
            if (s_dd30) s_dd30(listpair, &found, cell, bGoal, &outComp, dest, 0);
            if (found == 0) {
                // no target -> cooldown stamp (param_1+0x40 & +0xb0) + return 0
                int64_t stamp = 0;
                if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&stamp, kCooldown);
                *reinterpret_cast<int64_t*>(param_1 + 0x40) = stamp;
                if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&stamp, kCooldown);
                *reinterpret_cast<int64_t*>(param_1 + 0xb0) = stamp;
                if (TraceOn()) {
                    static int n = 0;
                    if (n < 6) { fprintf(stderr, "[AdultTrans] self=%p -> no target (bail)\n",
                                         reinterpret_cast<void*>(param_1)); fflush(stderr); n++; }
                }
                return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
            }
        }
    }

    if (!ctorDirect) {
        // common tail: validate found entity's Storage with ResidenceCheck_2ac30
        CheckPtr(&found);
        int64_t sbuf20[2] = {0, 0};
        void* p13 = s_findStor ? s_findStor(found, sbuf20) : nullptr;
        int64_t storComp = p13 ? *reinterpret_cast<int64_t*>(p13) : 0;
        if (storComp == 0) {
            flag = 0;
        } else {
            CheckPtr(&found);
            int64_t sbuf68[2] = {0, 0};
            void* p13b = s_findStor ? s_findStor(found, sbuf68) : nullptr;
            CheckPtr(p13b);
            int64_t storComp2 = p13b ? *reinterpret_cast<int64_t*>(p13b) : 0;
            flag = (storComp2 && s_2ac30 && s_2ac30(storComp2)) ? 1 : 0;
        }
        holderEntity = found;
        holderCitizen = *self;
    }

    if (s_ctor6) s_ctor6(&holderCitizen, &holderEntity, dest, flag);
    if (TraceOn()) {
        static int n = 0;
        if (n < 6) { fprintf(stderr, "[AdultTrans] self=%p ENTERED entity=%p flag=%d\n",
                             reinterpret_cast<void*>(param_1), reinterpret_cast<void*>(holderEntity), (int)flag); fflush(stderr); n++; }
    }
    return Pushed(self);
}

// ============================================================
// 0x14010d790 — CitizenAI_State_Work  (state id 5)
// ============================================================
extern "C" uint64_t CitizenAI_State_Work(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    CheckPtr(self);
    int64_t active = *reinterpret_cast<int64_t*>(*self + 0x228);

    if (active == 0) {
        uint64_t valid = s_db20 ? s_db20(param_1) : 0;
        if (static_cast<char>(valid) != '\0') {
            // NavigateTo the workplace
            CheckPtr(self);
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            CheckPtr(&wp);
            int64_t posBuf[2] = {0, 0};
            void* pp = s_findPos ? s_findPos(wp, posBuf) : nullptr;
            CheckPtr(pp);
            uint32_t navDest[4] = {0, 0, 0, 0};
            if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), navDest, 0, 0, nullptr);
            CheckPtr(self);
            int64_t holderCitizen = *self;
            int64_t holderEntity = *reinterpret_cast<int64_t*>(holderCitizen + 0x238);
            if (s_navTo) s_navTo(&holderCitizen, &holderEntity, navDest, 0);
            if (TraceOn()) {
                static int n = 0;
                if (n < 6) { fprintf(stderr, "[Work] self=%p NavigateTo workplace=%p\n",
                                     reinterpret_cast<void*>(param_1), reinterpret_cast<void*>(holderEntity)); fflush(stderr); n++; }
            }
            return Pushed(self);
        }
        return valid & 0xffffffffffffff00ULL;  // db20 false -> return 0
    }

    // active != 0: a work target is set -> search for a walkable adjacent position
    CheckPtr(&active);
    int64_t target = *reinterpret_cast<int64_t*>(active + 0x10);
    CheckPtr(&target);
    int64_t posCompBuf[2] = {0, 0};
    if (s_findPos) s_findPos(target, posCompBuf);

    float walkOut[4] = {0, 0, 0, 0};    // local_70 (out walkable pos)
    // citizen model pos -> pos vec (local_80/local_7c/local_78)
    int64_t model = CitizenModel(*self);
    float pos[4];
    pos[0] = *reinterpret_cast<float*>(model + 0x158);
    pos[1] = *reinterpret_cast<float*>(model + 0x15c);
    pos[2] = *reinterpret_cast<float*>(model + 0x160);
    pos[3] = 0.0f;

    CheckPtr(posCompBuf);
    int64_t posComp = posCompBuf[0];
    void* sub = posComp ? *reinterpret_cast<void**>(posComp + 0x18) : nullptr;
    int bestCost = 0x7fffffff;
    void* transformSub = (sub && fn_IsDerivedFrom && fn_IsDerivedFrom(sub, s_tid_InteractDescription)) ? sub : nullptr;
    uint8_t any = s_122be0 ? s_122be0(posComp, reinterpret_cast<int64_t>(transformSub) + 0x28, pos, walkOut, &bestCost) : 0;
    if (posComp != 0) {
        int count = *reinterpret_cast<int*>(posComp + 0x30);
        int i = 0;
        int64_t byteOff = 0;
        while (i < count) {
            int64_t* childSlot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(posComp + 0x28) + byteOff);
            CheckPtr(childSlot);
            uint8_t got = s_122b00 ? s_122b00(*childSlot, 0, pos, walkOut, &bestCost) : 0;
            any = static_cast<uint8_t>(any | got);
            byteOff += 8;
            ++i;
        }
    }

    if (any == 0) {
        // no walkable pos: release the attendance link, then NavigateTo the workplace
        CheckPtr(self);
        if (s_59b0) s_59b0(*self);
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            CheckPtr(self);
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            CheckPtr(&wp);
            int64_t posBuf[2] = {0, 0};
            void* pp = s_findPos ? s_findPos(wp, posBuf) : nullptr;
            CheckPtr(pp);
            uint32_t navDest[4] = {0, 0, 0, 0};
            if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), navDest, 0, 0, nullptr);
            CheckPtr(self);
            int64_t holderCitizen = *self;
            int64_t holderEntity = *reinterpret_cast<int64_t*>(holderCitizen + 0x238);
            if (s_navTo) s_navTo(&holderCitizen, &holderEntity, navDest, 0);
            if (TraceOn()) {
                static int n = 0;
                if (n < 6) { fprintf(stderr, "[Work] self=%p no-walkable -> NavigateTo workplace\n",
                                     reinterpret_cast<void*>(param_1)); fflush(stderr); n++; }
            }
            return Pushed(self);
        }
    }

    // WalkTowards the found walkable position (or workplace-less fallback)
    CheckPtr(self);
    void* cdesc = *reinterpret_cast<void**>(*self + 0x18);
    void* citDesc = (cdesc && fn_IsDerivedFrom && fn_IsDerivedFrom(cdesc, s_tid_CitizenDescription)) ? cdesc : nullptr;
    int64_t holderCitizen = *self;
    float walkTime = citDesc ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(citDesc) + 0x124) : 0.0f;
    if (s_walk) s_walk(&holderCitizen, 0xffffffff, reinterpret_cast<uint32_t*>(walkOut), 5, walkTime, 1);

    CheckPtr(self);
    int64_t cz = *self;
    *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
    *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
    *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
    if (TraceOn()) {
        static int n = 0;
        if (n < 6) { fprintf(stderr, "[Work] self=%p WalkTowards (walkable=%d)\n",
                             reinterpret_cast<void*>(param_1), (int)any); fflush(stderr); n++; }
    }
    return static_cast<uint64_t>(cz) & 0xffffffffffffff00ULL;
}

