// code/Game/Component/clearareacomponent_states.cpp
// Phase 3 / Track A refinement #1 — FAITHFUL harvest-over-time state machine for
// the ClearAreaComponent (standalone exe).
//
// Placing a building stamps a ClearAreaComponent (a ControllerComponent) onto the
// foundation entity. In retail the component's clearing behaviour is a small state
// machine registered once by ClearAreaDescription_Setup -> helper_8010:
//
//   state 0  (default): enter=helper_7960, update=helper_7890, perFrame=helper_78f0
//                       transitions -> {1, 3}
//   state 0x15        : enter=&LAB_1177e0, update=helper_7770, perFrame=&LAB_1177d0
//                       transitions -> {1, 3}   (a cancel/removal path — NOT reached
//                       from the clear flow; registered for structural fidelity)
//   state 1          : enter=helper_7820  transitions -> {2, 3}
//   state 2          : enter=helper_77f0  transitions -> {3}
//
// helper_7960 (state-0 ENTER) is the actual clearing dispatch:
//   * (guarded) instant footprint sweep — only when the global instant bit
//     (gw+0xd0 &1) is set OR the description's _flag5 (+0x2d) is set. The DEFAULT
//     ClearAreaDescription has _flag5=0, so the sweep is skipped and clearing goes
//     through the harvest path below (matching retail default behaviour).
//   * is-done check (FUN_140118440). If already clear -> request transition to 1.
//   * otherwise build a ClearArea WORK action (ClearArea_Ctor@0x1400e6960 over
//     Work_Ctor25@0x1400f72e0) and POST it to the citizen work-scheduler
//     (CitizenAI_impl_ec60 = CitizenAI_WorkScheduler_Post, already live). A laborer
//     then executes ClearArea::Execute (cleararea.cpp) over time, chopping each
//     footprint resource and yielding logs/stone/iron.
// helper_78f0 (state-0 PER-FRAME): re-checks is-done; when the footprint is finally
//   clear it requests the 0->1 transition (finishing the clear).
// helper_7890 (state-0 UPDATE, on transition): stops the work slot when leaving.
//
// This is driven by the entity's StateMachineComponent, which links every owner
// ControllerComponent's embedded state node (at comp+0x28) and ticks it. Opt-in via
// BANISHED_LIVE_CLEARCTRL (or the global the global live-components switch) — default OFF =>
// byte-for-byte unchanged run. Requires the StateMachineComponent to be live
// (BANISHED_LIVE_STATEMACHINE) so the node is linked and ticked.
//
// Faithful offset-by-offset port.

#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/Component.h"   // B.3.x W0: Component::OnEvent by name
#include "Work/Work.h"   // B.3.8 CB1a (Vtable)
#include "Work/ClearArea.h"   // B.3.8 CB1a (Vtable)
#include "resolve.h"
#include "clearareacomponent_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/NaturalResourceComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7b30 cell class
#include "Component/NaturalResourceDescription.h"   // ENDGAME B.3.9 C1: 0x1401c7b68 cell class
#include "Component/RawMaterialDescription.h"       // ENDGAME B.3.9 C1: 0x1401c7cb8 cell class
#include "Component/ClearAreaDescription.h"         // ENDGAME B.3.9 C1: 0x1401c7200 cell class
#include "system/MemoryAllocator.h"
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"

// The instant footprint sweep lives in clearareacomponent.cpp (also used by the
// harness + the _flag5 instant path). Reused here for the guarded sweep.
class ClearAreaComponent;
extern int ClearAreaComponent_FootprintSweep(ClearAreaComponent* self);

// ============================================================
// Helpers, called directly: FUN_140003050 WorkBase_FindComponent_3050 (the
// WorkComponent), 0x14012ec60 CitizenAI_WorkScheduler_Post.
// ============================================================

// ============================================================
// Grid area-status helpers (ports of FUN_14009d860 / dac0 / d930).
// grid = int*; grid[0] = width; *(char*)grid+0x80 = tile array base; tile stride
// 0x30; tile+0x08 = occupant entity IBP; tile+0x29 flags byte, +0x2a byte;
// *(void**)((char*)grid+0xe8) = reserved-cell BST root {key@0, left@0x10, right@0x18}.
// ============================================================
static inline uintptr_t GridTiles(int* grid) {
    return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
}

