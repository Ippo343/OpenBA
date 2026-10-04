// code/Game/Component/destroycomponent_workerrelease.cpp
// The two WorkPlaceComponent transitions the DestroyComponent state machine drives when
// a building is demolished or upgraded. Both were logged as NOT-IMPLEMENTED and skipped,
// which is what leaves a half-torn-down worker roster behind an in-flight upgrade.
//
// ============================================================
// What the pair actually does in retail (Ghidra-verified)
// ============================================================
// A building's WorkPlaceComponent carries ONE profession key at +0x48. During construction
// (and demolition, which is also builder labour) that key is the BUILDER key, read from
// *(gameWorld+0xb0)+0x38. Once the building is finished it holds the key its own
// WorkPlaceDescription declares (desc+0x20 — zero for a house). The town's profession
// registry (townMgr+0x100 AVL BST, townMgr = *(gameWorld+0xf0)) buckets workplaces BY that
// key, so every key change is a paired unregister/register.
//
//   FUN_14012f520  ENTER the builder profession ("state-4 workplace re-seat").
//       Release every seated worker (ee60), re-key any pending work item from the old
//       profession to Builder (f8400 — unported, see below), set +0x48 = Builder, add the
//       owner entity to the Builder bucket (0x1400895a0), recompute the worker capacity
//       (from the WorkComponent description, or from the position child count once the
//       entity has a BuildComponent), publish it and evict any excess (fb00), then
//       SetAccepting(true) (0x14012fc00) so the demolition site takes builders.
//
//   FUN_14012f670  LEAVE the builder profession ("worker release").
//       The exact inverse, gated on "+0x48 is CURRENTLY the Builder key": remove the owner
//       from the Builder bucket, refresh the WorkComponent capacity, hand any pending work
//       over to the natural profession (f8400), reset +0x48 to desc+0x20, and release every
//       seated worker (ee60).
//
// f670 was ALREADY ported — workplacecomponent.cpp WorkPlaceComponent_LeaveBuilderProfession,
// written for the build-COMPLETION hand-off (BuildComponent_State8Finalize ->
// WorkPlaceComponent_EnterBuiltState). It was simply never registered at its exe address, so
// the DestroySM's ResolveAddr(Module::Exe, 0x14012f670) handed back 0 and the call was
// skipped. It is called by name now; there is no second copy.
// (Same class of miss as commit 330710b, WorkPlaceComponent::SetAccepting.)
//
// f520 is new and lives below.
//
// ============================================================
// Call sites
// ============================================================
//   retail DestroyDescription_helper_cfd0 (0x14011cfd0, state-4 enter) -> f520 at 0x14011d02f
//     on FUN_140002e10(ownerEntity) — ported at destroycomponent_states.cpp
//     DestroyComponent::State4Enter.
//   retail DestroyDescription_helper_dd00_sub_bcc0 / _sub_c120 (state-5 tick, the
//     replacement spawn) -> f670 — ported at destroycomponent_states.cpp
//     DestroyComponent::State5Tick.
//   (retail also reaches them from the WorkPlaceComponent controller's own state table,
//   WorkPlaceDescription_helper_ee40 / _edf0 — our WorkPlaceComponent has no live
//   controller node, so those two run from BuildComponent_State8Finalize instead.)
//
// ============================================================
// Full closure
// ============================================================
// FUN_1400f8400 — the work-scheduler re-key (move the pending task node from the old
// profession bucket to the new one inside sched+0x120, keyed by the work id) — is now
// ported and registered: citizenai_work_scheduler.cpp, CitizenAI_WorkScheduler_Rekey. It is
// reachable only when the WorkComponent still holds a work id (+0x28 >= 0).

#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include <cstdio>
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Dialog/dialog_exe_helpers.h"
#include "Tutorial/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/ProfessionList.h"
#include "Game/Profession.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/WorkComponent.h"
#include "Component/BuildComponent.h"
#include "Component/CitizenComponent.h"
#include "Entity/ComponentEntity.h"

// ============================================================
// Closure, called directly
// ============================================================
//   0x14012ee60 release loop              WorkPlaceComponent_OnCreateInit
//   0x140003050 WorkComponent finder      WorkBase_FindComponent_3050
//   0x14000d6c0 BuildComponent finder     Dialog_FindComponent_d6c0 (type 1401c7008)
//   0x140002ed0 PositionComponent finder  WorkBase_FindComponent_2ed0
//   0x14012e5c0 WorkComponent_CalcMaxWorkers, 0x140123070 Pf_RecursiveChildSum_123070,
//   0x1400895a0 profession-bucket insert  ResidenceComponent_impl_95a0
//   0x14012fc00 WorkPlaceComponent_SetAccepting
//   0x14010ffc0 / 0x140110470 workplace finders  CitizenComp_sub_ffc0 / CitizenComp_sub_0470
//   0x140111ee0 CitizenComp_AssignHousing, 0x1400f8400 CitizenAI_WorkScheduler_Rekey

