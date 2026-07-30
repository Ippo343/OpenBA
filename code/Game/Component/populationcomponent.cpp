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

#include "Component/populationcomponent_funcs.h"
#include "Component/herdcomponent_funcs.h"   // HerdComponent_Populate
#include "Object/factory_registration.h"     // Object::GetTypeIdByName
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

// Startup (herd-creation) deps.
// ComponentEntity_CopyCtor(mem, description) — 0x1400c3720
typedef void* (*fn_CopyCtor_t)(void* mem, int64_t description);
static fn_CopyCtor_t s_fn_CopyCtor = nullptr;
// System::Random::Rand(rng, range) via the exe dispatch slot 0x1401512e8
typedef int (*fn_Rand_t)(void* rng, int range);
static fn_Rand_t s_fn_Rand = nullptr;
// EntityList_RegisterEntity(list, &entityIbp, categoryId) — 0x1400c48a0
typedef void (*fn_RegisterEntity_t)(void* list, void* entityIbp, int categoryId);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

// Component find-loop over a freshly-instantiated entity (IBP array at +0x30,
// count at +0x38); match on the component's vtable+0x10 GetTypeId.
static void* PopulationFindComponent(int64_t entity, uint32_t typeId) {
    if (!entity || typeId == 0) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        uint32_t t = (*reinterpret_cast<uint32_t (**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (t != typeId) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* c = *slot;
        return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, typeId)) ? c : nullptr;
    }
    return nullptr;
}

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

    // Cell count is the density array's element count at +0x40 (the DynArray at
    // +0x38/+0x40/+0x44), NOT the herd grid dim at +0x48. Retail reads +0x40 for
    // both the per-tick slice size and the round-robin wrap.
    int totalCells = *reinterpret_cast<int32_t*>(&self->field40);
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
// Reimpl of FUN_14000b0d0 (PopulationComponent_sub_b0d0)
// DynArray resize for the 12-byte density cells: ptr at arr[0], count at
// arr+0x8, capacity at arr+0xc. Grows (tripling on a single-element append),
// copies the surviving prefix, zero-fills the tail, frees the old block.
// ============================================================
static void PopulationComponent_ResizeCells(int64_t* arr, int newCount) {
    int oldCount = static_cast<int>(arr[1]);
    if (newCount == oldCount) return;

    uint8_t* oldBuf = reinterpret_cast<uint8_t*>(arr[0]);
    *reinterpret_cast<int*>(&arr[1]) = newCount;
    if (*reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc) < newCount) {
        int* pCap = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc);
        *pCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *pCap = newCount * 3;
        arr[0] = reinterpret_cast<int64_t>(
            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)(*pCap) * 0xc, 4));
    }

    int start = (oldBuf == reinterpret_cast<uint8_t*>(arr[0])) ? oldCount : 0;
    for (int64_t i = start; i < newCount; i++) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(arr[0]) + i * 0xc;
        if (!dst) continue;
        if (i < oldCount) {
            const uint8_t* src = oldBuf + i * 0xc;
            *reinterpret_cast<int32_t*>(dst + 0) = *reinterpret_cast<const int32_t*>(src + 0);
            *reinterpret_cast<int32_t*>(dst + 4) = *reinterpret_cast<const int32_t*>(src + 4);
            *reinterpret_cast<int32_t*>(dst + 8) = *reinterpret_cast<const int32_t*>(src + 8);
        } else {
            *reinterpret_cast<int64_t*>(dst + 0) = 0;
            *reinterpret_cast<int32_t*>(dst + 8) = 0;
        }
    }

    if (oldBuf != reinterpret_cast<uint8_t*>(arr[0])) fn_MemFreeStatic(oldBuf);
}

