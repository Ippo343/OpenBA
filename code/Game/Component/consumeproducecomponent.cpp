// code/Game/Component/consumeproducecomponent.cpp
// Clean reimplementation of ConsumeProduceComponent lifecycle methods from Application-x64.exe.
//
// ConsumeProduceComponent extends ResourceProduceComponent (0x70 bytes).
// Two non-trivial vtable overrides:
//   Shutdown  (0x140118e00) — parent Shutdown + versioned serialization of consume fields
//   Serialize (0x140118ef0) — init consume data from building template
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/consumeproducecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent_Shutdown (parent) at 0x14012b4a0
typedef void (*fn_ParentShutdown_t)(void* self, void* stream);
static fn_ParentShutdown_t s_fn_ParentShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// IO::Serializer::GetVersion — dispatch slot at 0x140152a48
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// FUN_140023a00 — resize float array (takes address of float* and new count)
typedef void (*fn_ResizeFloatArray_t)(float** arrayPtr, int newSize);
static fn_ResizeFloatArray_t s_fn_ResizeFloatArray = nullptr;

// IO::Serializer::Serialize(float array) — dispatch slot at 0x140151ea8
typedef void (*fn_Serialize_FloatArray_t)(void* self, int count, float* array, const char* name);
static fn_Serialize_FloatArray_t s_fn_Serialize_FloatArray = nullptr;

// fn_InternalBasePtr_CheckPtr — dispatch slot at entity owner
typedef void (*fn_CheckPtr_t)(void* self);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// ConsumeProduceComponent_sub_3a00 — resize consume rate array at +0x58
// Takes (void* arrayFieldPtr, int newCount)
typedef void (*fn_ResizeConsumeArray_t)(void* arrayFieldPtr, int newCount);
static fn_ResizeConsumeArray_t s_fn_ResizeConsumeArray = nullptr;

// DWORD_1401c7388 — ConsumeProduceDescription type ID
static uint32_t* s_TypeId_ConsumeProduceDescription = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ParentShutdown = reinterpret_cast<fn_ParentShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_fn_ResizeFloatArray = reinterpret_cast<fn_ResizeFloatArray_t>(
        ResolveAddr(Module::Exe, 0x140023a00));

    slot = ResolveAddr(Module::Exe, 0x140151ea8);
    if (slot) s_fn_Serialize_FloatArray = *reinterpret_cast<fn_Serialize_FloatArray_t*>(slot);

    // CheckPtr dispatch slot at 0x140152ab0
    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_ResizeConsumeArray = reinterpret_cast<fn_ResizeConsumeArray_t>(
        ResolveAddr(Module::Exe, 0x140023a00));

    // ConsumeProduceDescription type ID at 0x1401c7388
    s_TypeId_ConsumeProduceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7388));

    // IsDerivedFrom via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;
}

// ============================================================
// ConsumeProduceComponent::Shutdown (vtable slot [3], 0x140118e00)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then serializes:
//   - consumeResourceId (+0x50)
//   - consumeCount (+0x60) via local copy, with array resize on load
//   - float array at consumeRates (+0x58)
//   - consumeState (+0x68)
//   - Version-0 migration: if consumeResourceId < 0 on load, reset to 0

void ConsumeProduceComponent_Shutdown(ConsumeProduceComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;

    // Create versioned serializer (version 1)
    uint8_t serBuf[24];
    int64_t* pSerData = (int64_t*)serBuf;
    s_fn_Serializer_Ctor(serBuf, stream, 1);

    // Serialize consumeResourceId at +0x50
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &self->consumeResourceId, nullptr);
    }

    // Copy consume count to local (original uses local_res8, NOT direct field access).
    // On load, the resize function reads +0x60 internally as old count to decide
    // whether reallocation is needed. Writing directly to +0x60 before resize would
    // make old == new, causing resize to skip reallocation.
    int localCount = self->consumeCount;
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &localCount, nullptr);
    }

    // On load (direction == 0): resize array if needed
    if (*(int*)(pSerData[0] + 8) == 0) {
        if (s_fn_ResizeFloatArray) {
            s_fn_ResizeFloatArray(&self->consumeRates, localCount);
        }
    }

    // Serialize float array using local count and rates pointer
    if (s_fn_Serialize_FloatArray) {
        s_fn_Serialize_FloatArray(serBuf, localCount, self->consumeRates, nullptr);
    }

    // Serialize consumeState at +0x68
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &self->consumeState, nullptr);
    }

    // Version-0 migration: on load, if consumeResourceId < 0, reset to 0
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serBuf);
        if (version == 0 && self->consumeResourceId < 0) {
            // Check if we're loading (stream direction == 0)
            if (*(int*)((uintptr_t)stream + 8) == 0) {
                self->consumeResourceId = 0;
            }
        }
    }

    s_fn_Serializer_Dtor(serBuf);
}