// FUN_14009d860 — ground cells are clear & unreserved. Returns true if every cell in
// the rect is passable (flags bit0/1 clear, +0x2a == 0xff) and not in the reserved BST.
static bool Grid_GroundClear(int* grid, int x1, int y1, int w, int h) {
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    void* bstRoot = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(grid) + 0xe8);
    for (int row = y1; row < y1 + h; ++row) {
        for (int col = x1; col < x1 + w; ++col) {
            int cell = width * row + col;
            uintptr_t t = tiles + static_cast<uintptr_t>(cell) * 0x30;
            uint32_t f = 1u << (*reinterpret_cast<uint8_t*>(t + 0x29) & 0x1f);
            if ((f & 2) || (f & 1) || *reinterpret_cast<int8_t*>(t + 0x2a) != -1)
                return false;
            for (void* node = bstRoot; node;) {
                int nk = *reinterpret_cast<int*>(node);
                if (cell < nk)      node = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10);
                else if (cell <= nk) return false;   // reserved
                else                node = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18);
            }
        }
    }
    return true;
}

// FUN_14009dac0 — every cell in the rect is empty (no occupant entity at tile+0x08).
//
// STANDALONE DEVIATION (2026-08-23): `selfEntity`, and the `occ != selfEntity` test,
// are ours — retail's FUN_14009dac0 tests only `occ != 0`. Retail can afford that
// because nothing writes a *foundation's own* entity into cell+0x08; we do.
// StartTown_MarkBuildingCells stamps the owner into every footprint cell so runtime-
// spawned buildings are click-pickable, and it is called (only) from the rebuild/
// upgrade spawn in destroycomponent_states.cpp. Without the skip, the replacement
// building is its own obstruction: ClearArea state 0 posts clear-work, IsDone's
// AllEmpty branch reports done=0 forever, the SM parks in state 0 and never reaches
// BuildComponent's state 1 — so builders never deliver and the rebuilt house sits at
// 0% for good (user-reported 2026-08-23; measured as every footprint cell holding the
// new entity's own pointer). ClearArea_CountRemaining already skips self this exact
// way; this makes the two agree.
static bool Grid_AllEmpty(int* grid, int x1, int y1, int w, int h, void* selfEntity) {
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = y1; row < y1 + h; ++row) {
        for (int col = x1; col < x1 + w; ++col) {
            int cell = width * row + col;
            void* occ = *reinterpret_cast<void**>(
                tiles + static_cast<uintptr_t>(cell) * 0x30 + 8);
            if (occ != nullptr && occ != selfEntity)
                return false;
        }
    }
    return true;
}

// Scan an entity's component list for a NaturalResourceComponent (type 1c7b30).
static bool EntityHasNatResComp(void* entity) {
    if (!entity) return false;
    const uint32_t tidNatResComp = NaturalResourceComponent::TypeId();
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    if (!arr) return false;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* comp = *slot;
        if (!comp) continue;
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (tid == tidNatResComp)
            return ::Object::IsDerivedFromNullSafe(comp, tidNatResComp);
    }
    return false;
}

// FUN_14009d930 — true if NO target NaturalResourceComponent remains in the rect
// (optionally filtered to occupants whose description-ref (entity+0x28) == filter).
static bool Grid_NoTargetResource(int* grid, int x1, int y1, int w, int h, uint64_t filter) {
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = y1; row < y1 + h; ++row) {
        for (int col = x1; col < x1 + w; ++col) {
            int cell = width * row + col;
            uintptr_t t = tiles + static_cast<uintptr_t>(cell) * 0x30;
            void* occ = *reinterpret_cast<void**>(t + 8);
            if (!occ) continue;
            occ = *reinterpret_cast<void**>(t + 8);
            if (!EntityHasNatResComp(occ)) continue;
            if (filter == 0 || *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(occ) + 0x28) == filter)
                return false;
        }
    }
    return true;
}

