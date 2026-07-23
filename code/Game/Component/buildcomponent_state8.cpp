// code/Game/Component/buildcomponent_state8.cpp
// Brick 2a — the BuildComponent construction STATE MACHINE handlers past state-1 enter:
//   * State1Update  (sub_81e0 @ 0x1401081e0) — per-frame: when all raw materials are in
//                    the site storage (sub_8630) request transition 1 -> 8.
//   * State8Enter   (sub_7ae0 @ 0x140107ae0) — on entering state 8: reset the construction
//                    meshes to frame 0 and (STAGED branch) spawn the BuildStructure labor
//                    work. The work-spawn + instant branch are DEFERRED to brick 2c.
//   * State8Update  (sub_76e0 @ 0x1401076e0) — per-frame: advance each mesh's build frame
//                    proportional to progress(+0x48)/target; when progress >= target snap
//                    meshes to final + request transition 8 -> 2.
//   * State8OneShot (sub_7600 @ 0x140107600) — fires as state 8 exits: model mode 4 +
//                    remove the site from the pending-build registry (townMgr+0xd0).
//   * State8Finalize(sub_7ff0 @ 0x140107ff0) via State8To2Action (sub_75e0 @ 0x1401075e0)
//                    — the 8->2 transition ACTION: refund leftover materials to town stock
//                    (townMgr+0x170) and reconfigure the site storage as OUTPUT storage.
//
// Node-slot mapping (recovered 2026-07-11 — see reference_construction_pipeline_map):
//   StateMachineComponent::Update -> StateMachine::Tick(sm, time, altDraw=1, doDraw=1)
//   EVERY frame. Tick skips its UPDATE phase (slot +0x20) unless a transition is pending,
//   and runs the DRAW phase (slot +0x38, altDraw=1) every frame. So the PER-FRAME handler
//   is the trans38Handler (slot +0x38); the exit-transition handler is the updateHandler
//   (slot +0x20). The controller_state_table.h dispatcher forwards `sm` to both.
//
// A ported handler requests a transition via ControllerLive::RequestStateTransition(sm,id)
// (replicates `node->slot08(node, sm->index, &desired); sm[+0xa/+0xb] = ...`).
//
// Faithful offset-by-offset port of the Ghidra decompilations. All exe-internal helpers
// are resolved guarded; if one is unavailable the step is skipped.

#include "engine_funcs.h"
#include "resolve.h"
#include "Component/controller_state_table.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled (default-on gates)

extern "C" bool BuildComponent_HaveAllMaterials(void* buildComp);   // sub_8630 (ported)
extern "C" void* g_pConstructionDemoSite;                            // the seeded foundation
extern "C" void* g_pBuildStructureWork = nullptr;                    // 2c-B probe: last spawned build work
extern void BuildComponent_DrawReimpl(void* self);                   // 0x140108d40 mesh-list fill
extern "C" void* BuildStructure_CreateInPlace(void* mem);            // buildstructure.cpp 1-arg ctor (0x1400e5a90)
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim); // full ctor (0x1400e59c0)
extern "C" bool BuildComponent_DemoGateSkip(void* buildComp);        // buildcomponent_state1.cpp
// citizen_employ_cascade.cpp — drive the real ProfessionUI seat cascade (cbf0/ca20/c5f0/c4b0)
// against a SPECIFIC workplace entity (no FindAvailableBuilding / townMgr+0x100 lookup, which
// would activate the freeze-prone employment drain path). Seats up to `count` workers of
// `professionKey` onto `availEntity`; returns # seated.
extern "C" int  CitizenEmploy_SeatWorkersAt(int64_t townMgr, int64_t availEntity, uint64_t professionKey, int count);
// workplacecomponent.cpp — roster-only mode: the WPC attach/detach (fa20/f7e0 reimpl) skips the
// Scene mesh-visual toggle, so seating a Builder does NOT swap the site's build mesh.
extern bool g_wpcRosterOnly;

