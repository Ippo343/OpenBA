// engine/reimpl/growth.cpp
// Reimplementation of GrowthComponent::Update from Application-x64.exe.
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
//   3. Accumulate growth: owner+0x28 += dt * growthRate * tempModifier
//   4. If growth < 0 and not flagged: reset and harvest
//   5. If growth > threshold (config+0x54): spawn sub-resources on timer
//   6. If growth > growthScale and conditions met: harvest, reset growth

#include "GrowthComponent.h"
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
// Local helper function pointers (resolved from Application-x64.exe)
// ============================================================

// Object::InternalBasePtr::CheckPtr(InternalBasePtr*)
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// System::Time::AsSeconds(Time const*) -> float
typedef float (*fn_Time_AsSeconds_t)(void* time);
static fn_Time_AsSeconds_t fn_Time_AsSeconds = nullptr;

// System::Time::Time(Time*, float) — construct Time from seconds
typedef void (*fn_Time_Ctor_Float_t)(void* outTime, float seconds);
static fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;

// FUN_1400c3470 — flag notification (notify entity of flag change)
typedef void (*fn_FlagNotify_t)(void* entity);
static fn_FlagNotify_t fn_FlagNotify = nullptr;

// FUN_14011fba0 — spawn sub-resource (growth spawning)
typedef void (*fn_SpawnSubResource_t)(void* growthComp, void* config);
static fn_SpawnSubResource_t fn_SpawnSubResource = nullptr;

// FUN_140120470 — set resource amount / harvest helper
typedef void (*fn_SetResourceAmount_t)(void* growthComp, float amount);
static fn_SetResourceAmount_t fn_SetResourceAmount = nullptr;

// FUN_140120410 — get resource index
typedef int (*fn_GetResourceIndex_t)(void* growthComp);
static fn_GetResourceIndex_t fn_GetResourceIndex = nullptr;

// FUN_140126780 — reset growth state on entity
typedef void (*fn_ResetGrowthState_t)(void* entityPtr);
static fn_ResetGrowthState_t fn_ResetGrowthState = nullptr;

// FUN_140118950 — modify component flags (self, orMask, andNotMask)
typedef void (*fn_ModifyFlags_t)(void* comp, uint32_t orMask, uint32_t andNotMask);
static fn_ModifyFlags_t fn_ModifyFlags = nullptr;

// Global: DAT_1401c3dd8 — game world/simulation singleton pointer
static void** g_pWorldSingleton = nullptr;

// Float constants from .rdata
static float* g_fOne = nullptr;            // DAT_14017214c = 1.0f
static float* g_fSixty = nullptr;          // DAT_140172234 = 60.0f
static float* g_fFreezeTemp = nullptr;     // DAT_1401721d0 = freeze threshold
static float* g_fFreezeGrowth = nullptr;   // DAT_140172288 = growth rate at freeze

// ============================================================
// GrowthComponent::Update reimplementation
// ============================================================

static void Reimpl_Update(void* self, int64_t* deltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[GrowthComponent::Update] self=%p dt=%lld\n", self, *deltaTime);
        callCount++;
    }
