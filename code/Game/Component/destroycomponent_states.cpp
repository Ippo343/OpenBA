// code/Game/Component/destroycomponent_states.cpp
// Phase 3 / upgrade path — the DestroyComponent controller state machine, i.e.
// what demolishes a building and (when DestroyComponent+0x60 holds an upgrade
// target template) replaces it with the new building's construction site.
//
// Retail mechanism (Ghidra, DestroyDescription_helper_dd00 0x14011dd00): a
// global 0xf8-stride state table rooted at DAT_1401d7ae0, dispatched by the
// controller subobject at DestroyComponent+0x28 (vtable_DestroyComponent_
// Controller 0x1401630d0). States are entity-global ids shared with the Build
// controller's table:
//
//   state 3 (begin demolition): enter 0x14011d7b0 — stop production, strip
//           status icons/mesh, branch -> 4 (normal, flag +0x50==0) or 6.
//   state 4 (haul-away):        enter 0x14011cfd0 — queue a ClearStorage work
//           (type 1) unless storage already empty / materials never delivered;
//           tick 0x14011cec0 — when storage drains and no reclaim job remains,
//           -> 5. transitions {5, 2}.
//   state 5 (build/replace):    enter 0x14011cc70 — half-materials refund into
//           +0x58 + queue a BuildStructure work (type 0x2001), or (nothing to
//           build from) scatter contents + mark entity for removal;
//           tick 0x14011c120 — ★ THE REPLACEMENT SPAWN ★: GATED on demolition
//           progress (+0x54, credited per builder work unit via vtable slot 15
//           0x14011daf0) reaching the +0x58 target; then capture the old
//           footprint/transform, release workers, mark old entity for removal,
//           clone the target template (ComponentEntity_CopyCtor 0x1400c3720)
//           onto the old footprint and register it into the world
//           (EntityList::RegisterEntity 0x1400c48a0, category from
//           template+0x20). transitions {0, 2, 8, 7}.
//   state 6 (collapse/rubble):  enter 0x14011d4d0 / 0x14011d400 / 0x14011d490 —
//           DEFERRED (rubble-mesh path: sub_d5d0 0x14011d5d0 + sub_5020
//           0x140125020 need Graphics::MeshInstance::SetMesh /
//           GetMaterialList, unported). Safe one-shot-logging stub below.
//   state 7 (terminal):         no handlers. transitions {0, 8, 5}.
//
// Retail transition-action handlers (sub_4020 conds: 0x14011bcc0 / 0x14011d390
// / 0x14011d2a0 / 0x14011d1a0) are DEFERRED — registered as null, which the
// ControllerLive dispatcher treats as a graceful no-op.
//
// The live DFA cursor lives on the entity's StateMachineComponent; the pump
// that the UpgradeUI click handler calls (0x14011dfa0, ported disasm-faithfully
// as RemovalCalc_StateMachine_dfa0 in keystone_a_removalcalcpath.cpp) drives
// these handlers through the node linked at DestroyComponent+0x28.
//
// Handler placement (Ghidra-verified record layout for sub_3e50(state,A,B,C,D),
// 0xf8-stride records at DAT_1401d7ae0): A → record+0x08 (dispatched via
// controller 0x1400341a0), B → +0x20 (via 0x1400341d0, save/serialize),
// D → +0x18 (via 0x140034260), C is a dead parameter (never stored).
// Semantics follow the handler BODIES: every A-position body (d7b0/cfd0/cc70)
// is a one-shot entry action, every D-position body (cec0/c120/d490) is a
// repeated poll — so A = enter-once, D = per-frame tick in our dispatcher,
// the same placement the in-game-proven ResidenceComponent heat tick uses.
//
// STANDALONE deviations (each logged once at runtime):
//   * state-3 mesh/scene ops (Scene::RemoveObject, rubble-mesh swap) skipped —
//     visual-only; mirrors residencecomponent.cpp's SetMeshFlag skip.
//   * state 6 (bulldoze rubble) deferred entirely.
// NOW PORTED: sub_c530 contents-scatter (0x14011c530) — reclaimed goods spill as
// RawMaterial ground piles over the footprint (was: dropped). See below.
//
// Default-ON via kDefaultOnFlags (graduated 2026-07-25, upgrade path verified
// end-to-end); BANISHED_LIVE_DESTROY is now a kill-switch (=0). See
// destroycomponent.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (type-id fallback)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Component/controller_state_table.h"

// ============================================================
// Resolved helpers (once)
// ============================================================
typedef void* (*fn_FindIBP_t)(int64_t entity, void* outIBP);    // member-return finders
typedef char  (*fn_HasWork_t)(int64_t workComp);                // 0x14012ead0
typedef void  (*fn_StopWork_t)(int64_t workComp);               // 0x14012eb50
typedef void  (*fn_PostWork_t)(int64_t workComp, uint32_t code, char flag, void* workIBP); // 0x14012ec60
typedef void  (*fn_ReleaseWorkers_t)(int64_t wp);               // 0x14012f670 (reg_destroy_workers.cpp)
typedef void  (*fn_TradeNotify_t)(int64_t tradeMgr, void* entityIBP);  // 0x140103480
typedef void  (*fn_MarkForRemoval_t)(int64_t entity);           // 0x1400c2ea0
typedef void  (*fn_DynArrInsert_t)(int64_t* arr, int count, void* ibp); // 0x140003110
typedef void* (*fn_CopyCtor_t)(void* mem, void* templ);         // 0x1400c3720
typedef void  (*fn_RegEntity_t)(void* entityList, void* entityIBP, int category); // 0x1400c48a0
typedef void  (*fn_SetTransform_t)(int64_t fp, void* xyz, int yaw);  // 0x140003870
typedef void  (*fn_SetOrient_t)(int64_t fp, uint16_t orient, int16_t p3); // 0x1401253d0
typedef void* (*fn_Work_Ctor25_t)(void* self, void* pEntity);   // 0x1400f72e0

