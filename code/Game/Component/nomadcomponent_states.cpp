// code/Game/Component/nomadcomponent_states.cpp
// The NOMAD state machine — one per member of an arriving nomad group. Ported
// 2026-09-05 alongside immigrationcomponent_states.cpp (Immigrants achievement).
//
// NomadComponent (0x78, a ControllerComponent on the nomad template referenced by
// ImmigrationDescription+0x20). Its behaviour is the state table registered once by
// NomadDescription_Setup -> helper_fe70 (0x14004fe70):
//
//   state 0  WALK IN     enter faf0 : MoveTo(meeting point +0x3c)
//                        tick  f9c0 : once the action stack is idle (arrived): if the
//                                     hall reference (+0x28) is gone, destroy self;
//                                     else post event 6 (ARRIVED) to the hall's SM, -> 1
//                        transitions -> {1}          event 1 f4a0 : LEAVE -> 2
//   state 1  LOITER      enter LAB_f9b0 : `mov al,1; ret`
//                        tick  f600 : when idle, count the wander timer (+0x48) down;
//                                     expired -> re-arm 30 + rand*15 s and MoveTo a
//                                     random walkable cell within 8 of the meeting point
//                        transitions -> {2}
//                        event 0 f420 : DESTROY (converted to a citizen)
//                        event 1 f4a0 : LEAVE -> 2
//   state 2  WALK OUT    enter f5e0 : MoveTo(spawn point +0x30)
//                        tick  f530 : once idle (back at the edge), destroy self
//
// "Idle" is CharacterComponent+0x60 == -1: the action-stack top index that
// Pf_ActStackPush_54e0 / WorkBase_TaskListCleanup_c5400 maintain — the walk gate.
//
// Component layout (retail ctor 0x14004fdb0):
//   +0x10 owner entity IBP   +0x18 NomadDescription   +0x28 town-hall entity IBP
//   +0x30 Vector3 spawn point   +0x3c Vector3 meeting point   +0x48 Time wander timer
//   +0x50 int gender/variant    +0x58 embedded state node (node+0x18 = this)
//
// Opt-in with the town hall side: BANISHED_LIVE_IMMIGRATION. Faithful port of
// 0x14004f420 .. 0x14004fb60 and 0x1400c49d0.

#include "engine_funcs.h"
#include "Component/CharacterComponent.h"
#include "Component/ControllerComponent.h"
#include "Action/exe_bodies.h"   // Pf_ActStackPush_54e0, Pf_MoveToCtor_9170, ActHunt_MarkEntityRemoval_c2ea0
#include "Game/exe_bodies.h"     // Reimpl_MapGetTerrainPos
#include "Game/exe_constants.h"  // kRdata_1401721cc
#include "Player/exe_bodies.h"   // PlayerPath_FindComponent_3ea0
#include "Work/exe_bodies.h"     // WorkBase_TaskListCleanup_c5400
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

namespace {

constexpr float kWanderRandRange = 15.0f;   // DAT_1401721bc
constexpr float kWanderJitter    = 0.33f;   // DAT_14017210c
constexpr float kHalfCell        = 0.5f;    // DAT_140172114
constexpr float kMoveSpeed       = -1.0f;   // DAT_140172280 (MoveTo speed arg: default)

// Helpers, called by name (B3R):
//   0x140003ea0 PlayerPath_FindComponent_3ea0 (the StateMachineComponent)
//   0x1400c5400 WorkBase_TaskListCleanup_c5400   0x1400c54e0 Pf_ActStackPush_54e0
//   0x1400c9170 Pf_MoveToCtor_9170               0x1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0
//               (ComponentEntity::MarkForRemoval)
//   0x14009f3e0 Reimpl_MapGetTerrainPos

// The .rdata float constants (Game/exe_constants.h) that are stored as bit patterns.
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

uint32_t TidCharacter() { return CharacterComponent::TypeId(); }


inline uint8_t* GW() { return (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr; }
inline void MakeIBP(void* dst, void* obj) { new (dst) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge

// Entity_FindComponent_Family (0x1400012d0) — the CharacterComponent (DAT_1401c7078).
uintptr_t FindCharacter(void* entity) {
    if (!entity) return 0;
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    uint32_t tid = TidCharacter();
    int n = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* c = *slot;
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() != tid) continue;   // B.3.8 bridge (slot 2)
        c = *slot;
        return ::Object::IsDerivedFromNullSafe(c, tid) ? reinterpret_cast<uintptr_t>(c) : 0;
    }
    return 0;
}

// `*(int*)(chr+0x60) + 1 < 1` — the action stack is empty.
bool Idle(uintptr_t nomad, uintptr_t* chrOut) {
    uintptr_t chr = FindCharacter(*reinterpret_cast<void**>(nomad + 0x10));
    uint64_t ibp = 0; MakeIBP(&ibp, reinterpret_cast<void*>(chr));
    chr = static_cast<uintptr_t>(ibp);
    if (chrOut) *chrOut = chr;
    if (!chr) return false;
    return *reinterpret_cast<int*>(chr + 0x60) + 1 < 1;
}

}  // namespace

