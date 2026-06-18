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
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/workcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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

    // Legacy: version < 3 had a bool field (no longer used)
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version < 3) {
            bool legacyBool = false;
            s_fn_Serializer_Serialize(serializerBuf, (int*)&legacyBool, nullptr);
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
// WorkComponent::OnCreate (vtable slot [7], 0x14012e6c0)
// ============================================================
// Check if work slot is available (via BST lookup using workId).
// If available, register it with the work slot manager.

void WorkComponent_OnCreate(WorkComponent* self) {
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
// Tree at *(g_pGameWorldSingleton+0x118) + 0x30. Node ints: key @+0, value-ptr @+0x08,
// greater-child @+0x18, lesser-child @+0x20.
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
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, nullptr);
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
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(valueIbp, nullptr);
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
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(WorkComponent, 0x38, 0x83)
REGISTER_TYPE(WorkComponent, "Component",
              &WorkComponent_Factory, &WorkComponent_CreateInPlace);
#endif // STANDALONE_EXE
