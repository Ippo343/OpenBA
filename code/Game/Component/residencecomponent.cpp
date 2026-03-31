// code/Game/Component/residencecomponent.cpp
// Clean reimplementation of ResidenceComponent lifecycle methods from Application-x64.exe.
//
// ResidenceComponent is a ControllerComponent subclass (0x60 bytes).
// Manages citizen residence assignments (housing).
//
// Vtable at 0x1401642c0. Three non-trivial overrides:
//   Shutdown  (0x14012a170) — serialization with Time + arrays
//   Serialize (0x14012a120) — IsDerivedFrom check + field init from description
//   OnCreate  (0x14012a0e0) — calls init helper + registers with game state
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/residencecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_t)(void* serializer, int* field, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// ResidenceComponent_Controller_sub_7ad0 — resize resident array at +0x50
// Takes (longlong* arrayField, int newCount)
typedef void (*fn_ResizeResidentArray_t)(void* arrayFieldPtr, int newCount);
static fn_ResizeResidentArray_t s_fn_ResizeResidentArray = nullptr;

// ResidenceComponent_helper_7c50 — serialize resident array
// Takes (Serializer*, count, arrayPtr)
typedef void (*fn_SerializeResidentArray_t)(void* serializer, int count, void* arrayPtr);
static fn_SerializeResidentArray_t s_fn_SerializeResidentArray = nullptr;

// System::Time::Serialize — dispatch slot at 0x140152a08
typedef void (*fn_Time_Serialize_t)(void* timeObj, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// DAT_1401c7d28 — ResidenceDescription type ID
static uint32_t* s_TypeId_ResidenceDescription = nullptr;

// ResidenceComponent_helper_9ef0 — init helper (called before register)
typedef void (*fn_ResidenceInit_t)(void* self);
static fn_ResidenceInit_t s_fn_ResidenceInit = nullptr;

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

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
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    s_fn_ResizeResidentArray = reinterpret_cast<fn_ResizeResidentArray_t>(
        ResolveAddr(Module::Exe, 0x140037ad0));

    s_fn_SerializeResidentArray = reinterpret_cast<fn_SerializeResidentArray_t>(
        ResolveAddr(Module::Exe, 0x140037c50));

    // System::Time::Serialize via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    // IsDerivedFrom via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // ResidenceDescription type ID
    s_TypeId_ResidenceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7d28));

    s_fn_ResidenceInit = reinterpret_cast<fn_ResidenceInit_t>(
        ResolveAddr(Module::Exe, 0x140129ef0));

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

// ============================================================
// ResidenceComponent::Shutdown (vtable slot [3], 0x14012a170)
// ============================================================
// Serializes Component base + empty serializer block + versioned
// serializer with: resident count (+0x58), resident array resize on load,
// resident array serialize, and Time::Serialize at +0x48.
//
// Original decompilation:
//   Component_Serialize(self, stream)
//   Serializer_Ctor(local1, stream, 0); Serializer_Dtor(local1)    // empty block
//   Serializer_Ctor(local2, stream, 0)
//   local_res8 = *(int*)(self+0x58)
//   Serialize_Int(local2, &local_res8, null)
//   if loading: ResizeResidentArray(self+0x50, local_res8)
//   SerializeResidentArray(local2, local_res8, *(self+0x50))
//   Time::Serialize(self+0x48, local2)
//   Serializer_Dtor(local2)

void ResidenceComponent_Shutdown(ResidenceComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (version 0)
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        uint8_t serBuf1[24];
        s_fn_Serializer_Ctor(serBuf1, stream, 0);
        s_fn_Serializer_Dtor(serBuf1);
    }

    // Versioned serializer block (version 0) with resident data
    uint8_t serBuf2[24];
    int64_t* pSerData = (int64_t*)serBuf2;

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;
    s_fn_Serializer_Ctor(serBuf2, stream, 0);

    // Serialize resident count via local copy
    // (same pattern as ConsumeProduceComponent — local copy needed so resize
    // sees old vs new count correctly)
    int localCount = *(int*)((uintptr_t)self + 0x58);
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf2, &localCount, nullptr);
    }

    // On load (direction == 0): resize resident array
    if (*(int*)(pSerData[0] + 8) == 0) {
        if (s_fn_ResizeResidentArray) {
            s_fn_ResizeResidentArray((void*)((uintptr_t)self + 0x50), localCount);
        }
    }

    // Serialize resident array
    if (s_fn_SerializeResidentArray) {
        void* arrayPtr = *(void**)((uintptr_t)self + 0x50);
        s_fn_SerializeResidentArray(serBuf2, localCount, arrayPtr);
    }

    // Serialize Time at +0x48
    if (s_fn_Time_Serialize) {
        s_fn_Time_Serialize((void*)((uintptr_t)self + 0x48), serBuf2);
    }

    s_fn_Serializer_Dtor(serBuf2);
}

// ============================================================
// ResidenceComponent::Serialize (vtable slot [4], 0x14012a120)
// ============================================================
// Checks IsDerivedFrom on +0x18 with ResidenceDescription type,
// reads resident capacity from description at +0x20, resizes
// resident array at +0x50 to that capacity.
//
// Original decompilation:
//   obj = *(self+0x18)
//   derived = IsDerivedFrom(obj, DAT_1401c7d28)
//   pvVar2 = derived ? obj : nullptr
//   ResizeResidentArray(self+0x50, *(pvVar2+0x20))

void ResidenceComponent_Serialize(ResidenceComponent* self) {
    EnsureHelpers();

    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_ResidenceDescription ||
        !s_fn_ResizeResidentArray)
        return;

    void* obj = self->_unk18;
    if (!obj) return;

    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_ResidenceDescription);
    void* desc = derived ? obj : nullptr;
    if (!desc) return;

    // Read resident capacity from description at +0x20
    int capacity = *(int*)((uintptr_t)desc + 0x20);

    // Resize resident array at +0x50
    s_fn_ResizeResidentArray((void*)((uintptr_t)self + 0x50), capacity);
}

// ============================================================
// ResidenceComponent::OnCreate (vtable slot [7], 0x14012a0e0)
// ============================================================
// Calls init helper (ResidenceComponent_helper_9ef0), then registers
// owning entity with game world residence list at worldMgr+0x20.
//
// Original decompilation:
//   ResidenceComponent_helper_9ef0(self)
//   local_res8 = *(self+0x10)    // owner entity
//   FUN_140002cc0(*(g_pGameWorldSingleton+0xf0)+0x20, &local_res8)

void ResidenceComponent_OnCreate(ResidenceComponent* self) {
    EnsureHelpers();

    // Call init helper
    if (s_fn_ResidenceInit) {
        s_fn_ResidenceInit(self);
    }

    if (!s_fn_RegisterEntity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    // Build an InternalBasePtr on the stack with the owner entity
    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    // Get the world manager from game state
    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x20 (residence entity list)
    s_fn_RegisterEntity((void*)(worldMgr + 0x20), &entityPtr);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ResidenceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ResidenceComponent] Funcs resolved: Component_Serialize=%p "
           "ResizeArray=%p SerializeArray=%p TimeSer=%p Init=%p Register=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ResizeResidentArray,
           (void*)s_fn_SerializeResidentArray, (void*)s_fn_Time_Serialize,
           (void*)s_fn_ResidenceInit, (void*)s_fn_RegisterEntity);
#endif
}
