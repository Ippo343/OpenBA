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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

static bool s_initialized = false;

// ============================================================
// ClearAreaComponent::Shutdown (vtable slot [3], 0x140117EC0)
// ============================================================
// Calls Component_Serialize, then:
//   1. Empty serializer block (version 0) -- format compat stub
//   2. Serializer block with clearState at +0x48
void ClearAreaComponent_Shutdown(ClearAreaComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty serializer block (format compatibility stub)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize clearState
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serialize_Int && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
        s_fn_Serialize_Int(serializer2, (int*)&self->clearState, nullptr);
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ClearAreaComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    s_initialized = true;

#ifdef _DEBUG
    printf("[ClearAreaComponent] Funcs resolved: CompSerialize=%p Ctor=%p Dtor=%p SerInt=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_Serialize_Int);
#endif
}

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
typedef void (*fn_ArrayPush_t)(void* arr, int newCount, void* valuePtr);
static fn_MarkForRemoval_t s_fn_MarkForRemoval = nullptr;
static fn_ArrayPush_t      s_fn_ArrayPush = nullptr;
static bool                s_sweepResolved = false;

static void ClearArea_ResolveSweepFuncs() {
    if (s_sweepResolved) return;
    s_fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_fn_ArrayPush = reinterpret_cast<fn_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    s_sweepResolved = true;
}

// Returns the number of footprint entities newly marked for removal.
int ClearAreaComponent_FootprintSweep(ClearAreaComponent* self) {
    ClearArea_ResolveSweepFuncs();
    if (!self || !s_fn_MarkForRemoval || !s_fn_ArrayPush || !fn_InternalBasePtr_CheckPtr) {
        return 0;
    }

    uintptr_t sAddr = reinterpret_cast<uintptr_t>(self);
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return 0;
    uintptr_t gwA = reinterpret_cast<uintptr_t>(gw);

    // ownerIBP: self+0x10 holds the building entity IBP value (param_1+0x10 in 7d60).
    void* ownerIBP = reinterpret_cast<void*>(sAddr + 0x10);
    fn_InternalBasePtr_CheckPtr(ownerIBP);
    void* entity = *reinterpret_cast<void**>(sAddr + 0x10);
    if (!entity) return 0;

    // FindComponent_Build(entity) -> the BuildComponent that holds the footprint rect.
    uint64_t buildComp = 0;
    fn_FindComponent_Build(entity, &buildComp);
    fn_InternalBasePtr_CheckPtr(&buildComp);
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
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(sAddr + 0x10));
            void* selfEnt = *reinterpret_cast<void**>(sAddr + 0x10);
            if (occ == 0 || occ == reinterpret_cast<uint64_t>(selfEnt)) continue;

            void* local = reinterpret_cast<void*>(occ);
            fn_InternalBasePtr_CheckPtr(&local);
            if (!local) continue;
            uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(local) + 0x40);
            if (flags & 0x400) continue;   // already marked for removal

            // Mark + push (the proven backend).
            fn_InternalBasePtr_CheckPtr(&local);
            s_fn_MarkForRemoval(local);
            uintptr_t rl = reinterpret_cast<uintptr_t>(entityList) + 0xD0;
            int cnt = *reinterpret_cast<int*>(rl + 0x8);
            s_fn_ArrayPush(reinterpret_cast<void*>(rl), cnt + 1, &local);
            ++marked;
        }
    }
    return marked;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid ClearAreaComponent
// (size 0x50, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from ClearAreaComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstring>
#include <cstdio>

// ============================================================
// LIVE factory (Track A step 2): de-inert the ClearAreaComponent so it clears its
// building's footprint AUTOMATICALLY, without any harness.
// ============================================================
// Opt-in via BANISHED_CLEAR_FOOTPRINT (or the global BANISHED_LIVE_COMPONENTS); default
// OFF → byte-for-byte unchanged inert run. When ON:
//   * component capability flags get bit 0x8 (needs-Update-tick) added to 0x80, so the
//     EntityList bucket round-robin (entitylist.cpp: component+0x20 bit 3) ticks it.
//   * vtable slot 9 (+0x48 Update) routes to a driver that runs the footprint sweep.
// The building entity already carries the entity-level tick bit (+0x40 bit 0x8) from its
// BuildComponent, so our component's Update is reached. The sweep is idempotent (the
// +0x400 already-marked guard) so running it every tick is safe: once the footprint is
// clear there is nothing left to mark.  (Full retail also HARVESTS the resources over
// time via helper_7960 phase-2 → ClearArea work-action; that yield-over-time path is the
// remaining Track A step-2 refinement. This instant-clear matches the desc _flag5 path.)
static uint32_t ClearAreaComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ClearAreaComponent");
    return cached;
}
static void* ClearAreaComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void ClearAreaComponent_UpdateVSlot(void* self, void* /*deltaTime*/) {
    // Prove the EntityList is ticking our live component (once, on the first call).
    static bool s_firstTick = false;
    if (!s_firstTick) {
        s_firstTick = true;
        printf("[ClearArea LIVE] first Update tick from EntityList — component=%p is now "
               "auto-sweeping its footprint each frame\n", self);
        fflush(stdout);
    }
    int marked = ClearAreaComponent_FootprintSweep(reinterpret_cast<ClearAreaComponent*>(self));
    if (marked > 0) {
        static int s_logged = 0;
        if (s_logged < 12) {
            ++s_logged;
            printf("[ClearArea LIVE] component=%p auto-swept footprint: marked %d resource(s) "
                   "for removal\n", self, marked);
            fflush(stdout);
        }
    }
}

