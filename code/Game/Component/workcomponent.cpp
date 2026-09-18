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
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t s_fn_Serializer_Serialize = nullptr;

// IO::Serializer::GetVersion(this) -> int
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

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

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_fn_ComputeWorkCapacity = reinterpret_cast<fn_ComputeWorkCapacity_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_fn_CheckWorkSlot = reinterpret_cast<fn_CheckWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012ead0));

    s_fn_RegisterWorkSlot = reinterpret_cast<fn_RegisterWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012eb50));

    s_initialized = true;
}

// ============================================================
// WorkComponent::Shutdown (vtable slot [3], 0x14012e710)
// ============================================================
// Versioned serialization:
//   - Always: serialize workId (+0x28)
//   - Version < 3: serialize a legacy bool (discarded)
//   - Version >= 1: serialize workState (+0x30)
//   - Version >= 2: serialize workFlags (+0x2c)

void WorkComponent_Shutdown(WorkComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializerBuf[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializerBuf, stream, 3);
    }

    // Always serialize workId
    if (s_fn_Serializer_Serialize) {
        s_fn_Serializer_Serialize(serializerBuf, &self->workId, nullptr);
    }

    // Legacy: version < 3 had a bool field (no longer used).
    // ⚠ It is a BOOL, so it must go through the bool overload
    // (?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z), which moves ONE byte.
    // Casting a bool* to int* and using the int overload reads FOUR, and the
    // extra 3 bytes desync every object serialized after this component.
    // Measured on the packaged tutorial save (which is version < 3, so this
    // branch is live there): our reader ended 3 bytes PAST the start of the
    // next record. Our own saves hid it because the save leg wrote the same 4
    // bytes it read back -- only a retail-authored save can catch this.
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version < 3) {
            bool legacyBool = false;
            if (g_fn_Serialize_Bool) {
                g_fn_Serialize_Bool(serializerBuf, &legacyBool, nullptr);
            } else {
                static bool s_warned = false;
                if (!s_warned) {
                    s_warned = true;
                    fprintf(stderr, "[WorkComponent::Ser] *** Serialize(bool) is NULL "
                                    "-- the legacy version<3 byte is NOT being read "
                                    "and the stream is now DESYNCED.\n");
                    fflush(stderr);
                }
            }
        }
    }

    // Version >= 1: serialize workState
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version >= 1) {
            s_fn_Serializer_Serialize(serializerBuf, &self->workState, nullptr);
        }
    }

    // Version >= 2: serialize workFlags
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version >= 2) {
            s_fn_Serializer_Serialize(serializerBuf, (int*)&self->workFlags, nullptr);
        }
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializerBuf);
    }
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

void WorkComponent_OnRemove_CancelWork(WorkComponent* self) {
    EnsureHelpers();


    if (!self) return;

    if (s_fn_CheckWorkSlot && s_fn_RegisterWorkSlot) {
        bool available = s_fn_CheckWorkSlot(self);
        if (available) {
            s_fn_RegisterWorkSlot(self);
        }
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

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(valueIbp);
    *(uint32_t*)(self + 0x2c) = *(uint32_t*)(valueIbp[0] + 0x24);

    if (s_fn_CitizenAI_8e30)
        s_fn_CitizenAI_8e30(*(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118), key);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    int64_t compIbp[1] = {0};
    if (s_fn_WC_FindComp) s_fn_WC_FindComp(*(uintptr_t*)(self + 0x10), compIbp);
    if (compIbp[0] != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(compIbp);
        if (s_fn_WC_SetMeshFlag) s_fn_WC_SetMeshFlag((void*)compIbp[0], 2, '\0');
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(compIbp);
        if (s_fn_WC_SetMeshFlag) s_fn_WC_SetMeshFlag((void*)compIbp[0], 3, '\0');
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WorkComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[WorkComponent] Funcs resolved: Component_Serialize=%p Serializer_Ctor=%p "
           "ComputeWorkCapacity=%p CheckWorkSlot=%p RegisterWorkSlot=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_ComputeWorkCapacity, (void*)s_fn_CheckWorkSlot,
           (void*)s_fn_RegisterWorkSlot);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (WorkComponent_Factory MemAlloc), flags 0x83 (Ctor 0x14012e820:
// Component base sets +0x20=0x80, leaf OR 0x3 => 0x83).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// slot 3 wired unconditionally, save/load wave 2026-08-28 (fields previously reset silently on load).
static uint32_t WorkComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WorkComponent");
    return cached;
}

static void* WorkComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void WorkComponent_SerVSlot(void* s, void* stream) {
    WorkComponent_Shutdown(reinterpret_cast<WorkComponent*>(s), stream);
}
static void WorkComponent_OnRemoveVSlot(void* s) {
    WorkComponent_OnRemove_CancelWork(reinterpret_cast<WorkComponent*>(s));
}

static void WorkComponent_FinishVtable() {
    // Slot 3 unconditional — serializer wiring is not a feature flag.
    WorkComponent_LiveVtable[3] = reinterpret_cast<void*>(&WorkComponent_SerVSlot);
        WorkComponent_LiveVtable[7] = reinterpret_cast<void*>(&WorkComponent_OnRemoveVSlot);
}

// Retail ctor 0x14012e820 seeds these; our factory previously left them zeroed.
// This is load-bearing for the slot-7 hook: workId 0 is a VALID work id, so an
// unseeded component would ask the goals tree to cancel work 0 — someone else's
// task — instead of no-oping. -1 is retail's "I have posted no work" sentinel,
// and 0x80000000 is the same sentinel for workFlags (PostWork 0x14012ec60 tests
// it explicitly: `*(int*)(self+0x2c) == -0x80000000`).
static void WorkComponent_InitBody(void* p) {
    if (!p) return;
    WorkComponent* wc = reinterpret_cast<WorkComponent*>(p);
    wc->workId    = -1;
    wc->workFlags = 0x80000000u;
    wc->workState = 0;
}

extern "C" void* WorkComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(
        0x38, 0x83, WorkComponent_LiveVtable,
        reinterpret_cast<void*>(&WorkComponent_LiveGetTypeId));
    WorkComponent_FinishVtable();
    WorkComponent_InitBody(p);
    return p;
}
static void* WorkComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(
        mem, 0x38, 0x83, WorkComponent_LiveVtable,
        reinterpret_cast<void*>(&WorkComponent_LiveGetTypeId));
    WorkComponent_FinishVtable();
    WorkComponent_InitBody(mem);
    return mem;
}
REGISTER_TYPE(WorkComponent, "Component",
              &WorkComponent_Factory, &WorkComponent_CreateInPlace);