// ---- trace / demo-force gates ----------------------------------------------------
static int s_bc8_trace = -1;
static bool BC8_Trace() {
    if (s_bc8_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_bc8_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_bc8_trace == 1;
}
// Brick-2a plumbing harness: fake the citizen half so the state chain can be validated
// crash-free without live hauling/labor. When on, for the demo site only: State1Update
// treats materials as present (force 1->8), and State8Update ramps progress(+0x48) toward
// target each frame (drives the mesh progression + 8->2 completion). Brick 2b/2c replace
// these fakes with real GatherRawMaterials haul + BuildStructure labor.
static int s_bc8_force = -1;
static bool BC8_DemoForce() {
    if (s_bc8_force < 0) {
        const char* e = std::getenv("BANISHED_BUILD_DEMO_FORCE");
        s_bc8_force = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_bc8_force == 1;
}
static bool BC8_IsDemo(void* bc) { return g_pConstructionDemoSite && bc == g_pConstructionDemoSite; }

// Natural builder-flow gate. When on, the site was registered into townMgr+0x100 at
// State1Enter and the tick-driven Builder pool (BuilderPool_Tick) seats builders via
// FindAvailableBuilding — so State8Enter must NOT also run the hardcoded-entity direct
// seat (CitizenEmploy_SeatWorkersAt), or the site would be double-staffed. Off (=0
// kill-switch) = legacy path (direct seat here). Default-ON via kDefaultOnFlags (2026-07-22).
static bool BC8_BuilderNatural() {
    return ::ComponentFactory::LiveEnabled("BANISHED_BUILDER_NATURAL");
}

// ---- type ids (runtime registry) ----------------------------
static uint32_t BC8_TypeId(const char* name, uintptr_t rdataGlobal) {
    (void)rdataGlobal;
    return ::Object::GetTypeIdByName(name);
}

// ---- exe helper pointers ---------------------------------------------------------
typedef void* (*BC8_Find_t)(uintptr_t entity, void* retIBP);
typedef void  (*BC8_Stop_t)(uintptr_t workComp);            // FUN_14012eb50
typedef void  (*BC8_ModelRefresh_t)(uintptr_t modelComp, int mode); // 0x1400c2b70
typedef void  (*BC8_Entity8f50_t)(void* ibpOrComp);         // 0x140128f50
typedef void  (*BC8_StorageClear_t)(void* slotArrayField, int n); // 0x14000f090
typedef int   (*BC8_ResIdx_t)(void* slotArrayBase, void* matType); // 0x140088f90
typedef int   (*BC8_FindInQueue_t)(void* arrayBase, void* ibp);    // 0x140017550
typedef void  (*BC8_MeshFinalize_t)(void* meshInst, int a, char b); // 0x14012c3f0
typedef void  (*BC8_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP); // 0x14012ec60

static BC8_Find_t          s_find3050    = nullptr;  // FUN_140003050 (WorkComponent finder)
static BC8_Stop_t          s_stopWorker  = nullptr;  // FUN_14012eb50
static BC8_ModelRefresh_t  s_modelRefresh= nullptr;  // ModelComponent_helper_2b70 0x1400c2b70
static BC8_Entity8f50_t    s_entity8f50  = nullptr;  // Entity_impl_8f50 0x140128f50
static BC8_Find_t          s_findStorage = nullptr;  // Entity_FindComponent_Storage 0x140006270
static BC8_Find_t          s_findMeshInst= nullptr;  // Entity_FindComponent_MeshInstance 0x140002f90
static BC8_MeshFinalize_t  s_meshFinalize= nullptr;  // FUN_14012c3f0
static BC8_StorageClear_t  s_storageClear= nullptr;  // FUN_14000f090
static BC8_ResIdx_t        s_resIdx      = nullptr;  // FUN_140088f90
static BC8_FindInQueue_t   s_findInQueue = nullptr;  // BuildComponent_helper_7550
static BC8_PostWork_t      s_postEc60    = nullptr;  // CitizenAI_impl_ec60 (scheduler post)
static bool s_bc8_res = false;
static void BC8_Resolve() {
    if (s_bc8_res) return;
    s_postEc60     = reinterpret_cast<BC8_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_find3050     = reinterpret_cast<BC8_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_stopWorker   = reinterpret_cast<BC8_Stop_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_modelRefresh = reinterpret_cast<BC8_ModelRefresh_t>(ResolveAddr(Module::Exe, 0x1400c2b70));
    s_entity8f50   = reinterpret_cast<BC8_Entity8f50_t>(ResolveAddr(Module::Exe, 0x140128f50));
    s_findStorage  = reinterpret_cast<BC8_Find_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_findMeshInst = reinterpret_cast<BC8_Find_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_meshFinalize = reinterpret_cast<BC8_MeshFinalize_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_storageClear = reinterpret_cast<BC8_StorageClear_t>(ResolveAddr(Module::Exe, 0x14000f090));
    s_resIdx       = reinterpret_cast<BC8_ResIdx_t>(ResolveAddr(Module::Exe, 0x140088f90));
    s_findInQueue  = reinterpret_cast<BC8_FindInQueue_t>(ResolveAddr(Module::Exe, 0x140017550));
    s_bc8_res = true;
}

// Stop/release the site's build worker (FUN_140003050 -> FUN_14012eb50), as the exe does
// before requesting the 1->8 / 8->2 transition.
static void BC8_StopWorker(uintptr_t bc) {
    if (!s_find3050 || !s_stopWorker) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
    uintptr_t retIBP = 0;
    void* self = s_find3050(*reinterpret_cast<uintptr_t*>(bc + 0x10), &retIBP);
    if (!self) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    uintptr_t wc = *reinterpret_cast<uintptr_t*>(self);
    if (wc) s_stopWorker(wc);
}

// modelDesc of a mesh element (meshElem+0x18, gated to ModelDescription DAT_1401c7a88).
static void* BC8_MeshModelDesc(uintptr_t meshElem) {
    void* d = *reinterpret_cast<void**>(meshElem + 0x18);
    uint32_t tid = BC8_TypeId("ModelDescription", 0x1401c7a88);
    if (d && tid && fn_IsDerivedFrom && fn_IsDerivedFrom(d, tid)) return d;
    return nullptr;
}
// Frame count for a mesh element's current variant (modelDesc+0x20 group array, stride
// 0x18, frameCount @ +0x10).
static int BC8_MeshFrameCount(uintptr_t meshElem, void* modelDesc) {
    if (!modelDesc) return 0;
    int16_t variant = *reinterpret_cast<int16_t*>(meshElem + 0x164);
    uintptr_t groupBase = *reinterpret_cast<uintptr_t*>((uint8_t*)modelDesc + 0x20);
    if (!groupBase || variant < 0) return 0;
    return *reinterpret_cast<int32_t*>(groupBase + (int64_t)variant * 0x18 + 0x10);
}
// Frame mesh pointer: modelDesc+0x20 group[variant].framePtrs(+8)[frame].
static void* BC8_MeshFramePtr(uintptr_t meshElem, void* modelDesc, int frame) {
    if (!modelDesc || frame < 0) return nullptr;
    int16_t variant = *reinterpret_cast<int16_t*>(meshElem + 0x164);
    uintptr_t groupBase = *reinterpret_cast<uintptr_t*>((uint8_t*)modelDesc + 0x20);
    if (!groupBase || variant < 0) return nullptr;
    uintptr_t framePtrArr = *reinterpret_cast<uintptr_t*>(groupBase + (int64_t)variant * 0x18 + 8);
    if (!framePtrArr) return nullptr;
    return *reinterpret_cast<void**>(framePtrArr + (int64_t)frame * 8);
}
// Re-add a mesh element's draw children (host = meshElem+0x10 ModelComponent; iterate its
// +0x30 array / +0x38 count, calling each child's vtable slot +0x60).
static void BC8_ReaddDrawChildren(uintptr_t meshElem) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(meshElem + 0x10));
    uintptr_t host = *reinterpret_cast<uintptr_t*>(meshElem + 0x10);
    if (!host) return;
    int hc = *reinterpret_cast<int32_t*>(host + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(host + 0x30);
    for (int k = 0; k < hc; ++k) {
        void* h = *reinterpret_cast<void**>(arr + (int64_t)k * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&h);
        if (!h) continue;
        typedef void (*Slot60_t)(void*, int);
        void* vt = *reinterpret_cast<void**>(h);
        ((Slot60_t)(*reinterpret_cast<uintptr_t*>((uint8_t*)vt + 0x60)))(h, 0);
    }
}
// Set one mesh element to `frame` (hide old via slot +0x38, SetMesh new frame, show via
// slot +0x30). Mirrors the frame-set block shared by sub_76e0 / sub_7ae0.
static void BC8_SetMeshFrame(uintptr_t meshElem, int frame) {
    int16_t cur = *reinterpret_cast<int16_t*>(meshElem + 0x166);
    if ((int)cur == frame) return;
    typedef void (*SlotV_t)(void*);
    void* vt = *reinterpret_cast<void**>(meshElem);
    if (cur >= 0)
        ((SlotV_t)(*reinterpret_cast<uintptr_t*>((uint8_t*)vt + 0x38)))(reinterpret_cast<void*>(meshElem));
    *reinterpret_cast<int16_t*>(meshElem + 0x166) = (int16_t)frame;
    if (frame >= 0) {
        void* modelDesc = BC8_MeshModelDesc(meshElem);
        void* framePtr = BC8_MeshFramePtr(meshElem, modelDesc, frame);
        if (fn_Graphics_MeshInstance_SetMesh)
            fn_Graphics_MeshInstance_SetMesh(reinterpret_cast<void*>(meshElem + 0x28), framePtr);
        ((SlotV_t)(*reinterpret_cast<uintptr_t*>((uint8_t*)vt + 0x30)))(reinterpret_cast<void*>(meshElem));
    }
}

// Deref the i-th mesh element from bc+0x58 (IBP array) with CheckPtr; nullptr on empty.
static uintptr_t BC8_MeshElem(uintptr_t bc, int i) {
    uintptr_t slot = *reinterpret_cast<uintptr_t*>(bc + 0x58) + (int64_t)i * 8;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(slot));
    return *reinterpret_cast<uintptr_t*>(slot);
}

