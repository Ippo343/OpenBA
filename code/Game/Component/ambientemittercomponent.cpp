// code/Game/Component/ambientemittercomponent.cpp
// Clean reimplementation of AmbientEmitterComponent lifecycle methods.
//
// AmbientEmitterComponent (0x48 bytes) manages ambient sound emitters for entities.
// Fields: +0x28=emitter data ptr, +0x30=count, +0x38=emitter array ptr, +0x40=count2
//
// Vtable at 0x140158a60. Non-trivial overrides:
//   Shutdown (0x140042f30) -- Complex serialization with Vector3, emitter entries
//   Draw     (0x140042ba0) -- Resizes emitter list, calls audio setup
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/ambientemittercomponent_funcs.h"
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

// IO::Serializer::GetVersion(this) -- dispatch slot
typedef int (*fn_Serializer_GetVersion_t)(void* serializer);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// Math::Vector3::Serialize(this, serializer)
typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

// FUN_1400019a0(InternalBasePtr*, serializer) -- serialize InternalBasePtr
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IO::Serializer::Serialize(float*, name)
typedef void (*fn_Serialize_Float_t)(void* serializer, float* value, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// FUN_140001770(array_ptr_field, new_count) -- resize emitter array
typedef void (*fn_ResizeEmitterArray_t)(void* arrayField, int newCount);
static fn_ResizeEmitterArray_t s_fn_ResizeEmitterArray = nullptr;

// FUN_140042e10(self) -- audio setup
typedef void (*fn_AudioSetup_t)(void* self);
static fn_AudioSetup_t s_fn_AudioSetup = nullptr;

// IsDerivedFrom dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Type ID for AmbientEmitterDescription at DAT_1401c3e98
static uint32_t* s_TypeId_AmbientEmitterDesc = nullptr;

static bool s_initialized = false;

// ============================================================
// Helper: Get building description component
// ============================================================
static void* GetBuildingData(void* self) {
    void* obj = *(void**)((uintptr_t)self + 0x18);
    if (!obj || !s_fn_IsDerivedFrom_dispatch || !s_TypeId_AmbientEmitterDesc)
        return nullptr;
    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_AmbientEmitterDesc);
    return derived ? obj : nullptr;
}

// ============================================================
// AmbientEmitterComponent::Shutdown (vtable slot [3], 0x140042f30)
// ============================================================
// Serializes:
//   1. Base Component_Serialize
//   2. Serializer(version=1):
//      - Vector3 at +0x28 (position)
//      - count from +0x40, serialize it
//      - If loading: resize array at +0x38
//      - Loop: serialize (InternalBasePtr + uint + float) per entry (16 bytes each)
//      - If version==0: resize from building data +0x30
//   3. ~Serializer
void AmbientEmitterComponent_Shutdown(AmbientEmitterComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer, stream, 1);

    uintptr_t selfAddr = (uintptr_t)self;

    // Serialize Vector3 at +0x28
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x28), serializer);

    // Read count from +0x40 into local, serialize it
    uint32_t localCount = *(uint32_t*)(selfAddr + 0x40);
    if (s_fn_Serialize_Int)
        s_fn_Serialize_Int(serializer, (int*)&localCount, nullptr);

    // If loading (stream mode == 0), resize emitter array
    uintptr_t serializerObj = *(uintptr_t*)serializer;
    if (*(int*)(serializerObj + 8) == 0) {
        if (s_fn_ResizeEmitterArray)
            s_fn_ResizeEmitterArray((void*)(selfAddr + 0x38), (int)localCount);
    }

    // Serialize each emitter entry: InternalBasePtr(8) + uint(4) + float(4) = 16 bytes per entry
    // Array base at +0x38 (ptr). Entries: float* pfVar4 starts at array+4
    if ((int)localCount > 0 && s_fn_IBP_Serialize && s_fn_Serialize_Int && s_fn_Serialize_Float) {
        void* arrayBase = *(void**)(selfAddr + 0x38);
        // Each entry is 16 bytes. Loop pointer starts at +4 (the float field).
        // Layout per entry: [uint(4)][InternalBasePtr(8, at +4)][float(4, at +12)]...
        // Actually from decomp: pfVar4 starts at array+4, pfVar4+1 = IBP, pfVar4-1 = uint, pfVar4 = float
        // pfVar4 = (float*)(array + 4), stride = 4 floats = 16 bytes
        float* pfVar4 = (float*)((uintptr_t)arrayBase + 4);
        for (uint32_t i = 0; i < localCount; i++) {
            // Serialize InternalBasePtr at pfVar4+1 (offset +4 from pfVar4)
            s_fn_IBP_Serialize((void*)(pfVar4 + 1), serializer);
            // Serialize uint at pfVar4-1 (offset -4 from pfVar4)
            s_fn_Serialize_Int(serializer, (int*)(pfVar4 - 1), nullptr);
            // Serialize float at pfVar4
            s_fn_Serialize_Float(serializer, pfVar4, nullptr);
            pfVar4 += 4; // advance 16 bytes
        }
    }

    // Version-gated: if version==0, resize from building data
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version == 0) {
            void* building = GetBuildingData(self);
            if (building && s_fn_ResizeEmitterArray) {
                int buildingCount = *(int*)((uintptr_t)building + 0x30);
                s_fn_ResizeEmitterArray((void*)(selfAddr + 0x38), buildingCount);
            }
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// AmbientEmitterComponent::Draw (vtable slot [6], 0x140042ba0)
// ============================================================
// Gets building data via IsDerivedFrom(+0x18, DAT_1401c3e98).
// Resizes emitter list from building[0x30].
// If building[0x20] != 0, calls audio setup FUN_140042e10(self).
void AmbientEmitterComponent_Draw(AmbientEmitterComponent* self) {
    void* building = GetBuildingData(self);
    if (!building) return;

    uintptr_t selfAddr = (uintptr_t)self;

    // Resize emitter list: FUN_140001770(self+0x38, building[0x30])
    if (s_fn_ResizeEmitterArray) {
        int count = *(int*)((uintptr_t)building + 0x30);
        s_fn_ResizeEmitterArray((void*)(selfAddr + 0x38), count);
    }

    // If building has audio flag at +0x20
    if (*(char*)((uintptr_t)building + 0x20) != '\0') {
        if (s_fn_AudioSetup)
            s_fn_AudioSetup(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AmbientEmitterComponent_InitFuncs() {
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

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Serialize float -- uses same overload pattern as int, at 0x140152470
    slot = ResolveAddr(Module::Exe, 0x140152470);
    if (slot) s_fn_Serialize_Float = *reinterpret_cast<fn_Serialize_Float_t*>(slot);

    // Vector3::Serialize -- direct address
    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14014efd0));

    // InternalBasePtr::Serialize
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400019a0));

    // Resize emitter array
    s_fn_ResizeEmitterArray = reinterpret_cast<fn_ResizeEmitterArray_t>(
        ResolveAddr(Module::Exe, 0x140001770));

    // Audio setup
    s_fn_AudioSetup = reinterpret_cast<fn_AudioSetup_t>(
        ResolveAddr(Module::Exe, 0x140042e10));

    // Serializer::GetVersion
    s_fn_Serializer_GetVersion = reinterpret_cast<fn_Serializer_GetVersion_t>(
        ResolveAddr(Module::Exe, 0x14014ef90));

    // Type ID
    s_TypeId_AmbientEmitterDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c3e98));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AmbientEmitterComponent] Funcs resolved: CompSerialize=%p V3Serialize=%p Audio=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Vector3_Serialize,
           (void*)s_fn_AudioSetup);
#endif
}
