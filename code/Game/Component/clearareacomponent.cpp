// code/Game/Component/clearareacomponent.cpp
// Clean reimplementation of ClearAreaComponent lifecycle methods.
//
// ClearAreaComponent (0x50 bytes) -- ControllerComponent subclass.
// Has one extra field: clearState (uint32) at +0x48.
// Handles clearing terrain areas during construction/placement.
//
// Vtable at 0x1401626d0. Non-trivial override:
//   Shutdown (0x140117EC0) -- serialize with clearState
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/clearareacomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// ClearAreaComponent footprint sweep  (ClearAreaDescription_helper_7d60, 0x140117d60)
// ============================================================
// The heart of "placing a building marks trees/rocks/iron for removal". Reads the
// building's footprint rectangle off its BuildComponent (FUN_140004680 =
// fn_FindComponent_Build), walks every footprint cell, and for each occupying entity
// (tree/rock/iron) that isn't the building itself and isn't already flagged (+0x40 bit
// 0x400), marks it for removal (FUN_1400c2ea0) and pushes it onto the EntityList
// removal-pending list (FUN_140003110 → gw+0xe8,+0xD0). EntityList::Update Step 5 then
// destroys them. Closure is fully satisfied: fn_FindComponent_Build (inline), and
// c2ea0/003110 registered by reg_action_hunt.cpp.
//
// `self` is the ClearAreaComponent; self+0x10 (owner) holds the building entity IBP.
// Faithful 1:1 port of 0x140117d60 (verified vs Ghidra disasm).
//
// Map/tile layout (verified live + vs the markremoval backend harness):
//   gw+0xf8 -> map (int*);  *map = grid width;  *(map+0x80 bytes) = tile array base.
//   tile stride 0x30 bytes; entity ptr slot at +0x08.  gw+0xe8 -> EntityList.

typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t s_fn_MarkForRemoval = nullptr;
static bool                s_sweepResolved = false;

// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.
static void ClearArea_ResolveSweepFuncs() {
    if (s_sweepResolved) return;
    s_fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_sweepResolved = true;
}

// Returns the number of footprint entities newly marked for removal.
int ClearAreaComponent_FootprintSweep(ClearAreaComponent* self) {
    ClearArea_ResolveSweepFuncs();
    if (!self || !s_fn_MarkForRemoval) {
        return 0;
    }

    uintptr_t sAddr = reinterpret_cast<uintptr_t>(self);
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return 0;
    uintptr_t gwA = reinterpret_cast<uintptr_t>(gw);

    void* entity = *reinterpret_cast<void**>(sAddr + 0x10);
    if (!entity) return 0;

    // FindComponent_Build(entity) -> the BuildComponent that holds the footprint rect.
    uint64_t buildComp = 0;
    fn_FindComponent_Build(entity, &buildComp);
    if (!buildComp) return 0;
    uintptr_t bc = static_cast<uintptr_t>(buildComp);

    int16_t startCol = *reinterpret_cast<int16_t*>(bc + 0x28);
    int16_t startRow = *reinterpret_cast<int16_t*>(bc + 0x2a);
    int16_t width    = *reinterpret_cast<int16_t*>(bc + 0x2c);
    int16_t height   = *reinterpret_cast<int16_t*>(bc + 0x2e);

    int* map = *reinterpret_cast<int**>(gwA + 0xf8);
    if (!map) return 0;
    int gridWidth = *map;
    uintptr_t tileArray = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(map) + 0x80);
    void* entityList = *reinterpret_cast<void**>(gwA + 0xe8);
    if (!tileArray || !entityList) return 0;

    int marked = 0;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int cell = (startRow + row) * gridWidth + (startCol + col);
            // entity ptr slot at cell*0x30 + 0x08 (matches 7d60's "+ 8 + (...)*0x30")
            uint64_t occ = *reinterpret_cast<uint64_t*>(tileArray + static_cast<uintptr_t>(cell) * 0x30 + 8);

            // Skip empty cells and the building's own entity.
            void* selfEnt = *reinterpret_cast<void**>(sAddr + 0x10);
            if (occ == 0 || occ == reinterpret_cast<uint64_t>(selfEnt)) continue;

            void* local = reinterpret_cast<void*>(occ);
            if (!local) continue;
            uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(local) + 0x40);
            if (flags & 0x400) continue;   // already marked for removal

            // Mark + push (the proven backend).
            s_fn_MarkForRemoval(local);
            uintptr_t rl = reinterpret_cast<uintptr_t>(entityList) + 0xD0;
            int cnt = *reinterpret_cast<int*>(rl + 0x8);
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(rl)).SetSizeFill(cnt + 1, &local);
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

// [0] 0x1400310c0: ~ClearAreaComponent (0x140117f40), MemFree on &1.
Object::Base* ClearAreaComponent::DeletingDtor(unsigned flags) {
    this->~ClearAreaComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
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
    scope.Serialize(*reinterpret_cast<int*>(&clearState), nullptr);
}

DEFINE_EXE_VTABLE(ClearAreaComponent, "_ZTV18ClearAreaComponent")
DEFINE_EXE_TYPE(ClearAreaComponent, "ControllerComponent")

// Harness probe: does this building entity carry a live ClearAreaComponent? Scans the
// entity's component list for the ClearAreaComponent type-id and returns the component
// ptr (or null). outFlags gets the component's +0x20 capability flags (bit 0x4 = ticked).
extern "C" void* ClearAreaComponent_HarnessFind(void* entity, uint32_t* outFlags) {
    if (outFlags) *outFlags = 0;
    if (!entity) return nullptr;
    uint32_t wantTid = ::Object::GetTypeIdByName("ClearAreaComponent");
    if (!wantTid) return nullptr;
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    e = reinterpret_cast<uintptr_t>(entity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    void** comps = *reinterpret_cast<void***>(e + 0x30);
    if (!comps) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* c = comps[i];
        if (!c) continue;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        uint32_t tid = reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c);
        if (tid == wantTid) {
            if (outFlags) *outFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(c) + 0x20);
            return c;
        }
    }
    return nullptr;
}

// Harness entry: run the footprint sweep on a REAL live ClearAreaComponent (reads its
// own +0x10 owner) — exactly what a de-inerted component's Update tick would do.
extern "C" int ClearAreaComponent_HarnessSweepComp(void* realComp) {
    return ClearAreaComponent_FootprintSweep(reinterpret_cast<ClearAreaComponent*>(realComp));
}

// Read a live component's +0x10 owner field (to confirm it's wired to its entity).
extern "C" void* ClearAreaComponent_HarnessOwner(void* realComp) {
    if (!realComp) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(realComp) + 0x10);
}

// Harness entry: run the footprint sweep for a given building entity. Builds a
// synthetic ClearAreaComponent whose owner (+0x10) is the building, then sweeps —
// exactly what the live component's driver does on placement. Returns entities marked.
extern "C" int ClearAreaComponent_HarnessSweep(void* ownerEntity) {
    uint8_t comp[0x50];
    std::memset(comp, 0, sizeof(comp));
    *reinterpret_cast<void**>(comp + 0x10) = ownerEntity;
    return ClearAreaComponent_FootprintSweep(reinterpret_cast<ClearAreaComponent*>(comp));
}