static fn_FindIBP_t         s_find3050  = nullptr;  // production yard
static fn_FindIBP_t         s_findD6c0  = nullptr;  // build-material yard (+0x48 delivered, +0x4c mult)
static fn_FindIBP_t         s_find2e10  = nullptr;  // workplace-ext
static fn_FindIBP_t         s_find4740  = nullptr;  // yard0 (+0x58 multiplier)
static fn_FindIBP_t         s_find4680  = nullptr;  // FootprintComponent (type 1401c79e0)
static fn_FindIBP_t         s_find3f60  = nullptr;  // model/transform comp (ClearPathQueue)
static fn_HasWork_t         s_ead0      = nullptr;
static fn_StopWork_t        s_eb50      = nullptr;
static fn_PostWork_t        s_ec60      = nullptr;
static fn_ReleaseWorkers_t  s_f670      = nullptr;  // WorkPlaceComponent_LeaveBuilderProfession
static fn_TradeNotify_t     s_trade3480 = nullptr;
static fn_MarkForRemoval_t  s_markRemoval = nullptr;
static fn_DynArrInsert_t    s_dynInsert = nullptr;
static fn_CopyCtor_t        s_copyCtor  = nullptr;
static fn_RegEntity_t       s_regEntity = nullptr;
static fn_SetTransform_t    s_setTransform = nullptr;
static fn_SetOrient_t       s_setOrient = nullptr;
static fn_Work_Ctor25_t     s_workCtor25 = nullptr;
static void*                s_clearStorageVtbl = nullptr;  // 0x1401611a0
static uint32_t*            s_tidDestroyDesc = nullptr;    // DWORD_1401c7628

static bool s_resolved = false;
static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_find3050  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_findD6c0  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x14000d6c0));
    s_find2e10  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_find4740  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140004740));
    s_find4680  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_find3f60  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_ead0      = reinterpret_cast<fn_HasWork_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_eb50      = reinterpret_cast<fn_StopWork_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_ec60      = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_f670      = reinterpret_cast<fn_ReleaseWorkers_t>(ResolveAddr(Module::Exe, 0x14012f670));
    s_trade3480 = reinterpret_cast<fn_TradeNotify_t>(ResolveAddr(Module::Exe, 0x140103480));
    s_markRemoval = reinterpret_cast<fn_MarkForRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_dynInsert = reinterpret_cast<fn_DynArrInsert_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_copyCtor  = reinterpret_cast<fn_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_regEntity = reinterpret_cast<fn_RegEntity_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_setTransform = reinterpret_cast<fn_SetTransform_t>(ResolveAddr(Module::Exe, 0x140003870));
    s_setOrient = reinterpret_cast<fn_SetOrient_t>(ResolveAddr(Module::Exe, 0x1401253d0));
    s_workCtor25 = reinterpret_cast<fn_Work_Ctor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_clearStorageVtbl = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x1401611a0));
    s_tidDestroyDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7628));
}

// starttown.cpp — world-registration (region-map cells + pick mesh) for a building
// entity created at runtime; retail gets both from RegisterEntity's add-broadcast.
extern "C" void StartTown_RegisterRuntimeBuilding(void* entity);

// buildcomponent_state1.cpp — the construction-demo site/owner (standalone test scaffold).
extern "C" void* g_pConstructionDemoSite;
extern "C" void* g_pConstructionDemoEntity;

// Drop the construction-demo pointers when the building being retired owns them; they
// would otherwise dangle at a freed BuildComponent, and a reused heap block landing on
// the same address would compare equal to it in BuildComponent_DemoGateSkip. The spawn
// path retargets them to the replacement immediately afterwards.
static void Destroy_ReleaseDemoSite(int64_t buildComp) {
    if (buildComp && g_pConstructionDemoSite == reinterpret_cast<void*>(buildComp)) {
        g_pConstructionDemoSite   = nullptr;
        g_pConstructionDemoEntity = nullptr;
    }
}

// keystone_a_removalcalcpath.cpp — disasm-verified work-manager AVL lookup
// (walks the tree at param_1+0x30 for payload key == param_3).
extern "C" void* RemovalCalc_helper_8d50(int64_t param_1, void** param_2, int param_3);
// buildstructure.cpp — full retail BuildStructure ctor (0x1400e59c0 port).
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim);
// action_base_rawmat.cpp — RawMaterialComponent finder over an entity's IBP array
// (self+0x30 ptrs / +0x38 count), GetTypeId vs DAT_1401c7c80 with a GetTypeIdByName
// fallback; ctors retIBP from the found component. Reused verbatim by the scatter path.
// (C++ linkage — declared to match the definition's mangled symbol.)
void* ActRm_FindComponent_223c0(uintptr_t entity, void* retIBP);

static bool Trace() {
    static const bool t = [] {
        const char* e = std::getenv("BANISHED_DESTROY_TRACE");
        return e && e[0] && e[0] != '0';
    }();
    return t;
}

// ============================================================
// Worker release / builder re-seat gate (BANISHED_LIVE_DESTROY_WORKERS)
// ============================================================
// The WorkPlaceComponent half of a demolition: state-4 enter re-seats the workplace onto
// the BUILDER profession (FUN_14012f520) and the state-5 tick takes it back off before the
// old entity dies (FUN_14012f670). Both are now ported+registered (see
// destroycomponent_workerrelease.cpp and reg_destroy_workers.cpp) but stay OPT-IN: they
// drive live citizens through the release path mid-demolish, which is exactly the churn
// that has faulted before. With the flag unset the calls are skipped as they were, and the
// roster is left as-is — the state that produced the "READ of 0x8 walking a paused
// workplace roster" crash on 2026-07-25. Graduate into kDefaultOnFlags once a staffed
// building can be demolished AND upgraded repeatedly without a fault.
// component_factory_base.h isn't included in this TU; declare the one entry point.
namespace ComponentFactory { bool LiveEnabled(const char* envVar); }
static bool DestroyWorkersOn() {
    static const int s = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_DESTROY_WORKERS") ? 1 : 0;
    return s == 1;
}