// ============================================================
// Nomad_Remove — port of 0x1400c49d0: MarkForRemoval + queue on the entity list's
// removal array (entityList+0xd0/+0xd8), unless already marked (flag 0x400).
// ============================================================
extern "C" void Nomad_Remove(void* entityList, void* entityIBP) {
    uint64_t* ibp = reinterpret_cast<uint64_t*>(entityIBP);
    if (*ibp == 0) return;
    uintptr_t e = static_cast<uintptr_t>(*ibp);
    if ((*reinterpret_cast<uint32_t*>(e + 0x40) >> 10) & 1) return;
    ActHunt_MarkEntityRemoval_c2ea0(reinterpret_cast<void*>(*ibp));   // 0x1400c2ea0
    uint8_t* el = reinterpret_cast<uint8_t*>(entityList);
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>(el + 0xd0)
        .SetSizeFill(*reinterpret_cast<int*>(el + 0xd8) + 1, ibp);
}

// ============================================================
// Nomad_MoveTo — port of 0x14004fb60: clear the character's action stack and push a
// MoveTo(dest) (Slow, mask 0xe1f, default speed).
// ============================================================
extern "C" void Nomad_MoveTo(void* nomadV, float* dest) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(nomadV);
    uintptr_t chr = FindCharacter(*reinterpret_cast<void**>(nomad + 0x10));
    uint64_t chrIBP = 0; MakeIBP(&chrIBP, reinterpret_cast<void*>(chr));
    chr = static_cast<uintptr_t>(chrIBP);
    if (!chr || !fn_MemAlloc) return;
    WorkBase_TaskListCleanup_c5400(reinterpret_cast<void*>(chr + 0x48));   // 0x1400c5400
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x480, 8);
    void* act = nullptr;
    if (mem) {
        uint64_t ownerIBP = chrIBP;
        act = Pf_MoveToCtor_9170(mem, &ownerIBP, 1 /*Slow*/, 0, dest, 0xe1f, 0, kMoveSpeed);
    }
    uint64_t actIBP = 0; MakeIBP(&actIBP, act);
    Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(chr + 0x48), &actIBP);   // 0x1400c54e0
}

// ============================================================
// State handlers (data = the NomadComponent, sm = its StateMachine)
// ============================================================

