// code/Game/Component/boatcomponent.cpp
// Clean reimplementation of BoatComponent lifecycle methods.
//
// BoatComponent (0x98 bytes) manages fishing boat state:
// entity refs, position vectors, route waypoints, dock ID.
// Secondary vtable sub-object at +0x78 (ControllerComponent pattern).
//
// Vtable at 0x140158f40. Non-trivial override:
//   Shutdown (0x1400465d0) -- 95-instruction complex serialization
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/boatcomponent_funcs.h"
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

typedef void (*fn_Serialize_Float_t)(void* serializer, float* value, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// Object::InternalBasePtr::Serialize -- dispatch slot at 0x140152aa0
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// Math::Vector3::Serialize -- dispatch slot at 0x140152a68
typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

// IsDerivedFrom dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// FUN_14000d030 -- resize Vector3 array
typedef void (*fn_ResizeVec3Array_t)(void* arrayField, int newCount);
static fn_ResizeVec3Array_t s_fn_ResizeVec3Array = nullptr;

// Type IDs
static uint32_t* s_TypeId_ComponentEntity = nullptr; // TypeInfo_ComponentEntity
static uint32_t* s_TypeId_1401c7a50 = nullptr;       // DWORD_1401c7a50
static uint32_t* s_TypeId_1401c7078 = nullptr;       // DAT_1401c7078

static bool s_initialized = false;

// ============================================================
// BoatComponent::Shutdown (vtable slot [3], 0x1400465d0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Empty serializer block (version 0) -- format compat
//   3. Serializer(version=0):
//      - InternalBasePtr at +0x28 (entity ref, validated vs ComponentEntity)
//      - Vector3 at +0x40 (position)
//      - Float at +0x4c
//      - InternalBasePtr at +0x30 (validated vs type 0x1401c7a50)
//      - InternalBasePtr at +0x38 (validated vs type 0x1401c7078)
//      - Vector3 at +0x50 (direction/target)
//      - Int at +0x70 (dock ID)
//      - Vector3 array at +0x60 (count at +0x68) -- route waypoints
void BoatComponent_Shutdown(BoatComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty serializer block (format compat)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Main serializer block
    uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize InternalBasePtr at +0x28 (entity ref)
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x28), serializer);

    // Validate +0x28 against ComponentEntity type
    if (*(void**)(selfAddr + 0x28) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_ComponentEntity) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x28), *s_TypeId_ComponentEntity);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x28) = 0;
        }
    }

    // Serialize Vector3 at +0x40 (position)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x40), serializer);

    // Serialize float at +0x4c
    if (s_fn_Serialize_Float)
        s_fn_Serialize_Float(serializer, (float*)(selfAddr + 0x4c), nullptr);

    // Serialize InternalBasePtr at +0x30
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x30), serializer);

    // Validate +0x30 against type 0x1401c7a50
    if (*(void**)(selfAddr + 0x30) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_1401c7a50) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x30), *s_TypeId_1401c7a50);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x30) = 0;
        }
    }

    // Serialize InternalBasePtr at +0x38
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x38), serializer);

    // Validate +0x38 against type 0x1401c7078
    if (*(void**)(selfAddr + 0x38) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_1401c7078) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x38), *s_TypeId_1401c7078);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x38) = 0;
        }
    }

    // Serialize Vector3 at +0x50 (direction/target)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x50), serializer);

    // Serialize int at +0x70 (dock ID)
    if (s_fn_Serialize_Int)
        s_fn_Serialize_Int(serializer, (int*)(selfAddr + 0x70), nullptr);

    // Serialize Vector3 array at +0x60 (count at +0x68)
    uint32_t localCount = *(uint32_t*)(selfAddr + 0x68);
    if (s_fn_Serialize_Int)
        s_fn_Serialize_Int(serializer, (int*)&localCount, nullptr);

    // If loading, resize array
    uintptr_t serializerObj = *(uintptr_t*)serializer;
    if (*(int*)(serializerObj + 8) == 0) {
        if (s_fn_ResizeVec3Array)
            s_fn_ResizeVec3Array((void*)(selfAddr + 0x60), (int)localCount);
    }

    // Serialize each Vector3 in the array (12 bytes each)
    void* arrayBase = *(void**)(selfAddr + 0x60);
    if ((int)localCount > 0 && s_fn_Vector3_Serialize && arrayBase) {
        uint8_t* ptr = (uint8_t*)arrayBase;
        for (uint32_t i = 0; i < localCount; i++) {
            s_fn_Vector3_Serialize(ptr, serializer);
            ptr += 12; // sizeof(Vector3) = 12 bytes
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Reimpl of FUN_14000d030 — resize Vector3 array (stride 0xC, align 4)
// ============================================================
// Array struct: { void* data(+0), int count(+8), int capacity(+0xc) }.
// Grows storage when newCount exceeds capacity (capacity = newCount, or
// newCount*3 on the exactly-one-more growth case), copies min(old,new)
// 12-byte (3-float) elements, zero-inits the rest, frees the old block.
// NON-STATIC so the standalone registrar can take its address.
void BoatComponent_ResizeVec3Array(void* arrayField, int newCount) {
    struct Vec3Array { uint8_t* data; int count; int capacity; };
    Vec3Array* arr = reinterpret_cast<Vec3Array*>(arrayField);

    int oldCount = arr->count;
    if (newCount == oldCount) return;

    uint8_t* oldData = arr->data;
    arr->count = newCount;

    if (arr->capacity < newCount) {
        arr->capacity = newCount;
        // exactly-one-more growth heuristic from the original
        if (newCount == oldCount + 1 && oldCount != 0)
            arr->capacity = newCount * 3;
        arr->data = reinterpret_cast<uint8_t*>(
            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)arr->capacity * 0xC, 4));
    }

    // Number of elements carried over from the old buffer
    int keep = (oldData == arr->data) ? oldCount : 0;

    for (int i = keep; i < newCount; ++i) {
        uint8_t* dst = arr->data + (size_t)i * 0xC;
        if (i < oldCount) {
            // copy 12 bytes (3 floats) from the old buffer
            const uint8_t* src = oldData + (size_t)i * 0xC;
            *reinterpret_cast<uint32_t*>(dst + 0) = *reinterpret_cast<const uint32_t*>(src + 0);
            *reinterpret_cast<uint32_t*>(dst + 4) = *reinterpret_cast<const uint32_t*>(src + 4);
            *reinterpret_cast<uint32_t*>(dst + 8) = *reinterpret_cast<const uint32_t*>(src + 8);
        } else {
            // zero-init new elements
            *reinterpret_cast<uint64_t*>(dst + 0) = 0;
            *reinterpret_cast<uint32_t*>(dst + 8) = 0;
        }
    }

    if (oldData != arr->data && oldData != nullptr) {
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void BoatComponent_InitFuncs() {
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

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serialize_Float = *reinterpret_cast<fn_Serialize_Float_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152aa0);
    if (slot) s_fn_IBP_Serialize = *reinterpret_cast<fn_IBP_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a68);
    if (slot) s_fn_Vector3_Serialize = *reinterpret_cast<fn_Vector3_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_fn_ResizeVec3Array = reinterpret_cast<fn_ResizeVec3Array_t>(
        ResolveAddr(Module::Exe, 0x14000d030));

    // Type IDs
    s_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cf0));  // TypeInfo_ComponentEntity
    s_TypeId_1401c7a50 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a50));
    s_TypeId_1401c7078 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7078));

    s_initialized = true;

#ifdef _DEBUG
    printf("[BoatComponent] Funcs resolved: CompSerialize=%p IBP_Serialize=%p V3=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_Vector3_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(BoatComponent, 0x98, 0x80)
REGISTER_TYPE(BoatComponent, "ControllerComponent",
              &BoatComponent_Factory, &BoatComponent_CreateInPlace);