// workplacecomponent.cpp — roster-only mode for the Attach/Detach helpers: do the list
// mutation, skip the gameplay-time Scene mesh-visual toggle. See the long note there; the
// short version is that our inert StatusIconComponents are zero-filled, so a never-
// registered icon reads scene id 0 and Graphics::Scene::RemoveObject(scene, 0) spins forever.
extern bool g_wpcRosterOnly;

static bool Plausible(const void* p) {
    return reinterpret_cast<uintptr_t>(p) >= 0x10000;   // CAST-OK: ptr_int
}

// ============================================================
// WorkPlaceComponent::SetMaxWorkers (0x14012fb00) — publish the worker capacity, evict excess
// ============================================================
// Retail: write `newMax` into the owner's WorkComponent+0x30, then while the roster count
// (+0x60) exceeds newMax, take entry [0], ask the citizen's own workplace finder for
// somewhere else to work (sub_ffc0 when the profession record's +0x4c "assigned" byte is
// clear, sub_0470 when set) and AssignHousing it there — AssignHousing detaches from the
// old workplace, which is what decrements +0x60 and terminates the loop.
//
// REGISTERED at 0x14012fb00 (reg_destroy_workers.cpp). It was previously file-local, on the
// theory that registering it would newly expose the WorkPlaceUI worker spinner to the
// eviction loop. That is what retail does — the spinner's whole job is to lower the cap and
// let the excess re-home — and leaving it unregistered was the worse bug: the OTHER retail
// caller, WorkPlaceUI_AdjustWorkerCount (0x140087610, workplaceui.cpp), reaches it through
// fn_WorkplaceComp_SetMaxWorkers, which resolved to 0. That call is not on a live path today
// (the SA WorkPlaceUI vtable routes its spinner through W_OnRemove's seat cascade instead of
// the exe port), so this is a landmine defused rather than a crash fixed — but the address
// belongs to a real, ported body and must resolve to it.
//
// In the f520 flow the loop is dead by construction: ee60 has already drained the roster to
// zero, so `newMax < count` is false on entry. The guards below are for the UI path, where
// the roster is live.
void WorkPlaceComponent::SetMaxWorkers(int newMax) {
    WorkComponent* workComp = _entity->FindComponent<WorkComponent>();   // 0x140003050
    if (workComp) workComp->workState = newMax;   // +0x30 the work capacity

    if (newMax >= workerCount) return;

    std::fprintf(stderr, "[WorkPlaceComponent] SetMaxWorkers: evicting wpc=%p from roster %d down to %d\n",
                 (void*)this, workerCount, newMax);
    std::fflush(stderr);

    const bool savedRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    int guard = 0;
    while (newMax < workerCount) {
        const int before = workerCount;

        // [STANDALONE GUARD] Retail walks +0x58 unchecked because its roster is always
        // consistent; ours can be caught mid-teardown (live crash 2026-07-25 read base == 8
        // with count > 0). Bail instead of faulting.
        Object::InternalPtr<CitizenComponent>* roster = workers;
        if (!Plausible(roster)) break;
        CitizenComponent* citizen = roster[0].Get();
        if (!Plausible(citizen)) break;

        // [STANDALONE GUARD] Retail derefs citizen+0x1f0 (the profession record) unguarded;
        // a standalone worker can still be professionless, which reads through null+0x4c.
        Profession* profession = citizen->profession;
        int64_t targetIBP = 0;
        if (profession) {
            if (!profession->field4C) {
                CitizenComp_sub_ffc0(reinterpret_cast<int64_t>(citizen), &targetIBP);   // LEFT: SIG CitizenComp_sub_ffc0 (the int64_t IBP CitizenComp_AssignHousing takes)
            } else {
                CitizenComp_sub_0470(reinterpret_cast<int64_t>(citizen), &targetIBP);   // LEFT: SIG CitizenComp_sub_0470 (as above)
            }
        }

        // Re-read the roster base — the finders above can reallocate it (faithful: retail
        // re-reads *(wpc+0x58) too, right before the AssignHousing call).
        Object::InternalPtr<CitizenComponent>* rosterNow = workers;
        if (!Plausible(rosterNow)) break;
        if (!Plausible(rosterNow[0].Get())) break;

        CitizenComp_AssignHousing(rosterNow[0].Get(), &targetIBP);

        // [STANDALONE GUARD] Retail's `while` assumes the finder never hands back THIS
        // workplace. If it did, an unterminated loop here is an in-world FREEZE rather than
        // a crash, which is far harder to diagnose. Break out loudly.
        if (workerCount >= before || ++guard > 4096) {
            std::fprintf(stderr, "[WorkPlaceComponent] SetMaxWorkers: no progress on wpc=%p (roster %d -> %d, "
                         "citizen=%p) — breaking to avoid a spin\n", (void*)this,
                         before, workerCount, (void*)citizen);
            std::fflush(stderr);
            break;
        }
    }

    g_wpcRosterOnly = savedRosterOnly;
}