// ============================================================
// Demolition-labor progress bridge (BANISHED_DESTROY_SYNC_LABOR, default ON when
// LIVE_DESTROY is up; only "=0" disables). Destroy-side twin of the construction
// pipeline's BANISHED_BUILD_SYNC_LABOR (construction_force_harness.cpp
// CreditBuildProgress).
//
// In retail, DestroyComponent+0x54 (demolition progress) is raised one unit per
// completed builder demolish-PerformWork, which posts a SendComponentMessage
// carrying the DestroyComponent type-id → vtable slot 15 (DestroyComponent_MsgReceive
// 0x14011daf0) → ++comp+0x54. The standalone CitizenAI executor never reaches that
// timed-work completion node (the identical gap the gather/build halves hit at
// DropOff→CompleteTask and PerformWork→component-message), so +0x54 stays 0 and the
// state-5 tick (0x14011c120) wedges forever at its progress<target gate — the
// "deconstruction stuck at N%" symptom.
//
// Stand in for the missing message: credit +0x54 toward the +0x58 target computed by
// state-5 enter, each tick, ramped over BANISHED_DESTROY_DEMO_FRAMES (default 600 ≈
// 10s) so the bar is watchable. Runs only inside the state-5 tick (progress<target),
// exactly where the real labor message would fire. Faithful to the retail end-state:
// once +0x54 reaches +0x58 the tick proceeds to worker-release + spawn/scatter.
static bool DemoSyncLaborOn() {
    static const int s = [] {
        const char* e = std::getenv("BANISHED_DESTROY_SYNC_LABOR");
        return (e && e[0] == '0') ? 0 : 1;   // default ON; only "=0" disables
    }();
    return s == 1;
}
static void CreditDemolitionProgress(uintptr_t comp) {
    int target = static_cast<int>(*reinterpret_cast<uint32_t*>(comp + 0x58));
    if (target <= 0) return;   // no labor requirement (e.g. REMOVE branch) → nothing to credit

    static const int rampFrames = [] {
        const char* e = std::getenv("BANISHED_DESTROY_DEMO_FRAMES");
        int n = (e && e[0]) ? std::atoi(e) : 600;
        return n < 1 ? 600 : n;
    }();
    int inc = target / rampFrames;
    if (inc < 1) inc = 1;
    int* prog = reinterpret_cast<int*>(comp + 0x54);
    int before = *prog;
    int p = before + inc;
    if (p > target) p = target;
    *prog = p;
    if (Trace()) {
        static int s_last = -0x7fffffff;
        if (p != s_last) {
            s_last = p;
            std::fprintf(stderr, "[DestroySM] demo-labor bridge comp+0x54 %d -> %d / target %d (inc %d)\n",
                         before, p, target, inc);
            std::fflush(stderr);
        }
    }
}

// Owner-entity value from the component's IBP at +0x10.
static int64_t OwnerEntity(uintptr_t comp) {
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    return *reinterpret_cast<int64_t*>(comp + 0x10);
}

static int64_t FindOn(int64_t entity, fn_FindIBP_t finder) {
    if (!entity || !finder) return 0;
    int64_t out = 0;
    finder(entity, &out);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&out);
    return out;
}

// ============================================================
// DestroyDescription_helper_sub_da80 (0x14011da80) — storage-empty test.
// !storage || StorageComponent_QueryCount(storage) == 0.
// ============================================================
extern "C" char Destroy_StorageEmpty_da80(int64_t comp) {
    EnsureResolved();
    int64_t entity = OwnerEntity(static_cast<uintptr_t>(comp));
    if (!entity) return 1;
    void* storage = nullptr;
    // Ghidra-verified: retail da80 calls FUN_140006270 (StorageComponent
    // finder, DAT_1401c7e78) — NOT the ConsumeProduce finder.
    fn_FindComponent_Storage(reinterpret_cast<void*>(entity), &storage);
    if (!storage) return 1;
    return fn_StorageComponent_QueryCount(storage) == 0 ? 1 : 0;
}

// ============================================================
// DestroyDescription_helper_ea20 (0x14012ea20) — reclaim-job-pending test.
// Looks up the yard's resource key (+0x28) in the work-manager tree
// ((gWorld+0x118)+0x30) and calls the found node's vtable+0x58.
// ============================================================
static char Destroy_ReclaimPending_ea20(int64_t yard) {
    EnsureResolved();
    if (!yard) return 0;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t mgr = *reinterpret_cast<int64_t*>(gw + 0x118);
    if (!mgr) return 0;
    int key = *reinterpret_cast<int*>(yard + 0x28);
    void* node = nullptr;
    RemovalCalc_helper_8d50(mgr, &node, key);
    if (!node) return 0;
    void** vt = *reinterpret_cast<void***>(node);
    if (!vt) return 0;
    typedef char (*fn_slot58_t)(void*);
    fn_slot58_t fn = reinterpret_cast<fn_slot58_t>(
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(vt) + 0x58));
    return fn ? fn(node) : 0;
}

// ============================================================
// DestroyUI_helper_e600 (0x14011e600) — target differs from both the entity's
// current description AND the DestroyDescription's default target.
// DestroyDescription_helper_e5b0 (0x14011e5b0) — target == entity's current.
// ============================================================
extern "C" char Destroy_TargetDiffers_e600(int64_t comp) {
    EnsureResolved();
    void* desc = *reinterpret_cast<void**>(comp + 0x18);
    void* typed = (desc && fn_IsDerivedFrom && s_tidDestroyDesc && *s_tidDestroyDesc &&
                   fn_IsDerivedFrom(desc, *s_tidDestroyDesc)) ? desc : nullptr;
    int64_t target = *reinterpret_cast<int64_t*>(comp + 0x60);
    int64_t entity = OwnerEntity(static_cast<uintptr_t>(comp));
    if (entity && target == *reinterpret_cast<int64_t*>(entity + 0x28)) return 0;
    if (typed && target == *reinterpret_cast<int64_t*>(
            reinterpret_cast<uintptr_t>(typed) + 0x20)) return 0;
    return 1;
}
extern "C" char Destroy_TargetIsCurrent_e5b0(int64_t comp) {
    EnsureResolved();
    int64_t target = *reinterpret_cast<int64_t*>(comp + 0x60);
    int64_t entity = OwnerEntity(static_cast<uintptr_t>(comp));
    if (!entity) return 0;
    return target == *reinterpret_cast<int64_t*>(entity + 0x28) ? 1 : 0;
}

// ============================================================
// Scatter-path helpers (0x14011c530 dependencies), resolved once.
//   0x14009f3e0  grid->world terrain query (returns out3, [2]=height). Registered
//                mapquery, exercised in-game by ~100 sites.
//   0x140129760  CreateRawMaterial_helper_9760 (PositionEntity) — places the
//                cloned pile on a cell with a yaw. Registered (reg_action_rawmat).
//   0x1401c7cb8  RawMaterialDescription type-id; 0x1401c7040 BuildDescription.
// .rdata float constants (exe values verified via read_memory):
//   DAT_140172114 = 0.5f  (cell-centre offset), DAT_1401720fc = 0.2f (max tile
//   height for a valid drop), DAT_140172204 = 90.0f (RandTol yaw scale).
// ============================================================
typedef float* (*fn_GridToWorld_t)(int* grid, float* out3, float* in2, char flag, void* pick);
typedef void   (*fn_Pos9760_t)(int64_t rawMatComp, int cell, float yaw);
static fn_GridToWorld_t s_gridToWorld   = nullptr;  // 0x14009f3e0
static fn_Pos9760_t     s_pos9760       = nullptr;  // 0x140129760
static const uint32_t*  s_tidRawMatDesc = nullptr;  // 0x1401c7cb8
static const uint32_t*  s_tidBuildDesc  = nullptr;  // 0x1401c7040
static bool s_scatterResolved = false;
static void EnsureScatterResolved() {
    if (s_scatterResolved) return;
    s_scatterResolved = true;
    s_gridToWorld   = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_pos9760       = reinterpret_cast<fn_Pos9760_t>(ResolveAddr(Module::Exe, 0x140129760));
    s_tidRawMatDesc = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_tidBuildDesc  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7040));
}