// matComp (bc+0x18 gated to BuildDescription DWORD_1401c7040) + target labor.
static void* BC8_MatComp(uintptr_t bc) {
    void* d = *reinterpret_cast<void**>(bc + 0x18);
    uint32_t tid = BC8_TypeId("BuildDescription", 0x1401c7040);
    if (d && tid && fn_IsDerivedFrom && fn_IsDerivedFrom(d, tid)) return d;
    return nullptr;
}
static int BC8_Target(uintptr_t bc, void* matComp) {
    if (!matComp) return 0;
    // target = base build effort (matComp+0x30) * multiplier (bc+0x4c). The multiplier is
    // WorkplaceComponent+0x58 (0 when the WPC is inert in the standalone exe); clamp to the
    // retail default of 1 so the target never collapses to 0 (which would instant-complete
    // the build with no visible labor). See buildcomponent_state1.cpp for the source clamp.
    int mult = *reinterpret_cast<int32_t*>(bc + 0x4c);
    if (mult <= 0) mult = 1;
    return *reinterpret_cast<int32_t*>((uint8_t*)matComp + 0x30) * mult;
}

// Ensure the construction mesh list (bc+0x58 / count bc+0x60) is populated. In the
// retail game BuildComponent::Draw (0x140108d40) runs every frame as the component's
// draw-tick (vtable +0x40) and fills this list with the site's ModelComponent(s). In
// standalone the BuildComponent draw-tick is not wired for our seeded buildings, so the
// list stays empty (count=0) and the state-8 mesh loops iterate zero times (no visible
// rising). Invoke the faithful fill directly when the list is empty — the appended IBPs
// persist for the whole state-8 duration, so one call suffices.
static void BC8_EnsureMeshList(uintptr_t bc) {
    if (*reinterpret_cast<int32_t*>(bc + 0x60) > 0) return;
    BuildComponent_DrawReimpl(reinterpret_cast<void*>(bc));
    if (BC8_Trace())
        std::fprintf(stderr, "[BuildTrace] BC8_EnsureMeshList: filled via Draw, count now %d\n",
                     *reinterpret_cast<int32_t*>(bc + 0x60));
}

