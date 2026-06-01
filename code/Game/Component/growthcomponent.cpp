// code/Game/Component/growthcomponent.cpp
// Clean reimplementation of GrowthComponent::Update from Application-x64.exe.
//
// GrowthComponent manages tree/crop growth with seasonal temperature modulation,
// harvest threshold logic, and sub-resource spawning.
//
// Update address: 0x140120050
// Struct size: 0x50, vtable at 0x140163590
//
// Key logic:
//   1. Compute base growth rate from config (1.0 / (config+0x28 * 60.0))
//   2. Apply seasonal temperature modulation if config+0x2C flag set
//   3. Accumulate growth: resource+0x28 += dt * growthRate * tempModifier
//   4. If growth < 0 and not flagged: reset and harvest
//   5. If growth > threshold (config+0x54): spawn sub-resources on timer
//   6. If growth > growthScale and conditions met: harvest, reset growth
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/growthcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// ============================================================
// Resolved helpers
// ============================================================

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_1400c3470 -- flag notification (notify entity of flag change)
typedef void (*fn_FlagNotify_t)(void* entity);
static fn_FlagNotify_t s_fn_FlagNotify = nullptr;

// FUN_14011fba0 -- spawn sub-resource (growth spawning)
typedef void (*fn_SpawnSubResource_t)(void* growthComp, void* config);
static fn_SpawnSubResource_t s_fn_SpawnSubResource = nullptr;

// FUN_140120470 -- set resource amount / harvest helper
typedef void (*fn_SetResourceAmount_t)(void* growthComp, float amount);
static fn_SetResourceAmount_t s_fn_SetResourceAmount = nullptr;

// FUN_140120410 -- get resource index
typedef int (*fn_GetResourceIndex_t)(void* growthComp);
static fn_GetResourceIndex_t s_fn_GetResourceIndex = nullptr;

// FUN_140126780 -- reset growth state on entity
typedef void (*fn_ResetGrowthState_t)(void* entityPtr);
static fn_ResetGrowthState_t s_fn_ResetGrowthState = nullptr;

// FUN_140118950 -- modify component flags (self, orMask, andNotMask)
typedef void (*fn_ModifyFlags_t)(void* comp, uint32_t orMask, uint32_t andNotMask);
static fn_ModifyFlags_t s_fn_ModifyFlags = nullptr;

// Global: DAT_1401c3dd8 -- game world/simulation singleton pointer
static void** s_pWorldSingleton = nullptr;

// Float constants from .rdata
static float* s_fOne = nullptr;            // DAT_14017214c = 1.0f
static float* s_fSixty = nullptr;          // DAT_140172234 = 60.0f
static float* s_fFreezeTemp = nullptr;     // DAT_1401721d0 = freeze threshold
static float* s_fFreezeGrowth = nullptr;   // DAT_140172288 = growth rate at freeze
// DAT_1401722b0 -- double mask for fabsf (0x7FFFFFFFFFFFFFFF)
static uint64_t* s_dblAbsMask = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));

    s_fn_FlagNotify = reinterpret_cast<fn_FlagNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_SpawnSubResource = reinterpret_cast<fn_SpawnSubResource_t>(
        ResolveAddr(Module::Exe, 0x14011fba0));
    s_fn_SetResourceAmount = reinterpret_cast<fn_SetResourceAmount_t>(
        ResolveAddr(Module::Exe, 0x140120470));
    s_fn_GetResourceIndex = reinterpret_cast<fn_GetResourceIndex_t>(
        ResolveAddr(Module::Exe, 0x140120410));
    s_fn_ResetGrowthState = reinterpret_cast<fn_ResetGrowthState_t>(
        ResolveAddr(Module::Exe, 0x140126780));
    s_fn_ModifyFlags = reinterpret_cast<fn_ModifyFlags_t>(
        ResolveAddr(Module::Exe, 0x140118950));

    s_pWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_fSixty = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_fFreezeTemp = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721d0));
    s_fFreezeGrowth = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172288));
    s_dblAbsMask = reinterpret_cast<uint64_t*>(ResolveAddr(Module::Exe, 0x1401722b0));

    s_helpersResolved = true;
}