// ============================================================
// FUN_14012f520 — ENTER the builder profession (the DestroySM state-4 re-seat)
// ============================================================
// Faithful 1:1 port; every deviation is marked [STANDALONE GUARD].
void WorkPlaceComponent::EnterBuilderProfession() {
    // [STANDALONE GUARD] Retail is called only from a live state machine with a real world
    // singleton; ours can be driven from a harness before the world exists.
    if (GameUpdateLoop::_singleton == nullptr) return;
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;

    // 1. Release every seated worker (ee60). This is the piece whose absence left the
    //    roster half-torn-down: +0x60 non-zero over a clobbered +0x58.
    WorkPlaceComponent_OnCreateInit(this);

    // 2. The owner entity's WorkComponent.
    WorkComponent* workComp = _entity->FindComponent<WorkComponent>();   // 0x140003050

    ProfessionList* profReg = gw->m_professionList;   // +0xb0
    Profession* const builderKey = profReg ? profReg->field_0x38 : nullptr;

    // 3. Re-key a pending work item from the OLD profession (+0x48, still unmodified here)
    //    to Builder, so a task already in flight when demolition starts is offered to
    //    builders rather than to the building's former profession.
    if (workComp && workComp->workId >= 0) {
        const uintptr_t sched = reinterpret_cast<uintptr_t>(gw->m_tutorial);   // LEFT: SIG CitizenAI_WorkScheduler_Rekey (uintptr_t scheduler)
        if (sched) {
            CitizenAI_WorkScheduler_Rekey(sched, static_cast<uint32_t>(workComp->workId),
                    reinterpret_cast<uint64_t>(profession), reinterpret_cast<uint64_t>(builderKey));   // LEFT: SIG CitizenAI_WorkScheduler_Rekey (uint64_t keys)
        }
    }

    // 4/5. Seat the Builder key and register the owner into the Builder bucket. Retail
    //      writes +0x48 BEFORE the bucket insert (the insert reads the key from its own
    //      argument, so the order only matters for anything re-entering meanwhile).
    ComponentEntity* const ownerRef = _entity;
    profession = builderKey;
    const uintptr_t townMgr = reinterpret_cast<uintptr_t>(gw->m_citizenList);   // LEFT: SIG ResidenceComponent_impl_95a0 (int64_t town manager)
    if (townMgr) {
        ComponentEntity* ownerLocal = ownerRef;      // the insert takes an InternalBasePtr*
        ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerLocal,
                                     reinterpret_cast<uint64_t>(builderKey));   // LEFT: SIG ResidenceComponent_impl_95a0 (uint64_t key)
    }

    // 6. Worker capacity for the demolition site. No BuildComponent on the owner entity ->
    //    the WorkComponent's own description-derived capacity; with one -> the position
    //    child count (a construction/demolition site is sized by its footprint tiles).
    BuildComponent* buildComp = _entity->FindComponent<BuildComponent>();   // 0x14000d6c0

    int capacity = 0;
    if (buildComp == nullptr) {
        if (workComp) capacity = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(workComp));   // LEFT: SIG WorkComponent_CalcMaxWorkers
    } else {
        // 0x140002ed0's component class is not settled here: the finder stays (LEFT).
        int64_t posComp = 0;
        WorkBase_FindComponent_2ed0(reinterpret_cast<uintptr_t>(_entity), &posComp);   // LEFT: SIG WorkBase_FindComponent_2ed0 / Pf_RecursiveChildSum_123070 (int64_t)
        // [STANDALONE GUARD] Retail derefs the finder's out-IBP unconditionally; a partial
        // entity can lack the component, and 0x140123070 walks node+0x18/+0x30 from it.
        if (posComp) capacity = Pf_RecursiveChildSum_123070(posComp, 1);
    }

    // 7. Publish the capacity + evict the excess (dead after step 1, see fb00's note).
    SetMaxWorkers(capacity);

    // 8. SetAccepting(true) — the site now takes builders.
    SetAccepting(true);   // 0x14012fc00

    std::fprintf(stderr, "[DestroyWorkers] f520 re-seat wpc=%p: builderKey=%p capacity=%d "
                 "roster=%d (buildComp=%p) workComp=%p workId(+0x28)=%d%s\n",
                 (void*)this, (void*)builderKey, capacity,
                 workerCount, (void*)buildComp, (void*)workComp,
                 workComp ? workComp->workId : -1,
                 (workComp && workComp->workId >= 0)
                     ? " -> re-keyed" : " -> nothing in flight to re-key");
    std::fflush(stderr);
}

// ---- B4 FORWARD: outside callers in Dialog/workplaceui.cpp (SetMaxWorkers),
// Component/workplacecomponent.cpp and standalone/tool_harness.cpp
// (EnterBuilderProfession); the closing wave rewires them. The null test is the tree's.
extern "C" void WorkPlaceComponent_SetMaxWorkers(int64_t wpc, int newMax) {
    reinterpret_cast<WorkPlaceComponent*>(wpc)->SetMaxWorkers(newMax);   // LEFT: B4 FORWARD (workplaceui.cpp)
}
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc) {
    if (!wpc) return;
    reinterpret_cast<WorkPlaceComponent*>(wpc)->EnterBuilderProfession();   // LEFT: B4 FORWARD (workplacecomponent.cpp, tool_harness.cpp)
}