// ==================================================================================
// State1Update — sub_81e0 (per-frame while in state 1). trans38Handler for state 1.
// ==================================================================================
extern "C" char BuildComponent_State1Update(void* buildComp, void* sm) {
    uintptr_t bc = reinterpret_cast<uintptr_t>(buildComp);
    if (!bc) return 1;
    if (BuildComponent_DemoGateSkip(buildComp)) return 1;   // don't self-advance completed/non-demo
    BC8_Resolve();

    bool haveAll = BuildComponent_HaveAllMaterials(buildComp);
    if (!haveAll && BC8_DemoForce() && BC8_IsDemo(buildComp)) {
        haveAll = true;   // brick-2a harness: pretend materials arrived (real haul = 2b)
        if (BC8_Trace()) std::fprintf(stderr, "[BuildTrace] (force) State1Update demo materials present -> 8\n");
    }
    if (haveAll) {
        BC8_StopWorker(bc);
        ::ControllerLive::RequestStateTransition(sm, 8);
        if (BC8_Trace())
            std::fprintf(stderr, "[BuildTrace] State1Update: materials present, requested 1->8 (self=%p)\n", buildComp);
    }
    return 1;
}

// ==================================================================================
// State8Enter — sub_7ae0 (on entering state 8). enterHandler for state 8.
// Brick 2a: reset construction meshes to frame 0 (STAGED branch). The BuildStructure
// work-spawn (staged) and the instant branch (needs sub_8720) are DEFERRED to brick 2c.
// ==================================================================================
extern "C" char BuildComponent_State8Enter(void* buildComp, void* sm) {
    uintptr_t bc = reinterpret_cast<uintptr_t>(buildComp);
    if (!bc) return 1;
    if (BuildComponent_DemoGateSkip(buildComp)) return 1;
    (void)sm;
    BC8_Resolve();

    uint8_t instant = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        instant = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd0) & 1;

    if (instant == 0) {
        // STAGED: reset every construction mesh to frame 0.
        BC8_EnsureMeshList(bc);
        int meshCount = *reinterpret_cast<int32_t*>(bc + 0x60);
        for (int i = 0; i < meshCount; ++i) {
            uintptr_t me = BC8_MeshElem(bc, i);
            if (!me) continue;
            int16_t cur = *reinterpret_cast<int16_t*>(me + 0x166);
            if (cur == 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(me + 0x10));
                if (s_modelRefresh) s_modelRefresh(*reinterpret_cast<uintptr_t*>(me + 0x10), 0);
            } else {
                BC8_SetMeshFrame(me, 0);
                BC8_ReaddDrawChildren(me);
            }
            uintptr_t entSlot = *reinterpret_cast<uintptr_t*>(bc + 0x68) + (int64_t)i * 8;
            if (*reinterpret_cast<uintptr_t*>(entSlot) != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entSlot));
                if (s_entity8f50) s_entity8f50(reinterpret_cast<void*>(entSlot));
            }
        }
        // Brick 2c: alloc BuildStructure (0x70) + find the site WorkComponent (c8070)
        // + CitizenAI_impl_ec60(worker, 0x2001, 1, &bsIBP) to enqueue the construct work,
        // so citizens claim it and do timed BuildStructure labor (BuildStructure::Execute
        // assigns each to a build position; PerformWork accumulates progress into bc+0x48).
        // Faithful to sub_7ae0's STAGED tail. Default-ON via kDefaultOnFlags (2026-07-22);
        // env var BANISHED_BUILD_LIVE_HAUL is a kill-switch (=0). Needs the live-citizen
        // action stack + task manager (same as the gather post), both default-on.
        if (::ComponentFactory::LiveEnabled("BANISHED_BUILD_LIVE_HAUL")) {
            void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                            ? fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8) : nullptr;
            // Construct via the FULL retail ctor (0x1400e59c0): it routes base init through
            // Work_Ctor25 (sets field_10=site entity, +0x18 WorkComponent, +0x28 target cell)
            // AND allocates the build-position byte array field_50/field_58 from the site's
            // PositionComponent footprint. sub_7ae0 passes &(*(BuildComponent+0x10)) as the
            // entity IBP. Without the array, BuildStructure::Execute sees 0 unbuilt positions
            // and returns 1 (never assigns labor) — this is the 2c-B fix.
            void* bsWork = nullptr;
            if (mem) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
                void* siteEnt = *reinterpret_cast<void**>(bc + 0x10);   // *(BuildComponent+0x10)
                bsWork = BuildStructure_CreateInPlaceFull(mem, &siteEnt, 0);
            }
            if (bsWork) {
                g_pBuildStructureWork = bsWork;   // 2c-B probe: correlate with 7b60 claim walk
                if (BC8_Trace() && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                    // 2c-B probe: the scheduler per-entity tree keys the build work under
                    // BuildStructure::GetTarget = *(*(gw+0xb0)+0x38); the idle claim walk C looks
                    // up *(*(gw+0xb0)+0x30) (gather's key). If +0x38 differs / is null, no idle
                    // citizen reaches the build work.
                    uintptr_t pc = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xb0);
                    void* key30 = pc ? *reinterpret_cast<void**>(pc + 0x30) : nullptr;
                    void* key38 = pc ? *reinterpret_cast<void**>(pc + 0x38) : nullptr;
                    std::fprintf(stderr, "[BuildTrace] BuildTarget keys: gw+0xb0=%p +0x30(gather)=%p +0x38(build)=%p %s\n",
                                 (void*)pc, key30, key38, key30 == key38 ? "SAME" : "DIFFER");
                }
                uintptr_t bsIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&bsIBP, bsWork);
                // find the site WorkComponent (c8070) the work posts under
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
                uintptr_t workCompIBP = 0;
                if (s_find3050) s_find3050(*reinterpret_cast<uintptr_t*>(bc + 0x10), &workCompIBP);
                if (s_postEc60 && workCompIBP != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIBP);
                    s_postEc60(workCompIBP, 0x2001, 1, &bsIBP);
                    if (BC8_Trace())
                        std::fprintf(stderr, "[BuildTrace] State8Enter STAGED: meshes reset (count=%d); BuildStructure work spawned+posted (work=%p workComp=%p buildPositions=%d)\n",
                                     meshCount, bsWork, *reinterpret_cast<void**>(workCompIBP),
                                     *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(bsWork) + 0x58));

                    // ---- 2c-B: seat Builders now that the build work exists ----
                    // Materials are delivered (state 1->8 passed) and the idle Laborers that
                    // hauled them are free again, so NOW convert some to Builders to do the
                    // labor. The employment tick (0x1400d43b0) only rebalances workplaces that
                    // already have a worker; it never seats the FIRST builder. Retail's builders
                    // come from the town's builder-profession COUNT (town-hall Professions UI ->
                    // dd10 loops the FindAvailableBuilding + cbf0/ca20/c5f0/c4b0 cascade N times);
                    // with no player in autostart we drive that exact cascade via the ported
                    // headless driver, seating the site's builder capacity (WorkComponent+0x30,
                    // lazily via CalcMaxWorkers 0x14012e5c0) so no iteration runs with avail==0.
                    // Roster-only guard suppresses the site build-mesh toggle during the attach.
                    uintptr_t gwB = g_pGameWorldSingleton ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
                    uintptr_t townMgrB = gwB ? *reinterpret_cast<uintptr_t*>(gwB + 0xf0) : 0;
                    uintptr_t profRegB = gwB ? *reinterpret_cast<uintptr_t*>(gwB + 0xb0) : 0;
                    uint64_t builderMarker = profRegB ? *reinterpret_cast<uint64_t*>(profRegB + 0x38) : 0;
                    if (BC8_BuilderNatural()) {
                        // Natural flow: the tick-driven Builder pool seats builders via the
                        // townMgr+0x100 registration made at State1Enter — do NOT direct-seat here.
                        if (BC8_Trace())
                            std::fprintf(stderr, "[BuildTrace] State8Enter: natural flow -> deferring builder seat to BuilderPool_Tick (no direct SeatWorkersAt)\n");
                    } else if (townMgrB && builderMarker) {
                        static void* s_calcMaxWorkers = nullptr; static bool s_cmwRes = false;
                        if (!s_cmwRes) { s_calcMaxWorkers = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14012e5c0)); s_cmwRes = true; }
                        int builderCount = 0;
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIBP);
                        int cap = *reinterpret_cast<int*>(workCompIBP + 0x30);
                        if (cap <= 0 && s_calcMaxWorkers) {
                            cap = reinterpret_cast<int(*)(void*)>(s_calcMaxWorkers)(reinterpret_cast<void*>(workCompIBP));
                            *reinterpret_cast<int*>(workCompIBP + 0x30) = cap;
                        }
                        builderCount = cap > 0 ? cap : 1;
                        if (const char* e = std::getenv("BANISHED_BUILDER_COUNT"); e && e[0]) builderCount = std::atoi(e);
                        // site entity = *(BuildComponent+0x10) (the workplace the builders attach to)
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
                        int64_t siteEntity = *reinterpret_cast<int64_t*>(bc + 0x10);
                        bool prevRoster = g_wpcRosterOnly;
                        g_wpcRosterOnly = true;
                        int seated = siteEntity ? CitizenEmploy_SeatWorkersAt(static_cast<int64_t>(townMgrB), siteEntity, builderMarker, builderCount) : 0;
                        g_wpcRosterOnly = prevRoster;
                        if (BC8_Trace())
                            std::fprintf(stderr, "[BuildTrace] State8Enter seated %d/%d Builder(s) at site=%p (builderMarker=0x%llx)\n",
                                         seated, builderCount, (void*)siteEntity, (unsigned long long)builderMarker);
                    }
                } else if (BC8_Trace()) {
                    std::fprintf(stderr, "[BuildTrace] State8Enter STAGED: BuildStructure spawned but post skipped (postFn=%p workComp=%p)\n",
                                 (void*)s_postEc60, (void*)workCompIBP);
                }
            } else if (BC8_Trace()) {
                std::fprintf(stderr, "[BuildTrace] State8Enter STAGED: BuildStructure alloc/ctor failed (mem=%p)\n", mem);
            }
        } else
        if (BC8_Trace())
            std::fprintf(stderr, "[BuildTrace] State8Enter STAGED: meshes reset (count=%d); BuildStructure work-spawn (liveLabor off)\n", meshCount);
    } else {
        // Instant branch (gw+0xd0&1) needs sub_8720 — DEFERRED.
        if (BC8_Trace())
            std::fprintf(stderr, "[BuildTrace] State8Enter INSTANT branch DEFERRED (self=%p)\n", buildComp);
    }
    return 1;
}

