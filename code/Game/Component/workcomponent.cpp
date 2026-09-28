// code/Game/Component/workcomponent.cpp
// Clean reimplementation of WorkComponent lifecycle methods from Application-x64.exe.
//
// WorkComponent (0x38 bytes) is a direct Component subclass managing work ID,
// flags, and state fields. Three non-trivial vtable overrides:
//   Shutdown  (0x14012e710) — versioned serialization of workId, workState, workFlags
//   Draw      (0x14012e6f0) — compute work capacity and store at +0x30
//   OnCreate  (0x14012e6c0) — check if work slot available, register if so
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/workcomponent_funcs.h"
#include "engine_funcs.h"
#include "Component/WorkDescription.h"   // ENDGAME B.3.9 TR
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"

// ============================================================
// Resolved helpers
// ============================================================

// FUN_14012e5c0 — compute work capacity for this component
typedef int (*fn_ComputeWorkCapacity_t)(void* self);
static fn_ComputeWorkCapacity_t s_fn_ComputeWorkCapacity = nullptr;

// FUN_14012ead0 — check if work slot is available (BST lookup)
typedef bool (*fn_CheckWorkSlot_t)(void* self);
static fn_CheckWorkSlot_t s_fn_CheckWorkSlot = nullptr;

// FUN_14012eb50 — register work slot
typedef void (*fn_RegisterWorkSlot_t)(void* self);
static fn_RegisterWorkSlot_t s_fn_RegisterWorkSlot = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ComputeWorkCapacity = reinterpret_cast<fn_ComputeWorkCapacity_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_fn_CheckWorkSlot = reinterpret_cast<fn_CheckWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012ead0));

    s_fn_RegisterWorkSlot = reinterpret_cast<fn_RegisterWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012eb50));

    s_initialized = true;
}


// ============================================================
// WorkComponent::Draw (vtable slot [6], 0x14012e6f0)
// ============================================================
// Compute the effective work capacity and store it at +0x30.

void WorkComponent_Draw(WorkComponent* self) {
    EnsureHelpers();

    if (s_fn_ComputeWorkCapacity) {
        int capacity = s_fn_ComputeWorkCapacity(self);
        self->workState = capacity;
    }
}


// ============================================================
// Reimpl helpers for the work-slot BST functions
// ============================================================
// CitizenAI_impl_8e30(tree, workId) — notify AI of slot change (0x1400f8e30)
typedef void (*fn_CitizenAI_8e30_t)(void* tree, int workId);
static fn_CitizenAI_8e30_t s_fn_CitizenAI_8e30 = nullptr;
// Entity_FindComponent_MeshInstance (0x140002f90)
typedef void* (*fn_WC_FindComp_t)(uintptr_t entity, void* retbuf);
static fn_WC_FindComp_t s_fn_WC_FindComp = nullptr;
// FUN_14012c3f0(comp, idx, char) — set mesh flag (0x14012c3f0)
typedef void (*fn_WC_SetMeshFlag_t)(void* comp, int idx, char p3);
static fn_WC_SetMeshFlag_t s_fn_WC_SetMeshFlag = nullptr;
static bool s_wcHelpersResolved = false;
static void EnsureWorkSlotHelpers() {
    if (s_wcHelpersResolved) return;
    s_fn_CitizenAI_8e30 = reinterpret_cast<fn_CitizenAI_8e30_t>(
        ResolveAddr(Module::Exe, 0x1400f8e30));
    s_fn_WC_FindComp = reinterpret_cast<fn_WC_FindComp_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_WC_SetMeshFlag = reinterpret_cast<fn_WC_SetMeshFlag_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_wcHelpersResolved = true;
}