// RawMaterialDescription type-id, with the proven GetTypeIdByName fallback for the
// standalone case where the shared .rdata global is left unfilled (0/0xFFFFFFFF).
static uint32_t RawMatDescTid() {
    uint32_t t = s_tidRawMatDesc ? *s_tidRawMatDesc : 0;
    if (t == 0 || t == 0xFFFFFFFFu) {
        static uint32_t cached = 0;
        if (cached == 0) cached = ::Object::GetTypeIdByName("RawMaterialDescription");
        t = cached;
    }
    return t;
}

// Find the RawMaterialDescription in a resource TEMPLATE's component-description
// array (template+0x28 ptrs, count template+0x30) via GetTypeId (vtable+0x10). Its
// +0x48 (stack size) caps each dropped pile at 100/stackSize (retail c530). Raw
// pointers, not IBPs — no CheckPtr (matches FUN_140088f90).
static void* FindRawMatDescInTemplate(int64_t tmpl) {
    uint32_t tid = RawMatDescTid();
    int n = *reinterpret_cast<int*>(tmpl + 0x30);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(tmpl + 0x28);
    for (int i = 0; i < n; i++) {
        void* c = *reinterpret_cast<void**>(base + static_cast<uintptr_t>(i) * 8);
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(c) + 0x10))(c);
        if (t == tid)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

// ============================================================
// sub_c530 (0x14011c530) — scatter reclaimed contents as RawMaterial ground piles.
// Faithful port. Two-phase:
//   Phase 1 builds a {template, count} list. If the building was only PARTIALLY
//   built (BuildComponent delivered < required), spill the actual Storage stacks
//   ((count-committed)/(delivered?2:1)); otherwise refund the recipe materials
//   (recipe.count / (comp+0x50 ? 4 : 2)) — the "recover half your resources" spill.
//   Phase 2 walks the footprint tiles (width×depth from baseX,baseY), and on each
//   empty (cell+0==0) walkable (cell+0x29==2) tile whose queried terrain height is
//   in [0, 0.2), clones the resource template, positions it (9760) with a random
//   yaw, sets amount(+0x28)/clears +0x30, and registers it into the world. The
//   placement cursor (col,row) persists across resource types. Overflow beyond the
//   footprint (row reaches depth) is dropped, exactly as retail.
// The register category is read from recipe[pairIndex]->+0x20 (retail indexes the
// build recipe by the scatter-pair index; benign for the ordered partial-build case).
// ============================================================
static void Destroy_ScatterContents_c530(int64_t comp, int baseX, int baseY,
                                          int width, int depth) {
    EnsureResolved();
    EnsureScatterResolved();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t entity = OwnerEntity(static_cast<uintptr_t>(comp));
    if (!entity) return;

    // BuildComponent + its BuildDescription (+0x18). Scatter is only reached with
    // comp+0x51 set, i.e. the build comp existed — guard defensively regardless.
    int64_t buildComp = FindOn(entity, s_findD6c0);
    if (!buildComp) return;
    void* rawDesc = *reinterpret_cast<void**>(buildComp + 0x18);
    if (!rawDesc) return;
    // Retail narrows the desc to BuildDescription (DWORD_1401c7040) but then derefs
    // it unconditionally (required@+0x30, recipe@+0x20/+0x28) — the invariant is that
    // a BuildComponent always carries a BuildDescription. We trust the pointer (the
    // derive check would only null it, which retail never survives). s_tidBuildDesc
    // is resolved for parity / future use.
    (void)s_tidBuildDesc;
    uintptr_t desc = reinterpret_cast<uintptr_t>(rawDesc);

    int delivered = *reinterpret_cast<int*>(buildComp + 0x48);
    int required  = *reinterpret_cast<int*>(desc + 0x30) *
                    *reinterpret_cast<int*>(buildComp + 0x4c);
    uintptr_t recipeBase = *reinterpret_cast<uintptr_t*>(desc + 0x20);

    // ---- Phase 1: scatter list ----
    struct Pair { int64_t tmpl; int count; };
    std::vector<Pair> list;

    if (delivered < required) {
        // Partially built: spill the real Storage contents.
        void* storage = nullptr;
        fn_FindComponent_Storage(reinterpret_cast<void*>(entity), &storage);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storage);
        if (storage) {
            uintptr_t st = reinterpret_cast<uintptr_t>(storage);
            int nStacks = *reinterpret_cast<int*>(st + 0x30);
            uintptr_t stacks = *reinterpret_cast<uintptr_t*>(st + 0x28);
            int divisor = (delivered > 0) ? 2 : 1;
            for (int i = 0; i < nStacks; i++) {
                uintptr_t s = stacks + static_cast<uintptr_t>(i) * 0x10;
                int f8 = *reinterpret_cast<int*>(s + 8);
                int fc = *reinterpret_cast<int*>(s + 0xc);
                list.push_back({*reinterpret_cast<int64_t*>(s), (f8 - fc) / divisor});
            }
        }
    } else {
        // Fully built: refund half (or a quarter, when comp+0x50) of the recipe.
        int divisor = (*reinterpret_cast<int8_t*>(comp + 0x50) != 0) ? 4 : 2;
        int nRecipe = *reinterpret_cast<int*>(desc + 0x28);
        for (int i = 0; i < nRecipe; i++) {
            uintptr_t e = recipeBase + static_cast<uintptr_t>(i) * 0x10;
            list.push_back({*reinterpret_cast<int64_t*>(e),
                            *reinterpret_cast<int*>(e + 8) / divisor});
        }
    }
    if (list.empty()) return;

    // ---- Phase 2: place piles on empty walkable footprint tiles ----
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);
    if (!grid) return;
    int gw_width = *grid;
    if (gw_width <= 0) return;
    int64_t cellArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(grid) + 0x80);
    void* random = *reinterpret_cast<void**>(gw + 0xd8);
    void* entityList = *reinterpret_cast<void**>(gw + 0xe8);

    const float kHalfCell  = 0.5f;   // DAT_140172114
    const float kMaxHeight = 0.2f;   // DAT_1401720fc
    const float kYawScale  = 90.0f;  // DAT_140172204

    int placed = 0;
    int col = 0;   // local_148 — persists across resource types
    int row = 0;   // local_128

    for (size_t pi = 0; pi < list.size(); pi++) {
        int64_t tmpl = list[pi].tmpl;
        int remaining = list[pi].count;
        if (remaining <= 0) continue;

        int rowWorld = baseY + row;
        do {
            int cell = rowWorld * gw_width + baseX + col;
            float centre = static_cast<float>(gw_width >> 1) - kHalfCell;
            float in2[3] = { static_cast<float>(cell % gw_width) - centre,
                             static_cast<float>(cell / gw_width) - centre, 0.0f };
            float w0[3] = {0, 0, 0}, w1[3] = {0, 0, 0};
            if (s_gridToWorld) {
                s_gridToWorld(grid, w0, in2, 0, nullptr);
                s_gridToWorld(grid, w1, w0, 0, nullptr);
            }
            uintptr_t cp = static_cast<uintptr_t>(cellArr) +
                           static_cast<uintptr_t>(cell) * 0x30;
            bool valid = (*reinterpret_cast<int64_t*>(cp) == 0) &&
                         (*reinterpret_cast<int8_t*>(cp + 0x29) == 2) &&
                         (0.0f <= w1[2]) && (w1[2] < kMaxHeight);
            if (valid) {
                void* rmDesc = FindRawMatDescInTemplate(tmpl);
                int stackSize = rmDesc ? *reinterpret_cast<int*>(
                                    reinterpret_cast<uintptr_t>(rmDesc) + 0x48) : 0;
                if (stackSize > 0) {
                    int per = 100 / stackSize;
                    if (per > remaining) per = remaining;

                    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                                    ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
                    void* newEntity = (mem && s_copyCtor)
                                          ? s_copyCtor(mem, reinterpret_cast<void*>(tmpl)) : nullptr;
                    if (newEntity) {
                        int64_t entIBP = 0;
                        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entIBP, newEntity);
                        else entIBP = reinterpret_cast<int64_t>(newEntity);
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entIBP);

                        // RawMaterialComponent on the new entity → position + amount.
                        int64_t rmIBP = 0;
                        ActRm_FindComponent_223c0(static_cast<uintptr_t>(entIBP), &rmIBP);
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rmIBP);
                        if (rmIBP) {
                            int yaw = (fn_Random_RandTol && random)
                                          ? fn_Random_RandTol(random, 2) : 0;
                            if (s_pos9760)
                                s_pos9760(rmIBP, cell, static_cast<float>(yaw) * kYawScale);
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rmIBP);
                            *reinterpret_cast<int*>(rmIBP + 0x28) = per;
                            *reinterpret_cast<int64_t*>(rmIBP + 0x30) = 0;
                        }

                        // Register into the world with the recipe category.
                        int category = 0;
                        if (recipeBase) {
                            int64_t catTmpl = *reinterpret_cast<int64_t*>(
                                recipeBase + pi * 0x10);
                            if (catTmpl) category = *reinterpret_cast<int*>(catTmpl + 0x20);
                        }
                        int64_t regIBP = entIBP;
                        if (s_regEntity && entityList) s_regEntity(entityList, &regIBP, category);

                        remaining -= per;
                        placed++;
                    }
                }
            }

            if (++col >= width) {
                col = 0;
                row++;
                rowWorld++;
            }
        } while (row != depth && remaining > 0);
    }

    if (Trace()) {
        std::fprintf(stderr, "[DestroySM] scatter c530: %zu type(s), placed %d pile(s) "
                             "over footprint (%d,%d) %dx%d\n",
                     list.size(), placed, baseX, baseY, width, depth);
        std::fflush(stderr);
    }
}

