// code/Game/Component/buildcomponent_state1.cpp
// Clean C++ port of BuildDescription_helper_8290 @ 0x140108290 — the BuildComponent
// state-1 ("gather materials") ENTER handler. This is the construction spawner: it
//   1. reads the build cost multiplier off the workplace component (+0x4c),
//   2. resets the site ModelComponent build variant,
//   3. registers the site into the construction registry (townMgr+0xd0),
//   4. if not all materials are delivered (sub_8630) and not instant-build:
//        - constructs a GatherRawMaterials work (GatherRawMaterials_Ctor),
//        - POSTS it to the work-reservation ledger via CitizenAI_impl_ec60 (0x14012ec60)
//          [GUARDED/deferred in standalone — the shared-AVL ledger insert belongs to the
//           later citizen-consumer brick; here we only trace the intent],
//        - configures the site StorageComponent to ACCEPT the required raw materials
//          (accept-mask +0x3c = OR of each material's accept bits; capacity +0x38 =
//           Σ costQty·carryWeight·multiplier),
//   5. else (materials present / instant): forces the state node to transition to state 8.
//
// ABI: this is a state-node ENTER handler. The exe dispatcher (0x140002b60) tail-calls
// it as `MOV RCX,[node+0x18]; JMP handler`, so it receives RCX = the BuildComponent
// (node+0x18 = *(BuildComponent+0x40 selfPtr) = the BuildComponent) AND RDX = the state
// machine node (preserved through the tail-JMP — bug-class #6 implicit forwarding). The
// controller_state_table.h Dispatcher<Tag>::Enter forwards `sm` as the 2nd arg.
//
// Faithful offset-by-offset port of the Ghidra decompilation of 0x140108290. All
// exe-internal helpers are resolved guarded; if one is unavailable the step is skipped.

#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"   // ComponentFactory::the live gate (default-on gates)
#include "Component/controller_state_table.h"   // ControllerLive::RequestStateTransition
#include "Component/clearareacomponent_funcs.h" // ClearArea_Finalize8270 (port of ModelComponent_impl_8270)
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Ported leaves (this brick):
extern "C" bool  BuildComponent_HaveAllMaterials(void* buildComp);          // sub_8630
extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP); // 0x1400ec710
// town_workplace_registry.cpp — register a workplace into townMgr+0x100 (profession map).
extern "C" void  TownRegistry_RegisterWorkplace(int64_t townMgr, void* workplaceComp);

// ---- Exe helpers (all already registered in standalone unless noted) -------------
// FUN_140004740 InteractComponent_FindWorkplaceComponent(entity, retIBP) -> retIBP
typedef void* (*BCS1_FindComp_t)(uintptr_t entity, void* retIBP);
// FUN_14000a170 find (Model) component; FLAG: registration unverified -> guarded
typedef void* (*BCS1_FindComp2_t)(uintptr_t entity, void* retIBP);
// ModelComponent_impl_8270(modelComp) -> reset build variant; FLAG guarded
typedef void  (*BCS1_ModelReset_t)(uintptr_t modelComp);
// BuildComponent_helper_7550 == 0x140017550 == BuildComponent_FindInQueue(arrayBase, ibp)
typedef int   (*BCS1_FindInQueue_t)(void* arrayBase, void* ibp);
// FUN_140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised into
// the exe (ENDGAME B.3.2 step 2) — converted at its call site below.
// FUN_140003050 WorkBase_FindComponent_3050(entity, retIBP) -> WorkComponent (0x1401c8070)
typedef void* (*BCS1_FindWorkComp_t)(uintptr_t entity, void* retIBP);
// CitizenAI_impl_ec60(workComp, code, flag, workIBP) — reservation-ledger post (deferred)
typedef void  (*BCS1_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
// Entity_FindComponent_Storage @ 0x140006270 (entity, retIBP) -> site StorageComponent
typedef void* (*BCS1_FindStorage_t)(uintptr_t entity, void* retIBP);
// FUN_14000f090 StorageComponent_DynArrayResize(slotArrayField, count) was
// Collection::Array<Bytes<0x10,8>>::SetSize monomorphised into the exe
// (ENDGAME B.3.2 step 2: stride 0x10, align 8, zero tail) — converted below.

static uint32_t BCS1_TypeId(const char* name, uintptr_t rdataGlobal) {
    (void)rdataGlobal;
    return ::Object::GetTypeIdByName(name);
}

// Scan a material entity's component list (+0x28 array, +0x30 count) for the first
// component whose GetTypeId (vtable+0x10) == tid; verify with IsDerivedFrom. Mirrors
// the material-scan the exe does twice in 0x140108290 (and gather Execute's finder).
static void* BCS1_FindRawMatDesc(uintptr_t matEntity, uint32_t tid) {
    if (!matEntity || !tid) return nullptr;
    int count = *reinterpret_cast<int*>(matEntity + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(matEntity + 0x28);
    if (!arr) return nullptr;
    for (int i = 0; i < count; i++) {
        void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t t = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (t == tid) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tid)) return comp;
            return nullptr;
        }
    }
    return nullptr;
}

