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
//     (gw+0xd0 &1) is set OR the description's _instant (+0x2d) is set. The DEFAULT
//     ClearAreaDescription has _instant=0, so the sweep is skipped and clearing goes
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

#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Entity/ComponentEntity.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/Component.h"   // B.3.x W0: Component::OnEvent by name
#include "Component/ComponentDescription.h"
#include "Component/MapComponent.h"
#include "Component/WorkComponent.h"
#include "Work/Work.h"   // B.3.8 CB1a (Vtable)
#include "Work/ClearArea.h"   // B.3.8 CB1a (Vtable)
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

// ============================================================
// Helpers, called directly: 0x140003050 FindComponent<WorkComponent>,
// 0x14012ec60 CitizenAI_WorkScheduler_Post.
// ============================================================

// Retail's `IsDerivedFrom(p, tid) ? p : nullptr` idiom on a component's description.
template <class T>
static T* AsType(Description* obj) {
    if (!obj || !T::TypeId()) return nullptr;
    return ::Object::IsDerivedFromNullSafe(obj, T::TypeId()) ? static_cast<T*>(obj) : nullptr;
}

// Every finder is ComponentEntity::FindComponent<T>. The tree answers null for a null
// entity; retail has no such test. Kept (B9).
template <class T>
static T* FindOn(ComponentEntity* entity) {
    if (!entity) return nullptr;
    return entity->FindComponent<T>();
}

// ============================================================
// Grid area-status helpers (ports of FUN_14009d860 / dac0 / d930).
// grid = int*; grid[0] = width; *(char*)grid+0x80 = tile array base; tile stride
// 0x30; tile+0x08 = occupant entity IBP; tile+0x29 flags byte, +0x2a byte;
// *(void**)((char*)grid+0xe8) = reserved-cell BST root {key@0, left@0x10, right@0x18}.
// GameUpdateLoop::m_game is still void* (GAME-a) and Map's cells are untyped (GAME-b):
// every grid read below is LEFT.
// ============================================================
static inline uintptr_t GridTiles(int* grid) {
    return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
}

// tile+0x08: the cell's occupant entity.
static inline ComponentEntity* GridOccupant(uintptr_t tiles, int cell) {
    return *reinterpret_cast<ComponentEntity**>(tiles + static_cast<uintptr_t>(cell) * 0x30 + 8);
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
// new entity's own pointer). CountRemaining already skips self this exact
// way; this makes the two agree.
static bool Grid_AllEmpty(int* grid, int x1, int y1, int w, int h, ComponentEntity* selfEntity) {
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = y1; row < y1 + h; ++row) {
        for (int col = x1; col < x1 + w; ++col) {
            ComponentEntity* occ = GridOccupant(tiles, width * row + col);
            if (occ != nullptr && occ != selfEntity)
                return false;
        }
    }
    return true;
}

// FUN_14009d930 — true if NO target NaturalResourceComponent remains in the rect
// (optionally filtered to occupants whose description (entity+0x28) == filter).
static bool Grid_NoTargetResource(int* grid, int x1, int y1, int w, int h,
                                  const ComponentDescription* filter) {
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = y1; row < y1 + h; ++row) {
        for (int col = x1; col < x1 + w; ++col) {
            ComponentEntity* occ = GridOccupant(tiles, width * row + col);
            if (!occ) continue;
            if (occ->FindComponent<NaturalResourceComponent>() == nullptr) continue;
            if (filter == nullptr || occ->_description == filter)
                return false;
        }
    }
    return true;
}

