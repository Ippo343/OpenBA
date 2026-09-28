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
//   state 6 (collapse/rubble):  enter 0x14011d4d0, update 0x14011d400, tick
//           0x14011d490 — PORTED in destroycomponent_state6.cpp (with the rubble
//           step 0x14011d5d0 and the material re-point 0x140125020). The arm fire
//           and tornado take: the model steps DOWN its damage-stage mesh variants
//           to the DestroyDescription+0x30 rubble floor as the +0x48 burn
//           countdown runs out. transitions {7}.
//   state 7 (terminal):         no handlers. transitions {0, 8, 5}.
//
// sub_4020 registers per-state EVENT handlers (node vtable slot 2 = PostEvent), which
// is how the DestroyUI panel's buttons reach this machine — NOT transition actions.
// State 7's two, 0x14011d2a0 (rebuild/reclaim) and 0x14011d1a0 (remove), are now
// PORTED below; they were null, and a null handler is a graceful no-op in the
// ControllerLive dispatcher, so every click on a destroyed building's panel was
// swallowed. Still DEFERRED (null): 0x14011d390 (state 6 event 2) and 0x14011bcc0
// (states 4 and 5, event 0).
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
//   * state 6's pick-mesh rebuild (0x140128f50) is skipped — see
//     destroycomponent_state6.cpp.
// NOW PORTED: sub_c530 contents-scatter (0x14011c530) — reclaimed goods spill as
// RawMaterial ground piles over the footprint (was: dropped). See below.
//
// Default-ON via the default-on table (graduated 2026-07-25, upgrade path verified
// end-to-end); BANISHED_LIVE_DESTROY is now a kill-switch (=0). See
// destroycomponent.cpp.

#include "engine_funcs.h"
#include "Work/ClearStorage.h"   // B.3.8 CB1a (Vtable)
#include "Work/Work.h"           // B.3.x W0: Work::GetPriority by name
#include "Component/RawMaterialDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/DestroyDescription.h"       // ENDGAME B.3.9 C1: 0x1401c7628 cell class
#include "Component/ModelDescription.h"         // ENDGAME B.3.9 C1: 0x1401c7a88 cell class
#include "resolve.h"
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (type-id fallback)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Component/controller_state_table.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

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
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.
typedef void* (*fn_CopyCtor_t)(void* mem, void* templ);         // 0x1400c3720
typedef void  (*fn_RegEntity_t)(void* entityList, void* entityIBP, int category); // 0x1400c48a0
// WARNING: THE THIRD ARG IS A FLOAT. 0x140003870 reads it from XMM2 (`movaps xmm6,xmm2`
// at +0x14) — the Microsoft x64 float slot, NOT R8D. Declaring it `int` put the value in
// R8D and left XMM2 holding stale garbage, so a rebuilt building came out at an arbitrary
// yaw (user-reported on a storage barn, 2026-08-23; the spawn trace showed
// yaw=1119092736 = 0x42B40000 = 90.0f — an obvious float bit pattern read as an int).
// Ghidra types it `undefined4` and passes the raw 4 bytes, which hides the mistake.
// Inverse of [reference_int_arg_typed_as_float_hides_at_index_zero].
typedef void  (*fn_SetTransform_t)(int64_t fp, void* xyz, float yaw);  // 0x140003870
typedef void  (*fn_SetOrient_t)(int64_t fp, uint16_t orient, int16_t p3); // 0x1401253d0

