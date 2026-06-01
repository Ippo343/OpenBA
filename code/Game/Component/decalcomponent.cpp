// code/Game/Component/decalcomponent.cpp
// Clean reimplementation of DecalComponent lifecycle methods.
//
// DecalComponent (0x38 bytes) manages ground decals for buildings (shadows, footprints).
// Fields: +0x28=decalState (uint32), +0x2C=decalId (int32, -1=none), +0x30=decalScale (float, 1.0)
//
// Vtable at 0x140162fd0. Non-trivial overrides:
//   Shutdown  (0x14011b820) -- Serializes decalScale and decalState
//   Serialize (0x14011b7d0) -- Reads building data, copies scale from data+0x20
//   Draw      (0x14011b560) -- Complex terrain decal rendering (111 instrs)
//   OnCreate  (0x14011b510) -- Removes decal from terrain if decalId >= 0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/decalcomponent_funcs.h"
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

// IO::Serializer::Serialize(float*, name) -- dispatch slot at 0x140152a60
typedef void (*fn_Serialize_Float_t)(void* serializer, float* value, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// IO::Serializer::GetVersion -- dispatch slot at 0x140152a48
typedef int (*fn_Serializer_GetVersion_t)(void* serializer);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// IsDerivedFrom dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Type ID for DecalDescription at DAT_1401c75b8
static uint32_t* s_TypeId_DecalDesc = nullptr;

// Original Draw function (too complex: 111 instrs, terrain decal rendering)
typedef void (*fn_DecalDraw_t)(void* self);
static fn_DecalDraw_t s_fn_OriginalDraw = nullptr;

// Terrain decal remove: FUN_1400b0d00(terrainDecalSys, decalId)
typedef void (*fn_TerrainDecalRemove_t)(void* terrainDecalSys, int decalId);
static fn_TerrainDecalRemove_t s_fn_TerrainDecalRemove = nullptr;

static bool s_initialized = false;

// ============================================================
// Helper: Get DecalDescription config component
// ============================================================
static void* GetDecalDescription(void* self) {
    void* obj = *(void**)((uintptr_t)self + 0x18);
    if (!obj || !s_fn_IsDerivedFrom_dispatch || !s_TypeId_DecalDesc)
        return nullptr;
    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_DecalDesc);
    return derived ? obj : nullptr;
}

// ============================================================
// DecalComponent::Shutdown (vtable slot [3], 0x14011b820)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Serializer(version=1): serialize decalScale (float at +0x30)
//   3. If version > 0: serialize decalState (int at +0x28)
void DecalComponent_Shutdown(DecalComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer, stream, 1);

    // Serialize decalScale (float at +0x30)
    if (s_fn_Serialize_Float)
        s_fn_Serialize_Float(serializer, &self->decalScale, nullptr);

    // Version-gated: if version > 0, also serialize decalState
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version > 0) {
            if (s_fn_Serialize_Int)
                s_fn_Serialize_Int(serializer, (int*)&self->decalState, nullptr);
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// DecalComponent::Serialize (vtable slot [4], 0x14011b7d0)
// ============================================================
// Gets DecalDescription via IsDerivedFrom(+0x18, DAT_1401c75b8),
// reads scale from building data+0x20 and stores in decalScale (+0x30).
void DecalComponent_Serialize(DecalComponent* self) {
    void* config = GetDecalDescription(self);
    if (config) {
        self->decalScale = *(float*)((uintptr_t)config + 0x20);
    }
}

// ============================================================
// DecalComponent::Draw (vtable slot [6], 0x14011b560)
// ============================================================
// Complex terrain decal rendering: 111 instructions.
// Reads building data, entity position, decal textures.
// Delegates to original function.
void DecalComponent_Draw(DecalComponent* self) {
    if (s_fn_OriginalDraw)
        s_fn_OriginalDraw(self);
}

// ============================================================
// DecalComponent::OnCreate (vtable slot [7], 0x14011b510)
// ============================================================
// If decalId >= 0, removes the decal from terrain, resets decalId to -1.
void DecalComponent_OnCreate(DecalComponent* self) {
    if (self->decalId >= 0) {
        // Remove decal from terrain decal subsystem
        if (s_fn_TerrainDecalRemove && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* terrainDecalSys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x110);
            if (terrainDecalSys)
                s_fn_TerrainDecalRemove(terrainDecalSys, self->decalId);
        }
    }
    self->decalId = -1;
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DecalComponent_InitFuncs() {
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

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_TypeId_DecalDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c75b8));

    s_fn_OriginalDraw = reinterpret_cast<fn_DecalDraw_t>(
        ResolveAddr(Module::Exe, 0x14011b560));

    s_fn_TerrainDecalRemove = reinterpret_cast<fn_TerrainDecalRemove_t>(
        ResolveAddr(Module::Exe, 0x1400b0d00));

    s_initialized = true;

#ifdef _DEBUG
    printf("[DecalComponent] Funcs resolved: CompSerialize=%p Draw=%p DecalRemove=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_OriginalDraw,
           (void*)s_fn_TerrainDecalRemove);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (DecalComponent_Factory MemAlloc), flags 0xc3 (Ctor 0x14011b8f0:
// Component base sets +0x20=0x80, leaf OR 0xc3 => 0xc3).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(DecalComponent, 0x38, 0xc3)
REGISTER_TYPE(DecalComponent, "Component",
              &DecalComponent_Factory, &DecalComponent_CreateInPlace);
#endif // STANDALONE_EXE