// ============================================================
// ClearAreaComponent::IsDone — FUN_140118440: is the clear area fully done?
//   desc = the ClearAreaDescription; footprint from FindComponent<MapComponent>(owner).
//   groundOK = (desc._clearGround==0) || Grid_GroundClear
//   if (desc._clearTrees==0 || desc._clearBuildings!=0) done = groundOK && Grid_AllEmpty
//   else                                                done = groundOK && Grid_NoTargetResource(target)
// Retail takes the description through IsDerivedFrom (CMOVNZ to 0) and then reads its
// flags unguarded; the tree reads them off the component's description, answering zero
// flags when it has none. Retail derefs the MapComponent and the map unguarded; the tree
// answers false. Both kept (B9).
// ============================================================
bool ClearAreaComponent::IsDone() {
    if (GameUpdateLoop::_singleton == nullptr) return true;
    Description* d = _description;
    const ClearAreaDescription* desc = static_cast<ClearAreaDescription*>(d);
    bool clearTrees = false, clearGround = false, clearBuildings = false;
    const ComponentDescription* target = nullptr;
    if (desc) {
        clearTrees     = desc->_clearTrees;
        clearGround    = desc->_clearGround;
        clearBuildings = desc->_clearBuildings;
        target         = desc->_target;
    }

    MapComponent* mc = FindOn<MapComponent>(_entity);   // 0x140004680
    if (!mc) return false;
    int x1 = static_cast<int16_t>(mc->tileX0);
    int y1 = static_cast<int16_t>(mc->tileY0);
    int w  = static_cast<int16_t>(mc->tileX1);
    int h  = static_cast<int16_t>(mc->tileY1);

    int* grid = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);   // LEFT (GAME-a)
    if (!grid) return false;

    bool groundOK = !clearGround || Grid_GroundClear(grid, x1, y1, w, h);
    if (!groundOK) return false;
    // Owner entity — skipped by the AllEmpty scan; see the note on Grid_AllEmpty.
    const bool allEmptyBranch = (!clearTrees || clearBuildings);
    const bool done = allEmptyBranch ? Grid_AllEmpty(grid, x1, y1, w, h, _entity)
                                     : Grid_NoTargetResource(grid, x1, y1, w, h, target);

    return done;
}

// ============================================================
// ClearAreaComponent::CountRemaining — BuildUI_helper_8330 (0x140118330): count
// footprint occupants that are not the owner itself. Stored into clearState (+0x48) by
// State0Enter as progress bookkeeping.
// ============================================================
int ClearAreaComponent::CountRemaining() {
    if (GameUpdateLoop::_singleton == nullptr) return 0;
    MapComponent* mc = FindOn<MapComponent>(_entity);   // 0x140004680
    if (!mc) return 0;
    int x1 = static_cast<int16_t>(mc->tileX0);
    int y1 = static_cast<int16_t>(mc->tileY0);
    int w  = static_cast<int16_t>(mc->tileX1);
    int h  = static_cast<int16_t>(mc->tileY1);
    int* grid = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);   // LEFT (GAME-a)
    if (!grid) return 0;
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    int n = 0;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            ComponentEntity* occ = GridOccupant(tiles, (y1 + row) * width + (x1 + col));
            if (occ != _entity && occ != nullptr) ++n;
        }
    }
    return n;
}

