// code/Game/Component/fruitcomponent.cpp
// Clean reimplementation of FruitComponent::Update from Application-x64.exe.
//
// Fruit ripeness simulation: accumulates growth progress each tick based on
// temperature sensitivity and seasonal growth factor. Uses quadratic
// temperature curve -- growth peaks at optimal temp and falls off as temp
// diverges. Result is clamped to [0, 1].
//
// Key formula:
//   tempDelta = abs(currentTemp - optimalTemp) / tempRange
//   if fruitFlag clear && growthRate >= 1.0:
//     if tempDelta in [0, 1]: growthFactor = 1.0 - tempDelta^2
//     else if currentTemp <= freezeThreshold: growthFactor = -10.0 (frost damage)
//     else: growthFactor = 0.0
//   ripeness += deltaSeconds * growthRate * ripeningSpeed * growthFactor
//   clamp ripeness to [0.0, 1.0]
//
// Original: 0x140049de0 (105 instructions)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/fruitcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// ============================================================
// Constants from exe .rdata
// ============================================================
static constexpr float CONST_ONE       = 1.0f;
static constexpr float SECS_PER_SEASON = 300.0f;
static constexpr float SECS_PER_YEAR   = 3600.0f;
static constexpr float MONTHS_PER_YEAR = 12.0f;
static constexpr float FREEZE_THRESHOLD = 32.0f;
static constexpr float FROST_DAMAGE    = -10.0f;

// ============================================================
// Resolved helpers
// ============================================================

// Type ID for the fruit-bearing resource type (DAT_1401c4288)
static uint32_t* s_pFruitResourceTypeId = nullptr;

// Game state singleton (DAT_1401c3dd8)
// +0x128: pointer to season time object (+8 = Time value)
// +0x130: pointer to weather/environment data (+0x18 = current temperature)
static void** s_pGameState = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pFruitResourceTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4288));

    s_initialized = true;
}

// ============================================================
// FruitComponent::Update (vtable slot [9], 0x140049de0)
// ============================================================
// Fruit ripening simulation with temperature-dependent growth.

void FruitComponent_Update(FruitComponent* self, int64_t* pDeltaTime) {
    EnsureHelpers();

    // Get the resource data object via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    void* resourceData = nullptr;
    if (baseObj && fn_IsDerivedFrom && s_pFruitResourceTypeId) {
        if (fn_IsDerivedFrom(baseObj, *s_pFruitResourceTypeId)) {
            resourceData = baseObj;
        }
    }

    // ripeningSpeed = 1.0 / (resourceData->field20 * 300.0)
    float ripeningSpeed = CONST_ONE / (*(float*)((uintptr_t)resourceData + 0x20) * SECS_PER_SEASON);

    // Check fruit flag (at +0x34) -- if set, check seasonal month and possibly clear it
    if (self->fruitFlag != 0) {
        if (s_pGameState && *s_pGameState) {
            uintptr_t gs = (uintptr_t)*s_pGameState;
            int64_t seasonTimeTicks = *(int64_t*)(*(uintptr_t*)(gs + 0x128) + 8);
            float seasonTimeSecs = (float)((double)seasonTimeTicks * *g_Timer_secondsPerTick);
            // Calculate month index: (seasonSecs / 3600.0) * 12.0 - 7
            // If month > 4 (unsigned compare), clear the flag
            unsigned int monthIdx = (unsigned int)((int)((seasonTimeSecs / SECS_PER_YEAR) * MONTHS_PER_YEAR) - 7);
            if (monthIdx > 4) {
                self->fruitFlag = 0;
            }
        }
    }

    // Get current temperature from weather/environment data
    float currentTemp = 0.0f;
    if (s_pGameState && *s_pGameState) {
        uintptr_t gs = (uintptr_t)*s_pGameState;
        currentTemp = *(float*)(*(uintptr_t*)(gs + 0x130) + 0x18);
    }

    // Calculate temperature delta: abs(currentTemp - optimalTemp) / tempRange
    float optimalTemp = *(float*)((uintptr_t)resourceData + 0x24);
    float tempRange = *(float*)((uintptr_t)resourceData + 0x28);
    float tempDelta = (float)fabs((double)(currentTemp - optimalTemp)) / tempRange;

    float growthFactor;

    if (self->fruitFlag == 0) {
        // Dereference the InternalBasePtr at +0x28 to get growth rate
        void* linkedObj = *(void**)((uintptr_t)self + 0x28);
        float* pGrowthRate = (float*)((uintptr_t)linkedObj + 0x28);

        if (*pGrowthRate >= CONST_ONE) {
            // Growth rate is sufficient -- calculate temperature-based growth
            if (tempDelta >= 0.0f && tempDelta <= CONST_ONE) {
                // Quadratic falloff: 1.0 - delta^2
                growthFactor = CONST_ONE - tempDelta * tempDelta;
            } else {
                growthFactor = 0.0f;
                if (currentTemp <= FREEZE_THRESHOLD) {
                    growthFactor = FROST_DAMAGE;
                }
            }
            goto applyGrowth;
        }
    }

    // If fruit flag is set or growth rate < 1.0, no growth
    growthFactor = 0.0f;

applyGrowth:
    {
        void* linkedObj2 = *(void**)((uintptr_t)self + 0x28);
        float effectiveRate = *(float*)((uintptr_t)linkedObj2 + 0x28);
        if (effectiveRate > CONST_ONE) {
            effectiveRate = CONST_ONE;
        }

        // Convert delta time to seconds
        float deltaSecs = (float)((double)*pDeltaTime * *g_Timer_secondsPerTick);

        // Accumulate ripeness
        float* pRipeness = (float*)&self->field30;
        *pRipeness += deltaSecs * effectiveRate * ripeningSpeed * growthFactor;

        // Clamp to [0.0, 1.0]
        if (*pRipeness < 0.0f) {
            *pRipeness = 0.0f;
        }
        if (*pRipeness > CONST_ONE) {
            *pRipeness = CONST_ONE;
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FruitComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FruitComponent] Funcs resolved: GameState=%p FruitTypeId=%p IsDerivedFrom=%p\n",
           (void*)s_pGameState, (void*)s_pFruitResourceTypeId, (void*)fn_IsDerivedFrom);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid FruitComponent: size 0x38 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x88 (from FruitComponent_Ctor) + zeroed body.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(FruitComponent, 0x38, 0x88)
REGISTER_TYPE(FruitComponent, "Component",
              &FruitComponent_Factory, &FruitComponent_CreateInPlace);
#endif // STANDALONE_EXE