// Natural builder-flow: construction sites are registered into the townMgr+0x100
// profession BST under the Builder key at construction start (faithful to retail's WPC
// OnCreate FUN_14012f520), so the tick-driven Builder pool (BuilderPool_Tick) finds them
// via FindAvailableBuilding and seats idle laborers as Builders.

// ---- Construction-demo gating (standalone) ---------------------------------------
// StartTown force-completes every start-town building (advances its ModelComponent to
// the final construction sub-mesh). Driving State1Enter on such a building would RESET
// its storage (f090 resize + accept-mask), blanking finished houses. To keep the build
// pipeline testable without that damage we (a) seed ONE genuine foundation as the demo
// site and (b) skip everything else. `g_pConstructionDemoSite` (set by StartTown) is the
// BuildComponent of that one foundation; when non-null, ONLY it is driven. When it is
// null (no demo seeded) we still skip buildings whose model is already complete.
extern "C" { void* g_pConstructionDemoSite = nullptr; }
// The demo foundation's OWNER entity (set by StartTown alongside g_pConstructionDemoSite).
// The construction force harness uses it as the GatherRawMaterials/BuildStructure work-area.
extern "C" { void* g_pConstructionDemoEntity = nullptr; }

// Best-effort completion check: a force-completed building has its ModelComponent build
// variant (+0x166) at the final sub-mesh of a multi-stage group; a genuine foundation
// sits at variant 0. Foundation / single-mesh buildings -> false (i.e. "build me").
static bool BCS1_IsModelComplete(uintptr_t buildComp) {
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(buildComp + 0x10);
    if (!entity) return false;
    uint32_t tidModel = ::Object::GetTypeIdByName("ModelComponent");
    if (!tidModel) return false;
    int count = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return false;
    void* model = nullptr;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* comp = *slot;
        if (!comp) continue;
        typedef uint32_t (*GT_t)(void*);
        uint32_t t = ((GT_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (t == tidModel) { model = comp; break; }
    }
    if (!model) return false;
    uint8_t* m = reinterpret_cast<uint8_t*>(model);
    int16_t modelVariant = *reinterpret_cast<int16_t*>(m + 0x166);
    if (modelVariant <= 0) return false;                      // foundation / not advanced
    int16_t variantIdx = *reinterpret_cast<int16_t*>(m + 0x164);
    void* modelDesc = *reinterpret_cast<void**>(m + 0x18);
    if (!modelDesc) return true;                              // advanced but no desc -> treat complete
    void* groupArray = *reinterpret_cast<void**>((uint8_t*)modelDesc + 0x20);
    uint32_t groupCount = *reinterpret_cast<uint32_t*>((uint8_t*)modelDesc + 0x28);
    if (!groupArray || variantIdx < 0 || (uint32_t)variantIdx >= groupCount) return true;
    int subCount = *reinterpret_cast<int32_t*>((uint8_t*)groupArray +
                        (int64_t)variantIdx * 0x18 + 0x10);
    return (subCount > 1 && modelVariant >= subCount - 1);
}

// Shared construction-demo gate. Returns true if this BuildComponent must NOT be driven
// through the construction state machine (State1Enter / State1Update / state-8 handlers):
//   * the seeded demo foundation -> drive it (false);
//   * any completed building (model at final sub-mesh) -> skip;
//   * any other genuine foundation -> drive it.
// This must gate the PER-FRAME handlers too (State1Update), not just State1Enter — else a
// completed building's node self-advances 1->8->2 and its finalize clobbers finished storage.
//
// The completed-building test used to be bypassed whenever a demo site was registered — the
// gate then returned "skip" for EVERY other building, as belt-and-braces against resetting a
// finished building's storage. That also blocked buildings that genuinely need constructing,
// including the UPGRADE REPLACEMENT (DestroyComponent state-5 spawns a fresh stone-house
// foundation, which is never the demo site, so it sat inert forever). BCS1_IsModelComplete is
// the real protection and is known-good on its own: with no demo site registered all seven
// force-completed start-town BuildComponents skip, while a player-placed foundation builds.
// ---- Live-site table (standalone) ------------------------------------------------
// BCS1_IsModelComplete is a HEURISTIC for "was force-completed by StartTown, never
// constructed". It misreads one case: State8Update snaps the model to its final
// sub-mesh and THEN requests 8->2, so a genuinely-built building is "model complete"
// by the time the 8->2 transition action runs. DemoGateSkip then suppressed
// State8Finalize — the workplace hand-off (WorkPlaceComponent_EnterBuiltState, which
// drops the Builder key so WorkPlaceUI stops showing the worker up/down spinner) and
// the OUTPUT storage reconfigure. Only the demo site escaped, via the short-circuit
// above it; every upgrade replacement and every player-placed building lost both
// (observed live 2026-07-25: upgraded house kept the builder spinner, and its
// [Residence] state-2 enter had no State8Finalize before it).
//
// So record the exact fact the gate wants instead of inferring it: a BuildComponent
// that actually ran State1Enter is a live site until its completion clears it. Small
// fixed table — concurrent construction sites are a handful, and a linear scan keeps
// this allocation-free on a path that runs per-frame per-site.
namespace {
constexpr int kMaxLiveSites = 64;
void* s_liveSites[kMaxLiveSites];
}
extern "C" bool BuildComponent_IsLiveSite(void* bc) {
    if (!bc) return false;
    for (int i = 0; i < kMaxLiveSites; ++i)
        if (s_liveSites[i] == bc) return true;
    return false;
}
extern "C" void BuildComponent_MarkLiveSite(void* bc) {
    if (!bc || BuildComponent_IsLiveSite(bc)) return;
    for (int i = 0; i < kMaxLiveSites; ++i) {
        if (s_liveSites[i] == nullptr) { s_liveSites[i] = bc; return; }
    }
    // Table full: fall back to the heuristic for this site rather than evicting a
    // live one (worst case = the pre-fix behaviour, never a wrong "is complete").
}
extern "C" void BuildComponent_ClearLiveSite(void* bc) {
    if (!bc) return;
    for (int i = 0; i < kMaxLiveSites; ++i)
        if (s_liveSites[i] == bc) { s_liveSites[i] = nullptr; return; }
}

extern "C" bool BuildComponent_DemoGateSkip(void* buildComp) {
    if (!buildComp) return false;
    if (g_pConstructionDemoSite && buildComp == g_pConstructionDemoSite) return false;
    if (BuildComponent_IsLiveSite(buildComp)) return false;   // genuinely under construction
    return BCS1_IsModelComplete(reinterpret_cast<uintptr_t>(buildComp));
}

// The state-1 ENTER handler. param1 = BuildComponent, param2 = state-machine node.
extern "C" int BuildComponent_State1Enter(void* buildComp, void* smNode) {
    uintptr_t self = reinterpret_cast<uintptr_t>(buildComp);
    if (!self) return 1;


    // resolve helpers once
    static BCS1_FindComp_t     s_find4740 = nullptr;
    static BCS1_FindInQueue_t  s_findInQueue = nullptr;
    static BCS1_FindWorkComp_t s_find3050 = nullptr;
    static BCS1_PostWork_t     s_postEc60 = nullptr;
    static BCS1_FindStorage_t  s_findStorage = nullptr;
    // FUN_140002e10 FindComponent_WorkPlace(entity, retIBP) -> WorkPlaceComponent (0x1401c80e0).
    // Same finder GRM Execute uses to gate the gather on the site's "accepting" flag (+0x50).
    static BCS1_FindComp_t     s_findWpc2e10 = nullptr;
    static bool s_res = false;
    if (!s_res) {
        s_find4740    = reinterpret_cast<BCS1_FindComp_t>(ResolveAddr(Module::Exe, 0x140004740));
        // 0x14000a170 (FindComponent<ClearAreaComponent>) and 0x140118270
        // (ModelComponent_impl_8270) are unregistered in standalone; both already have
        // clean C++ ports (fn_FindComponent_SM / ClearArea_Finalize8270) used elsewhere
        // in this file's own state machine — call those directly instead of resolving
        // the raw exe addresses (which would stay permanently null).
        s_findInQueue = reinterpret_cast<BCS1_FindInQueue_t>(ResolveAddr(Module::Exe, 0x140017550));
        s_find3050    = reinterpret_cast<BCS1_FindWorkComp_t>(ResolveAddr(Module::Exe, 0x140003050));
        s_postEc60    = reinterpret_cast<BCS1_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
        s_findStorage = reinterpret_cast<BCS1_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
        s_findWpc2e10 = reinterpret_cast<BCS1_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
        s_res = true;
    }

    // ---- Construction-demo gate (see g_pConstructionDemoSite header note) ----------
    // Must run BEFORE any mutation (multiplier write / model reset / storage reset).
    if (BuildComponent_DemoGateSkip(buildComp)) {
        return 1;
    }

    // Past the gate: this is a real construction site. Record it so the state-8
    // handlers keep running once the mesh progression reaches the final sub-mesh
    // (see the live-site table note above). Cleared by the 8->2 completion.
    BuildComponent_MarkLiveSite(buildComp);

    // +0x4c = 1 (default multiplier)
    *reinterpret_cast<uint32_t*>(self + 0x4c) = 1;

    // multiplier from workplace component (+0x58)
    uintptr_t wpIbp = 0;
    if (s_find4740) s_find4740(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpIbp);
    if (wpIbp != 0) {
        // Retail: bc+0x4c = WorkplaceComponent+0x58 (the build-effort multiplier). In the
        // standalone exe the WorkPlaceComponent lifecycle is inert so +0x58 stays 0, which
        // would clobber the default 1 set just above and make the state-8 build target
        // (matComp[0x30] * mult) collapse to 0 (instant completion, no visible labor). Only
        // adopt the workplace value when it is a valid positive multiplier.
        uint32_t wpMult = *reinterpret_cast<uint32_t*>(wpIbp + 0x58);
        if ((int32_t)wpMult > 0)
            *reinterpret_cast<uint32_t*>(self + 0x4c) = wpMult;
    }

    // reset the site ClearAreaComponent build variant (a170 -> 8270)
    uintptr_t mcIbp = 0;
    fn_FindComponent_SM(reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(self + 0x10)), &mcIbp);
    if (mcIbp != 0) {
        ClearArea_Finalize8270(reinterpret_cast<void*>(mcIbp));
    }

    // register the site into the construction registry (townMgr+0xd0 / count +0xd8)
    uintptr_t townMgr = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    uintptr_t siteIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&siteIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(buildComp));   // B.3.3 bridge
    if (townMgr && s_findInQueue) {
        int idx = s_findInQueue(reinterpret_cast<void*>(townMgr + 0xd0), &siteIbp);
        if (idx < 0) {
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            int newCount = *reinterpret_cast<int*>(townMgr + 0xd8) + 1;
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0xd0))
                .SetSizeFill(newCount, &siteIbp);
        }
    }

    // instant-build flag: gw[0xd0] & 1
    uint8_t instant = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        instant = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd0) & 1;

    bool haveAll = BuildComponent_HaveAllMaterials(buildComp);

    if (!haveAll && instant == 0) {
        // -------- SPAWN branch: build the gather work + configure site storage --------
        void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                        ? fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8) : nullptr;
        void* work = nullptr;
        if (mem) {
            void* entRaw = *reinterpret_cast<void**>(self + 0x10);
            void* entIbp = entRaw;
            work = GatherRawMaterials_Ctor_Impl(mem, &entIbp);
        }
        uintptr_t workIbp = 0;
        if (fn_InternalBasePtr_Ctor) new (&workIbp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge

        // find the WorkComponent (0x1401c8070) the work posts under
        uintptr_t workCompIbp = 0;
        if (s_find3050) s_find3050(*reinterpret_cast<uintptr_t*>(self + 0x10), &workCompIbp);

        // POST to the reservation ledger (brick 2b): CitizenAI_impl_ec60 inserts the
        // gather work into the gw+0x118 priority AVL (via 0x1400f88a0) keyed by the
        // WorkComponent, so an idle citizen claims it and runs GatherRawMaterials::Execute
        // (the town->site haul). Exe: CitizenAI_impl_ec60(*pWorkCompIBP, 0x2001, 1, &workIBP).
        // Needs the live-citizen action stack + pathfinder.
        if (s_postEc60 && workCompIbp != 0) {
            s_postEc60(workCompIbp, 0x2001, 1, &workIbp);
        }

        // configure the site StorageComponent accept-mask + capacity
        void* descObj = *reinterpret_cast<void**>(self + 0x18);
        uint32_t buildDescTid = BCS1_TypeId("BuildDescription", 0x1401c7040);
        void* buildDesc = (descObj && fn_IsDerivedFrom && buildDescTid &&
                           fn_IsDerivedFrom(descObj, buildDescTid)) ? descObj : nullptr;

        uintptr_t storageIbp = 0;
        if (s_findStorage) s_findStorage(*reinterpret_cast<uintptr_t*>(self + 0x10), &storageIbp);

        if (storageIbp != 0) {
            // B.3.2 bridge: exe 0x14000f090 IS Collection::Array<Bytes<0x10,8>>::SetSize(n).
            GameColl::AsArray<GameColl::Bytes<0x10, 8>>(
                reinterpret_cast<void*>(storageIbp + 0x28)).SetSize(0);

            uint32_t acceptMask = 0;
            int totalQty = 0;
            int multiplier = *reinterpret_cast<int*>(self + 0x4c);
            uint32_t rawMatTid = BCS1_TypeId("RawMaterialDescription", 0x1401c7cb8);

            int matCount = buildDesc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(buildDesc) + 0x28) : 0;
            uintptr_t matArr = buildDesc ? *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(buildDesc) + 0x20) : 0;
            int found = 0;
            for (int i = 0; i < matCount; i++) {
                uintptr_t matEntry = matArr + static_cast<uintptr_t>(i) * 0x10;
                uintptr_t matEntity = *reinterpret_cast<uintptr_t*>(matEntry);      // element @0 = raw material
                int       costQty   = *reinterpret_cast<int*>(matEntry + 8);        // element @8 = cost qty
                void* rawMat = BCS1_FindRawMatDesc(matEntity, rawMatTid);
                if (rawMat) {
                    ++found;
                    acceptMask |= *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x5c);
                    int carryWeight = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMat) + 0x48);
                    totalQty += costQty * carryWeight * multiplier;
                }
            }

            *reinterpret_cast<uint32_t*>(storageIbp + 0x3c) = acceptMask;
            *reinterpret_cast<int*>(storageIbp + 0x38) = totalQty;

        }

    } else {
        // -------- Materials present / instant: force transition to state 8 --------
        // Retail (0x140108290 else-branch):
        //     req = 8;
        //     if ((int8_t)sm[9] < 0) { Time t(0.0f); StateMachine_Tick(sm,t,0,0); }
        //     i = node->slot08(node, sm[9], &req);
        //     if (i >= 0) { sm[0xb] = i; sm[0xa] = req; }
        // == ControllerLive::RequestStateTransition, which is the same idiom minus the
        // nested pre-tick (it bails when sm[9] < 0 instead). That pre-tick cannot fire on
        // the path this branch actually takes: StateMachine::Tick writes sm+9 from the
        // node Step BEFORE dispatching the Enter slot, so sm+9 is >= 0 whenever a state's
        // enter handler is running. The one caller that could reach it with sm+9 < 0 is a
        // hand-driven node outside Tick, which this project has none of.
        //
        // The instant-build leg of this branch (instant != 0) is retail's start-town
        // completion path: gw+0xd0 bit 0 is set across the spawner's SetModel +
        // RegisterEntity bracket, StateMachineComponent's own OnAdded (slot 6,
        // 0x14012b660) ticks the SM inside that same OnAddedPass, and StateMachine::Tick
        // loops while a pending state is set — so 1 -> 8 -> 2 all completes before the
        // spawner clears the bit. State8Enter's instant branch is what snaps the meshes to
        // their final frame and stamps +0x48 = target. See StartTown_SpawnStructures and
        // notes/BRIDGE_HIGHLIGHT_DIALOG_BRIEF.md.
        ::ControllerLive::RequestStateTransition(smNode, 8);
    }

    // ===========================================================================
    // WORKPLACE HAND-OFF — runs on BOTH legs of the branch above (moved out of the
    // spawn leg 2026-08-15).
    //
    // This block used to live inside `if (!haveAll && instant == 0)`, i.e. only when
    // the site had to gather materials. That is not where retail puts it: retail's
    // WorkPlaceComponent controller runs FUN_14012f520 ("enter Builder profession")
    // at construction START, unconditionally — it has nothing to do with materials.
    //
    // The consequence of the misplacement was total, not partial. An instant-built or
    // fully-stocked site never got WPC+0x48 = Builder, so at completion
    // WorkPlaceComponent_LeaveBuilderProfession bailed on its `key == 0` guard and
    // never assigned the building's NATURAL profession from the description (+0x20)
    // either. +0x48 stayed 0 for the entity's whole life, and WorkplaceUI_Startup
    // hides the entire workplace panel when +0x48 == 0 (faithful to retail
    // 0x140087080) — so no worker up/down spinner and no work-stopped icon on ANY
    // workplace: orchard, crop field, fishing dock, hunter lodge, gatherer hut.
    // Measured 2026-08-15: 23642 `[WorkPlaceUI] HIDDEN` lines, 0 shown, across 6
    // distinct entities; `[BuildTrace] ... profession 0x0 -> 0x0` on every site.
    // ===========================================================================
    // *** TWO CONVENTIONS SET WPC+0x50 IN THIS CODEBASE. READ BOTH BEFORE GREPPING. ***
    //   (a) WorkPlaceComponent_SetAccepting()  — workplacecomponent.cpp:503, the
    //       faithful Entity_impl_fc00 (0x14012fc00) port. Sets +0x50 AND drives the
    //       status-icon side effects (clears closed/disabled bits; raises the
    //       "no workers" warning when the roster is empty).
    //   (b) the RAW FIELD WRITE just below — sets +0x50 only, no icon.
    // A `grep SetAccepting` CANNOT SEE (b). That blind spot cost a wrong root-cause
    // call on 2026-07-30 ("the only true-setter is starttown.cpp:394"). If you are
    // auditing who makes a workplace accepting, grep for `0x50` too.
    //
    // Retail does NOT hand-roll this: FUN_14012f520 ("enter Builder profession",
    // ported at destroycomponent_workerrelease.cpp:222 and registered) ends with
    // WorkPlaceUI_helper_fb00(...) then Entity_impl_fc00(wpc, 1) — i.e. the full
    // SetAccepting, icon included. So converting (b) to a real SetAccepting call is
    // justifiable on faithfulness grounds; but the *properly* faithful change is to
    // call the already-ported f520 here instead of hand-rolling a subset of it, which
    // also brings ee60's worker release, the profession re-key, the BST register and
    // SetMaxWorkers. That is a real behaviour change and wants its own gated task —
    // do not do it as a drive-by while touching this comment.
    // ---------------------------------------------------------------------------
    // Enable the site's WorkPlaceComponent so citizens actually gather for it.
    // GatherRawMaterials::Execute (0x1400ebe40) gates the whole haul on the site's
    // WorkPlaceComponent (0x1401c80e0) "accepting" flag (+0x50): when 0 it returns
    // immediately ("workplace disabled") and never searches for source storage. In
    // retail that flag is set by the WorkPlaceComponent's own live lifecycle; in the
    // standalone exe that component is inert, so +0x50 stays 0 and no construction
    // haul ever runs. Set it here (construction-start) via the SAME finder GRM uses,
    // reproducing the accepting state a live WorkPlaceComponent would hold. (The full
    // WorkPlaceComponent_SetAccepting also drives a status-icon warning; only the
    // gate-relevant +0x50 is reproduced here — the visual is deferred to the live WPC.)
    if (s_findWpc2e10) {
        uintptr_t wpcIbp = 0;
        s_findWpc2e10(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpcIbp);
        if (wpcIbp != 0) {
            void* wpc = *reinterpret_cast<void**>(&wpcIbp);
            if (wpc) {
                *reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(wpc) + 0x50) = 1;
                // 2c-B: mark the site's WorkPlaceComponent as a "Builder workplace" —
                // WPC+0x48 = Builder marker = *(*(gw+0xb0)+0x38). Retail's construction-start
                // handler FUN_14012f520 does this (the inert WPC controller never runs it in
                // standalone). This keeps the seated builders consistent: the employment tick's
                // Phase-1 housing-sync would RESET a citizen whose +0x1f0 (Builder) != its
                // workplace's required profession (+0x48), so +0x48 must read Builder.
                //
                // NOTE (legacy off-path rationale): registering the site into the
                // townMgr+0x100 profession BST makes BuildPathGraph (0x1400d70c0) produce
                // pairs -> ProcessPaths drains -> the employment allocator (0x1400d43b0) runs
                // its Phase-5B AssignHousing(null) churn over every idle laborer. That churn
                // was ORIGINALLY believed to deadlock the WPC Scene mesh-visual toggle, which
                // is why the off-path defers the SEAT to State8Enter (direct CitizenEmploy_
                // SeatWorkersAt against the site entity) instead of registering. That deadlock
                // is now neutralized by g_wpcRosterOnly (held across the allocator body, the
                // builder pool, and the profession spinner), so the NATURAL register path below
                // is default-on and verified safe: a 30s autostart run with a registered site
                // stays hang-free and the town-hall Professions panel shows the site's real
                // Builder capacity ("X of 3" via the CapProbe in builder_pool_driver.cpp — the
                // same WorkPlaceComponent/WorkComponent/CalcMaxWorkers path ProfessionUI_Slot8
                // and FindAvailableBuilding use). Seating stays deferred to State8Enter either
                // way: seating at construction start would convert the idle Laborers that HAUL
                // the raw materials into Builders before any hauling. Laborers haul; Builders build.
                uintptr_t gwPtr = g_pGameWorldSingleton ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
                uintptr_t profReg = gwPtr ? *reinterpret_cast<uintptr_t*>(gwPtr + 0xb0) : 0;
                uint64_t builderMarker = profReg ? *reinterpret_cast<uint64_t*>(profReg + 0x38) : 0;
                if (builderMarker != 0)
                    *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(wpc) + 0x48) = builderMarker;
                // NATURAL builder flow (BANISHED_BUILDER_NATURAL): register the site into
                // the townMgr+0x100 profession BST keyed Builder — the step retail does at
                // WPC OnCreate (FUN_14012f520 → AddWorkplaceToProfessionTree 0x1400895a0).
                // This is what makes FindAvailableBuilding (0x14008d100) able to locate the
                // site, so the tick-driven Builder pool seats idle laborers here. Capacity
                // (WorkComponent+0x30) is computed lazily by FindAvailableBuilding via
                // CalcMaxWorkers; accepting (WPC+0x50) was set just above. The employment
                // allocator's AssignHousing churn (which townMgr+0x100 population activates)
                // is freeze-guarded by g_wpcRosterOnly inside CitizenAssignCitizens.
                if (townMgr) {
                    TownRegistry_RegisterWorkplace(static_cast<int64_t>(townMgr), wpc);
                }
            }
        }
    }


    return 1;
}

extern "C" void RegisterBuildComponentState1() {
    RegisterExeAddr(0x140108290, reinterpret_cast<uintptr_t>(&BuildComponent_State1Enter));
}
