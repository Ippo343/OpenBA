// code/Game/Component/terraformcomponent.cpp
// Clean reimplementation of TerraformComponent lifecycle methods from Application-x64.exe.
//
// TerraformComponent (0x48 bytes) — manages terrain modification during construction.
// Fields: +0x28=Vector3 array, +0x30=count, +0x38=tile index array, +0x40=tile count
//
// Vtable at 0x14015a880. Two non-trivial overrides:
//   Shutdown (0x140057500) — Serialize Vector3 array
//   OnCreate (0x140057350) — Apply terrain heights + tile type updates
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/terraformcomponent_funcs.h"
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

typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

// FUN_14000d030 — resize Vector3 array
typedef void (*fn_ResizeVec3Array_t)(void* arrayPtr, uint32_t newCount);
static fn_ResizeVec3Array_t s_fn_ResizeVec3Array = nullptr;

// Terrain helpers for OnCreate
// FUN_1400a4b50 — CitizenAI_impl_b150 (terrain recalc)
typedef void (*fn_TerrainRecalc_t)(void* terrainGrid, int x, int y);
static fn_TerrainRecalc_t s_fn_TerrainRecalc = nullptr;

// FUN_1400a8c90 — terrain tile visual update
typedef void (*fn_TerrainVisualUpdate_t)(void* ptr, int x, int y);
static fn_TerrainVisualUpdate_t s_fn_TerrainVisualUpdate = nullptr;

// FUN_1400a6fe0 — terrain type set
typedef void (*fn_TerrainTypeSet_t)(void* thread, int index, int type, uint64_t param4);
static fn_TerrainTypeSet_t s_fn_TerrainTypeSet = nullptr;

static float* s_pHeightScale = nullptr;  // DAT_140172230

static bool s_initialized = false;

// ============================================================
// TerraformComponent::Shutdown (0x140057500)
// ============================================================

void TerraformComponent_Shutdown(TerraformComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize Vector3 count from +0x30
    uint32_t count = *(uint32_t*)(selfAddr + 0x30);
    uint32_t localCount = count;
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, (int*)&localCount, nullptr);

    // If loading, resize the Vector3 array
    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeVec3Array) {
        s_fn_ResizeVec3Array((void*)(selfAddr + 0x28), localCount);
    }

    // Serialize each Vector3 (12 bytes each)
    void* vecArray = *(void**)(selfAddr + 0x28);
    if ((int)localCount > 0 && s_fn_Vector3_Serialize) {
        uint8_t* pVec = (uint8_t*)vecArray;
        for (uint32_t i = 0; i < localCount; i++) {
            s_fn_Vector3_Serialize(pVec, serializer);
            pVec += 0xC;  // sizeof(Vector3) = 12
        }
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TerraformComponent::OnCreate (0x140057350)
// ============================================================
// Applies stored terrain heights from Vector3 array to the terrain grid,
// then updates tile types for the tile index array.

void TerraformComponent_OnCreate(TerraformComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    float heightScale = s_pHeightScale ? *s_pHeightScale : 1.0f;

    // Phase 1: Apply Vector3 terrain heights
    uintptr_t terrainGrid = *(uintptr_t*)(gameWorld + 0x110);
    int vecCount = *(int*)(selfAddr + 0x30);

    if (vecCount > 0 && terrainGrid) {
        uintptr_t vecArray = *(uintptr_t*)(selfAddr + 0x28);
        float halfSize = (float)(*(int*)(terrainGrid + 0xC0) >> 1);

        for (int i = 0; i < vecCount; i++) {
            uintptr_t vec = vecArray + (uintptr_t)i * 0xC;
            float vx = *(float*)vec;
            float vy = *(float*)(vec + 4);
            float vz = *(float*)(vec + 8);

            // Offset by half grid size
            float mapX = halfSize + vx;
            float mapY = halfSize + vy;

            // Convert Z to height index
            uint32_t heightIdx = (uint32_t)(vz * heightScale);

            // Write height to terrain height array
            uintptr_t heightArray = *(uintptr_t*)(terrainGrid + 0xD8);
            int stride = *(int*)(terrainGrid + 0xD0);
            int64_t offset = ((int64_t)stride * (int64_t)(int)mapY + (int64_t)(int)mapX) * 2;
            *(int16_t*)(heightArray + offset) = (int16_t)heightIdx;

            // Recalculate terrain at this position
            if (s_fn_TerrainRecalc)
                s_fn_TerrainRecalc((void*)terrainGrid, (int)mapX, (int)mapY);
        }
    }

    // Phase 2: Update tile types from tile index array at +0x38/+0x40
    int* tileGrid = *(int**)(gameWorld + 0xF8);
    int tileCount = *(int*)(selfAddr + 0x40);

    if (tileCount > 0 && tileGrid) {
        uintptr_t tileArray = *(uintptr_t*)(selfAddr + 0x38);
        int gridWidth = *tileGrid;

        for (int i = 0; i < tileCount; i++) {
            int tileIndex = *(int*)(tileArray + (uintptr_t)i * 4);

            // Read current terrain type
            uintptr_t tileData = *(uintptr_t*)((uintptr_t)tileGrid + 0x80);
            uint8_t oldType = *(uint8_t*)(tileData + 0x29 + (int64_t)tileIndex * 0x30);

            // Update buildable flag
            bool isBuildable = ((1 << (oldType & 0x1F)) & 0x1E0) != 0;
            *(uint8_t*)((uintptr_t)tileGrid + 0xA2) |= (isBuildable ? 1 : 0);

            // Set tile type to 10 (terraformed)
            *(uint8_t*)(tileData + 0x29 + (int64_t)tileIndex * 0x30) = 10;

            // Update terrain visuals if was buildable
            if (isBuildable && s_fn_TerrainVisualUpdate) {
                uintptr_t visualPtr = *(uintptr_t*)((uintptr_t)tileGrid + 0x70);
                s_fn_TerrainVisualUpdate((void*)visualPtr, tileIndex % gridWidth, tileIndex / gridWidth);
            }

            // Set terrain type
            if (s_fn_TerrainTypeSet) {
                uintptr_t threadPtr = *(uintptr_t*)((uintptr_t)tileGrid + 0x68);
                s_fn_TerrainTypeSet((void*)threadPtr, tileIndex, 10, 0);
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TerraformComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ResizeVec3Array = reinterpret_cast<fn_ResizeVec3Array_t>(
        ResolveAddr(Module::Exe, 0x14000d030));
    s_fn_TerrainRecalc = reinterpret_cast<fn_TerrainRecalc_t>(
        ResolveAddr(Module::Exe, 0x1400a4b50));
    s_fn_TerrainVisualUpdate = reinterpret_cast<fn_TerrainVisualUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400a8c90));
    s_fn_TerrainTypeSet = reinterpret_cast<fn_TerrainTypeSet_t>(
        ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_pHeightScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172230));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);

    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}
