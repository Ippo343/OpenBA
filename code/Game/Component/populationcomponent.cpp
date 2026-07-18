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
#include <cstdlib>
#include <cstring>

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

    // Phase-1 bring-up verification trace (env BANISHED_POP_TRACE, default off →
    // byte-unchanged): follows ONE PopulationComponent and prints its accumulating
    // +0x60 timer to confirm the live recipe routes a second component through +0x48.
    {
        static int s_st = -1, s_n = 0;
        static void* s_lock = nullptr;
        if (s_st < 0) {
            const char* e = std::getenv("BANISHED_POP_TRACE");
            s_st = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_st == 1 && s_lock == nullptr) s_lock = self;
        if (s_st == 1 && self == s_lock && s_n < 12) {
            fprintf(stderr, "[PopTrace] self=%p accum(+60)=%lld owner(+28)=%p\n",
                    (void*)self, (long long)*pAccum, (void*)self->field28);
            fflush(stderr); s_n++;
        }
    }

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
// Reimpl of FUN_1400538c0 (PopulationComponent::SmoothDensity)
// Weighted 5x5 kernel smoothing pass over the population density grid.
// Self-contained: only MemAlloc/MemFree, pure pointer arithmetic.
// Registered so populationcomponent.cpp's s_fn_SmoothDensity fires
// (was silently skipped when unregistered in standalone).
// ============================================================
void PopulationComponent_SmoothDensity_Reimpl(void* selfV) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);

    // Phase 1: per-grid-cell, accumulate owner birthrate (+0x40) into smoothedDensity,
    //          then clamp smoothedDensity down to density.
    int count40 = *reinterpret_cast<int*>(self + 0x40);
    {
        int i = 0;
        int64_t off = 0;
        if (0 < count40) {
            do {
                uint8_t* grid = *reinterpret_cast<uint8_t**>(self + 0x38);
                int* pSmoothed = reinterpret_cast<int*>(grid + 4 + off);
                int owner = *reinterpret_cast<int*>(
                    *reinterpret_cast<uint8_t**>(self + 0x28) + 0x40);
                *pSmoothed = *pSmoothed + owner;
                uint8_t* grid2 = *reinterpret_cast<uint8_t**>(self + 0x38);
                int density = *reinterpret_cast<int*>(grid2 + off);
                if (density < *reinterpret_cast<int*>(grid2 + 4 + off)) {
                    *reinterpret_cast<int*>(grid2 + 4 + off) = density;
                }
                i++;
                off += 0xc;
            } while (i < *reinterpret_cast<int*>(self + 0x40));
        }
    }

    // Allocate temp buffer (one int per grid cell at +0x40 count).
    int* temp = nullptr;
    int n40 = *reinterpret_cast<int*>(self + 0x40);
    if (n40 != 0) {
        if (0 < n40) {
            temp = reinterpret_cast<int*>(
                fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n40 * 4, 4));
        }
        // Faithful port of the (quirky) zero-init loop from the original.
        for (int64_t k = 0; k < (int64_t)n40; k++) {
            if (temp != nullptr) {
                int v;
                if (k < 0) {
                    v = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(&temp[k]) -
                                               reinterpret_cast<uint8_t*>(temp))[0];
                } else {
                    v = 0;
                }
                temp[k] = v;
            }
        }
        // Original frees a null pointer here (no-op); preserved for fidelity.
        if (temp != nullptr) {
            fn_MemFreeStatic((void*)0);
        }
    }

    // Phase 2: 5x5 distance-weighted kernel over the density grid (dim at +0x30).
    int dim = *reinterpret_cast<int*>(self + 0x30);
    if (0 < dim) {
        int row = 0;
        int64_t rowL = 0;
        do {
            int col = 0;
            int64_t colL = 0;
            if (0 < dim) {
                do {
                    int acc = 0;
                    int wsum = 0;
                    int ky = -2;
                    int64_t kyL = rowL - 2;
                    do {
                        int kx = -2;
                        int64_t kxL = colL - 2;
                        do {
                            if (kxL >= 0 && (kx + col) < dim && kyL >= 0 && (ky + row) < dim) {
                                int* cell = reinterpret_cast<int*>(
                                    *reinterpret_cast<uint8_t**>(self + 0x38) +
                                    (dim * kyL + kxL) * 0xc);
                                if (*cell != 0) {
                                    int w = (0x10 - kx * kx) - ky * ky;
                                    if (-1 < w) {
                                        acc += cell[1] * w;
                                        wsum += w;
                                    }
                                }
                            }
                            kx++;
                            kxL++;
                        } while (kx < 3);
                        ky++;
                        kyL++;
                    } while (ky < 3);
                    if (wsum == 0) {
                        temp[dim * rowL + colL] = 0;
                    } else {
                        temp[dim * rowL + colL] = acc / wsum;
                    }
                    col++;
                    colL++;
                    dim = *reinterpret_cast<int*>(self + 0x30);
                } while (col < dim);
            }
            row++;
            rowL++;
            dim = *reinterpret_cast<int*>(self + 0x30);
        } while (row < dim);
    }

    // Phase 3: write smoothed values back into smoothedDensity (+4) and minDensity (+8).
    int n40b = *reinterpret_cast<int*>(self + 0x40);
    if (0 < n40b) {
        int i = 0;
        int64_t off = 0;
        int* t = temp;
        do {
            uint8_t* grid = *reinterpret_cast<uint8_t**>(self + 0x38);
            *reinterpret_cast<int*>(grid + off + 4) = *t;
            *reinterpret_cast<int*>(grid + off + 8) = *t;
            i++;
            t++;
            off += 0xc;
        } while (i < *reinterpret_cast<int*>(self + 0x40));
    }

    fn_MemFreeStatic(temp);
}