// ============================================================
// FUN_140118440 — is the clear area fully done?
//   desc = comp+0x18 (ClearAreaDescription); footprint from FindComponent_Build(owner).
//   groundOK = (desc._flag1==0) || Grid_GroundClear
//   if (desc._flag0==0 || desc._flag2!=0) done = groundOK && Grid_AllEmpty
//   else                                  done = groundOK && Grid_NoTargetResource(descRef)
// ============================================================
static bool ClearArea_IsDone(void* comp) {
    if (!comp || GameUpdateLoop::_singleton == nullptr) return true;
    uintptr_t c = reinterpret_cast<uintptr_t>(comp);
    void* descObj = *reinterpret_cast<void**>(c + 0x18);
    // desc must be a ClearAreaDescription; if not, treat flags as zero (defensive).
    uint8_t flag0 = 0, flag1 = 0, flag2 = 0; uint64_t descRef = 0;
    if (descObj) {
        uintptr_t d = reinterpret_cast<uintptr_t>(descObj);
        flag0 = *reinterpret_cast<uint8_t*>(d + 0x28);
        flag1 = *reinterpret_cast<uint8_t*>(d + 0x29);
        flag2 = *reinterpret_cast<uint8_t*>(d + 0x2a);
        descRef = *reinterpret_cast<uint64_t*>(d + 0x20);
    }

    uint64_t buildComp = 0;
    fn_FindComponent_Build(*reinterpret_cast<void**>(c + 0x10), &buildComp);
    if (!buildComp) return false;
    uintptr_t bc = static_cast<uintptr_t>(buildComp);
    int x1 = *reinterpret_cast<int16_t*>(bc + 0x28);
    int y1 = *reinterpret_cast<int16_t*>(bc + 0x2a);
    int w  = *reinterpret_cast<int16_t*>(bc + 0x2c);
    int h  = *reinterpret_cast<int16_t*>(bc + 0x2e);

    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    if (!grid) return false;

    bool groundOK = (flag1 == 0) || Grid_GroundClear(grid, x1, y1, w, h);
    if (!groundOK) return false;
    // Owner entity — skipped by the AllEmpty scan; see the note on Grid_AllEmpty.
    void* selfEntity = *reinterpret_cast<void**>(c + 0x10);
    const bool allEmptyBranch = (flag0 == 0 || flag2 != 0);
    const bool done = allEmptyBranch ? Grid_AllEmpty(grid, x1, y1, w, h, selfEntity)
                                     : Grid_NoTargetResource(grid, x1, y1, w, h, descRef);

    return done;
}

// ============================================================
// BuildUI_helper_8330 — count footprint occupants that are not the owner itself.
// Stored into comp+0x48 (clearState) by helper_7960 as progress bookkeeping.
// ============================================================
static int ClearArea_CountRemaining(void* comp) {
    if (!comp || GameUpdateLoop::_singleton == nullptr) return 0;
    uintptr_t c = reinterpret_cast<uintptr_t>(comp);
    uint64_t buildComp = 0;
    fn_FindComponent_Build(*reinterpret_cast<void**>(c + 0x10), &buildComp);
    if (!buildComp) return 0;
    uintptr_t bc = static_cast<uintptr_t>(buildComp);
    int x1 = *reinterpret_cast<int16_t*>(bc + 0x28);
    int y1 = *reinterpret_cast<int16_t*>(bc + 0x2a);
    int w  = *reinterpret_cast<int16_t*>(bc + 0x2c);
    int h  = *reinterpret_cast<int16_t*>(bc + 0x2e);
    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
    if (!grid) return 0;
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    int n = 0;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int cell = (y1 + row) * width + (x1 + col);
            void* occ = *reinterpret_cast<void**>(tiles + static_cast<uintptr_t>(cell) * 0x30 + 8);
            void* se = *reinterpret_cast<void**>(c + 0x10);
            if (occ != se && occ != nullptr) ++n;
        }
    }
    return n;
}