// Mark the owner entity for removal + push onto the EntityList removal drain —
// the exact guard+push idiom of clearresourcecomponent_states.cpp (the 0x400
// bit keeps sibling controllers from double-pushing the same entity).
static void Destroy_MarkAndPushRemoval(int64_t entity) {
    if (!entity) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t entityList = *reinterpret_cast<int64_t*>(gw + 0xe8);
    if (!entityList) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
    if (((*reinterpret_cast<uint32_t*>(entity + 0x40) >> 10) & 1) != 0) return;
    if (s_markRemoval) s_markRemoval(entity);
    if (s_dynInsert)
        s_dynInsert(reinterpret_cast<int64_t*>(entityList + 0xd0),
                    *reinterpret_cast<int*>(entityList + 0xd8) + 1, &entity);
}

// d6c0 yard material math: delivered (+0x48), required = desc(+0x18)->+0x30 *
// requiredMult (+0x4c).
static int Yard_Delivered(int64_t prod) {
    return prod ? *reinterpret_cast<int*>(prod + 0x48) : 0;
}
static int Yard_Required(int64_t prod) {
    if (!prod) return 0;
    void* desc = *reinterpret_cast<void**>(prod + 0x18);
    if (!desc) return 0;
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x30) *
           *reinterpret_cast<int*>(prod + 0x4c);
}

// ============================================================
// State 3 enter — 0x14011d7b0 (begin demolition).
// ============================================================
extern "C" char Destroy_State3Enter_d7b0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    *reinterpret_cast<int*>(comp + 0x54) = 0;   // reset progress

    int64_t entity = OwnerEntity(comp);

    // Stop pending production work on the yard (FUN_14012ead0/FUN_14012eb50).
    int64_t yard = FindOn(entity, s_find3050);
    if (yard && s_ead0 && s_eb50 && s_ead0(yard)) s_eb50(yard);

    // STANDALONE: status-icon strip (StatusIconComponent_helper_c2a0) and the
    // render-scene mesh removal / rubble swap are skipped — visual-only, and the
    // standalone mesh pipeline is partial (same policy as ResidenceInit's
    // SetMeshFlag skip in residencecomponent.cpp).

    int nextState = (*reinterpret_cast<int8_t*>(comp + 0x50) == 0) ? 4 : 6;
    ::ControllerLive::RequestStateTransition(sm, nextState);

    if (Trace()) {
        std::fprintf(stderr, "[DestroySM] state-3 enter comp=%p target(+0x60)=%llx -> %d\n",
                     data, static_cast<unsigned long long>(
                         *reinterpret_cast<int64_t*>(comp + 0x60)), nextState);
        std::fflush(stderr);
    }
    return 1;
}