// ============================================================
// Reimpl of FUN_140054220 — PopulationComponent::Startup (vtable slot 4).
// ============================================================
// Ghidra labels this PopulationComponent_Serialize; it isn't. It is the
// component's spawn-time body init, dispatched from
// ComponentEntity_StartupComponents while the "PopulationDeer" template entity
// is being instantiated, and it is the ONLY place wild herds come into
// existence:
//
//   +0x28 = the PopulationDescription (from the component's +0x18)
//   +0x30 = mapDim / popDesc+0x28   (habitability grid dimension)
//   +0x48 = mapDim / popDesc+0x2c   (herd grid dimension)
//   +0x38 = a (+0x30)^2 array of 12-byte cells {density, smoothed, min}
//   each cell scanned once, then smoothed/min seeded from density
//   then, if popDesc+0x50 (the herd entity template) is set, one herd entity
//   per cell of the (+0x48)^2 grid: instantiate the template, find its
//   HerdComponent, point it back at us (+0x28) and at its cell (+0x30), roll
//   a size in [herdDesc+0x2c, herdDesc+0x28), seed it via Populate, register.
static uint32_t PopulationTypeId(uintptr_t globalAddr, const char* name) {
    uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, globalAddr));
    if (!p) return 0;
    if (*p == 0) *p = ::Object::GetTypeIdByName(name);
    return *p;
}