static fn_FindIBP_t         s_find3050  = nullptr;  // production yard
static fn_FindIBP_t         s_findD6c0  = nullptr;  // build-material yard (+0x48 delivered, +0x4c mult)
static fn_FindIBP_t         s_find2e10  = nullptr;  // workplace-ext
static fn_FindIBP_t         s_find4740  = nullptr;  // yard0 (+0x58 multiplier)
static fn_FindIBP_t         s_find4680  = nullptr;  // FootprintComponent (type 1401c79e0)
static fn_FindIBP_t         s_find3f60  = nullptr;  // model/transform comp (ClearPathQueue)
static fn_FindIBP_t         s_find2f90  = nullptr;  // Entity_FindComponent_MeshInstance
typedef void (*fn_MeshSetFlag_t)(void* meshComp, uint8_t flagIdx, char value); // 0x14012c3f0
static fn_MeshSetFlag_t     s_meshFlag  = nullptr;
static fn_HasWork_t         s_ead0      = nullptr;
static fn_StopWork_t        s_eb50      = nullptr;
static fn_PostWork_t        s_ec60      = nullptr;
static fn_ReleaseWorkers_t  s_f670      = nullptr;  // WorkPlaceComponent_LeaveBuilderProfession
static fn_TradeNotify_t     s_trade3480 = nullptr;
static fn_MarkForRemoval_t  s_markRemoval = nullptr;
static fn_CopyCtor_t        s_copyCtor  = nullptr;
static fn_RegEntity_t       s_regEntity = nullptr;
static fn_SetTransform_t    s_setTransform = nullptr;
static fn_SetOrient_t       s_setOrient = nullptr;

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
    s_find2f90  = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_meshFlag  = reinterpret_cast<fn_MeshSetFlag_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_ead0      = reinterpret_cast<fn_HasWork_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_eb50      = reinterpret_cast<fn_StopWork_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_ec60      = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_f670      = reinterpret_cast<fn_ReleaseWorkers_t>(ResolveAddr(Module::Exe, 0x14012f670));
    s_trade3480 = reinterpret_cast<fn_TradeNotify_t>(ResolveAddr(Module::Exe, 0x140103480));
    s_markRemoval = reinterpret_cast<fn_MarkForRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_copyCtor  = reinterpret_cast<fn_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_regEntity = reinterpret_cast<fn_RegEntity_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_setTransform = reinterpret_cast<fn_SetTransform_t>(ResolveAddr(Module::Exe, 0x140003870));
    s_setOrient = reinterpret_cast<fn_SetOrient_t>(ResolveAddr(Module::Exe, 0x1401253d0));
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

// ============================================================
// Worker release / builder re-seat
// ============================================================
// The WorkPlaceComponent half of a demolition: state-4 enter re-seats the workplace onto
// the BUILDER profession (FUN_14012f520) and the state-5 tick takes it back off before the
// old entity dies (FUN_14012f670). Both are ported+registered (see
// destroycomponent_workerrelease.cpp and reg_destroy_workers.cpp).


// Owner-entity value from the component's IBP at +0x10.
static int64_t OwnerEntity(uintptr_t comp) {
    return *reinterpret_cast<int64_t*>(comp + 0x10);
}

static int64_t FindOn(int64_t entity, fn_FindIBP_t finder) {
    if (!entity || !finder) return 0;
    int64_t out = 0;
    finder(entity, &out);
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
    // `node` is the gw+0x118 scheduler's work (id map filled by CitizenAI_impl_88a0
    // 0x1400f88a0 with the posted Work leaves); retail reads AL.
    return static_cast<char>(reinterpret_cast<Work*>(node)->GetPriority());   // B.3.x bridge (slot 11)
}