// ============================================================
// Reimpl of FUN_14012ead0 (check whether this work slot is occupied:
//   BST lookup keyed by workId (+0x28) in the world work-slot tree)
// ============================================================
// Tree at *(g_pGameWorldSingleton+0x118) + 0x30. Node: key @+0x00, value IBP @+0x08,
// parent @+0x10, **LESSER child @+0x18, GREATER child @+0x20**, balance @+0x28 (0x30 bytes).
//
// The two child labels above were SWAPPED in this comment until Wave 9 lane N. The CODE
// was and is correct — it descends to +0x20 when the node's key is below the target — but
// the label was the wrong way round, and every other walker of this same node layout
// (CitizenAI_WorkScheduler_Rekey and _Insert in citizenai_work_scheduler.cpp,
// ActCt_TaskCleanup_8e30 steps 1/2/4 in action_base_complete.cpp,
// WorkComponent_RegisterWorkSlot just below) documents it correctly as lesser@+0x18.
// Left uncorrected in a family that has now paid THREE TIMES for a child-offset or
// slot mix-up (reference_workarea_bst_bad_entry.md), a comment that contradicts its own
// code and its four siblings is a defect waiting for the next reader.
bool WorkComponent_CheckWorkSlot(void* selfP) {
    uintptr_t self = (uintptr_t)selfP;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    int key = *(int*)(self + 0x28);
    int* node = *(int**)(*(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0x118) + 0x30);
    while (node != nullptr) {
        if (*node <= key) {
            if (key <= *node) {
                // equal — found; value pointer lives at node+0x08
                return *(int64_t*)((uintptr_t)node + 0x08) != 0;
            }
            node = *(int**)((uintptr_t)node + 0x20);   // lesser child
        } else {
            node = *(int**)((uintptr_t)node + 0x18);   // greater child
        }
    }
    // not found: construct an empty IBP and report non-null (matches original)
    int64_t ibp[1] = {0};
    if (fn_InternalBasePtr_Ctor) new (ibp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    return ibp[0] != 0;
}

// ============================================================
// Reimpl of FUN_14012eb50 (register/refresh this work slot:
//   when occupied, sync worker id, poke the AI tree, refresh mesh flags 2 & 3)
// ============================================================
void WorkComponent_RegisterWorkSlot(void* selfP) {
    EnsureWorkSlotHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!WorkComponent_CheckWorkSlot(selfP)) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    int key = *(int*)(self + 0x28);
    int64_t valueIbp[1] = {0};
    int* node = *(int**)(*(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0x118) + 0x30);
    bool found = false;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)((uintptr_t)node + 0x18);
        } else if (key <= *node) {
            valueIbp[0] = *(int64_t*)((uintptr_t)node + 0x08);
            found = true;
            break;
        } else {
            node = *(int**)((uintptr_t)node + 0x20);
        }
    }
    if (!found) {
        if (fn_InternalBasePtr_Ctor) new (valueIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    }

    *(uint32_t*)(self + 0x2c) = *(uint32_t*)(valueIbp[0] + 0x24);

    if (s_fn_CitizenAI_8e30)
        s_fn_CitizenAI_8e30(*(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118), key);

    int64_t compIbp[1] = {0};
    if (s_fn_WC_FindComp) s_fn_WC_FindComp(*(uintptr_t*)(self + 0x10), compIbp);
    if (compIbp[0] != 0) {
        if (s_fn_WC_SetMeshFlag) s_fn_WC_SetMeshFlag((void*)compIbp[0], 2, '\0');
        if (s_fn_WC_SetMeshFlag) s_fn_WC_SetMeshFlag((void*)compIbp[0], 3, '\0');
    }
}

// ============================================================================
// ENDGAME B.3.8 VIS — WorkComponent as a REAL class (WorkComponent.h).
//
// Before: a 48-slot WorkComponent_LiveVtable filled per create, Inert_ScalarDtor in
// slot 0 (only ~Internal), the ctor values patched in after the zero-fill.
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/WorkComponent.h"

// 0x14012e820. These seeds are load-bearing for the slot-7 hook: workId 0 is a
// VALID work id, so an unseeded component would ask the goals tree to cancel
// work 0 — someone else's task — instead of no-oping. -1 is retail's "I have
// posted no work" sentinel, and 0x80000000 is the same sentinel for workFlags
// (PostWork 0x14012ec60 tests `*(int*)(self+0x2c) == -0x80000000`).
WorkComponent::WorkComponent()
    : workId(-1), workFlags(0x80000000u), workState(0), _pad34{} {
    OrFlagsInCtor(0x3);
}

// ============================================================
// WorkComponent::Persist (vtable slot [3], 0x14012e710)
// ============================================================
// Versioned serialization:
//   - Always: serialize workId (+0x28)
//   - Version < 3: serialize a legacy bool (discarded)
//   - Version >= 1: serialize workState (+0x30)
//   - Version >= 2: serialize workFlags (+0x2c)