// ============================================================
// State 4 enter — 0x14011cfd0 (queue the haul-away).
// ============================================================
extern "C" char Destroy_State4Enter_cfd0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);

    // Re-seat the workplace onto the BUILDER profession (retail FUN_14012f520 on the 2e10
    // comp) — ported in destroycomponent_workerrelease.cpp, opt-in per DestroyWorkersOn().
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        typedef void (*fn_f520_t)(int64_t);
        static fn_f520_t f520 = reinterpret_cast<fn_f520_t>(
            ResolveAddr(Module::Exe, 0x14012f520));
        if (f520 && DestroyWorkersOn()) {
            f520(wp);
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[DestroySM] FUN_14012f520 (state-4 workplace re-seat) "
                             "SKIPPED — %s. The workplace keeps its pre-demolition profession "
                             "and roster.\n",
                             f520 ? "BANISHED_LIVE_DESTROY_WORKERS is not set (opt-in)"
                                  : "NOT REGISTERED (resolved to 0)");
                std::fflush(stderr);
            }
        }
    }

    int64_t prod = FindOn(entity, s_findD6c0);
    if (prod) *reinterpret_cast<int8_t*>(comp + 0x51) = 1;

    // Storage already empty, or build materials never fully delivered
    // (partially-built building has nothing to haul) -> straight to state 5.
    if (Destroy_StorageEmpty_da80(static_cast<int64_t>(comp)) ||
        (prod && Yard_Delivered(prod) < Yard_Required(prod))) {
        ::ControllerLive::RequestStateTransition(sm, 5);
        return 1;
    }

    // Queue a ClearStorage work (type 1) on the production yard. Faithful ctor
    // path: Work_Ctor25 base bind (field_10 = entity) + ClearStorage vtable +
    // zeroed tail fields (ClearStorage_Ctor 0x1400e74b0 shape).
    int64_t yard = FindOn(entity, s_find3050);
    if (yard && s_ec60 && s_workCtor25 && s_clearStorageVtbl &&
        fn_MemAlloc && g_pMemAllocSingleton) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
        if (mem) {
            int64_t entityTmp = entity;
            s_workCtor25(mem, &entityTmp);
            *reinterpret_cast<void**>(mem) = s_clearStorageVtbl;
            uintptr_t w = reinterpret_cast<uintptr_t>(mem);
            *reinterpret_cast<int64_t*>(w + 0x48) = 0;
            *reinterpret_cast<int64_t*>(w + 0x50) = 0;
            *reinterpret_cast<int64_t*>(w + 0x58) = 0;
            *reinterpret_cast<int64_t*>(w + 0x60) = 0;
            int64_t workIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIBP, mem);
            else workIBP = reinterpret_cast<int64_t>(mem);
            s_ec60(yard, 1, 0, &workIBP);
            if (Trace()) {
                std::fprintf(stderr, "[DestroySM] state-4 enter: queued ClearStorage "
                                     "work=%p on yard=%llx\n",
                             mem, static_cast<unsigned long long>(yard));
                std::fflush(stderr);
            }
        }
    }
    return 1;
}

// ============================================================
// State 4 tick — 0x14011cec0 (per-frame: storage drained -> state 5).
// ============================================================
extern "C" char Destroy_State4Tick_cec0(void* data, void* sm, void* /*time*/) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    if (!Destroy_StorageEmpty_da80(static_cast<int64_t>(comp))) return 1;
    int64_t entity = OwnerEntity(comp);
    int64_t yard = FindOn(entity, s_find3050);
    if (!Destroy_ReclaimPending_ea20(yard)) {
        ::ControllerLive::RequestStateTransition(sm, 5);
        if (yard && s_eb50) s_eb50(yard);
        if (Trace()) {
            std::fprintf(stderr, "[DestroySM] state-4 tick: storage drained -> 5\n");
            std::fflush(stderr);
        }
    }
    return 1;
}

// ============================================================
// State 5 enter — 0x14011cc70 (refund + queue BuildStructure, or finish up).
// ============================================================
extern "C" char Destroy_State5Enter_cc70(void* data, void* sm) {
    EnsureResolved();
    (void)sm;
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);
    int64_t prod = FindOn(entity, s_findD6c0);

    // Diagnostic: the refund+BuildStructure branch (which sets the +0x58 progress
    // target that gates the replacement spawn) fires ONLY when the building has a
    // live BuildComponent (d6c0) with Delivered(+0x48) != 0. When it doesn't, we
    // fall to the ELSE branch: scatter + immediate mark-for-removal, NO spawn — the
    // "building just vanishes" symptom. Log the decision inputs so a live upgrade
    // shows exactly why (prod null vs Delivered==0).
    if (Trace()) {
        std::fprintf(stderr, "[DestroySM] state-5 enter DECIDE: buildComp(d6c0)=%llx "
                     "delivered(+0x48)=%d required=%d target(+0x60)=%llx -> %s branch\n",
                     static_cast<unsigned long long>(prod),
                     Yard_Delivered(prod), Yard_Required(prod),
                     static_cast<unsigned long long>(*reinterpret_cast<int64_t*>(comp + 0x60)),
                     (prod && Yard_Delivered(prod) != 0) ? "REFUND+BUILD" : "REMOVE(no-spawn)");
        std::fflush(stderr);
    }

    if (prod && Yard_Delivered(prod) != 0) {
        // Half-materials refund into +0x58.
        int64_t yard0 = FindOn(entity, s_find4740);
        int mult = yard0 ? *reinterpret_cast<int*>(yard0 + 0x58) : 1;
        void* desc = *reinterpret_cast<void**>(prod + 0x18);
        int base = desc ? *reinterpret_cast<int*>(
                       reinterpret_cast<uintptr_t>(desc) + 0x30) : 0;
        int recovered = (base * mult) / 2;
        if (recovered == 0) recovered = 1;
        *reinterpret_cast<int*>(comp + 0x58) = recovered;

        // Queue the BuildStructure work (type 0x2001) via the full retail ctor.
        int64_t yard = FindOn(entity, s_find3050);
        if (yard && s_ec60 && fn_MemAlloc && g_pMemAllocSingleton) {
            void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8);
            if (mem) {
                int64_t entityIBP = entity;
                BuildStructure_CreateInPlaceFull(mem, &entityIBP, 1);
                int64_t workIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIBP, mem);
                else workIBP = reinterpret_cast<int64_t>(mem);
                s_ec60(yard, 0x2001, 0, &workIBP);
            }
        }
        if (Trace()) {
            std::fprintf(stderr, "[DestroySM] state-5 enter: refund=%d, queued "
                                 "BuildStructure\n", recovered);
            std::fflush(stderr);
        }
    } else {
        // Nothing recoverable: scatter old contents (over the footprint) + notify
        // trade + mark the old entity for removal.
        if (*reinterpret_cast<int8_t*>(comp + 0x51)) {
            int64_t fp = FindOn(entity, s_find4680);
            if (fp)
                Destroy_ScatterContents_c530(static_cast<int64_t>(comp),
                    *reinterpret_cast<int16_t*>(fp + 0x28),
                    *reinterpret_cast<int16_t*>(fp + 0x2a),
                    *reinterpret_cast<int16_t*>(fp + 0x2c),
                    *reinterpret_cast<int16_t*>(fp + 0x2e));
        }
        if (s_trade3480 && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
            int64_t tradeMgr = *reinterpret_cast<int64_t*>(gw + 0x120);
            int64_t entityIBP = entity;
            if (tradeMgr) s_trade3480(tradeMgr, &entityIBP);
        }
        Destroy_ReleaseDemoSite(prod);   // nothing replaces it on this branch
        Destroy_MarkAndPushRemoval(entity);
    }
    return 1;
}