// ============================================================
// DestroyUI_helper_e600 (0x14011e600) — target differs from both the entity's
// current description AND the DestroyDescription's default target.
// DestroyDescription_helper_e5b0 (0x14011e5b0) — target == entity's current.
// ============================================================
extern "C" char Destroy_TargetDiffers_e600(int64_t comp) {
    EnsureResolved();
    void* desc = *reinterpret_cast<void**>(comp + 0x18);
    void* typed = (desc && fn_IsDerivedFrom &&
                   fn_IsDerivedFrom(desc, DestroyDescription::TypeId())) ? desc : nullptr;
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
static bool s_scatterResolved = false;
static void EnsureScatterResolved() {
    if (s_scatterResolved) return;
    s_scatterResolved = true;
    s_gridToWorld   = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_pos9760       = reinterpret_cast<fn_Pos9760_t>(ResolveAddr(Module::Exe, 0x140129760));
}

// ENDGAME B.3.9 C1 (B39_RULES.md D5): 0x1401c7cb8 is RawMaterialDescription's
// registered type-id cell (INVENTORY_C Table 1); it always holds
// RawMaterialDescription's id once Fill* runs, so the raw-read-with-fallback
// this used to be is now mechanically equivalent to the direct static call.
// 0x1401c7040 (BuildDescription) had no reader at all (kept only for parity via
// a `(void)` cast) -- removed outright rather than converted.
static uint32_t RawMatDescTid() {
    return RawMaterialDescription::TypeId();
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
        uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
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
    // Retail narrows the desc to BuildDescription (0x1401c7040) but then derefs
    // it unconditionally (required@+0x30, recipe@+0x20/+0x28) — the invariant is that
    // a BuildComponent always carries a BuildDescription. We trust the pointer (the
    // derive check would only null it, which retail never survives).
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
                        if (fn_InternalBasePtr_Ctor) new (&entIBP) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge
                        else entIBP = reinterpret_cast<int64_t>(newEntity);

                        // RawMaterialComponent on the new entity → position + amount.
                        int64_t rmIBP = 0;
                        ActRm_FindComponent_223c0(static_cast<uintptr_t>(entIBP), &rmIBP);
                        if (rmIBP) {
                            int yaw = random ? reinterpret_cast<System::Random*>(random)->RandTol(2) : 0;   // B.3.4b bridge
                            if (s_pos9760)
                                s_pos9760(rmIBP, cell, static_cast<float>(yaw) * kYawScale);
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
    if (((*reinterpret_cast<uint32_t*>(entity + 0x40) >> 10) & 1) != 0) return;
    if (s_markRemoval) s_markRemoval(entity);
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(entityList + 0xd0))
        .SetSizeFill(*reinterpret_cast<int*>(entityList + 0xd8) + 1, &entity);
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

    // RETURN 0, NOT 1 — this is the state-machine ENTER WALK's stop bit, not a
    // success flag. StateMachine::Tick walks tail->parent through EVERY controller
    // node while handlers return 0 and STOPS at the first non-zero. Retail
    // 0x14011d7b0 zeroes AL on both exits (`return uVar3 & 0xffffffffffffff00`, and
    // a literal `return 0` on the +0x50 != 0 branch), so the walk continues past
    // DestroyComponent to the entity's other controllers.
    //
    // Returning 1 here stopped that walk on the destroy node, so
    // StorageLocationComponent's state-3 enter (StorageLoc_State3Enter ->
    // CemeteryListErase_Reimpl on townMgr+0x00) never ran for a demolished storage
    // building. Measured on tutorial 1 step 66 ("remove the starting cart"):
    //   * the cart stayed in the town storage-location list forever, so
    //     TutorialGoalCount mode=4 read count=3 against target=2 and the step could
    //     never complete (the goal sets no _greaterEqual, so the compare is EQUALITY);
    //   * and once the cart's StorageComponent was torn down, the stale list entry
    //     made CitizenBehavior_CountResources deref a null storage IBP —
    //     "READ of 0x0000000000000030", crashing the next sim tick.
    // One cause, both symptoms.
    return 0;
}

// ============================================================
// State 4 enter — 0x14011cfd0 (queue the haul-away).
// ============================================================
extern "C" char Destroy_State4Enter_cfd0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);

    // Re-seat the workplace onto the BUILDER profession (retail FUN_14012f520 on the 2e10
    // comp) — ported in destroycomponent_workerrelease.cpp.
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        typedef void (*fn_f520_t)(int64_t);
        static fn_f520_t f520 = reinterpret_cast<fn_f520_t>(
            ResolveAddr(Module::Exe, 0x14012f520));
        if (f520) f520(wp);
    }

    int64_t prod = FindOn(entity, s_findD6c0);
    if (prod) *reinterpret_cast<int8_t*>(comp + 0x51) = 1;

    // Storage already empty, or build materials never fully delivered
    // (partially-built building has nothing to haul) -> straight to state 5.
    if (Destroy_StorageEmpty_da80(static_cast<int64_t>(comp)) ||
        (prod && Yard_Delivered(prod) < Yard_Required(prod))) {
        ::ControllerLive::RequestStateTransition(sm, 5);
        // Retail 0x14011cfd0 reaches its single `return uVar3 & 0xffffffffffffff00`
        // from this branch too (LAB_14011d139 sets uVar3 = 0 first) — AL is zero, so
        // the enter walk continues to the sibling controllers. Same stop-bit rule as
        // state 3 above.
        return 0;
    }

    // Queue a ClearStorage work (type 1) on the production yard. Faithful ctor
    // path: Work_Ctor25 base bind (field_10 = entity) + ClearStorage vtable +
    // zeroed tail fields (ClearStorage_Ctor 0x1400e74b0 shape).
    int64_t yard = FindOn(entity, s_find3050);
    if (yard && s_ec60 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
        if (mem) {
            // ENDGAME B.3.9 OBJ: the real class (Work_Ctor25 + the table +
            // four zero qwords), not the `??0Internal` door + hand stamp.
            int64_t entityTmp = entity;
            ::new (mem) ClearStorage(&entityTmp);
            int64_t workIBP = 0;
            if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
            else workIBP = reinterpret_cast<int64_t>(mem);
            s_ec60(yard, 1, 0, &workIBP);
        }
    }
    // Retail 0x14011cfd0 ends `return uVar3 & 0xffffffffffffff00` — AL zeroed, walk
    // continues. See the state-3 note above for what a stray 1 costs.
    return 0;
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
                if (fn_InternalBasePtr_Ctor) new (&workIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
                else workIBP = reinterpret_cast<int64_t>(mem);
                s_ec60(yard, 0x2001, 0, &workIBP);
            }
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
    if (*reinterpret_cast<int*>(comp + 0x54) <
        static_cast<int>(*reinterpret_cast<uint32_t*>(comp + 0x58))) {
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
    // s_find4680 (FUN_140004680) is the MAPCOMPONENT finder — the same component
    // DecalComponent_Draw reads its decal rect from (mapComp +0x28 minX, +0x2a minY,
    // +0x2c width, +0x2e height, +0x30 rotation).
    //
    // ⚠ `rot` is 32-BIT. Retail c120 reads `*(int*)(fp + 0x30)` and writes
    // `*(int*)(new + 0x30)`, and normalises with `+= 0x168` (360) — it is a rotation
    // in DEGREES, the ONLY one of these five fields that is not a short. This port
    // had it as int16_t on both sides: the low half landed correctly (0/90/180/270
    // all fit), so the rebuilt building's MESH looked right, but the high half was
    // left as whatever ComponentEntity_CopyCtor put there. DecalComponent_Draw reads
    // the field as a full uint32_t, so the decal got `garbage<<16 | rot` and painted
    // its foundation at the wrong orientation — the "rebuilt house's decal footprint
    // doesn't match" report (2026-08-23). Same class as the narrowed-store trap in
    // [reference_narrowed_8byte_store_desyncs_count_and_cap].
    int64_t fp = FindOn(entity, s_find4680);
    int16_t sx = 0, sy = 0, sc = 0, sd = 0;
    int32_t rot = 0;
    if (fp) {
        sx = *reinterpret_cast<int16_t*>(fp + 0x28);
        sy = *reinterpret_cast<int16_t*>(fp + 0x2a);
        sc = *reinterpret_cast<int16_t*>(fp + 0x2c);
        sd = *reinterpret_cast<int16_t*>(fp + 0x2e);
        rot = *reinterpret_cast<int32_t*>(fp + 0x30);
    }
    int64_t pq = FindOn(entity, s_find3f60);
    float tf[3] = {0, 0, 0};
    float yaw = 0.0f;
    uint16_t orient = 0;
    if (pq) {
        tf[0] = *reinterpret_cast<float*>(pq + 0x158);
        tf[1] = *reinterpret_cast<float*>(pq + 0x15c);
        tf[2] = *reinterpret_cast<float*>(pq + 0x160);
        orient = *reinterpret_cast<uint16_t*>(pq + 0x164);
        yaw = *reinterpret_cast<float*>(pq + 0x168);
    }

    // 2) Release workers from the old workplace, i.e. leave the BUILDER profession
    //    (FUN_14012f670 = workplacecomponent.cpp WorkPlaceComponent_LeaveBuilderProfession,
    //    registered by reg_destroy_workers.cpp). Skipping it is what would leave the
    //    roster half-torn-down on the entity we are about to remove.
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        if (s_f670) s_f670(wp);
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
                    if (fn_InternalBasePtr_Ctor) new (&hold) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge
                    else hold = reinterpret_cast<int64_t>(newEntity);

                    int64_t nfp = FindOn(reinterpret_cast<int64_t>(newEntity), s_find4680);
                    if (nfp) {
                        *reinterpret_cast<int16_t*>(nfp + 0x28) = sx;
                        *reinterpret_cast<int16_t*>(nfp + 0x2a) = sy;
                        *reinterpret_cast<int16_t*>(nfp + 0x2c) = sc;
                        *reinterpret_cast<int16_t*>(nfp + 0x2e) = sd;
                        // 32-bit, like retail — see the capture-side note above.
                        int32_t r = rot;
                        while (r < 0) r += 360;   // retail: += 0x168 until >= 0
                        *reinterpret_cast<int32_t*>(nfp + 0x30) = r;
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
                    }

                    std::fprintf(stderr, "[DestroySM] UPGRADE SPAWN: new entity=%p from "
                                         "template=%llx at cell(%d,%d) %dx%d rot=%d "
                                         "orient=%u yaw=%.1f cat=%d\n",
                                 newEntity, static_cast<unsigned long long>(target),
                                 sx, sy, sc, sd, rot,
                                 static_cast<unsigned>(orient), static_cast<double>(yaw), category);
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
// State 6 (collapse/rubble) — PORTED in destroycomponent_state6.cpp.
// ============================================================
// enter 0x14011d4d0 / update 0x14011d400 / trans38-tick 0x14011d490, plus the
// rubble step 0x14011d5d0 and the material re-point 0x140125020. Gated at runtime
// by BANISHED_LIVE_DESTROY_COLLAPSE; see that file's header for the mechanism.
extern "C" char Destroy_State6Enter_d4d0(void* data, void* sm);
extern "C" char Destroy_State6Update_d400(void* data, void* sm);
extern "C" char Destroy_State6Tick_d490(void* data, void* sm, void* time);

// ============================================================
// State-7 EVENT handlers — the destroyed-building panel's buttons.
// ============================================================
// DestroyUI's three buttons (destroyui.cpp Slot9: buttonReclaim 0x1771,
// buttonRebuild 0x1772, buttonDestroy 0x1773) all end in the same retail idiom
// (0x14011e400 / 0x14011e470 / 0x14011e4d0):
//
//   smc  = FindStateMachineComponent(entity)     // FUN_140003ea0
//   node = *(smc + 0x48)                          // the CURRENT node, re-pointed by Tick
//   node->vtable[2](node, smc + 0x48, (int)(int8)smc[0x51], eventId)
//
// i.e. they POST AN EVENT (node vtable slot 2 = PostEvent) on whatever node the
// state machine is currently sitting on — for a collapsed building that is the
// DestroyComponent's node, in state 7. eventId is 0 for reclaim/rebuild and 1 for
// destroy, which is exactly what DestroyDescription_helper_dd00 registers on state 7:
//   sub_4020(0, 0x14011d2a0)   <- rebuild / reclaim
//   sub_4020(1, 0x14011d1a0)   <- destroy / remove
//
// Both handlers were registered as NULL here, and ControllerLive::PostEvent treats a
// null handler as a graceful no-op — so every click on a destroyed building's Remove
// or Rebuild button was swallowed silently. That is the whole bug.
//
// Both bodies are near-identical: re-seat the workplace onto the Builder profession,
// then request state 5 (the rebuild/demolish worker state). They differ only in how
// they set the DestroyComponent's +0x51 "has a build yard" flag, and in whether they
// clear the queued upgrade target at +0x60.

// Shared prologue: retail's FUN_14012f520 on the 2e10 (WorkPlaceComponent) match.
static void Destroy_ReseatWorkplaceOnBuilder(int64_t entity) {
    int64_t wp = FindOn(entity, s_find2e10);
    if (!wp) return;
    typedef void (*fn_f520_t)(int64_t);
    static fn_f520_t f520 = reinterpret_cast<fn_f520_t>(
        ResolveAddr(Module::Exe, 0x14012f520));
    if (f520) f520(wp);
}

// State 7 event 0 — 0x14011d2a0. "Rebuild" (buttonRebuild) and "Reclaim"
// (buttonReclaim; 0x14011e400 posts the same event 0, then additionally calls the
// component's own vtable slot 4).
extern "C" char Destroy_State7Event0_Rebuild_d2a0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);

    Destroy_ReseatWorkplaceOnBuilder(entity);

    // Retail sets the flag unconditionally here (unlike state-4 enter / the destroy
    // event, which both gate it on a BuildComponent being present).
    *reinterpret_cast<int8_t*>(comp + 0x51) = 1;

    // +0x60 = the queued upgrade/replacement description. Re-pointing it at the
    // entity's OWN description (entity+0x28) is what makes this "rebuild what was
    // here" rather than "build the upgrade that was queued" — DestroyUI's
    // CanRebuild (0x14011e600) tests exactly this equality to decide whether the
    // panel shows the upgrade text.
    if (entity) {
        *reinterpret_cast<int64_t*>(comp + 0x60) =
            *reinterpret_cast<int64_t*>(static_cast<uintptr_t>(entity) + 0x28);
    }

    // Retail runs a nested Tick(Time(0)) first when smc[0x51] < 0; ControllerLive's
    // RequestStateTransition bounds-checks the index instead, so a not-yet-settled
    // machine simply defers the request by one tick. See the note on that helper.
    ::ControllerLive::RequestStateTransition(sm, 5);
    return 1;
}

