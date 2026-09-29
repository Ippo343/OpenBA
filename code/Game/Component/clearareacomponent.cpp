// code/Game/Component/clearareacomponent.cpp
// Clean reimplementation of ClearAreaComponent lifecycle methods.
//
// ClearAreaComponent (0x50 bytes) -- ControllerComponent subclass.
// Has one extra field: clearState (int32) at +0x48.
// Handles clearing terrain areas during construction/placement.
//
// Vtable at 0x1401626d0. Non-trivial override:
//   Persist (0x140117EC0) -- serialize with clearState
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/clearareacomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "Action/exe_bodies.h"

// ============================================================
// ClearAreaComponent::FootprintSweep  (ClearAreaDescription_helper_7d60, 0x140117d60)
// ============================================================
// The heart of "placing a building marks trees/rocks/iron for removal". Reads the
// building's footprint rectangle off its MapComponent (FUN_140004680 =
// FindComponent<MapComponent>), walks every footprint cell, and for each occupying
// entity (tree/rock/iron) that isn't the building itself and isn't already flagged
// (+0x40 bit 0x400), marks it for removal (FUN_1400c2ea0) and pushes it onto the
// EntityList removal-pending list (FUN_140003110 → gw+0xe8,+0xD0). EntityList::Update
// Step 5 then destroys them.
//
// Map/tile layout (verified live + vs the markremoval backend harness):
//   gw+0xf8 -> map (int*);  *map = grid width;  *(map+0x80 bytes) = tile array base.
//   tile stride 0x30 bytes; entity ptr slot at +0x08.  gw+0xe8 -> EntityList.

// The mark-for-removal leaf 0x1400c2ea0 is ActHunt_MarkEntityRemoval_c2ea0, called directly.
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// Returns the number of footprint entities newly marked for removal. The tree's null
// tests (no game loop / owner / MapComponent / map / entity list) are its own; retail
// reads through each unguarded. Kept (B9).
int ClearAreaComponent::FootprintSweep() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (!gw) return 0;

    ComponentEntity* entity = _entity;
    if (!entity) return 0;

    // The MapComponent holds the footprint rect (retail MOVSX: the fields are signed).
    MapComponent* mc = entity->FindComponent<MapComponent>();   // 0x140004680
    if (!mc) return 0;

    int16_t startCol = static_cast<int16_t>(mc->tileX0);
    int16_t startRow = static_cast<int16_t>(mc->tileY0);
    int16_t width    = static_cast<int16_t>(mc->tileX1);
    int16_t height   = static_cast<int16_t>(mc->tileY1);

    // GameUpdateLoop::m_game (+0xf8) and m_entityList (+0xe8) are still void* (GAME-a)
    // and Map's cells are untyped (GAME-b): the grid and removal-array reads are LEFT.
    int* map = reinterpret_cast<int*>(gw->m_game);
    if (!map) return 0;
    int gridWidth = *map;
    uintptr_t tileArray = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(map) + 0x80);
    uintptr_t entityList = reinterpret_cast<uintptr_t>(gw->m_entityList);
    if (!tileArray || !entityList) return 0;

    int marked = 0;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int cell = (startRow + row) * gridWidth + (startCol + col);
            // entity ptr slot at cell*0x30 + 0x08 (matches 7d60's "+ 8 + (...)*0x30")
            ComponentEntity* occ = *reinterpret_cast<ComponentEntity**>(
                tileArray + static_cast<uintptr_t>(cell) * 0x30 + 8);

            // Skip empty cells and the building's own entity.
            if (occ == nullptr || occ == _entity) continue;
            if (occ->_entityFlags & 0x400) continue;   // already marked for removal

            // Mark + push (the proven backend).
            ActHunt_MarkEntityRemoval_c2ea0(occ);
            uintptr_t rl = entityList + 0xD0;
            int cnt = *reinterpret_cast<int*>(rl + 0x8);
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(rl).SetSizeFill(cnt + 1, &occ);
            ++marked;
        }
    }
    return marked;
}

// ============================================================================
// ENDGAME B.3.8 MI-A — ClearAreaComponent as a REAL class (ClearAreaComponent.h).
//
// Before: a 48-slot hand-built `ClearAreaComponent_LiveVtable` filled at every
// create (FillTypedVtable + FinishLiveVtable), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<ClearAreaCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ClearAreaComponent.h"
#include <cstring>
#include <cstdio>

