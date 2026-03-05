// engine/reimpl/populationcomponent.cpp
// Reimplementation of PopulationComponent::Update from Application-x64.exe.
//
// The Update method handles two tasks:
//   1. Density smoothing: accumulates time at +0x60. Every 300 game-seconds,
//      calls SmoothDensity to apply a weighted 5x5 kernel over the population grid.
//   2. Habitability scanning: processes totalCells/16 cells per tick in a
//      round-robin fashion, calling ScanHabitability for each cell. After scanning,
//      clamps smoothed density values to current density.
//
// Grid layout at +0x38: array of 12-byte entries per cell:
//   +0x00: int density (current population count)
//   +0x04: int smoothedDensity (result of kernel smoothing)
//   +0x08: int minDensity (tracking value, clamped to density)
//
// Original: 0x140054070 (256 bytes)

#include "PopulationComponent.h"
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

// PopulationComponent_SmoothDensity(this)
// Located at 0x1400538c0 in Application-x64.exe.
// Applies a weighted 5x5 kernel to smooth population density across the grid.
// Allocates a temp buffer, computes weighted averages, copies back to grid.
typedef void (*fn_SmoothDensity_t)(void* self);
static fn_SmoothDensity_t fn_SmoothDensity = nullptr;

// PopulationComponent_ScanHabitability(this, terrainGrid*, cellIndex)
// Located at 0x140053b50 in Application-x64.exe.
// Scans a grid cell's terrain tiles for habitability, writing result to grid[cellIndex].
typedef void (*fn_ScanHabitability_t)(void* self, void* terrainGrid, int cellIndex);
static fn_ScanHabitability_t fn_ScanHabitability = nullptr;

// System::Time::Time(outTime*, float seconds)
// System::Time::Time(float) — now resolved globally in engine_funcs.h

// ============================================================
// Globals
// ============================================================

// Game world/gameplay singleton at 0x1401c3dd8 in the exe.
// Read at +0xf8 to get the terrain grid pointer for ScanHabitability.
static void** g_pGameWorld = nullptr;

// Density smoothing interval constant: 300.0f seconds (at 0x140172234)
static const float SMOOTH_INTERVAL_SECONDS = 300.0f;

// ============================================================
// Update (0x140054070)
// Accumulates deltaTime. When accumulated time exceeds the smoothing
// interval (300s), runs SmoothDensity in a loop (draining accumulated time).
// Then scans habitability for totalCells/16 cells per tick.
//
// Parameters: RCX = this, RDX = Time* (pointer to int64)
// ============================================================
static void Reimpl_Update(PopulationComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "PopulationComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[PopulationComponent::Update] self=%p dt=%lld totalCells=%d\n",
               (void*)self, *pDeltaTime, self->field48);
        callCount++;
    }
#endif

    // Accumulate delta time into +0x60
    int64_t* pAccum = reinterpret_cast<int64_t*>(&self->field60);
    *pAccum += *pDeltaTime;

    // Convert smoothing interval (300.0f seconds) to engine ticks
    int64_t smoothInterval;
    fn_Time_Ctor_Float(&smoothInterval, SMOOTH_INTERVAL_SECONDS);

    // Run SmoothDensity for each full interval accumulated
    if (*pAccum > smoothInterval) {
        do {
            fn_SmoothDensity(self);
            *pAccum -= smoothInterval;
        } while (*pAccum > smoothInterval);
    }

    // Scan habitability: process totalCells/16 cells per tick
    // owner is at +0x28 (field28 in our struct); check byte at owner+0x48 != 0
    uint8_t* ownerPtr = reinterpret_cast<uint8_t*>(self->field28);
    if (ownerPtr == nullptr || *(ownerPtr + 0x48) == 0) {
        return;
    }

    // totalCells at +0x40 (field48 in our struct, mapped as uint32_t)
    int totalCells = static_cast<int>(self->field48);

    // cellsToProcess = (totalCells + (totalCells >> 31 & 0xF)) >> 4
    // This is equivalent to signed division by 16 with rounding toward zero
    int cellsToProcess = (totalCells + ((totalCells >> 31) & 0xF)) >> 4;

    if (cellsToProcess <= 0) {
        return;
    }

    // Get the terrain grid from the game world global
    void* gameWorld = *g_pGameWorld;
    void* terrainGrid = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(gameWorld) + 0xf8);

    // Current scan index at +0x68
    int32_t* pScanIndex = reinterpret_cast<int32_t*>(&self->field68);

    // Grid data at +0x38
    uint8_t* gridData = reinterpret_cast<uint8_t*>(self->field38);

    for (int i = 0; i < cellsToProcess; i++) {
        int idx = *pScanIndex;

        // Scan this cell's habitability
        fn_ScanHabitability(self, terrainGrid, idx);

        // After scanning, clamp smoothedDensity and minDensity to density
        // Grid entry = 12 bytes: [density(4), smoothedDensity(4), minDensity(4)]
        int32_t* cellBase = reinterpret_cast<int32_t*>(gridData + (int64_t)idx * 0xc);
        int density = cellBase[0];

        // Clamp smoothedDensity (cellBase[1]) to density
        if (cellBase[1] > density) {
            cellBase[1] = density;
        }

        // Clamp minDensity (cellBase[2]) to density
        if (cellBase[2] > density) {
            cellBase[2] = density;
        }

        // Advance scan index with wrap-around
        *pScanIndex = *pScanIndex + 1;
        if (*pScanIndex == totalCells) {
            *pScanIndex = 0;
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPopulationComponentUpdate() {
    // Resolve helper functions in the exe
    fn_SmoothDensity = reinterpret_cast<fn_SmoothDensity_t>(
        ResolveAddr(Module::Exe, 0x1400538c0));

    fn_ScanHabitability = reinterpret_cast<fn_ScanHabitability_t>(
        ResolveAddr(Module::Exe, 0x140053b50));

    // fn_Time_Ctor_Float — now resolved globally in EngineFuncsInit()

    // Resolve game world global
    g_pGameWorld = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));

#ifdef _DEBUG
    printf("[PopulationComponent] SmoothDensity = %p\n", (void*)fn_SmoothDensity);
    printf("[PopulationComponent] ScanHabitability = %p\n", (void*)fn_ScanHabitability);
    printf("[PopulationComponent] Time::Time(float) = %p\n", (void*)fn_Time_Ctor_Float);
    printf("[PopulationComponent] g_pGameWorld = %p -> %p\n",
           (void*)g_pGameWorld, g_pGameWorld ? *g_pGameWorld : nullptr);
#endif

    if (!fn_SmoothDensity || !fn_ScanHabitability || !fn_Time_Ctor_Float) {
#ifdef _DEBUG
        printf("[PopulationComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140054070);
    // Prologue: PUSH RBX (1) + SUB RSP,0x20 (4) + MOV RAX,[RDX] (3) +
    //           MOVSS XMM1,[rip+...] (8) + MOV RBX,RCX (3) = 19 bytes.
    // Next instruction (ADD [RCX+0x60],RAX) at offset 20 is a clean boundary.
    // Note: MOVSS at offset 9 is RIP-relative -- trampoline NOT usable.
    // We don't call the trampoline, so this is safe.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[PopulationComponent] Update hooked at %p (prologue=17)\n", (void*)target);
#endif
}

void RemovePopulationComponentUpdate() { RemoveHook(&g_hookUpdate); }
