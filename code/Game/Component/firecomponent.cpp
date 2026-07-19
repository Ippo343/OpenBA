// code/Game/Component/firecomponent.cpp
// Clean reimplementation of FireComponent::Update from Application-x64.exe.
//
// Two-state fire system:
//   Active (field30 != 0): countdown timer decrements each tick. When spread
//     timer (field38) expires, spreads fire to neighbors. When main timer
//     (field30) expires, extinguishes.
//   Inactive (field30 == 0): accumulates time, randomly ignites based on
//     probability check against building data.
//
// Original: 0x140048fb0 (53 instructions)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/firecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// FireComponent_Ignite(this) -- sets fire active, initializes timers (0x140049610)
typedef void (*fn_FireComponent_Ignite_t)(void* self);
static fn_FireComponent_Ignite_t s_fn_Ignite = nullptr;

// FireComponent_SpreadToNeighbors(this) -- spreads fire to adjacent buildings (0x140049130)
typedef void (*fn_FireComponent_SpreadToNeighbors_t)(void* self);
static fn_FireComponent_SpreadToNeighbors_t s_fn_SpreadToNeighbors = nullptr;

// FireComponent_Extinguish(this) -- puts out the fire, resets state (0x1400494c0)
typedef void (*fn_FireComponent_Extinguish_t)(void* self);
static fn_FireComponent_Extinguish_t s_fn_Extinguish = nullptr;

// FUN_1400a9290 -- probability check: accumulates time, rolls random vs threshold
// Returns true if random event triggers.
typedef bool (*fn_ProbabilityCheck_t)(void* timeAccum, float* fireData, void* random);
static fn_ProbabilityCheck_t s_fn_ProbabilityCheck = nullptr;

// Game state singleton pointer (DAT_1401c3dd8)
// +0xD0: flags (bit 6 = fire disabled)
// +0xD8: Random* rng
static void** s_pGameState = nullptr;

// Type ID at DAT_1401c4218 = FireDescription's BaseTypeInfo (writer
// FireDescription_Register 0x14014ac25) — retail FireComponent_* validate their
// description against it. (Older comment said "building/structure component";
// other files that resolved c4218 as "ComponentEntity" were mistranscriptions —
// the real ComponentEntity id is 0x1401c5d70.)
static uint32_t* s_pFireDescTypeId = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Ignite = reinterpret_cast<fn_FireComponent_Ignite_t>(
        ResolveAddr(Module::Exe, 0x140049610));
    s_fn_SpreadToNeighbors = reinterpret_cast<fn_FireComponent_SpreadToNeighbors_t>(
        ResolveAddr(Module::Exe, 0x140049130));
    s_fn_Extinguish = reinterpret_cast<fn_FireComponent_Extinguish_t>(
        ResolveAddr(Module::Exe, 0x1400494c0));
    s_fn_ProbabilityCheck = reinterpret_cast<fn_ProbabilityCheck_t>(
        ResolveAddr(Module::Exe, 0x1400a9290));

    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pFireDescTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4218));

    s_initialized = true;
}

// ============================================================
// FireComponent::Update (vtable slot [9], 0x140048fb0)
// ============================================================
// Two-state fire: active (countdown + spread + extinguish) vs
// inactive (accumulate time + random fire-start probability).

void FireComponent_Update(FireComponent* self, int64_t* pDeltaTime) {
    EnsureHelpers();

    int64_t dt = *pDeltaTime;
    int64_t* pFireTimer = (int64_t*)&self->field30;
    int64_t* pSpreadTimer = (int64_t*)&self->field38;
    int64_t* pAccumulator = (int64_t*)&self->field40;

    if (*pFireTimer == 0) {
        // Inactive state: check if fire should start

        // Check if fire is disabled in game flags (bit 6 of flags at +0xD0)
        if (s_pGameState && *s_pGameState) {
            uint32_t gameFlags = *(uint32_t*)((uintptr_t)*s_pGameState + 0xD0);
            if ((gameFlags >> 6) & 1) {
                return;  // Fire disabled
            }
        }

        // Get building data via IsDerivedFrom check on +0x18
        void* baseObj = self->_unk18;
        void* buildingData = nullptr;
        if (baseObj && fn_IsDerivedFrom && s_pFireDescTypeId) {
            bool isDerived = fn_IsDerivedFrom(baseObj, *s_pFireDescTypeId);
            if (isDerived) {
                buildingData = baseObj;
            }
        }

        // Accumulate time and check random fire probability
        *pAccumulator += dt;

        if (s_fn_ProbabilityCheck && buildingData && s_pGameState && *s_pGameState) {
            void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
            float* fireData = (float*)((uintptr_t)buildingData + 0x44);
            bool shouldIgnite = s_fn_ProbabilityCheck(pAccumulator, fireData, rng);
            if (shouldIgnite) {
                if (s_fn_Ignite) {
                    s_fn_Ignite(self);
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
            if (s_fn_SpreadToNeighbors) {
                s_fn_SpreadToNeighbors(self);
            }
        }

        // If main fire timer expired, extinguish
        if (*pFireTimer < 1) {
            if (s_fn_Extinguish) {
                s_fn_Extinguish(self);
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FireComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FireComponent] Funcs resolved: Ignite=%p Spread=%p Extinguish=%p ProbCheck=%p\n",
           (void*)s_fn_Ignite, (void*)s_fn_SpreadToNeighbors,
           (void*)s_fn_Extinguish, (void*)s_fn_ProbabilityCheck);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(FireComponent, 0x48, 0x88)
REGISTER_TYPE(FireComponent, "Component",
              &FireComponent_Factory, &FireComponent_CreateInPlace);