// State 7 event 1 — 0x14011d1a0. "Remove" (buttonDestroy).
extern "C" char Destroy_State7Event1_Remove_d1a0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);

    Destroy_ReseatWorkplaceOnBuilder(entity);

    // Same BuildComponent (d6c0) gate as state-4 enter: only a building with a build
    // yard gets the flag. Retail does NOT touch +0x60 here — the queued upgrade, if
    // any, survives the removal.
    int64_t prod = FindOn(entity, s_findD6c0);
    if (prod) *reinterpret_cast<int8_t*>(comp + 0x51) = 1;

    ::ControllerLive::RequestStateTransition(sm, 5);
    return 1;
}

// ============================================================
// State 6 event 2 — 0x14011d390.
// ============================================================
// The whole body is the request-transition idiom (identical to
// ImmigrationDescription_helper_9ac0, 0x1400a9ac0) asking for state 7. State 6 is
// the burning/collapsing stage and 7 is the destroyed-building rest state, so this
// is "the fire is out — show the rubble panel". Nothing in the shipped UI posts
// event 2 today (the Remove/Rebuild buttons are state 7 / events 0-1), but the
// registration exists in dd00, so the handler does too.
extern "C" char Destroy_State6Event2_ToRubble_d390(void* /*data*/, void* sm) {
    ::ControllerLive::RequestStateTransition(sm, 7);
    return 1;
}