// Harvest-over-time path (faithful): the entity's StateMachineComponent links this
// component's embedded controller (at +0x28) and ticks its clearing state machine
// (clearareacomponent_states.cpp), which posts a ClearArea work-action to a citizen
// so the footprint resources are chopped over time. The state table is built ONCE
// at static-init time, as retail's static initializer does (B38 D6/D7).
extern "C" void ClearAreaCtrl_PopulateTable();
static const bool s_clearAreaStateTableBuilt = (ClearAreaCtrl_PopulateTable(), true);

// 0x140117fa0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 = 0. No flag OR: the SM
// ticks the node, so the component needs no self-tick bit (0x80).
ClearAreaComponent::ClearAreaComponent() : _controller(this), clearState(0), _pad4C{} {}

// 0x140117f40: restamp + ~Internal, no own work.
ClearAreaComponent::~ClearAreaComponent() = default;

// create 0x140030ea0: MemAlloc(0x50, 8), then the ctor.
void* ClearAreaComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClearAreaComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ClearAreaComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ClearAreaComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ClearAreaComponent());
}

// [3] 0x140117ec0 — clearState round-trips (save/load wave 2026-08-28):
// Component::Persist (a direct CALL 0x1401189b0), an empty
// Serializer scope (format stub), then a scope carrying clearState (+0x48).
void ClearAreaComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);       // B.3.8 bridge
    scope.Serialize(clearState, nullptr);
}

DEFINE_EXE_TYPE(ClearAreaComponent, "ControllerComponent")

// Harness probe: does this building entity carry a live ClearAreaComponent? Scans the
// entity's component list for the ClearAreaComponent type-id and returns the component
// ptr (or null). outFlags gets the component's +0x20 capability flags (bit 0x4 = ticked).
extern "C" void* ClearAreaComponent_HarnessFind(void* entityV, uint32_t* outFlags) {
    if (outFlags) *outFlags = 0;
    if (!entityV) return nullptr;
    uint32_t wantTid = ClearAreaComponent::TypeId();
    if (!wantTid) return nullptr;
    ComponentEntity* entity = reinterpret_cast<ComponentEntity*>(entityV);   // B4 FORWARD (tool_harness.cpp)
    if (!entity->_components) return nullptr;
    for (int i = 0; i < entity->_componentCount; ++i) {
        Component* c = entity->_components[i].Get();
        if (!c) continue;
        if (c->Type() == wantTid) {
            if (outFlags) *outFlags = c->_flags;
            return c;
        }
    }
    return nullptr;
}

// Harness entry: run the footprint sweep on a REAL live ClearAreaComponent (reads its
// own +0x10 owner) — exactly what a de-inerted component's Update tick would do.
extern "C" int ClearAreaComponent_HarnessSweepComp(void* realComp) {
    return reinterpret_cast<ClearAreaComponent*>(realComp)->FootprintSweep();   // B4 FORWARD (tool_harness.cpp)
}

// Read a live component's +0x10 owner field (to confirm it's wired to its entity).
extern "C" void* ClearAreaComponent_HarnessOwner(void* realComp) {
    if (!realComp) return nullptr;
    return reinterpret_cast<ClearAreaComponent*>(realComp)->_entity;   // B4 FORWARD (tool_harness.cpp)
}

// Harness entry: run the footprint sweep for a given building entity. Builds a
// synthetic ClearAreaComponent whose owner (+0x10) is the building, then sweeps —
// exactly what the live component's driver does on placement. Returns entities marked.
// The raw zeroed buffer (no constructor) is the harness's own; it is kept.
extern "C" int ClearAreaComponent_HarnessSweep(void* ownerEntity) {
    alignas(8) uint8_t buf[sizeof(ClearAreaComponent)];
    std::memset(buf, 0, sizeof(buf));
    ClearAreaComponent* comp = reinterpret_cast<ClearAreaComponent*>(buf);        // harness-only
    comp->_entity = reinterpret_cast<ComponentEntity*>(ownerEntity);             // B4 FORWARD (tool_harness.cpp)
    return comp->FootprintSweep();
}