// ============================================================
// Reimpl of FUN_140053b50 (PopulationComponent::ScanHabitability)
// Scans one round-robin block of terrain cells, OR-ing habitability
// flags + counting walkable cells; writes the result density into the grid.
// Pure pointer arithmetic, no sub-deps.
// ============================================================
void PopulationComponent_ScanHabitability_Reimpl(void* selfV, void* terrainGridV, int cellIndex) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    int* terrainGrid = reinterpret_cast<int*>(terrainGridV);

    uint8_t* scan = *reinterpret_cast<uint8_t**>(self + 0x28);
    int walkable = 0;
    int blockDim = *reinterpret_cast<int*>(scan + 0x28);
    unsigned int orFlags = 0;
    int gridDim = *reinterpret_cast<int*>(self + 0x30);
    int rowStart = (cellIndex / gridDim) * blockDim;
    int colStart = (cellIndex % gridDim) * blockDim;
    int rowEnd = blockDim + rowStart;

    if (rowStart < rowEnd) {
        int r = rowStart;
        do {
            if (colStart < blockDim + colStart) {
                int64_t* tile = reinterpret_cast<int64_t*>(
                    (int64_t)(terrainGrid[0] * r + colStart) * 0x30 +
                    *reinterpret_cast<int64_t*>(terrainGrid + 0x20));
                uint64_t remaining = (uint64_t)(unsigned int)((blockDim + colStart) - colStart);
                do {
                    unsigned int bit = 1u << (*reinterpret_cast<uint8_t*>(
                                                 reinterpret_cast<uint8_t*>(tile) + 0x29) & 0x1f);
                    if ((bit & *reinterpret_cast<unsigned int*>(scan + 0x30)) != 0) {
                        walkable++;
                    }
                    orFlags |= *reinterpret_cast<unsigned int*>(scan + 0x34) & bit;
                    if (*reinterpret_cast<char*>(scan + 0x38) != '\0' && *tile != 0) {
                        orFlags |= 1;
                    }
                    tile += 6;  // 0x30 bytes
                    remaining--;
                } while (remaining != 0);
            }
            r++;
        } while (r < rowEnd);

        if (orFlags == 0 && 0 < walkable) {
            *reinterpret_cast<int*>(
                *reinterpret_cast<uint8_t**>(self + 0x38) + (int64_t)cellIndex * 0xc) =
                *reinterpret_cast<int*>(scan + 0x3c);
            return;
        }
    }
    *reinterpret_cast<int*>(
        *reinterpret_cast<uint8_t**>(self + 0x38) + (int64_t)cellIndex * 0xc) = 0;
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

// ============================================================
// Standalone factory (Foundation D — component instantiation)
// ============================================================
// In the standalone exe the original is not mapped, so the engine's
// Object::Factory::CreateObject(typeId) must resolve to a C++ factory here — the
// component analogue of the Foundation C description factories. Without it the
// type stays NullFactory and ComponentEntity_CtorFromDescription gets a null
// component (silently) — the entity spawns but is empty.
//
// Produces an inert-but-valid component via the shared infrastructure (see
// component_factory_base.h): correct size (0x70) + Object::Internal base + shared
// stub vtable + the real capability flags (+0x20 = 0xCB, from PopulationComponent_Ctor
// 0x1400546d0) + zeroed body. The component still registers in the world entity
// list and its update/draw buckets; the stubbed Update/Draw are safe no-ops until
// the real methods get true standalone reimpls.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Live bring-up (Phase 1, env BANISHED_LIVE_POPULATION or global BANISHED_LIVE_COMPONENTS).
// PopulationComponent_Update is crash-safe on an inert body: +0x60 accumulates
// unconditionally (observable), SmoothDensity is a no-op while the cell count
// (+0x40) is 0, and the habitability scan early-returns on the null owner (+0x28).
// Full density simulation needs the +0x28 owner / +0x38 grid / +0x40 count body-init.
COMPONENT_LIVE_FACTORY(PopulationComponent, 0x70, 0xCB,
                       "BANISHED_LIVE_POPULATION", PopulationComponent_Update)
REGISTER_TYPE(PopulationComponent, "Component",
              &PopulationComponent_Factory, &PopulationComponent_CreateInPlace);
