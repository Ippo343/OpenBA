// code/Game/Component/particlecomponent.cpp
// Clean reimplementation of ParticleComponent lifecycle methods from Application-x64.exe.
//
// ParticleComponent (0x48 bytes) manages particle effects attached to entities.
// Fields: +0x28=emitter flag array, +0x30=count, +0x38=geometry array, +0x40=total count
//
// Vtable at 0x14015a290. Six non-trivial overrides — all deeply coupled to
// Graphics::Scene, MeshInstance, and particle emitter subsystems.
//
// Due to deep Graphics subsystem dependencies (MeshInstance::GetNodeToWorld,
// Scene::RemoveObject, Geometry::SetTransform, etc.), several methods delegate
// to original exe functions for the complex Graphics work while the serialization
// and control flow logic is reimplemented.
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/particlecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

// Serialize(serializer, count, boolArray, name) — overload for bool arrays
typedef void (*fn_SerializeBoolArray_t)(void* self, int count, bool* array, const char* name);
static fn_SerializeBoolArray_t s_fn_SerializeBoolArray = nullptr;

// FUN_14000a590 — resize bool array
typedef void (*fn_ResizeBoolArray_t)(void* arrayPtr, int newCount);
static fn_ResizeBoolArray_t s_fn_ResizeBoolArray = nullptr;

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// ParticleComponent_sub_2480 — particle setup helper
typedef void (*fn_ParticleSetup_t)(void* self);
static fn_ParticleSetup_t s_fn_ParticleSetup = nullptr;

// ParticleComponent_helper_2a50 — update emitter position
typedef void (*fn_UpdateEmitterPos_t)(void* self, int index);
static fn_UpdateEmitterPos_t s_fn_UpdateEmitterPos = nullptr;

// ParticleComponent_helper_2990 — emitter tick
typedef void (*fn_EmitterTick_t)(void* self, int index);
static fn_EmitterTick_t s_fn_EmitterTick = nullptr;

// Graphics::Scene::RemoveObject
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
static fn_SceneRemoveObject_t s_fn_SceneRemoveObject = nullptr;

// FUN_1400c3470 — entity flag update
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;

// System::Time::AsSeconds
typedef float (*fn_TimeAsSeconds_t)(void* time);
static fn_TimeAsSeconds_t s_fn_TimeAsSeconds = nullptr;

// FUN_140053660 — emitter startup
typedef void (*fn_EmitterStartup_t)(void* self, int index);
static fn_EmitterStartup_t s_fn_EmitterStartup = nullptr;

// The original OnRemove is too complex for clean reimpl — use direct call
typedef void (*fn_OnRemove_Orig_t)(void* self, void* time);
static fn_OnRemove_Orig_t s_fn_OnRemove_Orig = nullptr;

static uint32_t* s_pParticleDescTypeId = nullptr;  // DAT_1401c4758
static float* s_pUVScale = nullptr;  // g_fUVScale (constant 1.0)

static bool s_initialized = false;

// Helper: get particle description from _unk18
static void* GetParticleDesc(void* self) {
    void* baseObj = *(void**)((uintptr_t)self + 0x18);
    if (s_fn_IsDerivedFrom_d && s_pParticleDescTypeId) {
        if (s_fn_IsDerivedFrom_d(baseObj, *s_pParticleDescTypeId))
            return baseObj;
    }
    return nullptr;
}

// ============================================================
// ParticleComponent::Shutdown (0x140053270)
// ============================================================
void ParticleComponent_Shutdown(ParticleComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    int localCount = *(int*)(selfAddr + 0x30);
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, &localCount, nullptr);

    // If loading, resize bool array
    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeBoolArray) {
        s_fn_ResizeBoolArray((void*)(selfAddr + 0x28), localCount);
    }

    // Serialize bool array
    if (s_fn_SerializeBoolArray)
        s_fn_SerializeBoolArray(serializer, localCount, *(bool**)(selfAddr + 0x28), nullptr);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ParticleComponent::Serialize (0x140053230) — actually setup/init
// ============================================================
void ParticleComponent_Serialize(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_ParticleSetup)
        s_fn_ParticleSetup((void*)selfAddr);

    // Zero all emitter flags
    int count = *(int*)(selfAddr + 0x30);
    if (count > 0) {
        uint8_t* flags = *(uint8_t**)(selfAddr + 0x28);
        for (int i = 0; i < count; i++) {
            flags[i] = 0;
        }
    }
}

// ============================================================
// ParticleComponent::Draw (0x140052f70)
// ============================================================
void ParticleComponent_Draw(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* desc = GetParticleDesc(self);
    if (!desc) return;

    float uvScale = s_pUVScale ? *s_pUVScale : 1.0f;

    int emitterCount = *(int*)((uintptr_t)desc + 0x30);
    if (emitterCount <= 0) return;

    uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);
    uintptr_t geoArray = *(uintptr_t*)(selfAddr + 0x38);

    for (int i = 0; i < emitterCount; i++) {
        uintptr_t descEntry = descArray + (uintptr_t)i * 0x28;

        // Check if emitter is active (byte at descEntry + 0x11)
        if (*(uint8_t*)(descEntry + 0x11) == 0) continue;

        // Re-check IsDerivedFrom for desc (original does this per iteration)
        void* descCheck = GetParticleDesc(self);
        if (!descCheck) continue;

        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Get timing from desc: entry array slot
        uintptr_t descArr2 = *(uintptr_t*)((uintptr_t)descCheck + 0x28);
        uintptr_t timeAddr = descArr2 + ((uintptr_t)i * 5 + 4) * 8;
        float timeSecs = 0.0f;
        if (s_fn_TimeAsSeconds)
            timeSecs = s_fn_TimeAsSeconds((void*)timeAddr);

        // Set emitter timing: uvScale / timeSecs
        *(float*)(geoEntry + 0x10) = uvScale / timeSecs;

        // Check/set geometry opacity
        uintptr_t geoObj = *(uintptr_t*)geoEntry;
        if (*(float*)(geoObj + 0x7C) != 0.0f) {
            *(uint32_t*)(geoObj + 0x2C) |= 4;
            *(uint64_t*)(geoObj + 0x7C) = 0;
        }

        if (s_fn_UpdateEmitterPos)
            s_fn_UpdateEmitterPos((void*)selfAddr, i);
    }
}

