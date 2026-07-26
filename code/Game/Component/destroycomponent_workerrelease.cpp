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
// skipped. reg_destroy_workers.cpp registers the existing body; there is no second copy.
// (Same class of miss as commit 330710b, WorkPlaceComponent::SetAccepting.)
//
// f520 is new and lives below.
//
// ============================================================
// Call sites
// ============================================================
//   retail DestroyDescription_helper_cfd0 (0x14011cfd0, state-4 enter) -> f520 at 0x14011d02f
//     on FUN_140002e10(ownerEntity) — ported at destroycomponent_states.cpp
//     Destroy_State4Enter_cfd0.
//   retail DestroyDescription_helper_dd00_sub_bcc0 / _sub_c120 (state-5 tick, the
//     replacement spawn) -> f670 — ported at destroycomponent_states.cpp
//     Destroy_State5Tick_c120.
//   (retail also reaches them from the WorkPlaceComponent controller's own state table,
//   WorkPlaceDescription_helper_ee40 / _edf0 — our WorkPlaceComponent has no live
//   controller node, so those two run from BuildComponent_State8Finalize instead.)
//
// ============================================================
// Not ported
// ============================================================
// FUN_1400f8400 — the work-scheduler re-key (move the pending task node from the old
// profession bucket to the new one inside mgr+0x120, keyed by the work id). Reachable only
// when the WorkComponent still holds a work id (+0x28 >= 0). Skipped identically to the way
// WorkPlaceComponent_LeaveBuilderProfession already skips it, and logged once.
//
// Gating: the two DestroySM call sites are OPT-IN behind BANISHED_LIVE_DESTROY_WORKERS.
// Unproven — this hands live citizens through the release path during a demolish/upgrade.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// component_factory_base.h is not pulled in here; declare the one entry point needed.
namespace ComponentFactory { bool LiveEnabled(const char* envVar); }

// ============================================================
// Resolved closure
// ============================================================
typedef void  (*fn_Ee60_t)(void* wpc);                            // 0x14012ee60 release loop
typedef void* (*fn_FindIBP_t)(int64_t entity, void* outIBP);      // member-return finders
typedef int   (*fn_CalcMaxWorkers_t)(int64_t workComp);           // 0x14012e5c0
typedef int   (*fn_ChildSum_t)(int64_t posComp, int flag);        // 0x140123070
typedef void  (*fn_AddToProfessionBucket_t)(int64_t townMgr, void* ownerIBP, uint64_t key); // 0x1400895a0
typedef void  (*fn_SetAccepting_t)(int64_t wpc, char accepting);  // 0x14012fc00
typedef void  (*fn_FindWork_t)(int64_t citizen, int64_t* outIBP); // 0x14010ffc0 / 0x140110470
typedef void  (*fn_AssignHousing_t)(void* citizen, int64_t* targetIBP);  // 0x140111ee0

static fn_Ee60_t                 s_ee60          = nullptr;
static fn_FindIBP_t              s_find3050      = nullptr;  // WorkComponent
static fn_FindIBP_t              s_findD6c0      = nullptr;  // BuildComponent (type 1401c7008)
static fn_FindIBP_t              s_find2ed0      = nullptr;  // PositionComponent
static fn_CalcMaxWorkers_t       s_calcMaxWorkers = nullptr;
static fn_ChildSum_t             s_childSum      = nullptr;
static fn_AddToProfessionBucket_t s_add95a0      = nullptr;
static fn_SetAccepting_t         s_setAccepting  = nullptr;
static fn_FindWork_t             s_findWorkUnassigned = nullptr;
static fn_FindWork_t             s_findWorkAssigned   = nullptr;
static fn_AssignHousing_t        s_assignHousing = nullptr;

static bool s_resolved = false;
static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_ee60           = reinterpret_cast<fn_Ee60_t>(ResolveAddr(Module::Exe, 0x14012ee60));
    s_find3050       = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_findD6c0       = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x14000d6c0));
    s_find2ed0       = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_calcMaxWorkers = reinterpret_cast<fn_CalcMaxWorkers_t>(ResolveAddr(Module::Exe, 0x14012e5c0));
    s_childSum       = reinterpret_cast<fn_ChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_add95a0        = reinterpret_cast<fn_AddToProfessionBucket_t>(ResolveAddr(Module::Exe, 0x1400895a0));
    s_setAccepting   = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    s_findWorkUnassigned = reinterpret_cast<fn_FindWork_t>(ResolveAddr(Module::Exe, 0x14010ffc0));
    s_findWorkAssigned   = reinterpret_cast<fn_FindWork_t>(ResolveAddr(Module::Exe, 0x140110470));
    s_assignHousing  = reinterpret_cast<fn_AssignHousing_t>(ResolveAddr(Module::Exe, 0x140111ee0));
}

