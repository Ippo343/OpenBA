// code/Game/Component/huntcomponent.cpp
// Clean reimplementation of HuntComponent::Update from Application-x64.exe.
//
// HuntComponent (0x58 bytes) extends ResourceProduceComponent. The Update
// reads the terrain speed ratio from the entity's components, scales the
// delta time by that ratio, and accumulates the result into the hunt
// progress accumulator at +0x50.
//
// Original: 0x140122740
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/huntcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002e10: Find component on entity by type (terrain/character component)
typedef void* (*fn_FindComponent1_t)(int64_t entity, void* outPtr);
static fn_FindComponent1_t s_fn_FindComponent1 = nullptr;

// FUN_140003050: Find component on entity by type (map tile component)
typedef void* (*fn_FindComponent2_t)(int64_t entity, void* outPtr);
static fn_FindComponent2_t s_fn_FindComponent2 = nullptr;

// FUN_14012e5c0: Compute terrain speed for a tile (cached at tile+0x30)
typedef int (*fn_ComputeTerrainSpeed_t)(int64_t tileData);
static fn_ComputeTerrainSpeed_t s_fn_ComputeTerrainSpeed = nullptr;

// FUN_140004680: find the map/tile component on an entity (returns InternalBasePtr*)
typedef void* (*fn_FindMapComponent_t)(int64_t owner, void* outIBP);
static fn_FindMapComponent_t s_fn_FindMapComponent = nullptr;

// DWORD_1401c80a8: type id checked by FUN_14012e5c0 (terrain/zone description)
static uint32_t* s_pTerrainSpeedTypeId = nullptr;
// g_fUVScale (DAT_140172140): 0.5 rounding constant used in the speed calc
static float* s_pRoundHalf = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_FindComponent1 = reinterpret_cast<fn_FindComponent1_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_FindComponent2 = reinterpret_cast<fn_FindComponent2_t>(
        ResolveAddr(Module::Exe, 0x140003050));

    s_fn_ComputeTerrainSpeed = reinterpret_cast<fn_ComputeTerrainSpeed_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_fn_FindMapComponent = reinterpret_cast<fn_FindMapComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_pTerrainSpeedTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c80a8));
    s_pRoundHalf = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172140));

    s_initialized = true;
}

// Reimpl of FUN_14012e5c0 (HuntComponent: compute terrain movement speed for a tile).
// pvVar3 = IsDerivedFrom(tile->_unk18, terrainSpeedTypeId) ? that : nullptr.
// If pvVar3+0x34 flag is clear, return the cached speed at pvVar3+0x24.
// Otherwise recompute from the map component footprint (w*h at +0x2c/+0x2e),
// scaled by pvVar3+0x24 / pvVar3+0x30, rounded (+0.5), then clamped to [1, base].
int HuntComponent_ComputeTerrainSpeed(int64_t tileData) {
    EnsureHelpers();

    void* obj = *reinterpret_cast<void**>(tileData + 0x18);
    bool isDerived = (s_pTerrainSpeedTypeId && fn_IsDerivedFrom)
                         ? fn_IsDerivedFrom(obj, *s_pTerrainSpeedTypeId)
                         : false;
    void* desc = isDerived ? obj : nullptr;

    if (*reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(desc) + 0x34) == '\0') {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x24);
    }

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(tileData + 0x10));

    int64_t mapIBP = 0;
    if (s_fn_FindMapComponent)
        s_fn_FindMapComponent(*reinterpret_cast<int64_t*>(tileData + 0x10), &mapIBP);
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&mapIBP);

    int baseSpeed = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x24);
    int16_t w = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
    int16_t h = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    int denom = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x30);
    float roundHalf = s_pRoundHalf ? *s_pRoundHalf : 0.5f;

    int computed = static_cast<int>(
        (static_cast<float>(static_cast<int>(h) * static_cast<int>(w)) /
         static_cast<float>(denom)) * static_cast<float>(baseSpeed) + roundHalf);

    int result = baseSpeed;
    if (computed <= baseSpeed) {
        result = computed;
        if (computed < 1)
            result = 1;
    }
    return result;
}

// ============================================================
// HuntComponent::Update (vtable slot [9], 0x140122740)
// ============================================================
// Computes hunt progress: accumulates delta time scaled by the
// terrain speed ratio (base speed / terrain speed).

void HuntComponent_Update(HuntComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (!s_fn_FindComponent1 || !s_fn_FindComponent2 || !s_fn_ComputeTerrainSpeed ||
        !g_Timer_secondsPerTick)
        return;

    // Find the terrain/character component on our owner entity
    int64_t entityPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    int64_t terrainComponent = 0;
    s_fn_FindComponent1(entityPtr, &terrainComponent);

    // Read terrain base speed from component+0x60
    int terrainBaseSpeed = *reinterpret_cast<int*>(terrainComponent + 0x60);

    // Find the map tile component via terrainComponent's owner
    int64_t tileOwner = *reinterpret_cast<int64_t*>(terrainComponent + 0x10);
    int64_t tileComponentPtr = 0;
    void* result = s_fn_FindComponent2(tileOwner, &tileComponentPtr);

    // Dereference to get actual tile data
    int64_t tileData = *reinterpret_cast<int64_t*>(result);

    // Lazily compute terrain speed if not cached
    int terrainSpeed = *reinterpret_cast<int*>(tileData + 0x30);
    if (terrainSpeed == 0) {
        terrainSpeed = s_fn_ComputeTerrainSpeed(tileData);
        *reinterpret_cast<int*>(tileData + 0x30) = terrainSpeed;
    }

    // Convert delta time to seconds (inline Time::AsSeconds)
    float deltaSeconds = static_cast<float>(
        static_cast<double>(deltaTime) * (*g_Timer_secondsPerTick));

    // Scale by terrain speed ratio and convert back to ticks
    float scaledSeconds = deltaSeconds * (static_cast<float>(terrainBaseSpeed) /
                                           static_cast<float>(terrainSpeed));
    int64_t scaledTicks = static_cast<int64_t>(
        static_cast<double>(scaledSeconds) / (*g_Timer_secondsPerTick));

    // Accumulate into hunt progress at +0x50
    *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x50) += scaledTicks;
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HuntComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HuntComponent] Funcs resolved: FindComponent1=%p FindComponent2=%p "
           "ComputeTerrainSpeed=%p\n",
           (void*)s_fn_FindComponent1, (void*)s_fn_FindComponent2,
           (void*)s_fn_ComputeTerrainSpeed);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(HuntComponent, 0x58, 0x88)
REGISTER_TYPE(HuntComponent, "ResourceProduceComponent",
              &HuntComponent_Factory, &HuntComponent_CreateInPlace);