// ============================================================
// Resource-kind scan (State0Enter's inner block, 0x140117960). Walks the footprint;
// for the first occupant carrying a NaturalResourceDescription sub-description, reads
// its nested RawMaterialDescription (_flags +0x5c kind bits) and OR's the scheduler
// category bit:
//   bit10 -> 0x400, bit12 -> 0x1000 (iron), bit11 -> 0x800 (stone), else -> 1 (tree).
// The result is the `code` argument the work-scheduler uses to categorise the job.
// ============================================================
static uint32_t ClearArea_ScanKindFlags(int* grid, int x1, int y1, int w, int h) {
    uint32_t kind = 1;
    if (!grid) return kind;
    const uint32_t tidNatResDesc = NaturalResourceDescription::TypeId();
    const uint32_t tidRawMatDesc = RawMaterialDescription::TypeId();
    int width = *grid;
    uintptr_t tiles = GridTiles(grid);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            ComponentEntity* occ = GridOccupant(tiles, (y1 + row) * width + (x1 + col));
            if (!occ) continue;
            // The occupant's description; iterate its sub-descriptions for the
            // NaturalResourceDescription.
            ComponentDescription* desc = occ->_description;
            if (!desc) continue;
            if (!desc->_subDescs) continue;
            NaturalResourceDescription* natRes = nullptr;
            for (int j = 0; j < desc->_subDescCount; ++j) {
                Description* sub = desc->_subDescs[j];
                if (!sub) continue;
                if (sub->Type() == tidNatResDesc) {
                    if (::Object::IsDerivedFromNullSafe(sub, tidNatResDesc))
                        natRes = static_cast<NaturalResourceDescription*>(sub);
                    break;
                }
            }
            if (!natRes) continue;
            // NaturalResourceDescription::_building = the nested RawMaterialDescription holder.
            ComponentDescription* holder = natRes->_building;
            if (!holder) continue;
            if (!holder->_subDescs) continue;
            for (int k = 0; k < holder->_subDescCount; ++k) {
                Description* rm = holder->_subDescs[k];
                if (!rm) continue;
                if (rm->Type() == tidRawMatDesc) {
                    if (::Object::IsDerivedFromNullSafe(rm, tidRawMatDesc)) {
                        uint32_t f = static_cast<RawMaterialDescription*>(rm)->_flags;
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
// Request a 0->target transition through the SM node, exactly like helper_7960's /
// helper_78f0's finalize tail (node slot +0x08 = RequestTransition). Uses the shared
// idiom in controller_state_table.h.
// ============================================================
#include "Component/controller_state_table.h"

// ============================================================
// State-0 ENTER — ClearAreaComponent::State0Enter, 0x140117960.
// ============================================================
char ClearAreaComponent::State0Enter(void* sm) {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (gw == nullptr) return 1;

    ClearAreaDescription* desc = AsType<ClearAreaDescription>(_description);

    // (1) Guarded instant sweep: only when the global instant bit or _instant is set.
    bool doSweep = (gw->m_gameFlags & 1) != 0;
    if (!doSweep && desc && desc->_instant) doSweep = true;
    if (doSweep)
        FootprintSweep();

    // (2) is-done? A footprint whose only occupant is the building itself (an
    // already-built house — its cells register their own entity at tile+0x08, so
    // Grid_AllEmpty is never true) has nothing to clear: CountRemaining skips self,
    // so remaining==0 means "no clearable resource" and we finish immediately rather
    // than posting phantom clear-work every state entry.
    int remaining = CountRemaining();
    clearState = remaining;   // progress bookkeeping (+0x48)
    if (remaining == 0 || IsDone()) {
        ::ControllerLive::RequestStateTransition(sm, 1);   // finish: 0 -> 1
        return 1;
    }

    // (3) dispatch the harvest work-action to the citizen work-scheduler.
    WorkComponent* workComp = FindOn<WorkComponent>(_entity);   // 0x140003050
    MapComponent* mc = FindOn<MapComponent>(_entity);           // 0x140004680

    int* grid = reinterpret_cast<int*>(gw->m_game);   // LEFT (GAME-a)
    uint32_t kind = 1;
    if (mc && grid) {
        int x1 = static_cast<int16_t>(mc->tileX0);
        int y1 = static_cast<int16_t>(mc->tileY0);
        int w  = static_cast<int16_t>(mc->tileX1);
        int h  = static_cast<int16_t>(mc->tileY1);
        kind = ClearArea_ScanKindFlags(grid, x1, y1, w, h);
    }

    if (!workComp || !::System::MemoryAllocator::_singleton) return 1;

    // Build the ClearArea work-action (ClearArea_Ctor 0x1400e6960) and post it.
    void* mem = ::System::MemoryAllocator::MemAlloc(0x78, 8);
    if (!mem) return 1;
    ComponentDescription* target = desc ? desc->_target : nullptr;
    uint8_t f0 = desc ? desc->_clearTrees : 1, f1 = desc ? desc->_clearGround : 0;
    uint8_t f2 = desc ? desc->_clearBuildings : 1;
    uint8_t f3 = desc ? desc->_priority : 0;
    void* ownerIBP = _entity;   // retail: local_res10 = *param_2 (the entity IBP)
    // ClearArea's ctor (WORK-a) takes the ExternalPtr as a raw uint64_t: LEFT.
    ClearArea* work = ::new (mem) ClearArea(&ownerIBP, reinterpret_cast<uint64_t>(target), f0, f1, f2);

    ::Object::InternalPtr<ClearArea> workIBP(work);
    // CitizenAI_WorkScheduler_Post (COMP-b) takes the WorkComponent as uintptr_t: LEFT.
    CitizenAI_WorkScheduler_Post(reinterpret_cast<uintptr_t>(workComp), kind, static_cast<char>(f3), &workIBP);

    return 1;
}

// ============================================================
// State-0 PER-FRAME — ClearAreaComponent::State0PerFrame, 0x1401178f0.
//   When the footprint is finally clear, request the 0 -> 1 transition.
// ============================================================
//   Faithful to 78f0: the ONLY done-test is the grid-emptiness IsDone (0x140118440).
//   The CountRemaining==0 short-circuit that State0Enter uses must NOT appear here.
//   CountRemaining only counts natural-resource occupants, so it drops to 0 the moment
//   the tree is felled — finishing the job (and stopping the citizen's work slot) before
//   ClearArea::Execute Phase 3 can gather the harvest pile the felling just spawned. That
//   is what stranded piles on the ground. The pile registers itself into its cell+0x08
//   (see the grid claim in action_base_cnr.cpp), so Grid_AllEmpty stays false while it
//   lies there, holding the job open until a citizen hauls it away — exactly as retail.
//   State0Enter keeps its short-circuit: that guards the already-built founding houses,
//   whose own footprint self-occupancy would otherwise post phantom clear-work forever.
char ClearAreaComponent::State0PerFrame(void* sm) {
    if (IsDone()) {
        ::ControllerLive::RequestStateTransition(sm, 1);
    }
    return 1;
}

// ============================================================
// State-0 UPDATE (on transition) — ClearAreaComponent::State0Update, 0x140117890.
//   Stop the owner WorkComponent's active work slot when leaving state 0.
//   FUN_14012ead0 = has-active-slot (WorkComponent_CheckWorkSlot),
//   FUN_14012eb50 = stop-slot (WorkComponent_RegisterWorkSlot).
// ============================================================
char ClearAreaComponent::State0Update() {
    WorkComponent* wc = FindOn<WorkComponent>(_entity);   // 0x140003050
    if (wc && WorkComponent_CheckWorkSlot(wc)) {
        WorkComponent_RegisterWorkSlot(wc);
    }
    return 1;
}

// ============================================================
// ModelComponent_helper_2b70 (0x1400c2b70) — broadcast a lifecycle event to every
// component on an entity via Component::OnEvent (slot 12). This is what tells the
// clear-job's StatusIcon/Model/etc. components to finalize (remove the removal icon,
// hide the felled model) when the area is cleared. The null entity / null slot tests
// are the tree's (retail's 0x1400c2b70 has neither); kept (B9). The same body is
// ModelComponentNotify_Reimpl in modelcomponent.cpp (not this block).
// ============================================================
static void ClearArea_BroadcastEvent(ComponentEntity* ownerEntity, int ev) {
    if (!ownerEntity) return;
    if (!ownerEntity->_components) return;
    for (int i = 0; i < ownerEntity->_componentCount; ++i) {
        Component* comp = ownerEntity->_components[i].Get();
        if (!comp) continue;
        comp->OnEvent(ev);
    }
}

// ============================================================
// ClearAreaComponent::FinishClear — 0x140118270 (Ghidra ModelComponent_impl_8270; its
// RCX is the ClearAreaComponent: +0x18 IsDerivedFrom ClearAreaDescription, +0x10 the
// entity). Gated on _regrowTerrain (+0x2c): when set, refreshes the region height over
// the footprint then broadcasts event 5 to the owner's components. When clear (the
// common case) it is a no-op.
// ============================================================
// CitizenAI_impl_e0e0 (0x14009e0e0) = Pf_RefreshRegionHeight_9e0e0, called directly.
void ClearAreaComponent::FinishClear() {
    ClearAreaDescription* desc = AsType<ClearAreaDescription>(_description);
    if (!desc) return;
    if (!desc->_regrowTerrain) return;

    if (GameUpdateLoop::_singleton != nullptr) {
        MapComponent* mc = FindOn<MapComponent>(_entity);   // 0x140004680
        if (mc) {
            // Map (m_game) is still void* (GAME-a): LEFT.
            unsigned int* map = reinterpret_cast<unsigned int*>(GameUpdateLoop::_singleton->m_game);
            int x1 = static_cast<int16_t>(mc->tileX0);
            int y1 = static_cast<int16_t>(mc->tileY0);
            int x2 = static_cast<int16_t>(mc->tileX1) + x1;
            int y2 = static_cast<int16_t>(mc->tileY1) + y1;
            Pf_RefreshRegionHeight_9e0e0(map, x1, y1, x2, y2);
        }
    }
    ClearArea_BroadcastEvent(_entity, 5);
}

// B4 FORWARD: buildcomponent_state1.cpp.
void ClearArea_Finalize8270(void* comp) {
    if (!comp) return;
    reinterpret_cast<ClearAreaComponent*>(comp)->FinishClear();
}

// ============================================================
// State-1 ENTER — ClearAreaComponent::State1Enter, 0x140117820.
//   FinishClear then request the 1 -> 2 transition.
// ============================================================
char ClearAreaComponent::State1Enter(void* sm) {
    FinishClear();
    ::ControllerLive::RequestStateTransition(sm, 2);   // 1 -> 2
    return 1;
}

// ============================================================
// State-2 ENTER — ClearAreaComponent::State2Enter, 0x1401177f0.
//   Broadcast event 1 to the owner's components (final cleanup: removes the removal
//   icon, finishes the model). The SM then rests in state 2 (terminal for the flow).
// ============================================================
char ClearAreaComponent::State2Enter() {
    ClearArea_BroadcastEvent(_entity, 1);
    return 1;
}

// The state table's handler adapters: the controller hands each one its owner
// (node+0x18), the ClearAreaComponent.
static char CA_State0Enter(ClearAreaComponent* c, void* sm)    { return c->State0Enter(sm); }     // 0x140117960
static char CA_State0PerFrame(ClearAreaComponent* c, void* sm) { return c->State0PerFrame(sm); }  // 0x1401178f0
static char CA_State0Update(ClearAreaComponent* c)             { return c->State0Update(); }      // 0x140117890
static char CA_State1Enter(ClearAreaComponent* c, void* sm)    { return c->State1Enter(sm); }     // 0x140117820
static char CA_State2Enter(ClearAreaComponent* c)              { return c->State2Enter(); }       // 0x1401177f0

// B4 FORWARD: tool_harness.cpp drives state 0's enter directly.
extern "C" char ClearArea_State0Enter(void* data, void* sm) {
    if (!data) return 1;
    return reinterpret_cast<ClearAreaComponent*>(data)->State0Enter(sm);
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
    TableRegisterState(t, 0, reinterpret_cast<void*>(&CA_State0Enter),
                       nullptr, reinterpret_cast<void*>(&CA_State0PerFrame),
                       reinterpret_cast<void*>(&CA_State0Update));
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 0x15: cancel/removal path — not reached from the clear flow; register the
    //             structure (ids + transitions) faithfully with null handlers.
    TableRegisterState(t, 0x15, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 1: model-finalize on entry (helper_7820: 8270 + request 1->2) -> {2, 3}.
    TableRegisterState(t, 1, reinterpret_cast<void*>(&CA_State1Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 2: model-finalize on entry (helper_77f0: broadcast event 1) -> {3}.
    TableRegisterState(t, 2, reinterpret_cast<void*>(&CA_State2Enter),
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
// B4 FORWARD: orchardcomponent_states.cpp. The tree answers true for a null component.
extern "C" bool ClearAreaCtrl_IsDone(void* comp) {
    if (!comp) return true;
    return reinterpret_cast<ClearAreaComponent*>(comp)->IsDone();
}

extern "C" void* ClearAreaCtrl_BuildWork(void* work, void* ownerEnt, uint64_t descRef,
                                         uint8_t clearTrees, uint8_t clearGround,
                                         uint8_t clearBuildings) {
    void* ownerIBP = ownerEnt;   // retail: local_res10 = *param_2 (the entity IBP)
    return ::new (work) ClearArea(&ownerIBP, descRef, clearTrees, clearGround, clearBuildings);
}