// ============================================================
// States 4 AND 5, event 0 — 0x14011bcc0 (registered twice by dd00).
// ============================================================
// The CANCEL of an in-flight demolition/upgrade: states 4 (haul the storage away)
// and 5 (spawn the replacement) both accept it. It stops the site work and sends the
// machine back to whatever "not being destroyed" state fits the building's current
// model variant.
//
// Retail derefs the three finder results unguarded — a building sitting in state 4/5
// always carries a MeshInstance, a production yard and (for the variant math) a model
// component. Those derefs stay faithful; only the finders our port may not have
// registered are null-checked, which is a standalone concern, not a retail one.
extern "C" char Destroy_State45Event0_Cancel_bcc0(void* data, void* sm) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    int64_t entity = OwnerEntity(comp);

    // Read first (retail order): does the building have a build yard at all?
    int64_t build = FindOn(entity, s_findD6c0);

    // MeshInstance state flag 5 off (the "under construction/demolition" overlay) and
    // stop the production yard's worker.
    int64_t mesh = FindOn(entity, s_find2f90);
    if (s_meshFlag) s_meshFlag(reinterpret_cast<void*>(mesh), 5, 0);
    int64_t yard = FindOn(entity, s_find3050);
    if (s_eb50) s_eb50(yard);

    // comp+0x50 — the "this was a REPLACEMENT/upgrade, not a plain demolish" flag that
    // state-4 enter and the panel buttons set. Cancelling one of those leaves nothing
    // standing, so the machine goes to the destroyed-building rest state instead.
    if (*reinterpret_cast<int8_t*>(comp + 0x50) != 0) {
        int64_t wp = FindOn(entity, s_find2e10);
        if (wp && s_f670) s_f670(wp);
        ::ControllerLive::RequestStateTransition(sm, 7);
        return 1;
    }

    int next;
    if (!build) {
        // No build yard: nothing was under construction, go straight back to state 2.
        next = 2;
    } else {
        // Pick the resume state from how far the model got. The variant source is the
        // yard-0 component's (+0x48) referenced entity when there is one — the
        // construction SITE — otherwise this entity itself.
        int64_t yard0  = FindOn(entity, s_find4740);
        int64_t target = entity;
        if (yard0) target = *reinterpret_cast<int64_t*>(
                                *reinterpret_cast<int64_t*>(yard0 + 0x48));

        int64_t model = FindOn(target, s_find3f60);
        int16_t modelVariant = *reinterpret_cast<int16_t*>(model + 0x166);
        int16_t variantIdx   = *reinterpret_cast<int16_t*>(model + 0x164);
        void*   obj = *reinterpret_cast<void**>(model + 0x18);
        void*   modelDesc = (fn_IsDerivedFrom &&
                             fn_IsDerivedFrom(obj, ModelDescription::TypeId())) ? obj : nullptr;

        if (modelVariant < 0) {
            next = 0;                       // no mesh chosen yet — back to the start
        } else {
            // group[variantIdx] is 0x18 bytes; +0x10 is the mesh COUNT (+0x08 is the
            // mesh array — the same layout destroycomponent_state6.cpp walks).
            int64_t groups = *reinterpret_cast<int64_t*>(
                reinterpret_cast<uintptr_t>(modelDesc) + 0x20);
            int meshCount = *reinterpret_cast<int*>(
                groups + 0x10 + static_cast<int64_t>(variantIdx) * 0x18);
            next = (modelVariant == meshCount - 1) ? 2 : 8;
        }
    }

    ::ControllerLive::RequestStateTransition(sm, next);

    // Release the workplace roster AFTER the request on this arm (retail order).
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp && s_f670) s_f670(wp);

    return 1;
}