void WorkComponent::Persist(void* stream) {
    EnsureHelpers();

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge

    // Always serialize workId
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(workId, nullptr);   // B.3.4 bridge

    // Legacy: version < 3 had a bool field (no longer used).
    // ⚠ It is a BOOL, so it must go through the bool overload
    // (?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z), which moves ONE byte.
    // Casting a bool* to int* and using the int overload reads FOUR, and the
    // extra 3 bytes desync every object serialized after this component.
    // Measured on the packaged tutorial save (which is version < 3, so this
    // branch is live there): our reader ended 3 bytes PAST the start of the
    // next record. Our own saves hid it because the save leg wrote the same 4
    // bytes it read back -- only a retail-authored save can catch this.
    int version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version < 3) {
        bool legacyBool = false;
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(legacyBool, nullptr);   // B.3.4 bridge
    }

    // Version >= 1: serialize workState
    version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version >= 1) {
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(workState, nullptr);   // B.3.4 bridge
    }

    // Version >= 2: serialize workFlags
    version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version >= 2) {
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&workFlags), nullptr);   // B.3.4 bridge
    }

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}

// [6] 0x14012e6f0. PORT GAP (retail 0x14012e6f0) — B38 D9: the body is ported
// above (WorkComponent_Draw, capacity -> +0x30) but the tree's table never
// installed it; the inert stub is kept here. Installing it is its own titled
// commit.
void WorkComponent::OnAdded() {}

// ============================================================
// WorkComponent slot [7] = +0x38, 0x14012e6c0 — CANCEL MY WORK ON REMOVAL
// ============================================================
// Ghidra names this "OnCreate"; it is the REMOVE hook (the documented
// OnCreate/OnRemove off-by-one). ComponentEntity::MarkForRemoval
// (0x1400c2ea0) broadcasts vtable +0x38 to every component whose +0x20
// flags have bit 1 — retail's ctor sets +0x20 |= 3, so WorkComponent is in
// that broadcast — and it fires BEFORE the 0x400 removal bit, i.e. while the
// entity is still fully alive.
//
// Retail body: `if (ead0(self)) eb50(self)` — "if the work I posted is still
// in the goals tree, cancel it", and eb50 is what calls Goals::RemoveWork
// (0x1400f8e30). This is the ONLY route by which a raw-material pile's Pickup
// leaves the goals trees: Pickup::OnComplete (0x1400f2ec0) returns false in
// retail too, so CompleteTask never removes it. A pile is created by
// RawMaterialComponent (0x1401291d0, which posts the Pickup through this
// component) and dies on an expiry timer in RawMaterialComponent::Update.
//
// UNWIRED until 2026-08-29: the body was ported but nothing installed it in
// slot 7, so EVERY Pickup ever posted stayed in the goals trees with a freed
// target entity. The next save walked one and faulted in Work::Serialize
// (0x1400f7400) dereferencing the dead target — measured as work ids 3..22,
// one per haul, zero exceptions. See [[reference_ported_registered_but_unwired]].

void WorkComponent::OnRemove() {
    EnsureHelpers();

    if (s_fn_CheckWorkSlot && s_fn_RegisterWorkSlot) {
        bool available = s_fn_CheckWorkSlot(this);
        if (available) {
            s_fn_RegisterWorkSlot(this);
        }
    }
}

// [13] 0x14012e6b0 / [14] 0x14012e6a0 — the class's own trivial bodies.
uint32_t WorkComponent::CommandMask(int) { return 0; }
void WorkComponent::Command(int) {}

// create 0x140039e70: MemAlloc(0x38, 8), then the ctor.
void* WorkComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WorkComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WorkComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* WorkComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WorkComponent());
}

DEFINE_EXE_TYPE(WorkComponent, "Component")

// moved from engine_funcs.h (ENDGAME B.3.9 TR): reads X::TypeId() of the cell's class.
int fn_StorageCountQuery(void* component) {
    uintptr_t c = (uintptr_t)component;
    void* base = *(void**)(c + 0x18);
    void* jobType = fn_IsDerivedFrom(base, WorkDescription::TypeId()) ? base : nullptr;
    if (!jobType) return 0;
    uintptr_t jt = (uintptr_t)jobType;
    if (*(uint8_t*)(jt + 0x34) == 0) {
        return *(int*)(jt + 0x24);
    }
    uint64_t buildBP = 0;
    fn_FindComponent_Build(*(void**)(c + 0x10), &buildBP);
    uintptr_t build = (uintptr_t)buildBP;
    int defaultWorkers = *(int*)(jt + 0x24);   // _defaultWorkers (NOT _maxWorkerCount)
    int maxArea        = *(int*)(jt + 0x30);   // _maxArea (NOT the building's tile area)
    int width      = (int)*(short*)(build + 0x2c);
    int height     = (int)*(short*)(build + 0x2e);
    int result = (int)(((float)(width * height) / (float)maxArea) * (float)defaultWorkers + 1.0f);
    if (result > defaultWorkers) result = defaultWorkers;
    if (result < 1)              result = 1;
    return result;
}
