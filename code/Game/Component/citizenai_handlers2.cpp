// code/Game/Component/citizenai_handlers2.cpp
//
// Brick 3b handler wave (part 2) — faithful 1:1 C++ ports of three CitizenAIComponent
// STATE HANDLERS that ValidateAndEnterState (citizenai_candidatelist.cpp) dispatches.
// Each makes its state ENTERABLE by building + pushing an Action/Work sequence onto the
// citizen action stack (CharacterComponent+0x48), exactly like the already-ported Wander
// handler (citizenai_handlers.cpp). Transcribed from the Ghidra decompile and disasm-
// verified offset-by-offset.
//
// Handlers ported here (all closure-ready — every exe sub-callee is REGISTERED):
//   0x14010c360  CitizenAI_State_Education      (case 3) -> PerformWork_Ctor10 + dd30 + 7fb0
//   0x14010c580  CitizenAI_State_SeekHappiness  (case 8) -> PerformWork_Ctor5  + dd30 + 7fb0
//                  + (no-target fallback) the MeshInstance "go invisible" render branch
//   0x14010d560  CitizenAI_State_FindWork       (case 10)-> WalkTowards + 122be0/122b00 + 115a50
//
// HANDLERS NOT PORTED IN *THIS* FILE — they live in citizenai_handlers3.cpp.
// (This list used to say "blocked on UNregistered deep helpers"; that stopped being
// true when reg_citizenai_findhelpers.cpp landed db20/e370/ec70/2ac30, and the stale
// wording survived long enough to be filed as PLAN.md §1.2 item 2 "unported". They
// are ported, registered and observed entering in-game — see
// docs/waves/exe_closeout/lane_c.md.)
//   0x14010d790  State_Work           -> citizenai_handlers3.cpp (helper db20 @ 0x14010db20)
//   0x14010cd80  State_AdultTransition-> citizenai_handlers3.cpp (e370 @ 0x14008e370 + 2ac30)
//   0x14010d2c0  State_ElderTransition-> citizenai_handlers3.cpp (ec70 @ 0x14008ec70 + 2ac30)
//
// self chain (all handlers): param = CitizenAIComponent ; self = param+0x38 (sibling
//   CitizenComponent IBP) ; *self = CitizenComponent. char = *(citizen+0x278). The action
//   builders take (CitizenComponent-IBP, ...) and push internally.
//
// Registered exe sub-callees (resolved via ResolveAddr — bodies NOT duplicated here):
//   0x14008dd30 CitizenAI_FindBestTarget_dd30   (citizenai_findtarget.cpp)
//   0x140087fb0 Pf_EntityAnchorCell_7fb0        (reg_pf_road.cpp)
//   0x1400bccb0 PerformWork_Action_Ctor10       (citizenai_actionctors.cpp)
//   0x1400bd070 PerformWork_Action_Ctor5        (citizenai_actionctors.cpp)
//   0x1400bb490 Pathfinding_WalkTowards         (citizenai_actionctors.cpp)
//   0x140002f90 Entity_FindComponent_MeshInstance (WorkBase_FindComponent_2f90)
//   0x140002ed0 Entity_FindComponent_Position   (WorkBase_FindComponent_2ed0)
//   0x140122be0 FUN_140122be0 walkable-pos       (WorkImpl_WalkablePos_122be0)
//   0x140122b00 FUN_140122b00 walkable-pos recur (WorkImpl_WalkablePosRecursive_122b00)
//   0x140115a50 FUN_140115a50                    (AttendanceComponent_ReleasePartnerSlot)
//   0x1400c3470 FUN_1400c3470 notify-entity      (ActHunt_NotifyEntityChanged_c3470)
//
// Engine helpers (engine_funcs.h): fn_InternalBasePtr_CheckPtr (== _slot in decompiles),
//   fn_Time_Ctor_Float, fn_IsDerivedFrom, fn_Graphics_Scene_AddObject, fn_SceneRemoveObject,
//   g_pGameWorldSingleton, g_pGameSettingsPtr.
//
// SELF-CONTAINED-BRICK type-ids: DAT_1401c7938 ("InteractDescription") and
//   DWORD_1401c7190 ("CitizenDescription") are UNFILLED (0xFFFFFFFF) in standalone, so the
//   original's IsDerivedFrom scans would miss and then dereference NULL (+0x28 / +0x120).
//   Resolved self-contained via Object::GetTypeIdByName, exactly like citizenai_findtarget.cpp.
//   TypeNames recovered from each global's *_Register [DATA] writer (Ghidra-verified):
//     DAT_1401c7938 -> InteractDescription_Register  -> "InteractDescription"
//     DWORD_1401c7190 -> CitizenDescription_Register -> "CitizenDescription"
//
// .rdata float const: DAT_1401721fc = 75.0 (the per-state cooldown-stamp seconds; written to
//   the CitizenAIComponent state-timestamp field at the top of Education/SeekHappiness).
//


// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/InteractDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "graphics/Mesh.h"
#include "Component/Component.h"   // B.3.x W0: Component slots by name
#include "engine_funcs.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "scene_watch.h"
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_constants.h"

namespace {

// --- registered exe sub-callees (resolved lazily via ResolveAddr) ---
// dd30: (param_1 listpair, &outBest, goalCellA, goalCellB, &outFoundComp, outVec3, category)
// 7fb0: (InternalBasePtr* ibp, float* outVec3orNull) -> cell idx
// Ctor10: (CitizenComp-IBP, &srcEntityIBP, float* dest, entity-list object)
// Ctor5: (CitizenComp-IBP, &srcEntityIBP, float* dest, entity-list object)
// WalkTowards: (CitizenComp-IBP, u32 completeArg, float* dest, u32 msgA4, float time, u32 flag)
// FindComponent variants: (entity, outIBP) -> outIBP
// 122be0: (entity, transformSub, &posVec3, outVec3, &bestCost) -> bool

// 122b00: (child, 0, &posVec3, outVec3, &bestCost) -> byte
// 115a50: (CitizenComponent, char) -> bool
// c3470: (MeshInstance) -> void


// --- self-resolved type-ids (Brick pattern) ---
static uint32_t s_tid_InteractDescription = 0;   // DAT_1401c7938
static uint32_t s_tid_CitizenDescription  = 0;   // DWORD_1401c7190


static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    if (s_tid_InteractDescription == 0)
        s_tid_InteractDescription = InteractDescription::TypeId();
    if (s_tid_CitizenDescription == 0)
        s_tid_CitizenDescription = CitizenDescription::TypeId();
    s_init = true;
}


} // namespace

// ============================================================
// 0x14010c360 — CitizenAI_State_Education  (state id 3)
// ------------------------------------------------------------
// Stamp cooldown (Time(75.0) -> param_1+0x58). self=param_1+0x38. Early-return if the
// citizen already has an education record (*self+0x1e4 != 0). Otherwise find the best
// education target via dd30 (goal cell = citizen ModelComponent position, B-goal = the
// workplace anchor cell or -1). If none found: clear edu state, set flag bit, return 0.
// If found: build the Ctor10 work sequence toward it and set state flags; return 1.
// ============================================================
extern "C" uint64_t CitizenAI_State_Education(int64_t param_1) {
    EnsureInit();

    // cooldown stamp: Time(DAT_1721fc) -> param_1+0x58 (the first 8 bytes of the Time)
    int64_t timeStamp = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeStamp), kRdata_1401721fc);
    *reinterpret_cast<int64_t*>(param_1 + 0x58) = timeStamp;

    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    if (*reinterpret_cast<int*>(*self + 0x1e4) != 0) {
        return static_cast<uint64_t>(*self) & 0xffffffffffffff00ULL;  // low byte = 0
    }

    int64_t local_res8 = 0;            // dd30 out: found component (also Ctor10 param_4)
    float dest[3] = { 0.0f, 0.0f, 0.0f };

    // citizen ModelComponent world pos -> grid cell
    int64_t cz = *self;
    int64_t ch = *reinterpret_cast<int64_t*>(cz + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);
    int mapSize = **reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    int half = mapSize >> 1;
    int cx = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(half));
    int cy = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(half));
    if (cx < 0) cx = 0;
    if (mapSize <= cx) cx = mapSize - 1;
    if (cy < 0) cy = 0;
    if (mapSize <= cy) cy = mapSize - 1;

    // workplace anchor cell (B-goal) or -1
    uint32_t bGoal;
    if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
        bGoal = 0xffffffff;
    } else {
        int64_t wpIBP = *reinterpret_cast<int64_t*>(*self + 0x238);
        bGoal = Pf_EntityAnchorCell_7fb0(&wpIBP, nullptr);
    }

    int64_t outBest = 0;
    int64_t* listpair = *reinterpret_cast<int64_t**>(
        reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf0);
    CitizenAI_FindBestTarget_dd30(listpair, &outBest, static_cast<uint32_t>(mapSize * cy + cx), bGoal,
                  &local_res8, dest, 4);

    if (outBest == 0) {
        *reinterpret_cast<int*>(*self + 0x1e0) = 0;
        int64_t cz2 = *self;
        *reinterpret_cast<uint32_t*>(cz2 + 0x88) |= 4;
        return static_cast<uint64_t>(cz2) & 0xffffffffffffff00ULL;  // low byte 0
    }

    // found: Ctor10(self, &foundEntity, dest, foundComponent)
    uint64_t foundEntity = static_cast<uint64_t>(outBest);
    int64_t ibpHolder = *self;   // CitizenComponent ptr in a stack IBP holder
    CitizenAI_PerformWorkCtor10_bccb0(&ibpHolder, &foundEntity, dest, static_cast<uint64_t>(local_res8));

    int64_t cz3 = *self;
    *reinterpret_cast<uint32_t*>(cz3 + 0x54) |= 4;
    *reinterpret_cast<int32_t*>(cz3 + 0xb0) = 0;
    *reinterpret_cast<int32_t*>(cz3 + 0x30) = static_cast<int32_t>(0xffffffff);

    return 1;  // CONCAT71(.., 1) -> low byte 1
}