// ============================================================
// Resource-kind scan (helper_7960 inner block). Walks the footprint; for the first
// occupant carrying a NaturalResourceDescription sub-description, reads its nested
// RawMaterialDescription (+0x5c kind bits) and OR's the scheduler category bit:
//   bit10 -> 0x400, bit12 -> 0x1000 (iron), bit11 -> 0x800 (stone), else -> 1 (tree).
// The result is the `code` argument the work-scheduler uses to categorise the job.
// ============================================================
static uint32_t ClearArea_ScanKindFlags(void* comp, int* grid, int x1, int y1, int w, int h) {
    uint32_t kind = 1;
    if (!comp || !grid) return kind;
    const uint32_t tidNatResDesc = NaturalResourceDescription::TypeId();
    const uint32_t tidRawMatDesc = RawMaterialDescription::TypeId();
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int cell = (y1 + row) * width + (x1 + col);
            void* occ = *reinterpret_cast<void**>(tiles + static_cast<uintptr_t>(cell) * 0x30 + 8);
            if (!occ) continue;
            occ = *reinterpret_cast<void**>(tiles + static_cast<uintptr_t>(cell) * 0x30 + 8);
            // entity+0x28 = its description; iterate sub-descriptions for NaturalResourceDescription.
            uintptr_t desc = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(occ) + 0x28);
            if (!desc) continue;
            int subCount = *reinterpret_cast<int*>(desc + 0x30);
            uintptr_t subArr = *reinterpret_cast<uintptr_t*>(desc + 0x28);
            if (!subArr) continue;
            void* natRes = nullptr;
            for (int j = 0; j < subCount; ++j) {
                void* sub = *reinterpret_cast<void**>(subArr + static_cast<uintptr_t>(j) * 8);
                if (!sub) continue;
                if (reinterpret_cast<Object::Base*>(sub)->Type() == tidNatResDesc) {   // B.3.8 bridge (slot 2)
                    if (::Object::IsDerivedFromNullSafe(sub, tidNatResDesc)) natRes = sub;
                    break;
                }
            }
            if (!natRes) continue;
            // NaturalResourceDescription+0x20 = nested RawMaterialDescription holder.
            uintptr_t holder = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(natRes) + 0x20);
            if (!holder) continue;
            int rCount = *reinterpret_cast<int*>(holder + 0x30);
            uintptr_t rArr = *reinterpret_cast<uintptr_t*>(holder + 0x28);
            if (!rArr) continue;
            for (int k = 0; k < rCount; ++k) {
                void* rm = *reinterpret_cast<void**>(rArr + static_cast<uintptr_t>(k) * 8);
                if (!rm) continue;
                if (reinterpret_cast<Object::Base*>(rm)->Type() == tidRawMatDesc) {   // B.3.8 bridge (slot 2)
                    if (::Object::IsDerivedFromNullSafe(rm, tidRawMatDesc)) {
                        uint32_t f = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c);
                        uint32_t u;
                        if ((f >> 10) & 1)      u = 0x400;
                        else if ((f >> 12) & 1) u = 0x1000;
                        else if ((f >> 11) & 1) u = 0x800;
                        else                    u = 1;
                        kind |= u;
                    }
                    break;
                }
            }
        }
    }
    return kind;
}

// ============================================================
// ClearArea_Ctor@0x1400e6960 (parameterized) over Work_Ctor25@0x1400f72e0.
//   work: 0x78-byte buffer. ownerEnt: the building entity ptr. descRef: filter.
//   clearTrees/Ground/Buildings: the description flags.
// ENDGAME B.3.9 EXA: the real class's parameterised ctor (was the `??0Internal`
// door on raw memory, a hand-inlined Work_Ctor25 and two stamps).
// ============================================================
static void* ClearArea_BuildWork(void* work, void* ownerEnt, uint64_t descRef,
                                 uint8_t clearTrees, uint8_t clearGround, uint8_t clearBuildings) {
    void* ownerIBP = ownerEnt;   // retail: local_res10 = *param_2 (the entity IBP)
    return ::new (work) ClearArea(&ownerIBP, descRef, clearTrees, clearGround, clearBuildings);
}

// ============================================================
// Request a 0->target transition through the SM node, exactly like helper_7960's /
// helper_78f0's finalize tail (node slot +0x08 = RequestTransition). Uses the shared
// idiom in controller_state_table.h.
// ============================================================
#include "Component/controller_state_table.h"