// ============================================================
// State 5 tick — 0x14011c120 ★ the replacement spawn ★.
// ============================================================
extern "C" char Destroy_State5Tick_c120(void* data, void* sm, void* /*time*/) {
    EnsureResolved();
    (void)sm;
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);

    // 0) RETAIL GATE (0x14011c120 first branch): wait for the demolition work.
    // +0x54 = progress (credited one unit per completed builder PerformWork via
    // vtable slot 15 / DestroyComponent_MsgReceive), +0x58 = the target the
    // state-5 enter computed. Until progress reaches the target the tick is a
    // pure no-op — no worker release, no trade notify, no mark-for-removal, no
    // spawn. Missing this gate raced the whole teardown on the first tick
    // (observed live 2026-07-20: instant mark-for-removal froze the open panel
    // via the 0x400 bit and the upgrade spawn logic misfired).
    //
    // Executor-gap bridge: the organic +0x54 credit (a completed builder demolish
    // PerformWork → SendComponentMessage → slot 15) never fires in standalone, so
    // credit it here toward the target, standing in for the missing labor message.
    if (DemoSyncLaborOn()) CreditDemolitionProgress(comp);

    if (*reinterpret_cast<int*>(comp + 0x54) <
        static_cast<int>(*reinterpret_cast<uint32_t*>(comp + 0x58))) {
        if (Trace()) {
            static int throttle = 0;
            if ((throttle++ & 0x3f) == 0) {
                std::fprintf(stderr, "[DestroySM] state-5 tick: waiting demolition "
                                     "progress %d/%u\n",
                             *reinterpret_cast<int*>(comp + 0x54),
                             *reinterpret_cast<uint32_t*>(comp + 0x58));
                std::fflush(stderr);
            }
        }
        return 1;
    }

    int64_t entity = OwnerEntity(comp);
    if (!entity) return 1;

    // 0b) Retail: stop the yard's pending work now that demolition is complete
    // (FUN_140003050 -> FUN_14012eb50) before capturing the footprint.
    {
        int64_t yard = FindOn(entity, s_find3050);
        if (yard && s_eb50) s_eb50(yard);
    }

    // 1) Capture the OLD footprint (grid cell + rotation) and world transform.
    int64_t fp = FindOn(entity, s_find4680);
    int16_t sx = 0, sy = 0, sc = 0, sd = 0, rot = 0;
    if (fp) {
        sx = *reinterpret_cast<int16_t*>(fp + 0x28);
        sy = *reinterpret_cast<int16_t*>(fp + 0x2a);
        sc = *reinterpret_cast<int16_t*>(fp + 0x2c);
        sd = *reinterpret_cast<int16_t*>(fp + 0x2e);
        rot = *reinterpret_cast<int16_t*>(fp + 0x30);
    }
    int64_t pq = FindOn(entity, s_find3f60);
    float tf[3] = {0, 0, 0};
    int yaw = 0;
    uint16_t orient = 0;
    if (pq) {
        tf[0] = *reinterpret_cast<float*>(pq + 0x158);
        tf[1] = *reinterpret_cast<float*>(pq + 0x15c);
        tf[2] = *reinterpret_cast<float*>(pq + 0x160);
        orient = *reinterpret_cast<uint16_t*>(pq + 0x164);
        yaw = *reinterpret_cast<int*>(pq + 0x168);
    }

    // 2) Release workers from the old workplace, i.e. leave the BUILDER profession
    //    (FUN_14012f670 = workplacecomponent.cpp WorkPlaceComponent_LeaveBuilderProfession,
    //    registered by reg_destroy_workers.cpp). Opt-in per DestroyWorkersOn(). Skipping it
    //    is what leaves the roster half-torn-down on the entity we are about to remove.
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        if (s_f670 && DestroyWorkersOn()) {
            s_f670(wp);
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[DestroySM] FUN_14012f670 (worker release) SKIPPED — %s. "
                             "The removed entity keeps its workers and its Builder-bucket "
                             "registration.\n",
                             s_f670 ? "BANISHED_LIVE_DESTROY_WORKERS is not set (opt-in)"
                                    : "NOT REGISTERED (resolved to 0)");
                std::fflush(stderr);
            }
        }
    }

    // 3) Trade notify + mark the old entity for removal.
    if (s_trade3480 && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        int64_t tradeMgr = *reinterpret_cast<int64_t*>(gw + 0x120);
        int64_t entityIBP = entity;
        if (tradeMgr) s_trade3480(tradeMgr, &entityIBP);
    }
    Destroy_MarkAndPushRemoval(entity);

    int64_t prod = FindOn(entity, s_findD6c0);
    int64_t target = *reinterpret_cast<int64_t*>(comp + 0x60);
    const bool wasDemoSite =
        prod != 0 && g_pConstructionDemoSite == reinterpret_cast<void*>(prod);
    Destroy_ReleaseDemoSite(prod);

    if (target != 0) {
        bool ready = (prod && Yard_Delivered(prod) >= Yard_Required(prod)) ||
                     (*reinterpret_cast<int8_t*>(comp + 0x50) != 0);
        if (ready) {
            // Retail 0x14011c3xx: scatter when e600 != 0 OR e5b0 != 0 (the
            // previous port had the e600 sense inverted).
            if (*reinterpret_cast<int8_t*>(comp + 0x51)) {
                if (Destroy_TargetDiffers_e600(static_cast<int64_t>(comp)) ||
                    Destroy_TargetIsCurrent_e5b0(static_cast<int64_t>(comp)))
                    Destroy_ScatterContents_c530(static_cast<int64_t>(comp), sx, sy, sc, sd);
            }

            // >>> CREATE THE NEW BUILDING at the old footprint <<<
            if (s_copyCtor && s_regEntity && fn_MemAlloc && g_pMemAllocSingleton &&
                g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
                void* newEntity = mem ? s_copyCtor(
                    mem, *reinterpret_cast<void**>(comp + 0x60)) : nullptr;
                if (newEntity) {
                    int64_t hold = 0;
                    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&hold, newEntity);
                    else hold = reinterpret_cast<int64_t>(newEntity);

                    int64_t nfp = FindOn(reinterpret_cast<int64_t>(newEntity), s_find4680);
                    if (nfp) {
                        *reinterpret_cast<int16_t*>(nfp + 0x28) = sx;
                        *reinterpret_cast<int16_t*>(nfp + 0x2a) = sy;
                        *reinterpret_cast<int16_t*>(nfp + 0x2c) = sc;
                        *reinterpret_cast<int16_t*>(nfp + 0x2e) = sd;
                        int r = rot;
                        while (r < 0) r += 360;   // retail: += 0x168 until >= 0
                        *reinterpret_cast<int16_t*>(nfp + 0x30) = static_cast<int16_t>(r);
                    }
                    int64_t npq = FindOn(reinterpret_cast<int64_t>(newEntity), s_find3f60);
                    if (npq) {
                        if (s_setTransform) s_setTransform(npq, tf, yaw);
                        if (s_setOrient)    s_setOrient(npq, orient, 0);
                    }

                    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
                    void* entityList = *reinterpret_cast<void**>(gw + 0xe8);
                    int category = *reinterpret_cast<int*>(
                        *reinterpret_cast<uintptr_t*>(comp + 0x60) + 0x20);
                    if (entityList) s_regEntity(entityList, &hold, category);

                    // Standalone-only: give the replacement the two world registrations
                    // retail derives from RegisterEntity's add-broadcast — the region-map
                    // cell occupancy (click-pick + nav blocking + the armed removal clear)
                    // and the pick mesh. Our MapComponent slot 6 only paints the minimap
                    // and PickingComponent registers no mesh, so without this the new
                    // building is unclickable and leaves stale cells when it is removed.
                    StartTown_RegisterRuntimeBuilding(newEntity);

                    // Hand the construction-demo baton to the replacement when the
                    // building we just retired owned it (Destroy_ReleaseDemoSite cleared
                    // the pointers a moment ago), so BUILD_TRACE's demo logging follows
                    // the building that is now actually constructing.
                    if (wasDemoSite) {
                        int64_t newProd = FindOn(reinterpret_cast<int64_t>(newEntity), s_findD6c0);
                        g_pConstructionDemoSite   = reinterpret_cast<void*>(newProd);
                        g_pConstructionDemoEntity = newEntity;
                        if (Trace()) {
                            std::fprintf(stderr, "[DestroySM] demo site handed to the "
                                         "replacement: buildComp=%llx entity=%p\n",
                                         static_cast<unsigned long long>(newProd), newEntity);
                            std::fflush(stderr);
                        }
                    }

                    std::fprintf(stderr, "[DestroySM] UPGRADE SPAWN: new entity=%p from "
                                         "template=%llx at cell(%d,%d) rot=%d cat=%d\n",
                                 newEntity, static_cast<unsigned long long>(target),
                                 sx, sy, rot, category);
                    std::fflush(stderr);
                }
            }
            return 1;
        }
    }

    if (*reinterpret_cast<int8_t*>(comp + 0x51))
        Destroy_ScatterContents_c530(static_cast<int64_t>(comp), sx, sy, sc, sd);
    return 1;
}