// state 0 enter 0x14004faf0: walk to the meeting point
extern "C" char Nomad_State0_Enter(void* data, void* /*sm*/) {
    Nomad_MoveTo(data, reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(data) + 0x3c));
    return 1;
}
// state 0 tick 0x14004f9c0: arrived -> tell the hall (event 6) -> state 1
extern "C" char Nomad_State0_Tick(void* data, void* sm, void* /*time*/) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(data);
    if (!Idle(nomad, nullptr)) return 1;
    uint8_t* gw = GW();
    if (*reinterpret_cast<uint64_t*>(nomad + 0x28) == 0) {
        uint64_t ibp = *reinterpret_cast<uint64_t*>(nomad + 0x10);
        if (gw) Nomad_Remove(*reinterpret_cast<void**>(gw + 0xe8), &ibp);
        return 1;
    }
    uint64_t smcIBP = 0;
    PlayerPath_FindComponent_3ea0(static_cast<uintptr_t>(*reinterpret_cast<uint64_t*>(nomad + 0x28)), &smcIBP);   // 0x140003ea0
    uintptr_t smc = static_cast<uintptr_t>(smcIBP);
    if (smc) {
        void* node = *reinterpret_cast<void**>(smc + 0x48);
        if (node) {
            // node = the controller member the StateMachineComponent linked (D6); slot 2.
            reinterpret_cast<ControllerComponent::Controller*>(node)->PostEvent(   // B.3.8 bridge
                reinterpret_cast<void*>(smc + 0x48),
                static_cast<int>(*reinterpret_cast<int8_t*>(smc + 0x51)), 6);
        }
    }
    ::ControllerLive::RequestStateTransition(sm, 1);
    return 1;
}
// event 1 0x14004f4a0: LEAVE -> state 2, forget the hall
extern "C" char Nomad_Event1_Leave(void* data, void* sm) {
    ::ControllerLive::RequestStateTransition(sm, 2);
    uint64_t nullIBP = 0; MakeIBP(&nullIBP, nullptr);
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(data) + 0x28) = nullIBP;
    return 1;
}
// state 1 enter LAB_14004f9b0: `mov al,1; ret`
extern "C" char Nomad_State1_Enter(void* /*data*/, void* /*sm*/) { return 1; }
// state 1 tick 0x14004f600: loiter around the meeting point
extern "C" char Nomad_State1_Tick(void* data, void* /*sm*/, void* time) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(data);
    if (!Idle(nomad, nullptr)) return 1;
    int64_t* timer = reinterpret_cast<int64_t*>(nomad + 0x48);
    *timer -= time ? *reinterpret_cast<int64_t*>(time) : 0;
    if (*timer >= 0) return 1;
    uint8_t* gw = GW();
    if (!gw) return 1;
    void* rng = *reinterpret_cast<void**>(gw + 0xd8);
    float secs = fn_Random_RandFloatTol(rng) * kWanderRandRange +
                 RdataF(kRdata_1401721cc);   // DAT_1401721cc = 30.0f, wander base
    int64_t t = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), secs);
    *timer = t;

    int* grid = *reinterpret_cast<int**>(gw + 0xf8);
    const int W = *grid;
    const float* meet = reinterpret_cast<const float*>(nomad + 0x3c);
    int cx = static_cast<int>(meet[0] + static_cast<float>(W >> 1));
    int cy = static_cast<int>(meet[1] + static_cast<float>(W >> 1));
    if (cx < 0) cx = 0;
    if (cx >= W) cx = W - 1;
    if (cy < 0) cy = 0;
    if (cy >= W) cy = W - 1;
    int centre = W * cy + cx;
    int x0 = centre % W - 8;
    int y0 = centre / W - 8;
    uintptr_t tiles = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
    // Retail grows a DynArray<int> of candidates; a fixed 16x16 buffer holds them all.
    int cand[256];
    int nc = 0;
    for (int ry = 0; ry < 16; ++ry) {
        int yy = y0 + ry;
        for (int rx = 0; rx < 16; ++rx) {
            int xx = x0 + rx;
            if (xx < 0 || xx >= W || yy < 0 || yy >= W) continue;
            int c = yy * W + xx;
            if (*reinterpret_cast<uint8_t*>(tiles + static_cast<uintptr_t>(c) * 0x30 + 0x29) < 3) cand[nc++] = c;
        }
    }
    if (nc != 0) {
        uint32_t pick = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(nc));   // B.3.4b bridge
        float jx = fn_Random_RandFloatTol(rng) * kWanderJitter;   // fVar18
        float jy = fn_Random_RandFloatTol(rng) * kWanderJitter;   // fVar19
        int c = cand[pick];
        float half = static_cast<float>(W >> 1) - kHalfCell;
        float cellPos[3] = { static_cast<float>(c % W) - half, static_cast<float>(c / W) - half, 0.0f };
        float pos[3] = {0, 0, 0};
        Reimpl_MapGetTerrainPos(grid, pos, cellPos, '\0', nullptr);
        float dest[3] = { jy + pos[0], jx + pos[1], pos[2] + 0.0f };
        Nomad_MoveTo(data, dest);
    }
    return 1;
}
// event 0 0x14004f420: DESTROY (the nomad became a citizen)
extern "C" char Nomad_Event0_Destroy(void* data, void* /*sm*/) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = GW();
    uint64_t ibp = *reinterpret_cast<uint64_t*>(nomad + 0x10);
    if (gw && ibp) Nomad_Remove(*reinterpret_cast<void**>(gw + 0xe8), &ibp);
    return 1;
}
// state 2 enter 0x14004f5e0: walk back to the spawn point
extern "C" char Nomad_State2_Enter(void* data, void* /*sm*/) {
    Nomad_MoveTo(data, reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(data) + 0x30));
    return 1;
}
// state 2 tick 0x14004f530: back at the edge -> destroy
extern "C" char Nomad_State2_Tick(void* data, void* /*sm*/, void* /*time*/) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(data);
    if (!Idle(nomad, nullptr)) return 1;
    uint8_t* gw = GW();
    uint64_t ibp = *reinterpret_cast<uint64_t*>(nomad + 0x10);
    if (gw && ibp) Nomad_Remove(*reinterpret_cast<void**>(gw + 0xe8), &ibp);
    return 1;
}

// ============================================================
// Dispatcher + table (replicates NomadDescription_helper_fe70, 0x14004fe70)
// ============================================================
// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<NomadCtrlTag>, NomadComponent.h).
#include "Component/NomadComponent.h"
using NomadDispatch = ::ControllerLive::Dispatcher<NomadCtrlTag>;

extern "C" void NomadCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = NomadDispatch::table;
    if (t.count != 0) return;
    TableRegisterState(t, 0, reinterpret_cast<void*>(&Nomad_State0_Enter), nullptr,
                       reinterpret_cast<void*>(&Nomad_State0_Tick), nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddEvent(t, 1, reinterpret_cast<void*>(&Nomad_Event1_Leave));

    TableRegisterState(t, 1, reinterpret_cast<void*>(&Nomad_State1_Enter), nullptr,
                       reinterpret_cast<void*>(&Nomad_State1_Tick), nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 0, reinterpret_cast<void*>(&Nomad_Event0_Destroy));
    TableAddEvent(t, 1, reinterpret_cast<void*>(&Nomad_Event1_Leave));

    TableRegisterState(t, 2, reinterpret_cast<void*>(&Nomad_State2_Enter), nullptr,
                       reinterpret_cast<void*>(&Nomad_State2_Tick), nullptr);
}
