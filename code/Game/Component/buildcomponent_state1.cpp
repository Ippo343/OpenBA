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
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#endif

// Ported leaves (this brick):
extern "C" bool  BuildComponent_HaveAllMaterials(void* buildComp);          // sub_8630
extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP); // 0x1400ec710

// ---- Exe helpers (all already registered in standalone unless noted) -------------
// FUN_140004740 InteractComponent_FindWorkplaceComponent(entity, retIBP) -> retIBP
typedef void* (*BCS1_FindComp_t)(uintptr_t entity, void* retIBP);
// FUN_14000a170 find (Model) component; FLAG: registration unverified -> guarded
typedef void* (*BCS1_FindComp2_t)(uintptr_t entity, void* retIBP);
// ModelComponent_impl_8270(modelComp) -> reset build variant; FLAG guarded
typedef void  (*BCS1_ModelReset_t)(uintptr_t modelComp);
// BuildComponent_helper_7550 == 0x140017550 == BuildComponent_FindInQueue(arrayBase, ibp)
typedef int   (*BCS1_FindInQueue_t)(void* arrayBase, void* ibp);
// FUN_140003110 array-add(arrayField, newCount, ibp)
typedef void  (*BCS1_ArrayAdd_t)(void* arrayField, int newCount, void* ibp);
// FUN_140003050 WorkBase_FindComponent_3050(entity, retIBP) -> WorkComponent (0x1401c8070)
typedef void* (*BCS1_FindWorkComp_t)(uintptr_t entity, void* retIBP);
// CitizenAI_impl_ec60(workComp, code, flag, workIBP) — reservation-ledger post (deferred)
typedef void  (*BCS1_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
// Entity_FindComponent_Storage @ 0x140006270 (entity, retIBP) -> site StorageComponent
typedef void* (*BCS1_FindStorage_t)(uintptr_t entity, void* retIBP);
// FUN_14000f090 StorageComponent_DynArrayResize(slotArrayField, count)
typedef void  (*BCS1_StorageResize_t)(void* slotArrayField, int count);

static uint32_t BCS1_TypeId(const char* name, uintptr_t rdataGlobal) {
#ifdef STANDALONE_EXE
    (void)rdataGlobal;
    return ::Object::GetTypeIdByName(name);
#else
    (void)name;
    uintptr_t g = ResolveAddr(Module::Exe, rdataGlobal);
    return g ? *reinterpret_cast<uint32_t*>(g) : 0;
#endif
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

static int  s_bcs1_trace = -1;
static bool BCS1_Trace() {
    if (s_bcs1_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_bcs1_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_bcs1_trace == 1;
}

// ---- Construction-demo gating (standalone) ---------------------------------------
// StartTown force-completes every start-town building (advances its ModelComponent to
// the final construction sub-mesh). Driving State1Enter on such a building would RESET
// its storage (f090 resize + accept-mask), blanking finished houses. To keep the build
// pipeline testable without that damage we (a) seed ONE genuine foundation as the demo
// site and (b) skip everything else. `g_pConstructionDemoSite` (set by StartTown) is the
// BuildComponent of that one foundation; when non-null, ONLY it is driven. When it is
// null (no demo seeded) we still skip buildings whose model is already complete.
extern "C" { void* g_pConstructionDemoSite = nullptr; }

#ifdef STANDALONE_EXE
// Best-effort completion check: a force-completed building has its ModelComponent build
// variant (+0x166) at the final sub-mesh of a multi-stage group; a genuine foundation
// sits at variant 0. Foundation / single-mesh buildings -> false (i.e. "build me").
static bool BCS1_IsModelComplete(uintptr_t buildComp) {
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(buildComp + 0x10));
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
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
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
//   * any other building when a demo site is registered -> skip (drive ONLY the demo);
//   * any completed building (model at final sub-mesh) when no demo -> skip.
// This must gate the PER-FRAME handlers too (State1Update), not just State1Enter — else a
// completed building's node self-advances 1->8->2 and its finalize clobbers finished storage.
extern "C" bool BuildComponent_DemoGateSkip(void* buildComp) {
    if (!buildComp) return false;
    if (g_pConstructionDemoSite && buildComp == g_pConstructionDemoSite) return false;
    if (g_pConstructionDemoSite != nullptr) return true;
    return BCS1_IsModelComplete(reinterpret_cast<uintptr_t>(buildComp));
}
#endif

// The state-1 ENTER handler. param1 = BuildComponent, param2 = state-machine node.
extern "C" int BuildComponent_State1Enter(void* buildComp, void* smNode) {
    uintptr_t self = reinterpret_cast<uintptr_t>(buildComp);
    if (!self) return 1;

    if (BCS1_Trace())
        std::fprintf(stderr, "[BuildTrace] === State1Enter ENTERED self=%p smNode=%p ===\n", buildComp, smNode);

    // resolve helpers once
    static BCS1_FindComp_t     s_find4740 = nullptr;
    static BCS1_FindComp2_t    s_findA170 = nullptr;
    static BCS1_ModelReset_t   s_model8270 = nullptr;
    static BCS1_FindInQueue_t  s_findInQueue = nullptr;
    static BCS1_ArrayAdd_t     s_arrayAdd = nullptr;
    static BCS1_FindWorkComp_t s_find3050 = nullptr;
    static BCS1_PostWork_t     s_postEc60 = nullptr;
    static BCS1_FindStorage_t  s_findStorage = nullptr;
    static BCS1_StorageResize_t s_storageResize = nullptr;
    // FUN_140002e10 FindComponent_WorkPlace(entity, retIBP) -> WorkPlaceComponent (0x1401c80e0).
    // Same finder GRM Execute uses to gate the gather on the site's "accepting" flag (+0x50).
    static BCS1_FindComp_t     s_findWpc2e10 = nullptr;
    static bool s_res = false;
    if (!s_res) {
        s_find4740    = reinterpret_cast<BCS1_FindComp_t>(ResolveAddr(Module::Exe, 0x140004740));
        s_findA170    = reinterpret_cast<BCS1_FindComp2_t>(ResolveAddr(Module::Exe, 0x14000a170));
        s_model8270   = reinterpret_cast<BCS1_ModelReset_t>(ResolveAddr(Module::Exe, 0x140118270));
        s_findInQueue = reinterpret_cast<BCS1_FindInQueue_t>(ResolveAddr(Module::Exe, 0x140017550));
        s_arrayAdd    = reinterpret_cast<BCS1_ArrayAdd_t>(ResolveAddr(Module::Exe, 0x140003110));
        s_find3050    = reinterpret_cast<BCS1_FindWorkComp_t>(ResolveAddr(Module::Exe, 0x140003050));
        s_postEc60    = reinterpret_cast<BCS1_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
        s_findStorage = reinterpret_cast<BCS1_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
        s_storageResize = reinterpret_cast<BCS1_StorageResize_t>(ResolveAddr(Module::Exe, 0x14000f090));
        s_findWpc2e10 = reinterpret_cast<BCS1_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
        s_res = true;
    }

#ifdef STANDALONE_EXE
    // ---- Construction-demo gate (see g_pConstructionDemoSite header note) ----------
    // Must run BEFORE any mutation (multiplier write / model reset / storage reset).
    if (BuildComponent_DemoGateSkip(buildComp)) {
        if (BCS1_Trace())
            std::fprintf(stderr, "[BuildTrace] State1Enter SKIP self=%p (demoSite=%p)\n",
                         buildComp, g_pConstructionDemoSite);
        return 1;
    }
#endif

    // +0x4c = 1 (default multiplier)
    *reinterpret_cast<uint32_t*>(self + 0x4c) = 1;

    // multiplier from workplace component (+0x58)
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uintptr_t wpIbp = 0;
    if (s_find4740) s_find4740(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpIbp);
    if (wpIbp != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpIbp);
        // Retail: bc+0x4c = WorkplaceComponent+0x58 (the build-effort multiplier). In the
        // standalone exe the WorkPlaceComponent lifecycle is inert so +0x58 stays 0, which
        // would clobber the default 1 set just above and make the state-8 build target
        // (matComp[0x30] * mult) collapse to 0 (instant completion, no visible labor). Only
        // adopt the workplace value when it is a valid positive multiplier.
        uint32_t wpMult = *reinterpret_cast<uint32_t*>(wpIbp + 0x58);
        if ((int32_t)wpMult > 0)
            *reinterpret_cast<uint32_t*>(self + 0x4c) = wpMult;
    }

    // reset the site ModelComponent build variant (a170 -> 8270); guarded
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uintptr_t mcIbp = 0;
    if (s_findA170) s_findA170(*reinterpret_cast<uintptr_t*>(self + 0x10), &mcIbp);
    if (mcIbp != 0 && s_model8270) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&mcIbp);
        s_model8270(mcIbp);
    }

    // register the site into the construction registry (townMgr+0xd0 / count +0xd8)
    uintptr_t townMgr = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    uintptr_t siteIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&siteIbp, buildComp);
    if (townMgr && s_findInQueue) {
        int idx = s_findInQueue(reinterpret_cast<void*>(townMgr + 0xd0), &siteIbp);
        if (idx < 0 && s_arrayAdd) {
            int newCount = *reinterpret_cast<int*>(townMgr + 0xd8) + 1;
            s_arrayAdd(reinterpret_cast<void*>(townMgr + 0xd0), newCount, &siteIbp);
            if (BCS1_Trace())
                std::fprintf(stderr, "[BuildTrace] site registered into townMgr+0xd0 (count now %d)\n", newCount);
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
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIbp, work);
        if (BCS1_Trace())
            std::fprintf(stderr, "[BuildTrace] GatherRawMaterials work constructed=%p\n", work);

        // find the WorkComponent (0x1401c8070) the work posts under
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
        uintptr_t workCompIbp = 0;
        if (s_find3050) s_find3050(*reinterpret_cast<uintptr_t*>(self + 0x10), &workCompIbp);

        // POST to the reservation ledger (brick 2b): CitizenAI_impl_ec60 inserts the
        // gather work into the gw+0x118 priority AVL (via 0x1400f88a0) keyed by the
        // WorkComponent, so an idle citizen claims it and runs GatherRawMaterials::Execute
        // (the town->site haul). Exe: CitizenAI_impl_ec60(*pWorkCompIBP, 0x2001, 1, &workIBP).
        // Gated behind BANISHED_BUILD_LIVE_HAUL so it can be A/B'd independently of the
        // state-machine harness (it needs the live-citizen action stack + pathfinder).
        static int s_liveHaul = -1;
        if (s_liveHaul < 0) {
            const char* e = std::getenv("BANISHED_BUILD_LIVE_HAUL");
            s_liveHaul = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (s_liveHaul && s_postEc60 && workCompIbp != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIbp);
            s_postEc60(workCompIbp, 0x2001, 1, &workIbp);
            if (BCS1_Trace())
                std::fprintf(stderr, "[BuildTrace] posted gather work to ledger via ec60 (code 0x2001), workComp=%p work=%p\n",
                             *reinterpret_cast<void**>(workCompIbp), work);
        } else if (BCS1_Trace()) {
            std::fprintf(stderr, "[BuildTrace] (gather post skipped: liveHaul=%d postFn=%p workComp=%p)\n",
                         s_liveHaul, (void*)s_postEc60, (void*)workCompIbp);
        }

        // configure the site StorageComponent accept-mask + capacity
        void* descObj = *reinterpret_cast<void**>(self + 0x18);
        uint32_t buildDescTid = BCS1_TypeId("BuildDescription", 0x1401c7040);
        void* buildDesc = (descObj && fn_IsDerivedFrom && buildDescTid &&
                           fn_IsDerivedFrom(descObj, buildDescTid)) ? descObj : nullptr;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
        uintptr_t storageIbp = 0;
        if (s_findStorage) s_findStorage(*reinterpret_cast<uintptr_t*>(self + 0x10), &storageIbp);

        if (storageIbp != 0 && s_storageResize) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIbp);
            s_storageResize(reinterpret_cast<void*>(storageIbp + 0x28), 0);

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

            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIbp);
            *reinterpret_cast<uint32_t*>(storageIbp + 0x3c) = acceptMask;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIbp);
            *reinterpret_cast<int*>(storageIbp + 0x38) = totalQty;

            if (BCS1_Trace())
                std::fprintf(stderr, "[BuildTrace] storage configured: matCount=%d found=%d acceptMask=0x%x totalQty=%d multiplier=%d\n",
                             matCount, found, acceptMask, totalQty, multiplier);
        }

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
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
            uintptr_t wpcIbp = 0;
            s_findWpc2e10(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpcIbp);
            if (wpcIbp != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIbp);
                void* wpc = *reinterpret_cast<void**>(&wpcIbp);
                if (wpc) {
                    char prev = *reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(wpc) + 0x50);
                    *reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(wpc) + 0x50) = 1;
                    if (BCS1_Trace())
                        std::fprintf(stderr, "[BuildTrace] WorkPlaceComponent accepting enabled (wpc=%p +0x50 %d->1)\n",
                                     wpc, (int)prev);
                }
            } else if (BCS1_Trace()) {
                std::fprintf(stderr, "[BuildTrace] (no WorkPlaceComponent on site to enable)\n");
            }
        }
    } else {
        // -------- Materials present / instant: force transition to state 8 --------
        // The faithful path StateMachine_Tick's the node then calls its Step vtable slot
        // to advance to state 8. That transition (+ the state-8 build handlers) is the
        // NEXT brick; here we only trace so brick 1 stays crash-safe even when a building
        // lacks a resolvable BuildDescription (sub_8630 -> true routes here).
        (void)smNode;
        if (BCS1_Trace())
            std::fprintf(stderr, "[BuildTrace] materials present/instant (haveAll=%d instant=%d) -> state-8 transition DEFERRED\n",
                         (int)haveAll, (int)instant);
    }

    return 1;
}

#ifdef STANDALONE_EXE
extern "C" void RegisterBuildComponentState1() {
    RegisterExeAddr(0x140108290, reinterpret_cast<uintptr_t>(&BuildComponent_State1Enter));
}
#endif