// Instant-clear path (Track A step 2, verified): the component's OWN Update tick
// (vtable slot 9) marks the footprint for removal → EntityList destroys next frame.
static bool ClearAreaComponent_InstantOn() {
    return ::ComponentFactory::LiveEnabled("BANISHED_CLEAR_FOOTPRINT");
}
// Harvest-over-time path (Track A refinement #1, faithful): the entity's
// StateMachineComponent links this component's embedded state node (at +0x28) and
// ticks its clearing state machine, which posts a ClearArea work-action to a citizen
// so the footprint resources are chopped over time (logs/stone/iron). Requires the
// StateMachineComponent to be live (BANISHED_LIVE_STATEMACHINE).
static bool ClearAreaComponent_SMOn() {
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CLEARCTRL");
}

// State-machine hooks defined in clearareacomponent_states.cpp.
extern "C" void** ClearAreaCtrl_NodeVtable();
extern "C" void   ClearAreaCtrl_PopulateTable();
static int   ClearAreaComponent_GetDefaultState(void*) { return 0; }        // slot 16
static void* ClearAreaComponent_CreateState(void* self) {                   // slot 17
    return reinterpret_cast<uint8_t*>(self) + 0x28;                         // node @ +0x28
}

// Install the live vtable slots (re-run every instance: InitInertTyped re-stubs the
// shared vtable). SM path takes precedence over the instant path when both are set.
static void ClearAreaComponent_FinishLiveVtable(void* mem) {
    if (ClearAreaComponent_SMOn()) {
        ClearAreaComponent_LiveVtable[16] = reinterpret_cast<void*>(&ClearAreaComponent_GetDefaultState);
        ClearAreaComponent_LiveVtable[17] = reinterpret_cast<void*>(&ClearAreaComponent_CreateState);
        ClearAreaCtrl_PopulateTable();
        if (mem) {
            // Embedded node: vtable at +0x28, back-pointer (node+0x18) = the component.
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28) =
                reinterpret_cast<void*>(ClearAreaCtrl_NodeVtable());
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28 + 0x18) = mem;
        }
    } else if (ClearAreaComponent_InstantOn()) {
        ClearAreaComponent_LiveVtable[9] = reinterpret_cast<void*>(&ClearAreaComponent_UpdateVSlot);
    }
}

// Capability flags: SM path uses the plain ControllerComponent flags (0x80) — the SM
// ticks the node, so the component needs no self-tick bit. Instant path adds 0x8 so
// the EntityList ticks the component's own Update.
static uint32_t ClearAreaComponent_Flags() {
    if (ClearAreaComponent_SMOn())      return 0x80u;
    if (ClearAreaComponent_InstantOn()) return 0x88u;
    return 0x80u;
}

extern "C" void* ClearAreaComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x50, ClearAreaComponent_Flags(),
              ClearAreaComponent_LiveVtable,
              reinterpret_cast<void*>(&ClearAreaComponent_LiveGetTypeId));
    ClearAreaComponent_FinishLiveVtable(p);
    return p;
}
extern "C" void* ClearAreaComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x50, ClearAreaComponent_Flags(),
             ClearAreaComponent_LiveVtable,
             reinterpret_cast<void*>(&ClearAreaComponent_LiveGetTypeId));
    ClearAreaComponent_FinishLiveVtable(mem);
    return mem;
}
REGISTER_TYPE(ClearAreaComponent, "ControllerComponent",
              &ClearAreaComponent_Factory, &ClearAreaComponent_CreateInPlace);

// Harness probe: does this building entity carry a live ClearAreaComponent? Scans the
// entity's component list for the ClearAreaComponent type-id and returns the component
// ptr (or null). outFlags gets the component's +0x20 capability flags (bit 0x4 = ticked).
extern "C" void* ClearAreaComponent_HarnessFind(void* entity, uint32_t* outFlags) {
    if (outFlags) *outFlags = 0;
    if (!entity || !fn_InternalBasePtr_CheckPtr) return nullptr;
    uint32_t wantTid = ::Object::GetTypeIdByName("ClearAreaComponent");
    if (!wantTid) return nullptr;
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    fn_InternalBasePtr_CheckPtr(&entity);
    e = reinterpret_cast<uintptr_t>(entity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    void** comps = *reinterpret_cast<void***>(e + 0x30);
    if (!comps) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* c = comps[i];
        if (!c) continue;
        fn_InternalBasePtr_CheckPtr(&c);
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