// ============================================================
// ParticleComponent::OnCreate (0x140052e80)
// ============================================================
void ParticleComponent_OnCreate(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    int totalCount = *(int*)(selfAddr + 0x40);
    if (totalCount <= 0) return;

    uintptr_t geoArray = *(uintptr_t*)(selfAddr + 0x38);
    uint8_t* flagArray = *(uint8_t**)(selfAddr + 0x28);

    for (int i = 0; i < totalCount; i++) {
        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Remove scene object if valid
        uint32_t objId = *(uint32_t*)(geoEntry + 8);
        if ((int)objId >= 0) {
            if (s_fn_SceneRemoveObject && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
                s_fn_SceneRemoveObject(scene, objId);
            }
            *(uint32_t*)(geoEntry + 8) = 0xFFFFFFFF;
        }

        // Clear emitter flag
        flagArray[i] = 0;

        // Check if any emitter still has a valid scene object
        bool anyValid = false;
        if (totalCount > 0) {
            for (int j = 0; j < totalCount; j++) {
                if ((int)*(uint32_t*)(*(uintptr_t*)(selfAddr + 0x38) + (uintptr_t)j * 0x18 + 8) >= 0) {
                    anyValid = true;
                    break;
                }
            }
        }

        if (!anyValid) {
            // Clear bit 2 of component flags
            uint32_t oldFlags = *(uint32_t*)(selfAddr + 0x20);
            uint32_t newFlags = oldFlags & ~0x4u;
            *(uint32_t*)(selfAddr + 0x20) = newFlags;

            if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(selfAddr + 0x10) != 0) {
                if (s_fn_CheckPtr)
                    s_fn_CheckPtr((void*)(selfAddr + 0x10));
                if (s_fn_EntityFlagUpdate)
                    s_fn_EntityFlagUpdate(*(void**)(selfAddr + 0x10));
            }
        }
    }
}

// ============================================================
// ParticleComponent::OnRemove (0x140052b30)
// ============================================================
// This is the most complex method (200 instructions) — positions particles
// relative to model using MeshInstance::GetNodeToWorld, Transform operations.
// Too many Graphics dependencies to cleanly reimplement. We call the original.

void ParticleComponent_OnRemove(ParticleComponent* self, void* time) {
    if (s_fn_OnRemove_Orig)
        s_fn_OnRemove_Orig(self, time);
}

// ============================================================
// ParticleComponent::OnUpdate2 (0x1400531b0)
// ============================================================
void ParticleComponent_OnUpdate2(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* desc = GetParticleDesc(self);

    if (s_fn_ParticleSetup)
        s_fn_ParticleSetup((void*)selfAddr);

    if (!desc) return;

    int emitterCount = *(int*)((uintptr_t)desc + 0x30);
    if (emitterCount <= 0) return;

    uint8_t* flagArray = *(uint8_t**)(selfAddr + 0x28);

    for (int i = 0; i < emitterCount; i++) {
        if (flagArray[i] != 0) {
            if (s_fn_EmitterStartup)
                s_fn_EmitterStartup((void*)selfAddr, i);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ParticleComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ParticleSetup = reinterpret_cast<fn_ParticleSetup_t>(
        ResolveAddr(Module::Exe, 0x140052480));
    s_fn_UpdateEmitterPos = reinterpret_cast<fn_UpdateEmitterPos_t>(
        ResolveAddr(Module::Exe, 0x140052a50));
    s_fn_EmitterTick = reinterpret_cast<fn_EmitterTick_t>(
        ResolveAddr(Module::Exe, 0x140052990));
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_ResizeBoolArray = reinterpret_cast<fn_ResizeBoolArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_EmitterStartup = reinterpret_cast<fn_EmitterStartup_t>(
        ResolveAddr(Module::Exe, 0x140053660));

    // OnRemove original — for vtable-hooked slot, get from vtable
    // The original function pointer will be stored in the hook trampoline
    // For standalone exe, this will be the actual function
    s_fn_OnRemove_Orig = reinterpret_cast<fn_OnRemove_Orig_t>(
        ResolveAddr(Module::Exe, 0x140052b30));

    s_pParticleDescTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4758));
    s_pUVScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));  // g_fUVScale

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_SceneRemoveObject = reinterpret_cast<fn_SceneRemoveObject_t>(
        ResolveExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z"));
    s_fn_TimeAsSeconds = reinterpret_cast<fn_TimeAsSeconds_t>(
        ResolveExport("?AsSeconds@Time@System@@QEBAMXZ"));
    s_fn_SerializeBoolArray = reinterpret_cast<fn_SerializeBoolArray_t>(
        ResolveAddr(Module::Exe, 0x140152a58));
    // Note: the bool array serialize is different from single bool serialize
    // The original uses IO::Serializer::Serialize(serializer, count, boolArray, name)

    s_initialized = true;
}