// ============================================================
// 0x14010c580 — CitizenAI_State_SeekHappiness  (state id 8)
// ------------------------------------------------------------
// Stamp cooldown (Time(75.0) -> param_1+0x80). Early-return if happiness already satisfied
// (*self+0xac > 0). Find best happiness target via dd30 (category 2). If found: build the
// Ctor5 work sequence and set state flags (return 1). If NOT found: clear happiness state,
// set flag bit, and run the MeshInstance "go invisible" render branch (faithful inline copy
// of FUN_14012c3f0's value=1/flagIndex=4 path), then return 0.
// ============================================================
extern "C" uint64_t CitizenAI_State_SeekHappiness(int64_t param_1) {
    EnsureInit();

    int64_t timeStamp = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeStamp), kRdata_1401721fc);
    *reinterpret_cast<int64_t*>(param_1 + 0x80) = timeStamp;

    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    if (0 < *reinterpret_cast<int*>(*self + 0xac)) {
        return static_cast<uint64_t>(*self) & 0xffffffffffffff00ULL;
    }

    int64_t local_res8 = 0;            // dd30 out: found component (Ctor5 param_4)
    float dest[3] = { 0.0f, 0.0f, 0.0f };

    int64_t cz = *self;
    int64_t ch = *reinterpret_cast<int64_t*>(cz + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);
    int mapSize = **reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    int half = mapSize >> 1;
    int cx = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(half));
    int cy = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(half));
    if (cx < 0) cx = 0;
    if (mapSize <= cx) cx = mapSize - 1;
    if (cy < 0) cy = 0;
    if (mapSize <= cy) cy = mapSize - 1;

    uint32_t bGoal;
    if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
        bGoal = 0xffffffff;
    } else {
        int64_t wpIBP = *reinterpret_cast<int64_t*>(*self + 0x238);
        bGoal = Pf_EntityAnchorCell_7fb0(&wpIBP, nullptr);
    }

    int64_t outBest = 0;
    int64_t* listpair = *reinterpret_cast<int64_t**>(
        reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf0);
    CitizenAI_FindBestTarget_dd30(listpair, &outBest, static_cast<uint32_t>(mapSize * cy + cx), bGoal,
                  &local_res8, dest, 2);

    if (outBest == 0) {
        // --- no happiness target: clear state + run the MeshInstance "hide" render branch ---
        *reinterpret_cast<int*>(*self + 0xa8) = 0;
        *reinterpret_cast<uint32_t*>(*self + 0x88) |= 4;

        int64_t entityIBP = *reinterpret_cast<int64_t*>(*self + 0x10);
        int64_t meshOutIBP = 0;
        void* meshRet = WorkBase_FindComponent_2f90(static_cast<uintptr_t>(entityIBP), &meshOutIBP);
        uint64_t retLow = 0;
        if (meshRet) {
            int64_t mi = *reinterpret_cast<int64_t*>(meshRet);   // MeshInstance object (plVar3)
            if (mi) {
                int64_t inner = *reinterpret_cast<int64_t*>(mi + 0x10);
                retLow = static_cast<uint64_t>(mi);
                // gated by ((inner+0x40 >> 10) & 1) == 0  (disasm: SHR 0xa; TEST CL,1).
                // inner is just-CheckPtr'd by the original (never 0 with valid data); guard
                // it to stay crash-safe (skip rather than fault, matching nothing-to-do).
                if (inner != 0 && (*reinterpret_cast<uint32_t*>(inner + 0x40) >> 10 & 1) == 0) {
                    // OR [mi+0x28 + 0xc8], 0x10   == set status-mask bit 4 at mi+0xf0
                    *reinterpret_cast<uint32_t*>(mi + 0xf0) |= 0x10;
                    uint32_t changed;
                    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
                    if (*reinterpret_cast<int*>(mi + 0xf0) == 0) {
                        // mask empty -> remove from scene
                        if (-1 < *reinterpret_cast<int32_t*>(mi + 0x100)) {
                            reinterpret_cast<Graphics::Scene*>(*reinterpret_cast<void**>(gw + 0x60))
                                ->RemoveObject(*reinterpret_cast<uint32_t*>(mi + 0x100));   // B.3.5 bridge
                            *reinterpret_cast<int32_t*>(mi + 0x100) = static_cast<int32_t>(0xffffffff);
                        }
                        uint32_t before = *reinterpret_cast<uint32_t*>(mi + 0x20);
                        uint32_t after = before & 0xfffffffb;
                        *reinterpret_cast<uint32_t*>(mi + 0x20) = after;
                        changed = after ^ before;
                    } else {
                        // mask non-empty -> ensure in scene + run vtable+0x40 update
                        int32_t* settings = g_gameSettings;
                        if (settings && reinterpret_cast<char*>(settings)[0xc] != '\0' &&
                            *reinterpret_cast<int32_t*>(mi + 0x100) < 0) {
                            uint32_t id = reinterpret_cast<Graphics::Scene*>(*reinterpret_cast<void**>(gw + 0x60))
                                ->AddObject(reinterpret_cast<Graphics::Geometry*>(mi + 0x28));   // B.3.5 bridge
                            *reinterpret_cast<uint32_t*>(mi + 0x100) = id;
                            SceneWatch_Note(id, reinterpret_cast<void*>(mi + 0x28), "CitizenAI::handlers2 carry-mesh");
                        }
                        int64_t t0 = 0;
                        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t0), 0.0f);
                        // `mi` is a Component (the +0x10 entity / +0x20 flag-notify idiom below).
                        reinterpret_cast<Component*>(mi)->Tick(&t0);   // B.3.x bridge (slot 8)
                        uint32_t before = *reinterpret_cast<uint32_t*>(mi + 0x20);
                        uint32_t after = before | 4;
                        *reinterpret_cast<uint32_t*>(mi + 0x20) = after;
                        changed = after ^ before;
                    }
                    retLow = static_cast<uint64_t>(changed & 0xffff);
                    if ((changed & 0xffff) != 0 && *reinterpret_cast<int64_t*>(mi + 0x10) != 0) {
                        int64_t notifyTarget = *reinterpret_cast<int64_t*>(mi + 0x10);
                        ActHunt_NotifyEntityChanged_c3470(reinterpret_cast<void*>(notifyTarget));
                        retLow = 0;  // c3470 returns void; original tail value is its (void) ret
                    }
                }
            }
        }
        return retLow & 0xffffffffffffff00ULL;  // low byte 0
    }

    // found: Ctor5(self, &foundEntity, dest, foundComponent)
    uint64_t foundEntity = static_cast<uint64_t>(outBest);
    int64_t ibpHolder = *self;
    CitizenAI_PerformWorkCtor5_bd070(&ibpHolder, &foundEntity, dest, local_res8);

    int64_t cz3 = *self;
    *reinterpret_cast<uint32_t*>(cz3 + 0x54) |= 4;
    *reinterpret_cast<int32_t*>(cz3 + 0xb0) = 0;
    *reinterpret_cast<int32_t*>(cz3 + 0x30) = static_cast<int32_t>(0xffffffff);

    return 1;
}