// ============================================================
// Helper: do harvest/reset sequence
// ============================================================
// Shared by both negative-growth and threshold-harvest paths.
static void DoHarvestReset(uint8_t* comp, uintptr_t resourceMgr, float harvestAmount) {
    // Set resource amount
    if (s_fn_SetResourceAmount) {
        s_fn_SetResourceAmount(comp, harvestAmount);
    }

    // Update resource manager entry with owner position
    s_fn_CheckPtr(comp + 0x10);
    uint64_t ownerPos = *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(comp + 0x10) + 0x28);
    int resIdx = 0;
    if (s_fn_GetResourceIndex) {
        resIdx = s_fn_GetResourceIndex(comp);
    }
    uintptr_t resMgrArray = *reinterpret_cast<uintptr_t*>(resourceMgr + 0x80);
    *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(resMgrArray) + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

    // Reset growth to 0
    s_fn_CheckPtr(comp + 0x28);
    *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = 0;

    // Clear growth flag byte
    s_fn_CheckPtr(comp + 0x28);
    *reinterpret_cast<uint8_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x2D) = 0;

    // Reset growth state
    s_fn_CheckPtr(comp + 0x28);
    if (s_fn_ResetGrowthState) {
        s_fn_ResetGrowthState(*reinterpret_cast<void**>(comp + 0x28));
    }
}

// ============================================================
// GrowthComponent::Update (vtable slot [9], 0x140120050)
// ============================================================

