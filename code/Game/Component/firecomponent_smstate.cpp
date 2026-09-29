// code/Game/Component/firecomponent_smstate.cpp
// The two "force the building's StateMachineComponent into state N" helpers that
// FireComponent::Ignite / ::Extinguish drive, plus the demolition gate they check.
//
//   0x14011e690  FireComponent_IsBeingDemolished  — abort gate for Ignite
//   0x14011e7b0  FireComponent_helper_e7b0        — begin burning  (SM state 3)
//   0x14011ea10  FireComponent_helper_ea10        — stop burning   (SM state 7)
//
// ⚠ SHARED WITH THE TORNADO LANE. The tornado building-damage arm reaches
// 0x14011e7b0 / 0x14011ea10 as well. They are ported ONCE,
// here, and registered once (standalone/reg_fire.cpp) so both lanes dispatch the
// same body — the "one retail fn ported twice, each copy faithful in a different
// half" failure mode this project has already been bitten by.
//
// ── What the two helpers actually are ────────────────────────────────────────
// Ghidra renders both as ~90 lines of nested loops. They are NOT bespoke: the
// whole tail is StateMachine::Tick (0x1400a9660) INLINED. Verified field by field
// against our own port (StateMachine_TickReimpl, statemachinecomponent.cpp:448)
// with `sm = SMC + 0x48`:
//     Tick sm[0]        == *(void**)(SMC+0x48)   active state node
//     Tick (char)sm[1]  == *(int8*)(SMC+0x50)    current state
//     Tick *(sm+9)      == *(int8*)(SMC+0x51)    active index
//     Tick *(sm+10)     == *(int8*)(SMC+0x52)    pending state (0xff = none)
//     Tick *(sm+0xb)    == *(int8*)(SMC+0x53)    transition index
//     Tick param3       == 0   (the slot +0x30 Transition arm is the one taken)
//     Tick param4       == 1   (bVar2 starts true and is cleared after one pass)
// Entry test, both inner loop exits and the outer loop condition all match
// exactly. So the tail is precisely `StateMachine::Tick(SMC+0x48, Time(0), 0, 1)`.
//
// The head is the state request:
//     out = N
//     if ((int8)SMC[0x51] < 0) StateMachine::Tick(SMC+0x48, Time(0), 0, 0)
//     r = node->vtable[1](node, (int)(int8)SMC[0x51], &out)   // find transition to N
//     if (r >= 0) { SMC[0x53] = (int8)r; SMC[0x52] = (int8)out; }
//
// Decompiled READ-ONLY from Application-x64.exe.

#include "system/Value.h"
#include <cstdint>
#include "Component/firecomponent_funcs.h"
#include "Component/controller_state_table.h"
#include "Player/exe_bodies.h"
#include "Dialog/dialog_exe_helpers.h"
#include "Component/DestroyComponent.h"
#include "Component/BuildComponent.h"
#include "Component/StateMachineComponent.h"
#include "Entity/ComponentEntity.h"

namespace {

// Called directly: FUN_140003ea0 finds the entity's StateMachineComponent
// (PlayerPath_FindComponent_3ea0); FUN_14000d6c0 finds the entity's BuildComponent
// (Dialog_FindComponent_d6c0 — DAT_1401c7008 is the BuildComponent id);
// StateMachine::Tick 0x1400a9660 is StateMachine_TickReimpl.

// ---------------------------------------------------------------------------
// The shared body of e7b0 and ea10: request state `targetState` on the entity's
// StateMachineComponent and pump the machine until it settles.
// `owner` is the component's +0x10 entity. The StateMachine sub-object at
// StateMachineComponent +0x48..+0x53 is raw bytes in COMP-e's header: LEFT.
// ---------------------------------------------------------------------------
void ForceSMState(ComponentEntity* owner, int targetState, const char* /*who*/) {
    uint8_t* L = reinterpret_cast<uint8_t*>(
        owner->FindComponent<StateMachineComponent>());   // 0x140003ea0
    if (!L) {
        return;
    }

    void*  sm   = L + 0x48;                      // the StateMachine sub-object
    void** node = reinterpret_cast<void**>(sm);  // sm[0] = the active state node

    // STANDALONE DIVERGENCE (deliberate, same one statemachinecomponent.cpp:558
    // already carries): retail always has a linked state-node chain here, so it
    // derefs the node unguarded. In standalone a component whose ControllerComponent
    // is still inert has an EMPTY chain (sm[0] == 0) and the unguarded deref is a
    // null crash that says nothing. Report the outcome and leave, rather than
    // pretending the state was requested.
    if (*node == nullptr) {
        return;
    }

    int32_t out = targetState;

    // Settle a half-finished transition before asking for a new state.
    if (static_cast<int8_t>(L[0x51]) < 0) {
        ::System::Time t0 = {0};
        RtSystem_Time_Ctor(&t0, 0.0f);
        StateMachine_TickReimpl(sm, t0._ticks, 0, 0);
    }

    // node->vtable[1](node, currentIndex, &targetState) -> transition index (or < 0)
    typedef int (*fn_FindTrans_t)(void* node, int curIdx, int32_t* pTarget);
    void*  n  = *node;
    void** vt = *reinterpret_cast<void***>(n);
    int r = reinterpret_cast<fn_FindTrans_t>(vt[1])(
        n, static_cast<int>(static_cast<int8_t>(L[0x51])), &out);
    if (r >= 0) {
        L[0x53] = static_cast<uint8_t>(r);
        L[0x52] = static_cast<uint8_t>(out);
    }


    // The inlined tail == StateMachine::Tick(sm, Time(0), useAlt=0, doTransition=1).
    ::System::Time tZero = {0};
    RtSystem_Time_Ctor(&tZero, 0.0f);
    StateMachine_TickReimpl(sm, tZero._ticks, 0, 1);
}

}  // namespace