// ============================================================
// State-0 ENTER — port of ClearAreaDescription_helper_7960.
//   data = the ClearAreaComponent (node+0x18); sm = the state machine.
// ============================================================
extern "C" char ClearArea_State0Enter(void* data, void* sm) {
    if (!data || GameUpdateLoop::_singleton == nullptr) return 1;
    uintptr_t c = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);

    void* descObj = *reinterpret_cast<void**>(c + 0x18);
    bool descIsCA = descObj && ::Object::IsDerivedFromNullSafe(descObj, ClearAreaDescription::TypeId());
    uint8_t* desc = descIsCA ? reinterpret_cast<uint8_t*>(descObj) : nullptr;

    // (1) Guarded instant sweep: only when global instant bit or _flag5(+0x2d) set.
    bool doSweep = (gw[0xd0] & 1) != 0;
    if (!doSweep && desc && desc[0x2d] != 0) doSweep = true;
    if (doSweep)
        ClearAreaComponent_FootprintSweep(reinterpret_cast<ClearAreaComponent*>(data));

    // (2) is-done? A footprint whose only occupant is the building itself (an
    // already-built house — its cells register their own entity at tile+0x08, so
    // Grid_AllEmpty is never true) has nothing to clear: CountRemaining skips self,
    // so remaining==0 means "no clearable resource" and we finish immediately rather
    // than posting phantom clear-work every state entry.
    int remaining = ClearArea_CountRemaining(data);
    *reinterpret_cast<int32_t*>(c + 0x48) = remaining;   // clearState bookkeeping (+0x48)
    if (remaining == 0 || ClearArea_IsDone(data)) {
        ::ControllerLive::RequestStateTransition(sm, 1);   // finish: 0 -> 1
        return 1;
    }

    // (3) dispatch the harvest work-action to the citizen work-scheduler.
    uint64_t workCompIBP = 0;
    WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(c + 0x10), &workCompIBP);

    uint64_t buildComp = 0;
    fn_FindComponent_Build(*reinterpret_cast<void**>(c + 0x10), &buildComp);

    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gw) + 0xf8);
    uint32_t kind = 1;
    if (buildComp && grid) {
        uintptr_t b = static_cast<uintptr_t>(buildComp);
        int x1 = *reinterpret_cast<int16_t*>(b + 0x28);
        int y1 = *reinterpret_cast<int16_t*>(b + 0x2a);
        int w  = *reinterpret_cast<int16_t*>(b + 0x2c);
        int h  = *reinterpret_cast<int16_t*>(b + 0x2e);
        kind = ClearArea_ScanKindFlags(data, grid, x1, y1, w, h);
    }

    if (!workCompIBP || !::System::MemoryAllocator::_singleton) return 1;

    // Build the ClearArea work-action and post it.
    void* work = ::System::MemoryAllocator::MemAlloc(0x78, 8);
    if (!work) return 1;
    void* ownerEnt = *reinterpret_cast<void**>(c + 0x10);
    uint64_t descRef = desc ? *reinterpret_cast<uint64_t*>(desc + 0x20) : 0;
    uint8_t f0 = desc ? desc[0x28] : 1, f1 = desc ? desc[0x29] : 0, f2 = desc ? desc[0x2a] : 1;
    uint8_t f3 = desc ? desc[0x2b] : 0;
    ClearArea_BuildWork(work, ownerEnt, descRef, f0, f1, f2);

    uint64_t workIBP = 0;
    new (&workIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
    CitizenAI_WorkScheduler_Post(static_cast<uintptr_t>(workCompIBP), kind, static_cast<char>(f3), &workIBP);

    return 1;
}