// ==================================================================================
// State8Update — sub_76e0 (per-frame while in state 8). trans38Handler for state 8.
// Advances mesh frames proportional to progress/target; on completion snaps to final +
// requests 8 -> 2.
// ==================================================================================
extern "C" char BuildComponent_State8Update(void* buildComp, void* sm) {
    uintptr_t bc = reinterpret_cast<uintptr_t>(buildComp);
    if (!bc) return 1;
    if (BuildComponent_DemoGateSkip(buildComp)) return 1;
    BC8_Resolve();

    void* matComp = BC8_MatComp(bc);
    int target = BC8_Target(bc, matComp);

    if (BC8_DemoForce() && BC8_IsDemo(buildComp)) {
        // brick-2a harness: ramp progress toward target (~60 frames) to drive the mesh
        // progression + completion. Real BuildStructure labor = brick 2c.
        //
        // The real target = matComp[0x30] (base build effort) * bc[0x4c] (build
        // multiplier). In standalone bc[0x4c] is 0 — BuildComponent::Draw stage 4 copies
        // it from the WorkPlaceComponent's +0x58 field, which is never initialized here —
        // so the real target is 0. Substitute a nominal positive target (base effort, or
        // 120 frames) so the mesh progression + completion demonstrate visibly. Real
        // BuildStructure labor (2c) computes and raises the true target.
        // Ramp budget in frames (~60fps): default 600 (~10s) so the rise is easy to watch.
        // Override with BANISHED_BUILD_DEMO_FRAMES.
        static int rampFrames = -1;
        if (rampFrames < 0) {
            const char* e = std::getenv("BANISHED_BUILD_DEMO_FRAMES");
            rampFrames = (e && e[0]) ? std::atoi(e) : 600;
            if (rampFrames < 1) rampFrames = 600;
        }
        // Demo mode ALWAYS ramps over rampFrames. The real target (matComp[0x30] *
        // bc[0x4c]) is tiny in SA — when bc[0x4c] (build multiplier) is 0, BC8_Target
        // falls back to the bare base effort (~30), so the foundation would complete in
        // ~30 ticks, too fast to inspect or click. Stretching to rampFrames keeps the
        // building visibly under construction (default 600 ≈ 10s; raise BANISHED_BUILD_DEMO_FRAMES
        // to e.g. 30000 for a leisurely click test). Real BuildStructure labor (2c) uses
        // the true target and is unaffected (this block is demo-force + demo-site only).
        {
            target = rampFrames;
            static bool s_loggedTarget = false;
            if (BC8_Trace() && !s_loggedTarget) {
                s_loggedTarget = true;
                std::fprintf(stderr, "[BuildTrace] (force) demo ramp: bc+0x4c=%d matComp=%p -> ramp over %d frames\n",
                             *reinterpret_cast<int32_t*>(bc + 0x4c), matComp, rampFrames);
            }
        }
        int inc = target / rampFrames; if (inc < 1) inc = 1;
        int p = *reinterpret_cast<int32_t*>(bc + 0x48) + inc;
        if (p > target) p = target;
        *reinterpret_cast<int32_t*>(bc + 0x48) = p;
    }

    BC8_EnsureMeshList(bc);
    int progress = *reinterpret_cast<int32_t*>(bc + 0x48);
    int meshCount = *reinterpret_cast<int32_t*>(bc + 0x60);

    if (BC8_Trace()) {
        static int s_lastProg = -999;
        if (progress != s_lastProg) {
            s_lastProg = progress;
            std::fprintf(stderr, "[BuildTrace] State8Update progress=%d target=%d (bc+0x4c mult=%d meshCount=%d) self=%p\n",
                         progress, target, *reinterpret_cast<int32_t*>(bc + 0x4c), meshCount, buildComp);
        }
    }

    if (progress < target) {
        // still building — advance each mesh to floor((progress/target)*frameCount),
        // clamped to frameCount-2 so it never shows the final frame while incomplete.
        for (int i = 0; i < meshCount; ++i) {
            uintptr_t me = BC8_MeshElem(bc, i);
            if (!me) continue;
            void* modelDesc = BC8_MeshModelDesc(me);
            int frameCount = BC8_MeshFrameCount(me, modelDesc);
            if (frameCount <= 0) continue;
            int frame = (int)(((float)progress / (float)target) * (float)frameCount);
            if (frame == frameCount - 1) frame = frameCount - 2;
            int16_t cur = *reinterpret_cast<int16_t*>(me + 0x166);
            if (frame != (int)cur) {
                BC8_SetMeshFrame(me, frame);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(me + 0x10));
                if (s_modelRefresh) s_modelRefresh(*reinterpret_cast<uintptr_t*>(me + 0x10), 0);
                uintptr_t entSlot = *reinterpret_cast<uintptr_t*>(bc + 0x68) + (int64_t)i * 8;
                if (*reinterpret_cast<uintptr_t*>(entSlot) != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entSlot));
                    if (s_entity8f50) s_entity8f50(reinterpret_cast<void*>(entSlot));
                }
            }
        }
    } else {
        // labor complete — stop worker, snap every mesh to final frame, request 8 -> 2.
        BC8_StopWorker(bc);
        for (int i = 0; i < meshCount; ++i) {
            uintptr_t me = BC8_MeshElem(bc, i);
            if (!me) continue;
            void* modelDesc = BC8_MeshModelDesc(me);
            int frameCount = BC8_MeshFrameCount(me, modelDesc);
            int finalFrame = frameCount - 1;
            int16_t cur = *reinterpret_cast<int16_t*>(me + 0x166);
            if (finalFrame == (int)cur) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(me + 0x10));
                if (s_modelRefresh) s_modelRefresh(*reinterpret_cast<uintptr_t*>(me + 0x10), 0);
            } else {
                BC8_SetMeshFrame(me, finalFrame);
                BC8_ReaddDrawChildren(me);
            }
            uintptr_t entSlot = *reinterpret_cast<uintptr_t*>(bc + 0x68) + (int64_t)i * 8;
            if (*reinterpret_cast<uintptr_t*>(entSlot) != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entSlot));
                if (s_entity8f50) s_entity8f50(reinterpret_cast<void*>(entSlot));
            }
        }
        ::ControllerLive::RequestStateTransition(sm, 2);
        if (BC8_Trace())
            std::fprintf(stderr, "[BuildTrace] State8Update: progress %d>=target %d -> requested 8->2 (self=%p)\n",
                         progress, target, buildComp);
    }
    return 1;
}

