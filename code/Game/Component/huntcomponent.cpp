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

// FUN_14012e5c0: WorkComponent::CalcMaxWorkers — the description-derived, footprint-scaled
// worker capacity, cached at workComp+0x30. HuntComponent::Update reuses that same cache as
// its speed divisor, which is why the old name here was "ComputeTerrainSpeed". The single
// body lives in code/Game/Tutorial/tutorial_base.cpp (WorkComponent_CalcMaxWorkers) and is
// registered ONCE, from standalone/reg_tutorial_base.cpp. A second, identically-behaving
// copy used to be registered from reg_component_nature.cpp; whichever batch ran last won.
typedef int (*fn_CalcMaxWorkers_t)(int64_t workComp);
static fn_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_FindComponent1 = reinterpret_cast<fn_FindComponent1_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_FindComponent2 = reinterpret_cast<fn_FindComponent2_t>(
        ResolveAddr(Module::Exe, 0x140003050));

    s_fn_CalcMaxWorkers = reinterpret_cast<fn_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_initialized = true;
}

// ============================================================
// HuntComponent::Update (vtable slot [9], 0x140122740)
// ============================================================
// Computes hunt progress: accumulates delta time scaled by the
// terrain speed ratio (base speed / terrain speed).

void HuntComponent_Update(HuntComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (!s_fn_FindComponent1 || !s_fn_FindComponent2 || !s_fn_CalcMaxWorkers ||
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

    // Lazily compute the capacity divisor if not cached
    int terrainSpeed = *reinterpret_cast<int*>(tileData + 0x30);
    if (terrainSpeed == 0) {
        terrainSpeed = s_fn_CalcMaxWorkers(tileData);
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
           "CalcMaxWorkers=%p\n",
           (void*)s_fn_FindComponent1, (void*)s_fn_FindComponent2,
           (void*)s_fn_CalcMaxWorkers);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(HuntComponent, 0x58, 0x88)
REGISTER_TYPE(HuntComponent, "ResourceProduceComponent",
              &HuntComponent_Factory, &HuntComponent_CreateInPlace);
