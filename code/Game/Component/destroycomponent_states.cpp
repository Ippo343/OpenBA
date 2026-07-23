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
//   * sub_c530 contents-scatter deferred — stored goods are dropped, not
//     spilled as ground piles (deps 0x140129760/CopyCtor exist; port later).
//   * state 6 (bulldoze rubble) deferred entirely.
//
// Opt-in: BANISHED_LIVE_DESTROY (see destroycomponent.cpp). NOT in
// kDefaultOnFlags — unproven features start opt-in.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Component/controller_state_table.h"

// ============================================================
// Resolved helpers (once)
// ============================================================
typedef void* (*fn_FindIBP_t)(int64_t entity, void* outIBP);    // member-return finders
typedef char  (*fn_HasWork_t)(int64_t workComp);                // 0x14012ead0
typedef void  (*fn_StopWork_t)(int64_t workComp);               // 0x14012eb50
typedef void  (*fn_PostWork_t)(int64_t workComp, uint32_t code, char flag, void* workIBP); // 0x14012ec60
typedef void  (*fn_ReleaseWorkers_t)(int64_t wp);               // 0x14012f670 (may be unregistered)
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
static fn_ReleaseWorkers_t  s_f670      = nullptr;  // may stay null (unported)
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

// keystone_a_removalcalcpath.cpp — disasm-verified work-manager AVL lookup
// (walks the tree at param_1+0x30 for payload key == param_3).
extern "C" void* RemovalCalc_helper_8d50(int64_t param_1, void** param_2, int param_3);
// buildstructure.cpp — full retail BuildStructure ctor (0x1400e59c0 port).
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim);

static bool Trace() {
    static const bool t = [] {
        const char* e = std::getenv("BANISHED_DESTROY_TRACE");
        return e && e[0] && e[0] != '0';
    }();
    return t;
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
// sub_c530 (0x14011c530) — scatter old contents as RawMaterial ground piles.
// DEFERRED: walks the Storage stacks and clones resource templates onto
// walkable tiles (deps 0x140129760 PositionEntity + CopyCtor + RegisterEntity
// are all ported — port the walk when stored-goods fidelity matters). Until
// then upgrade/demolish DROPS stored goods instead of spilling them.
// ============================================================
static void Destroy_ScatterContents_c530(int64_t comp) {
    (void)comp;
    static bool warned = false;
    if (!warned) {
        warned = true;
        std::fprintf(stderr, "[DestroySM] NOT-IMPLEMENTED sub_c530 contents-scatter "
                             "(0x14011c530): stored goods dropped, not spilled\n");
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

    // Re-seat the workplace subsystem (retail FUN_14012f520 on the 2e10 comp).
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        typedef void (*fn_f520_t)(int64_t);
        static fn_f520_t f520 = reinterpret_cast<fn_f520_t>(
            ResolveAddr(Module::Exe, 0x14012f520));
        if (f520) {
            f520(wp);
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[DestroySM] NOT-IMPLEMENTED FUN_14012f520 "
                                     "(state-4 workplace re-seat) — skipped\n");
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
        // Nothing recoverable: scatter old contents (deferred) + notify trade +
        // mark the old entity for removal.
        if (*reinterpret_cast<int8_t*>(comp + 0x51))
            Destroy_ScatterContents_c530(static_cast<int64_t>(comp));
        if (s_trade3480 && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
            int64_t tradeMgr = *reinterpret_cast<int64_t*>(gw + 0x120);
            int64_t entityIBP = entity;
            if (tradeMgr) s_trade3480(tradeMgr, &entityIBP);
        }
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

    // 2) Release workers from the old workplace (FUN_14012f670).
    int64_t wp = FindOn(entity, s_find2e10);
    if (wp) {
        if (s_f670) {
            s_f670(wp);
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[DestroySM] NOT-IMPLEMENTED FUN_14012f670 "
                                     "(worker release) — skipped\n");
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

    if (target != 0) {
        bool ready = (prod && Yard_Delivered(prod) >= Yard_Required(prod)) ||
                     (*reinterpret_cast<int8_t*>(comp + 0x50) != 0);
        if (ready) {
            // Retail 0x14011c3xx: scatter when e600 != 0 OR e5b0 != 0 (the
            // previous port had the e600 sense inverted).
            if (*reinterpret_cast<int8_t*>(comp + 0x51)) {
                if (Destroy_TargetDiffers_e600(static_cast<int64_t>(comp)) ||
                    Destroy_TargetIsCurrent_e5b0(static_cast<int64_t>(comp)))
                    Destroy_ScatterContents_c530(static_cast<int64_t>(comp));
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
        Destroy_ScatterContents_c530(static_cast<int64_t>(comp));
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