// ============================================================
// FUN_14011e690 — "is this building already being demolished / reclaimed?"
// ============================================================
// param_1 = the DestroyComponent. Finds the entity's StateMachineComponent and
// returns true when the effective state (pending +0x52 if >= 0, else current
// +0x50) is 6 or 7. Ignite aborts on true: a building on its way down does not
// catch fire.
bool DestroyComponent::IsBeingDemolished() {
    uint8_t* L = reinterpret_cast<uint8_t*>(
        _entity->FindComponent<StateMachineComponent>());   // 0x140003ea0
    if (!L) return false;   // standalone: no SMC => not demolishing
    int8_t s = static_cast<int8_t>(L[0x52]);
    if (s < 0) s = static_cast<int8_t>(L[0x50]);
    if (s == 6) return true;
    s = static_cast<int8_t>(L[0x52]);
    if (s < 0) s = static_cast<int8_t>(L[0x50]);
    return s == 7;
}

// ============================================================
// FUN_14011e7b0 — begin burning.
// ============================================================
// param_1 = the DestroyComponent, param_2 = the (already halved) burn Time.
// Returns false — without touching anything — when the building is not finished
// (its BuildComponent's +0x48 progress word is still 0). Otherwise stamps the
// DestroyComponent's burning flag (+0x50 = 1) and burn duration (+0x48), then
// drives the entity's state machine to state 3 (BURNING).
bool DestroyComponent::BeginBurning(const System::Time* time) {
    // Unfinished-construction gate.
    {
        BuildComponent* build = _entity->FindComponent<BuildComponent>();   // 0x14000d6c0
        if (build != nullptr) {
            if (build->laborProgress == 0) {
                return false;
            }
        }
    }

    burning = true;
    burnTime._ticks = time ? time->_ticks : 0;

    ForceSMState(_entity, 3, "begin");
    return true;
}

// ============================================================
// FUN_14011ea10 — stop burning.
// ============================================================
// param_1 = the DestroyComponent. Drives the entity's state machine to state 7.
// Note it does NOT clear +0x50 / +0x48; the state-7 handler owns that.
void DestroyComponent::StopBurning() {
    ForceSMState(_entity, 7, "stop");
}

// ---- B4 FORWARD: 1 outside caller file (Component/tornadocomponent.cpp); the
// closing wave rewires it. The null tests are the tree's (kept, B9).
extern "C" bool FireComponent_IsBeingDemolished_11e690(void* destroyComp) {
    if (!destroyComp) return false;
    return reinterpret_cast<DestroyComponent*>(destroyComp)->IsBeingDemolished();
}
extern "C" bool FireComponent_BeginBurning_11e7b0(void* destroyComp, const int64_t* time) {
    if (!destroyComp) return false;
    return reinterpret_cast<DestroyComponent*>(destroyComp)->BeginBurning(
        reinterpret_cast<const System::Time*>(time));
}
extern "C" void FireComponent_StopBurning_11ea10(void* destroyComp) {
    if (!destroyComp) return;
    reinterpret_cast<DestroyComponent*>(destroyComp)->StopBurning();
}