// ============================================================
// 0x14010d560 — CitizenAI_State_FindWork  (state id 10)
// ------------------------------------------------------------
// (elder citizen path) Resolve the citizen's anchor entity (*self+0x208 -> +0x10), find its
// Position component, read the citizen ModelComponent world pos, run the walkable-position
// search (122be0 over the InteractDescription transform sub + 122b00 over each child). If a
// walkable target is found (bVar3 != 0): push the WalkTowards action toward it (time read
// from the citizen's CitizenDescription +0x120) and set state flags (return 1). Else:
// FUN_140115a50(self, 1) (release/reroll the partner slot) and return 0.
// ============================================================
extern "C" uint64_t CitizenAI_State_FindWork(int64_t param_1) {
    EnsureInit();

    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    int64_t anchor = *reinterpret_cast<int64_t*>(*self + 0x208);
    int64_t anchorEntity = *reinterpret_cast<int64_t*>(anchor + 0x10);
    int64_t posIBP[2] = { 0, 0 };
    WorkBase_FindComponent_2ed0(static_cast<uintptr_t>(anchorEntity), posIBP);

    float outVec[3] = { 0.0f, 0.0f, 0.0f };   // local_58 (target out)

    // citizen ModelComponent world pos -> local_48 vec3 (x,y,z packed)
    int64_t cz = *self;
    int64_t ch = *reinterpret_cast<int64_t*>(cz + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);
    float pos[3];
    pos[0] = *reinterpret_cast<float*>(model + 0x158);
    pos[1] = *reinterpret_cast<float*>(model + 0x15c);
    pos[2] = *reinterpret_cast<float*>(model + 0x160);

    int bestCost = 0x7fffffff;            // local_res8 low 32

    // transform sub-object on the Position component (posIBP[0]+0x18), InteractDescription.
    int64_t posComp = posIBP[0];
    void* sub = *reinterpret_cast<void**>(posComp + 0x18);
    void* transform = (sub && Object::IsDerivedFromNullSafe(sub, s_tid_InteractDescription))
                      ? sub : nullptr;
    uint8_t found = 0;
    found = WorkImpl_WalkablePos_122be0(static_cast<uintptr_t>(posComp),
                            reinterpret_cast<uintptr_t>(transform) + 0x28, pos, outVec, &bestCost);

    // walk each child component (+0x28 array, count +0x30)
    int childCount = *reinterpret_cast<int*>(posComp + 0x30);
    if (0 < childCount) {
        int i = 0; int64_t off = 0;
        do {
            int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(posComp + 0x28) + off);
            uint8_t r = WorkImpl_WalkablePosRecursive_122b00(static_cast<uintptr_t>(*slot), 0, pos, outVec, &bestCost);
            found |= r;
            i++; off += 8;
        } while (i < *reinterpret_cast<int*>(posComp + 0x30));
    }

    if (found != 0) {
        // WalkTowards toward outVec; time = citizen CitizenDescription +0x120
        void* descRaw = *reinterpret_cast<void**>(*self + 0x18);
        void* desc = (descRaw && Object::IsDerivedFromNullSafe(descRaw, s_tid_CitizenDescription))
                     ? descRaw : nullptr;
        int64_t ibpHolder = *self;
        // [STANDALONE GUARD] the desc null-check is ours; the original derefs +0x120
        // unconditionally (would fault if the description type-check failed).
        float walkTime = desc ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x120) : 0.0f;
        CitizenAI_Pathfinding_WalkTowards_bb490(&ibpHolder, 0xffffffff, reinterpret_cast<uint32_t*>(outVec), 4, walkTime, 1);

        int64_t cz2 = *self;
        *reinterpret_cast<uint32_t*>(cz2 + 0x54) |= 4;
        *reinterpret_cast<int32_t*>(cz2 + 0x30) = static_cast<int32_t>(0xffffffff);
        *reinterpret_cast<int32_t*>(cz2 + 0xb0) = 0;

        return 1;
    }

    // 0x140115a50 returns nothing; the only caller (CitizenAI_ValidateAndEnterState)
    // reads the low byte, which is 0.
    AttendanceComponent_ReleasePartnerSlot(reinterpret_cast<void*>(*self), '\x01');
    uint64_t r = 0;
    return r & 0xffffffffffffff00ULL;  // low byte 0
}