// ============================================================
// State 6 enter — 0x14011d4d0. DEFERRED (rubble/collapse path).
// ============================================================
extern "C" char Destroy_State6Enter_d4d0(void* data, void* sm) {
    (void)data; (void)sm;
    static bool warned = false;
    if (!warned) {
        warned = true;
        std::fprintf(stderr, "[DestroySM] NOT-IMPLEMENTED state-6 collapse/rubble "
                             "(0x14011d4d0/d400/d490) — deferred stub\n");
        std::fflush(stderr);
    }
    return 1;
}

// ============================================================
// Dispatcher + state table for the DestroyComponent controller.
// Faithful graph from DestroyDescription_helper_dd00 (0x14011dd00); the
// per-transition action handlers (0x14011bcc0/d390/d2a0/d1a0) are deferred
// (null = graceful no-op in the dispatcher).
// ============================================================
namespace { struct DestroyCtrlTag {}; }
using DestroyDispatch = ::ControllerLive::Dispatcher<DestroyCtrlTag>;

extern "C" void** DestroyCtrl_NodeVtable() { return DestroyDispatch::Vtable(); }

extern "C" void DestroyCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = DestroyDispatch::table;
    if (t.count != 0) return;   // idempotent

    TableRegisterState(t, 3, reinterpret_cast<void*>(&Destroy_State3Enter_d7b0),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 6, nullptr);

    TableRegisterState(t, 6, reinterpret_cast<void*>(&Destroy_State6Enter_d4d0),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 7, nullptr);

    TableRegisterState(t, 7, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 0, nullptr);
    TableAddTransition(t, 8, nullptr);
    TableAddTransition(t, 5, nullptr);

    TableRegisterState(t, 4, reinterpret_cast<void*>(&Destroy_State4Enter_cfd0),
                       nullptr, reinterpret_cast<void*>(&Destroy_State4Tick_cec0),
                       nullptr);
    TableAddTransition(t, 5, nullptr);
    TableAddTransition(t, 2, nullptr);

    TableRegisterState(t, 5, reinterpret_cast<void*>(&Destroy_State5Enter_cc70),
                       nullptr, reinterpret_cast<void*>(&Destroy_State5Tick_c120),
                       nullptr);
    TableAddTransition(t, 0, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddTransition(t, 8, nullptr);
    TableAddTransition(t, 7, nullptr);
}