// ============================================================
// State-0 PER-FRAME — port of ClearAreaDescription_helper_78f0.
//   When the footprint is finally clear, request the 0 -> 1 transition.
// ============================================================
//   Faithful to 78f0: the ONLY done-test is the grid-emptiness is-done (140118440).
//   The CountRemaining==0 short-circuit that State0Enter uses must NOT appear here.
//   CountRemaining only counts natural-resource occupants, so it drops to 0 the moment
//   the tree is felled — finishing the job (and stopping the citizen's work slot) before
//   ClearArea::Execute Phase 3 can gather the harvest pile the felling just spawned. That
//   is what stranded piles on the ground. The pile registers itself into its cell+0x08
//   (see the grid claim in action_base_cnr.cpp), so Grid_AllEmpty stays false while it
//   lies there, holding the job open until a citizen hauls it away — exactly as retail.
//   State0Enter keeps its short-circuit: that guards the already-built founding houses,
//   whose own footprint self-occupancy would otherwise post phantom clear-work forever.
extern "C" char ClearArea_State0PerFrame(void* data, void* sm) {
    if (!data) return 1;
    if (ClearArea_IsDone(data)) {
        ::ControllerLive::RequestStateTransition(sm, 1);
    }
    return 1;
}

// ============================================================
// State-0 UPDATE (on transition) — port of ClearAreaDescription_helper_7890.
//   Stop the owner WorkComponent's active work slot when leaving state 0.
//   FUN_14012ead0 = has-active-slot (WorkComponent_CheckWorkSlot),
//   FUN_14012eb50 = stop-slot (WorkComponent_RegisterWorkSlot).
// ============================================================
extern "C" char ClearArea_State0Update(void* data, void* /*sm*/) {
    if (!data) return 1;
    uintptr_t c = reinterpret_cast<uintptr_t>(data);
    uint64_t wcIBP = 0;
    WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(c + 0x10), &wcIBP);
    if (wcIBP && WorkComponent_CheckWorkSlot(reinterpret_cast<void*>(wcIBP))) {
        WorkComponent_RegisterWorkSlot(reinterpret_cast<void*>(wcIBP));
    }
    return 1;
}

// ============================================================
// ModelComponent_helper_2b70 (0x1400c2b70) — broadcast a lifecycle event to every
// component on an entity via the component vtable "event" slot +0x60. This is what
// tells the clear-job's StatusIcon/Model/etc. components to finalize (remove the
// removal icon, hide the felled model) when the area is cleared.
// ============================================================
static void ClearArea_BroadcastEvent(void* ownerEntity, uint32_t ev) {
    if (!ownerEntity) return;
    uintptr_t e = reinterpret_cast<uintptr_t>(ownerEntity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    if (!arr) return;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* comp = *slot;
        if (!comp) continue;
        reinterpret_cast<Component*>(comp)->OnEvent(static_cast<int>(ev));   // B.3.x bridge (slot 12)
    }
}

// ============================================================
// ModelComponent_impl_8270 (0x140118270) — state-1 finalize body. Gated on the
// ClearAreaDescription +0x2c flag: when set, notifies CitizenAI over the footprint
// (terrain regrow — DEFERRED, e0e0 not yet ported) then broadcasts event 5 to the
// owner's components. When +0x2c is clear (the common case) it is a no-op.
// ============================================================
// CitizenAI_impl_e0e0 (0x14009e0e0) = Pf_RefreshRegionHeight_9e0e0, called directly.

void ClearArea_Finalize8270(void* comp) {
    if (!comp) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(comp);
    void* descObj = *reinterpret_cast<void**>(c + 0x18);
    bool isCA = descObj && ::Object::IsDerivedFromNullSafe(descObj, ClearAreaDescription::TypeId());
    if (!isCA) return;
    uint8_t flag2c = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(descObj) + 0x2c);
    if (flag2c == 0) return;

    // desc+0x2c set: terrain regrow over the footprint, then broadcast event 5.
    if (GameUpdateLoop::_singleton != nullptr) {
        uint64_t bc = 0;
        fn_FindComponent_Build(*reinterpret_cast<void**>(c + 0x10), &bc);
        if (bc) {
            void* map = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
            uintptr_t b = static_cast<uintptr_t>(bc);
            int x1 = *reinterpret_cast<int16_t*>(b + 0x28);
            int y1 = *reinterpret_cast<int16_t*>(b + 0x2a);
            int x2 = *reinterpret_cast<int16_t*>(b + 0x2c) + x1;
            int y2 = *reinterpret_cast<int16_t*>(b + 0x2e) + y1;
            Pf_RefreshRegionHeight_9e0e0(reinterpret_cast<unsigned int*>(map), x1, y1, x2, y2);
        }
    }
    ClearArea_BroadcastEvent(*reinterpret_cast<void**>(c + 0x10), 5);
}