void GrowthComponent_Update(GrowthComponent* self, int64_t* deltaTime) {
    EnsureHelpers();

    uint8_t* comp = reinterpret_cast<uint8_t*>(self);
    float fOne = *s_fOne;       // 1.0f
    float fSixty = *s_fSixty;  // 60.0f

    // config = comp->field38 (component config/description pointer)
    uintptr_t config = *reinterpret_cast<uintptr_t*>(comp + 0x38);

    // Base growth rate: 1.0 / (config+0x28 * 60.0)
    float configGrowthTime = *reinterpret_cast<float*>(config + 0x28);
    float growthRate = fOne / (configGrowthTime * fSixty);

    // Get current growth value from resource component at +0x28
    s_fn_CheckPtr(comp + 0x28);
    uintptr_t resourceComp = *reinterpret_cast<uintptr_t*>(comp + 0x28);
    float currentGrowth = *reinterpret_cast<float*>(resourceComp + 0x28);

    // Apply seasonal temperature modulation
    float tempModifier = fOne;
    if (*reinterpret_cast<char*>(config + 0x2C) != '\0') {
        // Get current temperature from world singleton
        // world->weather(+0x130)->temp(+0x18)
        uintptr_t world = reinterpret_cast<uintptr_t>(*s_pWorldSingleton);
        uintptr_t weatherPtr = *reinterpret_cast<uintptr_t*>(world + 0x130);
        float currentTemp = *reinterpret_cast<float*>(weatherPtr + 0x18);

        // Compute temperature distance from optimal
        float optimalTemp = *reinterpret_cast<float*>(config + 0x30);
        float tempRange = *reinterpret_cast<float*>(config + 0x34);

        // fabsf via double abs with bitmask (matches original bitwise pattern)
        double diff = static_cast<double>(currentTemp - optimalTemp);
        uint64_t rawBits;
        __builtin_memcpy(&rawBits, &diff, sizeof(rawBits));
        rawBits &= *s_dblAbsMask;
        __builtin_memcpy(&diff, &rawBits, sizeof(diff));
        tempModifier = static_cast<float>(diff) / tempRange;

        if (tempModifier < 0.0f || fOne < tempModifier) {
            // Out of range: check freeze
            tempModifier = 0.0f;
            if (currentTemp <= *s_fFreezeTemp) {
                tempModifier = *s_fFreezeGrowth;
            }
        } else {
            // Parabolic falloff: 1 - dist^2
            tempModifier = fOne - tempModifier * tempModifier;
        }
    }

    // Get resource manager from world
    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pWorldSingleton);
    uintptr_t resourceMgr = *reinterpret_cast<uintptr_t*>(world + 0xF8);

    // Compute time delta in seconds
    float dtSeconds = fn_Time_AsSeconds(deltaTime);

    // Accumulate growth
    float newGrowth = currentGrowth + dtSeconds * growthRate * tempModifier;

    if (newGrowth >= 0.0f ||
        (s_fn_CheckPtr(comp + 0x28),
         (*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) != 0)) {
        // Growth is positive OR entity has special flag (bit 20)

        // Store new growth value
        s_fn_CheckPtr(comp + 0x28);
        *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = newGrowth;

        // Set flag bit 0x10 (growth active)
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(comp + 0x20);
        uint32_t newFlags = oldFlags | 0x10;
        *reinterpret_cast<uint32_t*>(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 &&
            *reinterpret_cast<void**>(comp + 0x10) != nullptr) {
            s_fn_CheckPtr(comp + 0x10);
            if (s_fn_FlagNotify) {
                s_fn_FlagNotify(*reinterpret_cast<void**>(comp + 0x10));
            }
        }

        // Sub-resource spawning check
        if (*reinterpret_cast<char*>(config + 0x44) != '\0') {
            float spawnThreshold = *reinterpret_cast<float*>(config + 0x54);
            if (spawnThreshold <= newGrowth && newGrowth != spawnThreshold) {
                // Accumulate spawn timer at comp+0x40
                *reinterpret_cast<int64_t*>(comp + 0x40) += *deltaTime;

                float spawnTimerSec = fn_Time_AsSeconds(reinterpret_cast<int64_t*>(comp + 0x40));
                float spawnInterval = static_cast<float>(
                    *reinterpret_cast<int32_t*>(config + 0x50)) * fSixty;

                if (spawnInterval < spawnTimerSec) {
                    do {
                        if (s_fn_SpawnSubResource) {
                            s_fn_SpawnSubResource(comp, reinterpret_cast<void*>(config));
                        }
                        spawnTimerSec -= spawnInterval;
                    } while (spawnInterval < spawnTimerSec);

                    // Reset spawn timer to remainder
                    int64_t remainderTime;
                    fn_Time_Ctor_Float(&remainderTime, spawnTimerSec);
                    *reinterpret_cast<int64_t*>(comp + 0x40) = remainderTime;
                }
            }
        }

        // Harvest check: growthScale threshold
        float growthScale = *reinterpret_cast<float*>(comp + 0x4C);
        if (growthScale > 0.0f && growthScale < newGrowth) {
            // Check entity doesn't have bit 20 set
            s_fn_CheckPtr(comp + 0x28);
            if ((*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) == 0) {
                // Check world flag (bit 10 of world+0xD0)
                if ((*reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uintptr_t>(*s_pWorldSingleton) + 0xD0) >> 10 & 1) == 0) {
                    // Clamp growth to 1.0
                    float harvestAmount = (newGrowth <= fOne) ? newGrowth : fOne;

                    // Set resource amount scaled by config+0x40
                    float configResourceScale = *reinterpret_cast<float*>(config + 0x40);
                    DoHarvestReset(comp, resourceMgr, harvestAmount * configResourceScale);

                    // Clear flags bits 3,4 via ModifyFlags(self, 0, 0x18)
                    if (s_fn_ModifyFlags) {
                        s_fn_ModifyFlags(comp, 0, 0x18);
                    }
                }
            }
        }
    } else {
        // Growth went negative -- do full harvest/reset
        float configResourceScale = *reinterpret_cast<float*>(config + 0x40);
        DoHarvestReset(comp, resourceMgr, configResourceScale);

        // Clear flags bits 3,4 directly
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(comp + 0x20);
        uint32_t newFlags = oldFlags & 0xFFFFFFE7;
        *reinterpret_cast<uint32_t*>(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 &&
            *reinterpret_cast<void**>(comp + 0x10) != nullptr) {
            s_fn_CheckPtr(comp + 0x10);
            if (s_fn_FlagNotify) {
                s_fn_FlagNotify(*reinterpret_cast<void**>(comp + 0x10));
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void GrowthComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[GrowthComponent] Funcs resolved: CheckPtr=%p FlagNotify=%p "
           "SpawnSub=%p SetResAmt=%p GetResIdx=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_FlagNotify,
           (void*)s_fn_SpawnSubResource, (void*)s_fn_SetResourceAmount,
           (void*)s_fn_GetResourceIndex);
    printf("[GrowthComponent]   WorldSingleton=%p -> %p\n",
           (void*)s_pWorldSingleton,
           s_pWorldSingleton ? *s_pWorldSingleton : nullptr);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid GrowthComponent: size 0x50 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0xcb (from GrowthComponent_Ctor) + zeroed body.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(GrowthComponent, 0x50, 0xcb)
REGISTER_TYPE(GrowthComponent, "Component",
              &GrowthComponent_Factory, &GrowthComponent_CreateInPlace);
#endif // STANDALONE_EXE