// ============================================================
// Dispatcher + state table for the DestroyComponent controller.
// Faithful graph from DestroyDescription_helper_dd00 (0x14011dd00). All four event
// handlers (0x14011bcc0/d390/d2a0/d1a0) are now ported and registered.
// ============================================================
// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<DestroyCtrlTag>, DestroyComponent.h).
#include "Component/DestroyComponent.h"
using DestroyDispatch = ::ControllerLive::Dispatcher<DestroyCtrlTag>;

extern "C" void DestroyCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = DestroyDispatch::table;
    if (t.count != 0) return;   // idempotent

    TableRegisterState(t, 3, reinterpret_cast<void*>(&Destroy_State3Enter_d7b0),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 6, nullptr);

    // sub_3e50(6, A=d4d0, B=d400, D=d490): A -> enter, B -> update, D -> trans38.
    TableRegisterState(t, 6, reinterpret_cast<void*>(&Destroy_State6Enter_d4d0),
                       nullptr, reinterpret_cast<void*>(&Destroy_State6Tick_d490),
                       reinterpret_cast<void*>(&Destroy_State6Update_d400));
    TableAddTransition(t, 7, nullptr);
    // sub_4020(2, d390) — state 6's only event.
    TableAddEvent(t, 2, reinterpret_cast<void*>(&Destroy_State6Event2_ToRubble_d390));

    TableRegisterState(t, 7, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 0, nullptr);
    TableAddTransition(t, 8, nullptr);
    TableAddTransition(t, 5, nullptr);
    // sub_4020(0, d2a0) / sub_4020(1, d1a0) — the destroyed-building panel's
    // Rebuild/Reclaim and Remove buttons. Registration order matches dd00.
    TableAddEvent(t, 0, reinterpret_cast<void*>(&Destroy_State7Event0_Rebuild_d2a0));
    TableAddEvent(t, 1, reinterpret_cast<void*>(&Destroy_State7Event1_Remove_d1a0));

    TableRegisterState(t, 4, reinterpret_cast<void*>(&Destroy_State4Enter_cfd0),
                       nullptr, reinterpret_cast<void*>(&Destroy_State4Tick_cec0),
                       nullptr);
    TableAddTransition(t, 5, nullptr);
    TableAddTransition(t, 2, nullptr);
    // sub_4020(0, bcc0) — cancel, registered on state 4 AND state 5.
    TableAddEvent(t, 0, reinterpret_cast<void*>(&Destroy_State45Event0_Cancel_bcc0));

    TableRegisterState(t, 5, reinterpret_cast<void*>(&Destroy_State5Enter_cc70),
                       nullptr, reinterpret_cast<void*>(&Destroy_State5Tick_c120),
                       nullptr);
    TableAddTransition(t, 0, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddTransition(t, 8, nullptr);
    TableAddTransition(t, 7, nullptr);
    TableAddEvent(t, 0, reinterpret_cast<void*>(&Destroy_State45Event0_Cancel_bcc0));
}