// ============================================================
// State-1 ENTER — port of ClearAreaDescription_helper_7820.
//   ModelComponent_impl_8270(comp) then request the 1 -> 2 transition.
// ============================================================
extern "C" char ClearArea_State1Enter(void* data, void* sm) {
    if (!data) return 1;
    ClearArea_Finalize8270(data);
    ::ControllerLive::RequestStateTransition(sm, 2);   // 1 -> 2
    return 1;
}

// ============================================================
// State-2 ENTER — port of ClearAreaDescription_helper_77f0.
//   Broadcast event 1 to the owner's components (final cleanup: removes the removal
//   icon, finishes the model). The SM then rests in state 2 (terminal for the flow).
// ============================================================
extern "C" char ClearArea_State2Enter(void* data, void* /*sm*/) {
    if (!data) return 1;
    uintptr_t c = reinterpret_cast<uintptr_t>(data);
    ClearArea_BroadcastEvent(*reinterpret_cast<void**>(c + 0x10), 1);
    return 1;
}

// ============================================================
// Dispatcher + state table for the ClearAreaComponent controller.
// ============================================================
// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<ClearAreaCtrlTag>, ClearAreaComponent.h).
#include "Component/ClearAreaComponent.h"
using ClearAreaDispatch = ::ControllerLive::Dispatcher<ClearAreaCtrlTag>;

// Table field -> node-slot mapping (see controller_state_table.h):
//   enterHandler (arg2 of sub_1140) -> slot +0x18 Enter    (on entry)
//   updateHandler(arg3)             -> slot +0x20 Update    (transition pending)
//   trans38Handler(arg5)            -> slot +0x38 DrawAlt    (PER-FRAME)
extern "C" void ClearAreaCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = ClearAreaDispatch::table;
    if (t.count != 0) return;   // idempotent
    // state 0: enter=State0Enter (dispatch harvest), update=State0Update (stop slot),
    //          per-frame=State0PerFrame (finish when clear). transitions -> {1, 3}.
    TableRegisterState(t, 0, reinterpret_cast<void*>(&ClearArea_State0Enter),
                       nullptr, reinterpret_cast<void*>(&ClearArea_State0PerFrame),
                       reinterpret_cast<void*>(&ClearArea_State0Update));
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 0x15: cancel/removal path — not reached from the clear flow; register the
    //             structure (ids + transitions) faithfully with null handlers.
    TableRegisterState(t, 0x15, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 1: model-finalize on entry (helper_7820: 8270 + request 1->2) -> {2, 3}.
    TableRegisterState(t, 1, reinterpret_cast<void*>(&ClearArea_State1Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 2: model-finalize on entry (helper_77f0: broadcast event 1) -> {3}.
    TableRegisterState(t, 2, reinterpret_cast<void*>(&ClearArea_State2Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
}

// ============================================================
// Shared with the ORCHARD controller's state 20 (the panel's "cut/replace" path).
//
// Retail's orchard state 20 calls the very same two leaves this file already ports:
//   * enter    0x1400504e0 builds a ClearArea work with ClearArea_Ctor (0x1400e6960)
//   * trans38  0x140050430 gates its return-to-planting on FUN_140118440
// Re-porting either one into orchardcomponent_states.cpp would fork two copies of the
// grid-scan semantics, so they are exported here instead of duplicated.
// ============================================================
extern "C" bool ClearAreaCtrl_IsDone(void* comp) { return ClearArea_IsDone(comp); }

extern "C" void* ClearAreaCtrl_BuildWork(void* work, void* ownerEnt, uint64_t descRef,
                                         uint8_t clearTrees, uint8_t clearGround,
                                         uint8_t clearBuildings) {
    return ClearArea_BuildWork(work, ownerEnt, descRef, clearTrees, clearGround, clearBuildings);
}
