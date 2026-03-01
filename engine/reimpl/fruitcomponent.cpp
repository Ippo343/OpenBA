// engine/reimpl/fruitcomponent.cpp
// Reimplementation of FruitComponent::Update from Application-x64.exe.
//
// Fruit ripeness simulation: accumulates growth progress each tick based on
// temperature sensitivity and seasonal growth factor. Uses quadratic
// temperature curve — growth peaks at optimal temp and falls off as temp
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

#include "FruitComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>
#include <cmath>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Constants from exe .rdata
// ============================================================
// DAT_14017214c = 1.0f
// DAT_140172234 = 300.0f (seconds per season / ticks conversion)
// DAT_14017225c = 3600.0f (seconds per year)
// DAT_1401721b8 = 12.0f  (months per year)
// DAT_1401721d0 = 32.0f  (freeze threshold temperature)
// DAT_140172288 = -10.0f (frost damage growth factor)

static constexpr float CONST_ONE     = 1.0f;
static constexpr float SECS_PER_SEASON = 300.0f;
static constexpr float SECS_PER_YEAR = 3600.0f;
static constexpr float MONTHS_PER_YEAR = 12.0f;
static constexpr float FREEZE_THRESHOLD = 32.0f;
static constexpr float FROST_DAMAGE = -10.0f;

// ============================================================
// Helper function pointers & globals (resolved at install time)
// ============================================================

// Object::Base::IsDerivedFrom — already in engine_funcs.h (fn_IsDerivedFrom)

// Type ID for the fruit-bearing resource type (DAT_1401c4288)
static uint32_t* g_pFruitResourceTypeId = nullptr;

// Game state singleton (DAT_1401c3dd8)
// +0x128: pointer to season time object (+8 = Time value)
// +0x130: pointer to weather/environment data (+0x18 = current temperature)
static void** g_pGameState = nullptr;

// Time::AsSeconds — converts Time (int64 ticks) to float seconds
// Already resolved globally: (float)((double)ticks * *g_Timer_secondsPerTick)
// We inline this instead of calling through a pointer.

// Object::InternalBasePtr::CheckPtr — no-op in release build, skip

// ============================================================
// Update (0x140049de0)
// Fruit ripening simulation with temperature-dependent growth.
//
// Parameters: RCX = this (FruitComponent*), RDX = Time* (int64*)
// ============================================================
static void Reimpl_Update(FruitComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "FruitComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[FruitComponent::Update] self=%p dt=%lld ripeness=%.3f flag=%d\n",
               (void*)self, *pDeltaTime, *(float*)&self->field30, self->fruitFlag);
        callCount++;
    }
#endif

    // Get the resource data object via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    void* resourceData = nullptr;
    if (baseObj && fn_IsDerivedFrom && g_pFruitResourceTypeId) {
        if (fn_IsDerivedFrom(baseObj, *g_pFruitResourceTypeId)) {
            resourceData = baseObj;
        }
    }

    // ripeningSpeed = 1.0 / (resourceData->field20 * 300.0)
    // field20 at offset +0x20 is a float (likely "years to ripen" or similar)
    float ripeningSpeed = CONST_ONE / (*(float*)((uintptr_t)resourceData + 0x20) * SECS_PER_SEASON);

    // Check fruit flag (at +0x34) — if set, check seasonal month and possibly clear it
    if (self->fruitFlag != 0) {
        // Get season time from game state
        if (g_pGameState && *g_pGameState) {
            uintptr_t gs = (uintptr_t)*g_pGameState;
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
    if (g_pGameState && *g_pGameState) {
        uintptr_t gs = (uintptr_t)*g_pGameState;
        currentTemp = *(float*)(*(uintptr_t*)(gs + 0x130) + 0x18);
    }

    // Calculate temperature delta: abs(currentTemp - optimalTemp) / tempRange
    // resourceData+0x24 = optimal temperature, resourceData+0x28 = temperature range
    float optimalTemp = *(float*)((uintptr_t)resourceData + 0x24);
    float tempRange = *(float*)((uintptr_t)resourceData + 0x28);
    float tempDelta = (float)fabs((double)(currentTemp - optimalTemp)) / tempRange;

    float growthFactor;

    if (self->fruitFlag == 0) {
        // Dereference the InternalBasePtr at +0x28 to get growth rate
        // InternalBasePtr: the actual pointer is at *(void**)(self+0x28)
        // then the growth rate float is at +0x28 of that object
        void* linkedObj = *(void**)((uintptr_t)self + 0x28);
        float* pGrowthRate = (float*)((uintptr_t)linkedObj + 0x28);

        if (*pGrowthRate >= CONST_ONE) {
            // Growth rate is sufficient — calculate temperature-based growth
            if (tempDelta >= 0.0f && tempDelta <= CONST_ONE) {
                // Quadratic falloff: 1.0 - delta^2
                growthFactor = CONST_ONE - tempDelta * tempDelta;
            } else {
                // Out of range
                growthFactor = 0.0f;
                if (currentTemp <= FREEZE_THRESHOLD) {
                    growthFactor = FROST_DAMAGE;  // Frost damage
                }
            }
            goto applyGrowth;
        }
    }

    // If fruit flag is set or growth rate < 1.0, no growth
    growthFactor = 0.0f;

applyGrowth:
    // Get effective growth rate (clamped to [0, 1.0])
    {
        void* linkedObj2 = *(void**)((uintptr_t)self + 0x28);
        float effectiveRate = *(float*)((uintptr_t)linkedObj2 + 0x28);
        if (effectiveRate > CONST_ONE) {
            effectiveRate = CONST_ONE;
        }

        // Convert delta time to seconds
        float deltaSecs = (float)((double)*pDeltaTime * *g_Timer_secondsPerTick);

        // Accumulate ripeness: ripeness += dt * effectiveRate * ripeningSpeed * growthFactor
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
// Install / Remove
// ============================================================
void InstallFruitComponentUpdate() {
    // Game state singleton (pointer at 0x1401c3dd8 in exe)
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Fruit resource type ID (at 0x1401c4288 in exe)
    g_pFruitResourceTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4288));

#ifdef _DEBUG
    printf("[FruitComponent] GameState = %p\n", (void*)g_pGameState);
    printf("[FruitComponent] FruitResourceTypeId = %p\n", (void*)g_pFruitResourceTypeId);
    printf("[FruitComponent] IsDerivedFrom = %p\n", (void*)fn_IsDerivedFrom);
    printf("[FruitComponent] secondsPerTick = %p\n", (void*)g_Timer_secondsPerTick);
#endif

    if (!fn_IsDerivedFrom || !g_Timer_secondsPerTick) {
#ifdef _DEBUG
        printf("[FruitComponent] ERROR: missing critical deps, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140049de0);
    // Prologue bytes:
    //   0: MOV RAX,RSP (3)
    //   3: MOV [RAX+0x8],RBX (4)
    //   7: MOV [RAX+0x10],RBP (4)
    //  11: MOV [RAX+0x18],RSI (4)
    // = 15 bytes, clean instruction boundary
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[FruitComponent] Update hooked at %p (prologue=15)\n", (void*)target);
#endif
}

void RemoveFruitComponentUpdate() { RemoveHook(&g_hookUpdate); }
