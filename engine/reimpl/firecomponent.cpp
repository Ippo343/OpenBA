// engine/reimpl/firecomponent.cpp
// Reimplementation of FireComponent::Update from Application-x64.exe.
//
// Two-state fire system:
//   Active (field30 != 0): countdown timer decrements each tick. When spread
//     timer (field38) expires, spreads fire to neighbors. When main timer
//     (field30) expires, extinguishes.
//   Inactive (field30 == 0): accumulates time, randomly ignites based on
//     probability check against building data.
//
// Original: 0x140048fb0 (53 instructions)

#include "FireComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// FireComponent_Ignite(this) — sets fire active, initializes timers
// Located at 0x140049610 in Application-x64.exe
typedef void (*fn_FireComponent_Ignite_t)(void* self);
static fn_FireComponent_Ignite_t fn_Ignite = nullptr;

// FireComponent_SpreadToNeighbors(this) — spreads fire to adjacent buildings
// Located at 0x140049130 in Application-x64.exe
typedef void (*fn_FireComponent_SpreadToNeighbors_t)(void* self);
static fn_FireComponent_SpreadToNeighbors_t fn_SpreadToNeighbors = nullptr;

// FireComponent_Extinguish(this) — puts out the fire, resets state
// Located at 0x1400494c0 in Application-x64.exe
typedef void (*fn_FireComponent_Extinguish_t)(void* self);
static fn_FireComponent_Extinguish_t fn_Extinguish = nullptr;

// FUN_1400a9290 — probability check: accumulates time, rolls random vs threshold
// Returns true if random event triggers.
// Params: Time* accumulator, float* buildingFireData, Random* rng
typedef bool (*fn_ProbabilityCheck_t)(void* timeAccum, float* fireData, void* random);
static fn_ProbabilityCheck_t fn_ProbabilityCheck = nullptr;

// Game state singleton pointer (DAT_1401c3dd8)
// +0xD0: flags (bit 6 = fire disabled)
// +0xD8: Random* rng
static void** g_pGameState = nullptr;

// Type ID for building/structure component (DAT_1401c4218)
static uint32_t* g_pBuildingTypeId = nullptr;

// Object::Base::IsDerivedFrom(this, typeId) — already in engine_funcs.h
// fn_IsDerivedFrom is resolved globally

// ============================================================
// Update (0x140048fb0)
// Two-state fire: active (countdown + spread + extinguish) vs
// inactive (accumulate time + random fire-start probability).
//
// Parameters: RCX = this, RDX = &Time (int64*)
// ============================================================
static void Reimpl_Update(FireComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "FireComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[FireComponent::Update] self=%p dt=%lld fireTimer=%lld\n",
               (void*)self, *pDeltaTime, (long long)(int64_t)(uintptr_t)self->field30);
        callCount++;
    }
#endif

    int64_t dt = *pDeltaTime;
    int64_t* pFireTimer = (int64_t*)&self->field30;
    int64_t* pSpreadTimer = (int64_t*)&self->field38;
    int64_t* pAccumulator = (int64_t*)&self->field40;

    if (*pFireTimer == 0) {
        // Inactive state: check if fire should start

        // Check if fire is disabled in game flags (bit 6 of flags at +0xD0)
        if (g_pGameState && *g_pGameState) {
            uint32_t gameFlags = *(uint32_t*)((uintptr_t)*g_pGameState + 0xD0);
            if ((gameFlags >> 6) & 1) {
                return;  // Fire disabled
            }
        }

        // Get building data via IsDerivedFrom check on +0x18
        void* baseObj = self->_unk18;
        void* buildingData = nullptr;
        if (baseObj && fn_IsDerivedFrom && g_pBuildingTypeId) {
            bool isDerived = fn_IsDerivedFrom(baseObj, *g_pBuildingTypeId);
            if (isDerived) {
                buildingData = baseObj;
            }
        }

        // Accumulate time and check random fire probability
        *pAccumulator += dt;

        if (fn_ProbabilityCheck && buildingData && g_pGameState && *g_pGameState) {
            void* rng = *(void**)((uintptr_t)*g_pGameState + 0xD8);
            float* fireData = (float*)((uintptr_t)buildingData + 0x44);
            bool shouldIgnite = fn_ProbabilityCheck(pAccumulator, fireData, rng);
            if (shouldIgnite) {
                if (fn_Ignite) {
                    fn_Ignite(self);
                }
                return;
            }
        }
    } else {
        // Active state: count down timers
        *pFireTimer -= dt;
        *pSpreadTimer -= dt;

        // If fire still burning but spread timer expired, spread to neighbors
        if (*pFireTimer > 0 && *pSpreadTimer < 1) {
            if (fn_SpreadToNeighbors) {
                fn_SpreadToNeighbors(self);
            }
        }

        // If main fire timer expired, extinguish
        if (*pFireTimer < 1) {
            if (fn_Extinguish) {
                fn_Extinguish(self);
            }
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFireComponentUpdate() {
    // Resolve helper functions in the exe
    fn_Ignite = reinterpret_cast<fn_FireComponent_Ignite_t>(
        ResolveAddr(Module::Exe, 0x140049610));
    fn_SpreadToNeighbors = reinterpret_cast<fn_FireComponent_SpreadToNeighbors_t>(
        ResolveAddr(Module::Exe, 0x140049130));
    fn_Extinguish = reinterpret_cast<fn_FireComponent_Extinguish_t>(
        ResolveAddr(Module::Exe, 0x1400494c0));
    fn_ProbabilityCheck = reinterpret_cast<fn_ProbabilityCheck_t>(
        ResolveAddr(Module::Exe, 0x1400a9290));

    // Game state singleton (pointer stored at 0x1401c3dd8)
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Building type ID (at 0x1401c4218)
    g_pBuildingTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4218));

#ifdef _DEBUG
    printf("[FireComponent] Ignite = %p\n", (void*)fn_Ignite);
    printf("[FireComponent] SpreadToNeighbors = %p\n", (void*)fn_SpreadToNeighbors);
    printf("[FireComponent] Extinguish = %p\n", (void*)fn_Extinguish);
    printf("[FireComponent] ProbabilityCheck = %p\n", (void*)fn_ProbabilityCheck);
    printf("[FireComponent] GameState = %p\n", (void*)g_pGameState);
    printf("[FireComponent] BuildingTypeId = %p\n", (void*)g_pBuildingTypeId);
#endif

    if (!fn_Ignite || !fn_SpreadToNeighbors || !fn_Extinguish || !fn_ProbabilityCheck) {
#ifdef _DEBUG
        printf("[FireComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140048fb0);
    // Prologue bytes:
    //   0: MOV [RSP+0x10],RSI (5)
    //   5: PUSH RDI (1)
    //   6: SUB RSP,0x20 (4)
    //  10: MOV RAX,[RCX+0x30] (4)
    // = 14 bytes, clean instruction boundary
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 14);
#ifdef _DEBUG
    printf("[FireComponent] Update hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveFireComponentUpdate() { RemoveHook(&g_hookUpdate); }