#endif

    uint8_t* comp = (uint8_t*)self;
    float fOne = *g_fOne;         // 1.0f
    float fSixty = *g_fSixty;    // 60.0f

    // config = comp->field38 (component config/description pointer)
    void* config = *(void**)(comp + 0x38);

    // Base growth rate: 1.0 / (config+0x28 * 60.0)
    float configGrowthTime = *(float*)((uint8_t*)config + 0x28);
    float growthRate = fOne / (configGrowthTime * fSixty);

    // Get current growth value from owner component at +0x28
    fn_CheckPtr(comp + 0x28);
    void* ownerComp = *(void**)(comp + 0x28);

    float currentGrowth = *(float*)((uint8_t*)ownerComp + 0x28);

    // Apply seasonal temperature modulation
    float tempModifier = fOne;
    if (*(char*)((uint8_t*)config + 0x2C) != '\0') {
        // Get current temperature from world singleton
        void* world = *g_pWorldSingleton;
        float currentTemp = *(float*)(*(uint8_t**)(*(uint8_t**)((uint8_t*)world + 0x130)) + 0x18);

        // Compute temperature distance from optimal
        float optimalTemp = *(float*)((uint8_t*)config + 0x30);
        float tempRange = *(float*)((uint8_t*)config + 0x34);

        // fabsf via double abs trick (matches original bitwise)
        double absDiff = fabs((double)(currentTemp - optimalTemp));
        tempModifier = (float)absDiff / tempRange;

        if (tempModifier < 0.0f || fOne < tempModifier) {
            // Out of range: check freeze
            tempModifier = 0.0f;
            if (currentTemp <= *g_fFreezeTemp) {
                tempModifier = *g_fFreezeGrowth;
            }
        } else {
            // Parabolic falloff: 1 - dist^2
            tempModifier = fOne - tempModifier * tempModifier;
        }
    }

    // Get world resource manager
    void* world = *g_pWorldSingleton;
    void* resourceMgr = *(void**)((uint8_t*)world + 0xF8);

    // Compute time delta in seconds
    float dtSeconds = fn_Time_AsSeconds(deltaTime);

    // Accumulate growth
    float newGrowth = currentGrowth + dtSeconds * growthRate * tempModifier;

    if (newGrowth >= 0.0f ||
        (fn_CheckPtr(comp + 0x28),
         (*(uint32_t*)(*(uint8_t**)(comp + 0x28) + 0x20) >> 0x14 & 1) != 0)) {
        // Growth is positive or entity has special flag (bit 20)

        // Store new growth value
        fn_CheckPtr(comp + 0x28);
        *(float*)(*(uint8_t**)(comp + 0x28) + 0x28) = newGrowth;

        // Set flag bit 0x10 (growth active)
        uint32_t oldFlags = *(uint32_t*)(comp + 0x20);
        uint32_t newFlags = oldFlags | 0x10;
        *(uint32_t*)(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(void**)(comp + 0x10) != nullptr) {
            fn_CheckPtr(comp + 0x10);
            fn_FlagNotify(*(void**)(comp + 0x10));
        }

        // Sub-resource spawning check
        if (*(char*)((uint8_t*)config + 0x44) != '\0') {
            float* spawnThreshold = (float*)((uint8_t*)config + 0x54);
            if (*spawnThreshold <= newGrowth && newGrowth != *spawnThreshold) {
                // Accumulate spawn timer at comp+0x40
                *(int64_t*)(comp + 0x40) = *(int64_t*)(comp + 0x40) + *deltaTime;

                float spawnTimerSec = fn_Time_AsSeconds(comp + 0x40);
                float spawnInterval = (float)(*(int32_t*)((uint8_t*)config + 0x50)) * fSixty;

                if (spawnInterval < spawnTimerSec) {
                    // Spawn sub-resources until timer is below interval
                    do {
                        fn_SpawnSubResource(self, config);
                        spawnTimerSec -= spawnInterval;
                    } while (spawnInterval < spawnTimerSec);

                    // Reset spawn timer to remainder
                    int64_t remainderTime;
                    fn_Time_Ctor_Float(&remainderTime, spawnTimerSec);
                    *(int64_t*)(comp + 0x40) = remainderTime;
                }
            }
        }

        // Harvest check: growthScale threshold
        float growthScale = *(float*)(comp + 0x4C);
        if (growthScale > 0.0f && growthScale < newGrowth) {
            // Check entity doesn't have bit 20 set
            fn_CheckPtr(comp + 0x28);
            if ((*(uint32_t*)(*(uint8_t**)(comp + 0x28) + 0x20) >> 0x14 & 1) == 0) {
                // Check world flag (bit 10 of world+0xD0)
                if ((*(uint32_t*)((uint8_t*)(*g_pWorldSingleton) + 0xD0) >> 10 & 1) == 0) {
                    // Clamp growth to 1.0
                    float harvestAmount = (newGrowth <= fOne) ? newGrowth : fOne;

                    // Set resource amount
                    fn_SetResourceAmount(self, harvestAmount * *(float*)((uint8_t*)config + 0x40));

                    // Update resource manager entry
                    fn_CheckPtr(comp + 0x10);
                    uint64_t ownerPos = *(uint64_t*)(*(uint8_t**)(comp + 0x10) + 0x28);
                    int resIdx = fn_GetResourceIndex(self);
                    *(uint64_t*)(*(uint8_t**)(*(uint8_t**)((uint8_t*)resourceMgr + 0x80)) + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

                    // Reset growth to 0
                    fn_CheckPtr(comp + 0x28);
                    *(uint32_t*)(*(uint8_t**)(comp + 0x28) + 0x28) = 0;

                    // Clear growth flag byte
                    fn_CheckPtr(comp + 0x28);
                    *(uint8_t*)(*(uint8_t**)(comp + 0x28) + 0x2D) = 0;

                    // Reset growth state
                    fn_CheckPtr(comp + 0x28);
                    fn_ResetGrowthState(*(void**)(comp + 0x28));

                    // Modify flags: clear bits 3 and 4 (andNot=0x18)
                    fn_ModifyFlags(self, 0, 0x18);
                }
            }
        }
    } else {
        // Growth went negative — do full harvest/reset

        // Set resource amount from config
        fn_SetResourceAmount(self, *(float*)((uint8_t*)config + 0x40));

        // Update resource manager entry
        fn_CheckPtr(comp + 0x10);
        uint64_t ownerPos = *(uint64_t*)(*(uint8_t**)(comp + 0x10) + 0x28);
        int resIdx = fn_GetResourceIndex(self);
        *(uint64_t*)(*(uint8_t**)(*(uint8_t**)((uint8_t*)resourceMgr + 0x80)) + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

        // Reset growth to 0
        fn_CheckPtr(comp + 0x28);
        *(uint32_t*)(*(uint8_t**)(comp + 0x28) + 0x28) = 0;

        // Clear growth flag byte
        fn_CheckPtr(comp + 0x28);
        *(uint8_t*)(*(uint8_t**)(comp + 0x28) + 0x2D) = 0;

        // Reset growth state
        fn_CheckPtr(comp + 0x28);
        fn_ResetGrowthState(*(void**)(comp + 0x28));

        // Clear flags bits 3,4 (mask ~0x18 = 0xFFFFFFE7)
        uint32_t oldFlags = *(uint32_t*)(comp + 0x20);
        uint32_t newFlags = oldFlags & 0xFFFFFFE7;
        *(uint32_t*)(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(void**)(comp + 0x10) != nullptr) {
            fn_CheckPtr(comp + 0x10);
            fn_FlagNotify(*(void**)(comp + 0x10));
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================

void InstallGrowthComponentUpdate() {
    // Resolve Runtime functions (called indirectly via exe dispatch table in original code)
    // Object::InternalBasePtr::CheckPtr @ Runtime 0x1800c0290
    fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));
    // System::Time::AsSeconds @ Runtime 0x180007900
    fn_Time_AsSeconds = reinterpret_cast<fn_Time_AsSeconds_t>(
        ResolveAddr(Module::Runtime, 0x180007900));
    // System::Time::Time(float) @ Runtime 0x1800078c0
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        ResolveAddr(Module::Runtime, 0x1800078c0));
    fn_FlagNotify = reinterpret_cast<fn_FlagNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    fn_SpawnSubResource = reinterpret_cast<fn_SpawnSubResource_t>(
        ResolveAddr(Module::Exe, 0x14011fba0));
    fn_SetResourceAmount = reinterpret_cast<fn_SetResourceAmount_t>(
        ResolveAddr(Module::Exe, 0x140120470));
    fn_GetResourceIndex = reinterpret_cast<fn_GetResourceIndex_t>(
        ResolveAddr(Module::Exe, 0x140120410));
    fn_ResetGrowthState = reinterpret_cast<fn_ResetGrowthState_t>(
        ResolveAddr(Module::Exe, 0x140126780));
    fn_ModifyFlags = reinterpret_cast<fn_ModifyFlags_t>(
        ResolveAddr(Module::Exe, 0x140118950));

    // Resolve globals
    g_pWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Resolve float constants
    g_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    g_fSixty = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    g_fFreezeTemp = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721d0));
    g_fFreezeGrowth = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172288));

    // Install hook
    uintptr_t target = ResolveAddr(Module::Exe, 0x140120050);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);

#ifdef _DEBUG
    printf("[GrowthComponent] Update hooked at %p (prologue=13)\n", (void*)target);
    printf("[GrowthComponent]   CheckPtr=%p AsSeconds=%p FlagNotify=%p\n",
           (void*)fn_CheckPtr, (void*)fn_Time_AsSeconds, (void*)fn_FlagNotify);
    printf("[GrowthComponent]   SpawnSub=%p SetResAmt=%p GetResIdx=%p\n",
           (void*)fn_SpawnSubResource, (void*)fn_SetResourceAmount, (void*)fn_GetResourceIndex);
    printf("[GrowthComponent]   WorldSingleton=%p -> %p\n",
           (void*)g_pWorldSingleton, g_pWorldSingleton ? *g_pWorldSingleton : nullptr);
#endif
}

void RemoveGrowthComponentUpdate() {
    RemoveHook(&g_hookUpdate);
}