void PopulationComponent_Startup(PopulationComponent* selfC) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfC);
    PopulationComponent_InitFuncs();
    if (!fn_IsDerivedFrom || !fn_MemAlloc || !fn_InternalBasePtr_Ctor) return;
    if (!s_pGameWorld || !*s_pGameWorld) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*s_pGameWorld);

    uint32_t tidPopDesc = PopulationTypeId(0x1401c47c8, "PopulationDescription");
    void* descObj = *reinterpret_cast<void**>(self + 0x18);
    void* popDesc = (tidPopDesc && fn_IsDerivedFrom(descObj, tidPopDesc)) ? descObj : nullptr;
    *reinterpret_cast<void**>(self + 0x28) = popDesc;
    if (!popDesc) {
        fprintf(stderr, "[Herd] PopulationComponent::Startup: self=%p has no "
                "PopulationDescription (+0x18=%p, tid=%u) — no herds will spawn\n",
                (void*)self, descObj, tidPopDesc);
        fflush(stderr);
        return;
    }

    int* mapDims = *reinterpret_cast<int**>(gw + 0xf8);
    if (!mapDims) return;
    int habBlock  = *reinterpret_cast<int*>((uintptr_t)popDesc + 0x28);
    int herdBlock = *reinterpret_cast<int*>((uintptr_t)popDesc + 0x2c);
    if (habBlock <= 0 || herdBlock <= 0) return;

    int habDim = *mapDims / habBlock;
    *reinterpret_cast<int32_t*>(self + 0x30) = habDim;
    *reinterpret_cast<int32_t*>(self + 0x48) = *mapDims / herdBlock;

    PopulationComponent_ResizeCells(reinterpret_cast<int64_t*>(self + 0x38), habDim * habDim);

    int cellCount = *reinterpret_cast<int32_t*>(self + 0x40);
    for (int i = 0; i < cellCount; i++) {
        PopulationComponent_ScanHabitability_Reimpl(self, mapDims, i);
        uint8_t* cell = *reinterpret_cast<uint8_t**>(self + 0x38) + (int64_t)i * 0xc;
        *reinterpret_cast<int32_t*>(cell + 4) = *reinterpret_cast<int32_t*>(cell + 0);
        *reinterpret_cast<int32_t*>(cell + 8) = *reinterpret_cast<int32_t*>(cell + 0);
        cellCount = *reinterpret_cast<int32_t*>(self + 0x40);
    }

    // Retail vtable slot 11 (0x140054210) is an empty function.
    {
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(self);
        auto slot11 = *reinterpret_cast<void (**)(void*)>(vt + 0x58);
        if (slot11) slot11(self);
    }

    int64_t herdTemplate = *reinterpret_cast<int64_t*>((uintptr_t)popDesc + 0x50);
    int herdDim = *reinterpret_cast<int32_t*>(self + 0x48);
    int herdCount = herdDim * herdDim;
    fprintf(stderr, "[Herd] PopulationComponent::Startup: self=%p mapDim=%d "
            "habDim=%d cells=%d herdDim=%d herds=%d template=%p\n",
            (void*)self, *mapDims, habDim, cellCount, herdDim,
            herdTemplate ? herdCount : 0, (void*)herdTemplate);
    fflush(stderr);
    if (herdTemplate == 0) return;

    uint32_t tidHerdComp = PopulationTypeId(0x1401c7820, "HerdComponent");
    uint32_t tidHerdDesc = PopulationTypeId(0x1401c7858, "HerdDescription");
    void* rng = *reinterpret_cast<void**>(gw + 0xd8);
    void* entityList = *reinterpret_cast<void**>(gw + 0xe8);

    int created = 0;
    for (int i = 0; i < herdCount; i++) {
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
        void* ent = (mem && s_fn_CopyCtor) ? s_fn_CopyCtor(mem, herdTemplate) : nullptr;
        uint64_t entRef = 0;
        fn_InternalBasePtr_Ctor(&entRef, ent);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        int64_t entObj = static_cast<int64_t>(entRef);
        if (!entObj) continue;

        void* herdComp = PopulationFindComponent(entObj, tidHerdComp);
        uint64_t herdRef = 0;
        fn_InternalBasePtr_Ctor(&herdRef, herdComp);
        uint64_t selfRef = 0;
        fn_InternalBasePtr_Ctor(&selfRef, self);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&herdRef);
        uint8_t* herd = reinterpret_cast<uint8_t*>(herdRef);
        if (!herd) continue;

        *reinterpret_cast<int32_t*>(herd + 0x30) = i;
        *reinterpret_cast<uint64_t*>(herd + 0x28) = selfRef;

        void* hDescObj = *reinterpret_cast<void**>(herd + 0x18);
        void* herdDesc = (tidHerdDesc && fn_IsDerivedFrom(hDescObj, tidHerdDesc))
                             ? hDescObj : nullptr;
        int size = 0;
        if (herdDesc) {
            int maxSize = *reinterpret_cast<int*>((uintptr_t)herdDesc + 0x28);
            int minSize = *reinterpret_cast<int*>((uintptr_t)herdDesc + 0x2c);
            size = (s_fn_Rand ? s_fn_Rand(rng, maxSize - minSize) : 0) + minSize;
        }
        HerdComponent_Populate(reinterpret_cast<int64_t>(herd), size);

        if (s_fn_RegisterEntity) {
            uint64_t regRef = entRef;
            s_fn_RegisterEntity(entityList, &regRef, *reinterpret_cast<int*>(herdTemplate + 0x20));
        }
        created++;
    }
    fprintf(stderr, "[Herd] PopulationComponent::Startup: %d/%d herd entities created\n",
            created, herdCount);
    fflush(stderr);
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

    s_fn_CopyCtor = reinterpret_cast<fn_CopyCtor_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    uintptr_t randSlot = ResolveAddr(Module::Exe, 0x1401512e8);
    if (randSlot) s_fn_Rand = *reinterpret_cast<fn_Rand_t*>(randSlot);
    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));

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

// Live bring-up (env BANISHED_LIVE_POPULATION or global BANISHED_LIVE_COMPONENTS).
// Slot 4 Startup does the body init retail does at spawn — description at +0x28,
// habitability grid at +0x38/+0x40, herd grid dim at +0x48 — and, crucially,
// creates the wild-animal herd entities. Slot 9 Update then has the +0x28 owner
// and +0x40 cell count it needs for real density simulation.
//
// Pair with BANISHED_LIVE_HERD so the herds it creates actually tick.
COMPONENT_LIVE_FACTORY_S(PopulationComponent, 0x70, 0xCB,
                         "BANISHED_LIVE_POPULATION",
                         PopulationComponent_Update, PopulationComponent_Startup)
REGISTER_TYPE(PopulationComponent, "Component",
              &PopulationComponent_Factory, &PopulationComponent_CreateInPlace);