// ==================================================================================
// State8OneShot — sub_7600 (updateHandler for state 8; fires as state 8 exits).
// Model mode 4 + remove site from the pending-build registry (townMgr+0xd0) + finalize
// the site MeshInstance.
// ==================================================================================
extern "C" char BuildComponent_State8OneShot(void* buildComp, void* sm) {
    uintptr_t bc = reinterpret_cast<uintptr_t>(buildComp);
    if (!bc) return 1;
    if (BuildComponent_DemoGateSkip(buildComp)) return 1;
    (void)sm;
    BC8_Resolve();

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
    if (s_modelRefresh) s_modelRefresh(*reinterpret_cast<uintptr_t*>(bc + 0x10), 4);

    uintptr_t townMgr = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    uintptr_t selfIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfIBP, buildComp);
    if (townMgr && s_findInQueue) {
        int idx = s_findInQueue(reinterpret_cast<void*>(townMgr + 0xd0), &selfIBP);
        if (idx >= 0) {
            int count = *reinterpret_cast<int32_t*>(townMgr + 0xd8);
            uintptr_t base = *reinterpret_cast<uintptr_t*>(townMgr + 0xd0);
            for (int j = idx; j + 1 < count; ++j)
                *reinterpret_cast<uint64_t*>(base + (int64_t)j * 8) =
                    *reinterpret_cast<uint64_t*>(base + (int64_t)(j + 1) * 8);
            *reinterpret_cast<int32_t*>(townMgr + 0xd8) = count - 1;
        }
    }
    if (s_findMeshInst) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
        uintptr_t miIBP = 0;
        void* mi = s_findMeshInst(*reinterpret_cast<uintptr_t*>(bc + 0x10), &miIBP);
        if (mi) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mi);
            uintptr_t inst = *reinterpret_cast<uintptr_t*>(mi);
            if (inst && s_meshFinalize) s_meshFinalize(reinterpret_cast<void*>(inst), 0, 0);
        }
    }
    if (BC8_Trace()) std::fprintf(stderr, "[BuildTrace] State8OneShot: model mode 4 + dequeued (self=%p)\n", buildComp);
    return 1;
}