// ============================================================
// ConsumeProduceComponent::Serialize (vtable slot [4], 0x140118ef0)
// ============================================================
// Looks up entity's component list via owner (+0x10), iterates components
// to find one matching ConsumeProduceDescription type ID (0x1401c7388),
// then copies consume slot info (count at +0x30, resourceId at +0x4c),
// and initializes the consumeRates float array to 1.0f per slot.
//
// Original decompilation:
//   CheckPtr(self+0x10)
//   entity = *(self+0x10)
//   compList = entity->+0x28 (component list: +0x28=array, +0x30=count)
//   for i in range(compList.count):
//     comp = compList[i]
//     typeId = comp->vtable[+0x10]()  // GetTypeId virtual
//     if typeId == DWORD_1401c7388:
//       desc = compList[i]
//       if IsDerivedFrom(desc, DWORD_1401c7388): desc is valid
//       break
//   ResizeConsumeArray(self+0x58, *(desc+0x30))   // count from description
//   self->consumeResourceId = *(desc+0x4c)        // resource ID from description
//   for i in range(self->consumeCount):
//     consumeRates[i] = 1.0f

void ConsumeProduceComponent_Serialize(ConsumeProduceComponent* self) {
    EnsureHelpers();

    if (!s_fn_CheckPtr || !s_fn_IsDerivedFrom_dispatch || !s_TypeId_ConsumeProduceDescription)
        return;

    // CheckPtr on the owner InternalBasePtr at +0x10
    s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));

    void* entity = self->owner;
    if (!entity) return;

    // Get component list from entity: +0x28 = array ptr, +0x30 = count
    uintptr_t entityAddr = (uintptr_t)entity;
    void** compArray = *(void***)(entityAddr + 0x28);
    int compCount = *(int*)(entityAddr + 0x30);

    // Find ConsumeProduceDescription in component list
    void* desc = nullptr;
    uint32_t targetTypeId = *s_TypeId_ConsumeProduceDescription;

    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;

        // Call GetTypeId virtual at vtable+0x10
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(vtable + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom
            if (s_fn_IsDerivedFrom_dispatch(comp, targetTypeId)) {
                desc = comp;
            }
            break;
        }
    }

    if (!desc) return;

    // Resize consume array from description count at +0x30
    int descCount = *(int*)((uintptr_t)desc + 0x30);
    if (s_fn_ResizeConsumeArray) {
        s_fn_ResizeConsumeArray((void*)((uintptr_t)self + 0x58), descCount);
    }

    // Copy resource ID from description at +0x4c
    self->consumeResourceId = *(int*)((uintptr_t)desc + 0x4c);

    // Initialize all consume rates to 1.0f
    float* rates = self->consumeRates;
    if (rates && self->consumeCount > 0) {
        for (int i = 0; i < self->consumeCount; i++) {
            rates[i] = 1.0f;
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ConsumeProduceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Funcs resolved: ParentShutdown=%p "
           "Ctor=%p Dtor=%p SerInt=%p GetVer=%p ResizeFloat=%p SerFloat=%p "
           "CheckPtr=%p ResizeConsume=%p TypeId=%p IsDerived=%p\n",
           (void*)s_fn_ParentShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_Serialize_Int,
           (void*)s_fn_Serializer_GetVersion, (void*)s_fn_ResizeFloatArray,
           (void*)s_fn_Serialize_FloatArray, (void*)s_fn_CheckPtr,
           (void*)s_fn_ResizeConsumeArray,
           (void*)s_TypeId_ConsumeProduceDescription,
           (void*)s_fn_IsDerivedFrom_dispatch);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(ConsumeProduceComponent, 0x70, 0x80)
REGISTER_TYPE(ConsumeProduceComponent, "ResourceProduceComponent",
              &ConsumeProduceComponent_Factory, &ConsumeProduceComponent_CreateInPlace);
#endif // STANDALONE_EXE
