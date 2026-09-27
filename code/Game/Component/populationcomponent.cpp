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
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"     // Object::GetTypeIdByName
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

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
// EntityList_RegisterEntity(list, &entityIbp, categoryId) — 0x1400c48a0
typedef void (*fn_RegisterEntity_t)(void* list, void* entityIbp, int categoryId);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

// Town-registry helpers used by the OnAdd/OnRemove pair below.
// DynArray8_ResizeFill(arrayField, newCount, valueIbp) — 0x140003110, converted
// to GameColl::AsArray<GameColl::Ptr8>(h).SetSizeFill(n, fill) (ENDGAME B.3.2
// step 2); DynArray8_EraseByValue(arrayField, valueIbp) — 0x140002cc0.
typedef void (*fn_ArrErase_t)(int64_t* arrayField, void* valueIbp);
static fn_ArrErase_t      s_fn_ArrErase      = nullptr;

// Component find-loop over a freshly-instantiated entity (IBP array at +0x30,
// count at +0x38); match on the component's vtable+0x10 GetTypeId.
static void* PopulationFindComponent(int64_t entity, uint32_t typeId) {
    if (!entity || typeId == 0) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + (int64_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (t != typeId) continue;
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
void PopulationComponent_ResizeCells(int64_t* arr, int newCount) {
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
        new (&entRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(ent));   // B.3.3 bridge
        int64_t entObj = static_cast<int64_t>(entRef);
        if (!entObj) continue;

        void* herdComp = PopulationFindComponent(entObj, tidHerdComp);
        uint64_t herdRef = 0;
        new (&herdRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(herdComp));   // B.3.3 bridge
        uint64_t selfRef = 0;
        new (&selfRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
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
            size = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(maxSize - minSize)) + minSize;   // B.3.4b bridge
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
// PopulationComponent::OnAdd (vtable slot 6) — retail 0x1400541c0
// ============================================================
// Ghidra names this one "PopulationComponent_Draw"; it draws nothing. It is the
// town-registry APPEND: the component is pushed (as an InternalBasePtr) onto the
// TownManager's population array at (GameWorld+0xF0)+0xC0, whose count lives at
// +0xC8. That array is the ONLY enumeration of the world's populations — it is
// what DebugUI::Slot4 (0x1400619E0) walks to fill the "deer population" combo on
// the debug panel's Game tab, reading each element's +0x18 PopulationDescription
// for the row text. With slot 6 never installed the array stayed empty and the
// combo had zero rows.
void PopulationComponent_OnAdd(void* selfV) {
    PopulationComponent_InitFuncs();
    if (!selfV || !s_pGameWorld || !*s_pGameWorld) return;
    uint8_t* townMgr = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(*s_pGameWorld) + 0xf0);
    if (!townMgr) return;

    uint64_t selfRef = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(selfV));   // B.3.3 bridge
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>(townMgr + 0xc0)
        .SetSizeFill(*reinterpret_cast<int*>(townMgr + 0xc8) + 1, &selfRef);

}

// ============================================================
// PopulationComponent::OnRemove (vtable slot 7) — retail 0x140054180
// ============================================================
// Ghidra names it "PopulationComponent_OnCreate"; 0x140002cc0 is the ERASE. Pairs
// with OnAdd above — without it a destroyed population entity would leave a stale
// InternalBasePtr in the town array for the combo (and anything else walking it)
// to dereference.
void PopulationComponent_OnRemove(void* selfV) {
    PopulationComponent_InitFuncs();
    if (!selfV || !s_pGameWorld || !*s_pGameWorld) return;
    uint8_t* townMgr = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(*s_pGameWorld) + 0xf0);
    if (!townMgr || !s_fn_ArrErase) return;

    uint64_t selfRef = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(selfV));   // B.3.3 bridge
    s_fn_ArrErase(reinterpret_cast<int64_t*>(townMgr + 0xc0), &selfRef);
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
    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_ArrErase = reinterpret_cast<fn_ArrErase_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

// ============================================================
// ENDGAME B.3.8 CIT — PopulationComponent as a REAL class (PopulationComponent.h)
// ============================================================
// Before: a 48-slot hand-built PopulationComponent_LiveVtable filled at every
// create (slots 3 Serialize, 4 Startup, 6 OnAdd, 7 OnRemove, 9 Update),
// Inert_ScalarDtor in slot 0. After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2). Pair with a live HerdComponent: Startup
// creates the herd entities.
#include "Object/factory_registration.h"
#include "Component/PopulationComponent.h"

// Retail 0x1400544a0 (animalwave_serialize.cpp; Ghidra calls it "Shutdown").
extern "C" void PopulationComponent_Serialize_Real(void* self, void* stream);

// 0x1400546d0. Component() is the CALL 0x1401188b0; the 0x14015a360 stamp is
// the compiler's; every field = 0; flags |= 0xcb.
PopulationComponent::PopulationComponent()
    : field28(nullptr), field30(0), _pad34{}, field38(nullptr), field40(nullptr),
      field48(0), _pad4C{}, field50(nullptr), field58(nullptr), field60(nullptr),
      field68(nullptr) {
    OrFlagsInCtor(0xcb);
}

// [0] 0x14000b0a0. PORT GAP (retail 0x140054660 frees the +0x50 and +0x38
// arrays first) — the tree's Inert_ScalarDtor never did; kept
// (PopulationComponent.h).
Object::Base* PopulationComponent::DeletingDtor(unsigned flags) {
    this->~PopulationComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

void PopulationComponent::Persist(void* stream) { PopulationComponent_Serialize_Real(this, stream); }   // [3] 0x1400544a0
void PopulationComponent::Startup()  { PopulationComponent_Startup(this); }                              // [4] 0x140054220
// Town-registry membership (retail slots 6/7), a pair: slot 7 is the only
// remover of what slot 6 appends.
void PopulationComponent::OnAdded()  { PopulationComponent_OnAdd(this); }                                // [6] 0x1400541c0
void PopulationComponent::OnRemove() { PopulationComponent_OnRemove(this); }                             // [7] 0x140054180

// [9] 0x140054070. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void PopulationComponent::Update(void* time) {
    PopulationComponent_Update(this, static_cast<int64_t*>(time));
}

void PopulationComponent::Slot11() {}                             // [11] 0x140054210: retail's own `RET`

// create 0x14000af50: MemAlloc(0x70, 8), then the ctor.
void* PopulationComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PopulationComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PopulationComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* PopulationComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PopulationComponent());
}

DEFINE_EXE_VTABLE(PopulationComponent, "_ZTV19PopulationComponent")
DEFINE_EXE_TYPE(PopulationComponent, "Component")