// ==================================================================================
// State8Finalize — sub_7ff0 (via State8To2Action sub_75e0, the 8->2 transition action).
// Refund leftover site-storage materials to town stock (townMgr+0x170) + reconfigure the
// site storage as OUTPUT storage.
// ==================================================================================
static void BuildComponent_State8Finalize(uintptr_t bc) {
    BC8_Resolve();
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(bc + 0x10));
    uintptr_t storageIBP = 0;
    if (s_findStorage) s_findStorage(*reinterpret_cast<uintptr_t*>(bc + 0x10), &storageIBP);
    if (storageIBP == 0) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
    uintptr_t storage = storageIBP;

    uintptr_t townMgr = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);

    // (a) refund leftover storage slots to town stock (townMgr+0x170; entry idx*0x20+0xc).
    int slotCount = *reinterpret_cast<int32_t*>(storage + 0x30);
    for (int k = 0; k < slotCount; ++k) {
        uintptr_t slotArr = *reinterpret_cast<uintptr_t*>(storage + 0x28);
        uintptr_t slot = slotArr + (int64_t)k * 0x10;
        int count = *reinterpret_cast<int32_t*>(slot + 8);
        void* matType = *reinterpret_cast<void**>(slot);
        int resIdx = s_resIdx ? s_resIdx(reinterpret_cast<void*>(slotArr), matType) : -1;
        if (resIdx >= 0 && townMgr) {
            uintptr_t stockBase = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
            if (stockBase) {
                int* pStock = reinterpret_cast<int*>((int64_t)resIdx * 0x20 + 0xc + stockBase);
                *pStock += count;
            }
        }
    }

    // (b) reconfigure storage as OUTPUT storage (StorageDescription-driven).
    void* desc = nullptr;
    {
        void* d = *reinterpret_cast<void**>(storage + 0x18);
        uint32_t tid = BC8_TypeId("StorageDescription", 0x1401c7eb0);
        if (d && tid && fn_IsDerivedFrom && fn_IsDerivedFrom(d, tid)) desc = d;
    }
    if (!desc) return;
    int mult = 1;
    if (*reinterpret_cast<char*>((uint8_t*)desc + 0x20) != 0) {
        // sized output: scan storage entity (storage+0x10) for a MapComponent -> footprint.
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(storage + 0x10));
        uintptr_t ent = *reinterpret_cast<uintptr_t*>(storage + 0x10);
        uint32_t tidMap = BC8_TypeId("MapComponent", 0x1401c79e0);
        void* mapc = nullptr;
        if (ent && tidMap) {
            int cc = *reinterpret_cast<int32_t*>(ent + 0x38);
            uintptr_t carr = *reinterpret_cast<uintptr_t*>(ent + 0x30);
            for (int i = 0; i < cc; ++i) {
                void* c = *reinterpret_cast<void**>(carr + (int64_t)i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
                if (!c) continue;
                typedef uint32_t (*GT_t)(void*);
                uint32_t t = ((GT_t)(*(uintptr_t*)(*(uintptr_t*)c + 0x10)))(c);
                if (t == tidMap) { if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tidMap)) mapc = c; break; }
            }
        }
        if (mapc)
            mult = (int)*reinterpret_cast<int16_t*>((uint8_t*)mapc + 0x2e) *
                   (int)*reinterpret_cast<int16_t*>((uint8_t*)mapc + 0x2c);
    }
    int perItem   = *reinterpret_cast<int32_t*>((uint8_t*)desc + 0x24);
    int acceptTyp = *reinterpret_cast<int32_t*>((uint8_t*)desc + 0x28);
    if (s_storageClear) s_storageClear(reinterpret_cast<void*>(storage + 0x28), 0);
    *reinterpret_cast<int32_t*>(storage + 0x3c) = acceptTyp;
    *reinterpret_cast<int32_t*>(storage + 0x38) = perItem * mult;
    if (BC8_Trace())
        std::fprintf(stderr, "[BuildTrace] State8Finalize: storage reconfigured OUTPUT accept=0x%x cap=%d (mult=%d)\n",
                     acceptTyp, perItem * mult, mult);
}

// sub_75e0 — the 8->2 transition ACTION (node slot +0x28 / our OnState). 1-arg (bc).
extern "C" char BuildComponent_State8To2Action(void* buildComp) {
    if (buildComp && !BuildComponent_DemoGateSkip(buildComp))
        BuildComponent_State8Finalize(reinterpret_cast<uintptr_t>(buildComp));
    return 0;   // return ignored by Tick (Phase A2 discards the slot-0x28 return)
}
