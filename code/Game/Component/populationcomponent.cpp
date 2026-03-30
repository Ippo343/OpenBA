// code/Game/Component/populationcomponent.cpp
// Clean reimplementation of PopulationComponent::Update from Application-x64.exe.
//
// The Update method handles two tasks:
//   1. Density smoothing: accumulates time at +0x60. Every 300 game-seconds,
//      calls SmoothDensity to apply a weighted 5x5 kernel over the population grid.
//   2. Habitability scanning: processes totalCells/16 cells per tick in a
//      round-robin fashion, calling ScanHabitability for each cell. After scanning,
//      clamps smoothed density values to current density.
//
// Original: 0x140054070 (256 bytes)
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/populationcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// PopulationComponent_SmoothDensity(this) — 0x1400538c0
typedef void (*fn_SmoothDensity_t)(void* self);
static fn_SmoothDensity_t s_fn_SmoothDensity = nullptr;

// PopulationComponent_ScanHabitability(this, terrainGrid*, cellIndex) — 0x140053b50
typedef void (*fn_ScanHabitability_t)(void* self, void* terrainGrid, int cellIndex);
static fn_ScanHabitability_t s_fn_ScanHabitability = nullptr;

// Game world singleton
static void** s_pGameWorld = nullptr;

// Density smoothing interval constant: 300.0f seconds
static constexpr float SMOOTH_INTERVAL_SECONDS = 300.0f;

static bool s_initialized = false;

// ============================================================
// PopulationComponent::Update (0x140054070)
// ============================================================
void PopulationComponent_Update(PopulationComponent* self, int64_t* pDeltaTime) {
    // Accumulate delta time into +0x60
    int64_t* pAccum = reinterpret_cast<int64_t*>(&self->field60);
    *pAccum += *pDeltaTime;

    // Convert smoothing interval (300.0f seconds) to engine ticks
    int64_t smoothInterval;
    if (fn_Time_Ctor_Float)
        fn_Time_Ctor_Float(&smoothInterval, SMOOTH_INTERVAL_SECONDS);

    // Run SmoothDensity for each full interval accumulated
    if (s_fn_SmoothDensity && *pAccum > smoothInterval) {
        do {
            s_fn_SmoothDensity(self);
            *pAccum -= smoothInterval;
        } while (*pAccum > smoothInterval);
    }

    // Scan habitability: check owner at +0x28, byte at owner+0x48
    uint8_t* ownerPtr = reinterpret_cast<uint8_t*>(self->field28);
    if (ownerPtr == nullptr || *(ownerPtr + 0x48) == 0) {
        return;
    }

    int totalCells = static_cast<int>(self->field48);
    // Signed division by 16 with rounding toward zero
    int cellsToProcess = (totalCells + ((totalCells >> 31) & 0xF)) >> 4;
    if (cellsToProcess <= 0) return;

    // Get terrain grid from game world global
    if (!s_pGameWorld || !*s_pGameWorld) return;
    void* gameWorld = *s_pGameWorld;
    void* terrainGrid = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(gameWorld) + 0xf8);

    int32_t* pScanIndex = reinterpret_cast<int32_t*>(&self->field68);
    uint8_t* gridData = reinterpret_cast<uint8_t*>(self->field38);

    for (int i = 0; i < cellsToProcess; i++) {
        int idx = *pScanIndex;

        // Scan habitability
        if (s_fn_ScanHabitability)
            s_fn_ScanHabitability(self, terrainGrid, idx);

        // Clamp smoothedDensity and minDensity to density
        // Grid entry = 12 bytes: [density(4), smoothedDensity(4), minDensity(4)]
        int32_t* cellBase = reinterpret_cast<int32_t*>(gridData + (int64_t)idx * 0xc);
        int density = cellBase[0];

        if (cellBase[1] > density) cellBase[1] = density;
        if (cellBase[2] > density) cellBase[2] = density;

        // Advance scan index with wrap-around
        *pScanIndex = *pScanIndex + 1;
        if (*pScanIndex == totalCells) *pScanIndex = 0;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void PopulationComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_SmoothDensity = reinterpret_cast<fn_SmoothDensity_t>(
        ResolveAddr(Module::Exe, 0x1400538c0));
    s_fn_ScanHabitability = reinterpret_cast<fn_ScanHabitability_t>(
        ResolveAddr(Module::Exe, 0x140053b50));
    s_pGameWorld = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;
}