// workplacecomponent.cpp — roster-only mode for the Attach/Detach helpers: do the list
// mutation, skip the gameplay-time Scene mesh-visual toggle. See the long note there; the
// short version is that our inert StatusIconComponents are zero-filled, so a never-
// registered icon reads scene id 0 and Graphics::Scene::RemoveObject(scene, 0) spins forever.
extern bool g_wpcRosterOnly;

static bool Plausible(const void* p) {
    return reinterpret_cast<uintptr_t>(p) >= 0x10000;
}

// ============================================================
// WorkPlaceUI_helper_fb00 (0x14012fb00) — publish the worker capacity, evict the excess
// ============================================================
// Retail: write `newMax` into the owner's WorkComponent+0x30, then while the roster count
// (+0x60) exceeds newMax, take entry [0], ask the citizen's own workplace finder for
// somewhere else to work (sub_ffc0 when the profession record's +0x4c "assigned" byte is
// clear, sub_0470 when set) and AssignHousing it there — AssignHousing detaches from the
// old workplace, which is what decrements +0x60 and terminates the loop.
//
// Kept FILE-LOCAL and deliberately NOT registered at 0x14012fb00. The other retail caller
// is WorkPlaceUI_Update_sub_7610, i.e. the panel's worker up/down spinner
// (workplaceui.cpp:80 calls fn_WorkplaceComp_SetMaxWorkers, currently null). Registering
// the address would silently switch that UNGATED UI path onto this eviction loop as a side
// effect of a destroy-path fix; that is a separate change with its own risk. See the report.
//
// In the f520 flow the loop is dead by construction: ee60 has already drained the roster to
// zero, so `newMax < count` is false on entry. It is ported for faithfulness and carries the
// same guards WorkPlaceComponent_EvictRoster needed, and logs if it ever does iterate.
static void WPC_SetWorkerCapacity_fb00(int64_t wpc, int newMax) {
    EnsureResolved();

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpc + 0x10));
    int64_t workComp = 0;
    if (s_find3050) s_find3050(*reinterpret_cast<int64_t*>(wpc + 0x10), &workComp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workComp);
    if (workComp) *reinterpret_cast<int32_t*>(workComp + 0x30) = newMax;

    if (newMax >= *reinterpret_cast<int32_t*>(wpc + 0x60)) return;

    if (!s_assignHousing) {
        std::fprintf(stderr, "[DestroyWorkers] fb00: roster %d > capacity %d on wpc=%p but "
                     "AssignHousing (0x140111ee0) is unresolved — excess kept\n",
                     *reinterpret_cast<int32_t*>(wpc + 0x60), newMax, reinterpret_cast<void*>(wpc));
        std::fflush(stderr);
        return;
    }

    std::fprintf(stderr, "[DestroyWorkers] fb00: evicting wpc=%p from roster %d down to %d\n",
                 reinterpret_cast<void*>(wpc), *reinterpret_cast<int32_t*>(wpc + 0x60), newMax);
    std::fflush(stderr);

    const bool savedRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    int guard = 0;
    while (newMax < *reinterpret_cast<int32_t*>(wpc + 0x60)) {
        const int before = *reinterpret_cast<int32_t*>(wpc + 0x60);

        // [STANDALONE GUARD] Retail walks +0x58 unchecked because its roster is always
        // consistent; ours can be caught mid-teardown (live crash 2026-07-25 read base == 8
        // with count > 0). Bail instead of faulting.
        int64_t* roster = *reinterpret_cast<int64_t**>(wpc + 0x58);
        if (!Plausible(roster)) break;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(roster);
        int64_t citizen = *roster;
        if (!Plausible(reinterpret_cast<void*>(citizen))) break;

        // [STANDALONE GUARD] Retail derefs citizen+0x1f0 (the profession record) unguarded;
        // a standalone worker can still be professionless, which reads through null+0x4c.
        int64_t profession = *reinterpret_cast<int64_t*>(citizen + 0x1f0);
        int64_t targetIBP = 0;
        if (profession) {
            if (*reinterpret_cast<char*>(profession + 0x4c) == '\0') {
                if (s_findWorkUnassigned) s_findWorkUnassigned(citizen, &targetIBP);
            } else {
                if (s_findWorkAssigned) s_findWorkAssigned(citizen, &targetIBP);
            }
        }

        // Re-read the roster base — the finders above can reallocate it (faithful: retail
        // re-reads *(wpc+0x58) too, right before the AssignHousing call).
        int64_t* rosterNow = *reinterpret_cast<int64_t**>(wpc + 0x58);
        if (!Plausible(rosterNow)) break;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(rosterNow);
        if (!Plausible(reinterpret_cast<void*>(*rosterNow))) break;

        s_assignHousing(reinterpret_cast<void*>(*rosterNow), &targetIBP);

        // [STANDALONE GUARD] Retail's `while` assumes the finder never hands back THIS
        // workplace. If it did, an unterminated loop here is an in-world FREEZE rather than
        // a crash, which is far harder to diagnose. Break out loudly.
        if (*reinterpret_cast<int32_t*>(wpc + 0x60) >= before || ++guard > 4096) {
            std::fprintf(stderr, "[DestroyWorkers] fb00: no progress on wpc=%p (roster %d -> %d, "
                         "citizen=%p) — breaking to avoid a spin\n", reinterpret_cast<void*>(wpc),
                         before, *reinterpret_cast<int32_t*>(wpc + 0x60), reinterpret_cast<void*>(citizen));
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
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc) {
    EnsureResolved();

    // [STANDALONE GUARD] Retail is called only from a live state machine with a real world
    // singleton; ours can be driven from a harness before the world exists.
    if (!wpc || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);

    // 1. Release every seated worker (ee60). This is the piece whose absence left the
    //    roster half-torn-down: +0x60 non-zero over a clobbered +0x58.
    if (s_ee60) s_ee60(reinterpret_cast<void*>(wpc));

    // 2. The owner entity's WorkComponent.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpc + 0x10));
    int64_t workComp = 0;
    if (s_find3050) s_find3050(*reinterpret_cast<int64_t*>(wpc + 0x10), &workComp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workComp);

    const uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    const uint64_t builderKey = profReg ? *reinterpret_cast<uint64_t*>(profReg + 0x38) : 0;

    // 3. Re-key a pending work item from the OLD profession (+0x48, still unmodified here)
    //    to Builder. FUN_1400f8400 is unported — same skip as
    //    WorkPlaceComponent_LeaveBuilderProfession's.
    if (workComp && *reinterpret_cast<int32_t*>(workComp + 0x28) >= 0) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::fprintf(stderr, "[DestroyWorkers] NOT-IMPLEMENTED FUN_1400f8400 (work-scheduler "
                         "profession re-key) — workComp+0x28=%d still holds a work id; the pending "
                         "task keeps its pre-demolition key\n",
                         *reinterpret_cast<int32_t*>(workComp + 0x28));
            std::fflush(stderr);
        }
    }

    // 4/5. Seat the Builder key and register the owner into the Builder bucket. Retail
    //      writes +0x48 BEFORE the bucket insert (the insert reads the key from its own
    //      argument, so the order only matters for anything re-entering meanwhile).
    const int64_t ownerRef = *reinterpret_cast<int64_t*>(wpc + 0x10);
    *reinterpret_cast<uint64_t*>(wpc + 0x48) = builderKey;
    const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (townMgr && s_add95a0) {
        int64_t ownerLocal = ownerRef;      // the insert takes an InternalBasePtr*
        s_add95a0(static_cast<int64_t>(townMgr), &ownerLocal, builderKey);
    }

    // 6. Worker capacity for the demolition site. No BuildComponent on the owner entity ->
    //    the WorkComponent's own description-derived capacity; with one -> the position
    //    child count (a construction/demolition site is sized by its footprint tiles).
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpc + 0x10));
    int64_t buildComp = 0;
    if (s_findD6c0) s_findD6c0(*reinterpret_cast<int64_t*>(wpc + 0x10), &buildComp);

    int capacity = 0;
    if (buildComp == 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workComp);
        if (workComp && s_calcMaxWorkers) capacity = s_calcMaxWorkers(workComp);
    } else {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpc + 0x10));
        int64_t posComp = 0;
        if (s_find2ed0) s_find2ed0(*reinterpret_cast<int64_t*>(wpc + 0x10), &posComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posComp);
        // [STANDALONE GUARD] Retail derefs the finder's out-IBP unconditionally; a partial
        // entity can lack the component, and 0x140123070 walks node+0x18/+0x30 from it.
        if (posComp && s_childSum) capacity = s_childSum(posComp, 1);
    }

    // 7. Publish the capacity + evict the excess (dead after step 1, see fb00's note).
    WPC_SetWorkerCapacity_fb00(wpc, capacity);

    // 8. SetAccepting(true) — the site now takes builders.
    if (s_setAccepting) s_setAccepting(wpc, 1);

    std::fprintf(stderr, "[DestroyWorkers] f520 re-seat wpc=%p: builderKey=%llx capacity=%d "
                 "roster=%d (buildComp=%p)\n", reinterpret_cast<void*>(wpc),
                 static_cast<unsigned long long>(builderKey), capacity,
                 *reinterpret_cast<int32_t*>(wpc + 0x60), reinterpret_cast<void*>(buildComp));
    std::fflush(stderr);
}
