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

// FUN_140042e10(self) -- building ambient audio setup (clean reimpl below).
extern "C" void AmbientEmitterAudioSetup_Reimpl(void* self);

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
// Reimpl of FUN_140001770 (grow/shrink the 0x10-byte-element emitter array)
// ============================================================
// Resizes the array {ptr@arr+0, count@arr+8, cap@arr+0xc} (stride 0x10). On grow
// past capacity: cap = newCount (or newCount*3 when growing by exactly one from a
// non-empty array), MemAlloc(cap*0x10, 8), copy existing elements (2 qwords each),
// zero-init the new tail with element layout [u32 0][f32 1.0][u64 0]. The exe body
// is hand-unrolled x4; this is the equivalent scalar 1:1 port.
extern "C" void AmbientEmitterResize_Reimpl(void* arrayBase, int newCount) {
    uint8_t* arr = reinterpret_cast<uint8_t*>(arrayBase);
    int oldCount = *(int*)(arr + 8);
    if (newCount == oldCount) return;

    void* oldPtr = *(void**)(arr + 0);
    *(int*)(arr + 8) = newCount;
    if (*(int*)(arr + 0xc) < newCount) {
        *(int*)(arr + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)(arr + 0xc) = newCount * 3;
        }
        if (fn_MemAlloc && g_pMemAllocSingleton) {
            void* newPtr = fn_MemAlloc(g_pMemAllocSingleton,
                                       (uint64_t)(*(int*)(arr + 0xc)) << 4, 8);
            *(void**)(arr + 0) = newPtr;
        }
    }

    int start = 0;
    if (oldPtr == *(void**)(arr + 0)) start = oldCount;  // grown in place

    uint8_t* base = *(uint8_t**)(arr + 0);
    for (int i = start; i < newCount; ++i) {
        uint8_t* dst = base + (intptr_t)i * 0x10;
        if (!dst) continue;
        if (i < oldCount) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldPtr) + (intptr_t)i * 0x10;
            *(uint64_t*)(dst + 0) = *(const uint64_t*)(src + 0);
            *(uint64_t*)(dst + 8) = *(const uint64_t*)(src + 8);
        } else {
            *(uint32_t*)(dst + 0) = 0;
            *(uint32_t*)(dst + 4) = 0x3f800000;   // 1.0f
            *(uint64_t*)(dst + 8) = 0;
        }
    }

    if (oldPtr != *(void**)(arr + 0) && oldPtr && fn_MemFreeStatic) {
        fn_MemFreeStatic(oldPtr);
    }
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
// Building ambient audio setup — clean reimpl of FUN_140042e10.
// ============================================================
// For each emitter entry (count at self+0x40, 0x10-byte entries in the array at
// self+0x38), ensures a 3D looping SoundEffect is playing at the building's
// position (self+0x28) via the world Audio::Scene (gameWorld+0x78):
//   entry {+0x00 uint handle, +0x04 float volume, +0x08 ExternalPtr soundRef}
// The soundRef is lazily copied from the building's AmbientEmitterDescription
// (self+0x18, type DAT_1401c3e98) sound array at desc+0x28 (stride 8). FindEffect3D
// reuses an existing channel; PlayEffect3D (SoundType 3 = ambient) starts a new
// one; AddEmitter registers this building as a spatial source of that effect.
extern "C" void AmbientEmitterAudioSetup_Reimpl(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    int count = *(int*)(s + 0x40);
    if (count <= 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    if (!fn_Audio_Scene_FindEffect3D || !fn_Audio_Scene_PlayEffect3D ||
        !fn_Audio_Scene_AddEmitter) return;

    void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x78);
    if (!scene) return;

    void* building = GetBuildingData(self);   // AmbientEmitterDescription (may be null)
    void* soundArray = building ? *(void**)((uintptr_t)building + 0x28) : nullptr;
    uint8_t* arrBase = *(uint8_t**)(s + 0x38);
    if (!arrBase) return;

    const void* pos = reinterpret_cast<const void*>(s + 0x28);   // Vector3 position

    for (int i = 0; i < count; ++i) {
        uint8_t* entry = arrBase + (intptr_t)i * 0x10;
        uint64_t* soundRef = reinterpret_cast<uint64_t*>(entry + 8);
        // Lazily bind the sound resource from the building description.
        if (*soundRef == 0 && soundArray)
            *soundRef = reinterpret_cast<const uint64_t*>(soundArray)[i];

        uint32_t handle = fn_Audio_Scene_FindEffect3D(scene, *soundRef);
        *(uint32_t*)(entry + 0) = handle;
        if (handle == 0) {
            int64_t timeZero = 0;                 // System::Time(0)
            float   posZero[3] = { 0.0f, 0.0f, 0.0f };
            handle = fn_Audio_Scene_PlayEffect3D(scene, *soundRef, 3, &timeZero, posZero);
            *(uint32_t*)(entry + 0) = handle;
        }
        fn_Audio_Scene_AddEmitter(scene, handle, pos, *(float*)(entry + 4));
    }
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

    // If building has audio flag at +0x20, set up the 3D ambient emitters.
    if (*(char*)((uintptr_t)building + 0x20) != '\0') {
        AmbientEmitterAudioSetup_Reimpl(self);
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

    // Math::Vector3::Serialize -- Runtime export.
    // (Was wrongly pointed at exe 0x14014efd0 = FloorComponent_RegisterType, an
    //  unrelated type-registration thunk — a dropped/garbled-address caller bug.)
    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    // InternalBasePtr::Serialize
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400019a0));

    // Resize emitter array
    s_fn_ResizeEmitterArray = reinterpret_cast<fn_ResizeEmitterArray_t>(
        ResolveAddr(Module::Exe, 0x140001770));

    // (Building ambient audio setup is now the clean AmbientEmitterAudioSetup_Reimpl.)

    // IO::Serializer::GetVersion -- read from the registered dispatch slot
    // 0x140152a48 (same slot decalcomponent.cpp uses). Was wrongly pointed at
    // exe 0x14014ef90 = FishingDescription_Register — a garbled-address caller bug.
    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    // Type ID
    s_TypeId_AmbientEmitterDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c3e98));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AmbientEmitterComponent] Funcs resolved: CompSerialize=%p V3Serialize=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Vector3_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(AmbientEmitterComponent, 0x48, 0xC3)
REGISTER_TYPE(AmbientEmitterComponent, "Component",
              &AmbientEmitterComponent_Factory, &AmbientEmitterComponent_CreateInPlace);
#endif // STANDALONE_EXE
