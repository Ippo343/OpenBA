// code/Game/Component/herdcomponent.cpp
// Clean reimplementation of HerdComponent::Update from Application-x64.exe.
//
// HerdComponent manages wild animal herds. Two phases:
//   Phase 0 (grazing): countdown timer at +0x50, check terrain validity,
//                       then transition to migration when timer expires or invalid terrain.
//   Phase 1 (migrating): check migration done, then transition back to grazing
//                         with randomized timer.
//
// On each timer tick (interval from config+0x3C): checks food via helper,
// computes target herd size, calls AdjustSize.
//
// Update address: 0x140121bf0
// Struct size: 0x60, vtable at 0x140163680
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/herdcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// System::Random::RandFloat(Random*) @ Runtime 0x1800c7d40
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t s_fn_RandFloat = nullptr;

// FUN_140053f20 -- food availability check helper
typedef float (*fn_CheckFood_t)(int64_t entityComp, int gridCell);
static fn_CheckFood_t s_fn_CheckFood = nullptr;

// HerdComponent_AdjustSize (0x140121450) -- adjusts herd population
typedef void (*fn_AdjustSize_t)(void* herdComp, int targetSize);
static fn_AdjustSize_t s_fn_AdjustSize = nullptr;

// HerdComponent_StartMigration (0x140122340) -- starts herd migration
typedef void (*fn_StartMigration_t)(int64_t herdComp);
static fn_StartMigration_t s_fn_StartMigration = nullptr;

// HerdComponent_CheckMigrationDone (0x140121980) -- checks if migration complete
typedef char (*fn_CheckMigrationDone_t)(int64_t herdComp);
static fn_CheckMigrationDone_t s_fn_CheckMigrationDone = nullptr;

// Globals
static void** s_pWorldSingleton = nullptr;   // DAT_1401c3dd8
static uint32_t* s_pHerdTypeId = nullptr;    // DAT_1401c7858
static float* s_fSixty = nullptr;            // DAT_140172234 = 60.0f
static float* s_fOne = nullptr;              // DAT_14017214c = 1.0f
static float* s_fFoodThreshold = nullptr;    // DAT_140172114 = 0.5f
static float* s_fFoodScale = nullptr;        // DAT_140172174 = 2.0f

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));
    s_fn_RandFloat = reinterpret_cast<fn_RandFloat_t>(
        ResolveAddr(Module::Runtime, 0x1800c7d40));

    s_fn_CheckFood = reinterpret_cast<fn_CheckFood_t>(
        ResolveAddr(Module::Exe, 0x140053f20));
    s_fn_AdjustSize = reinterpret_cast<fn_AdjustSize_t>(
        ResolveAddr(Module::Exe, 0x140121450));
    s_fn_StartMigration = reinterpret_cast<fn_StartMigration_t>(
        ResolveAddr(Module::Exe, 0x140122340));
    s_fn_CheckMigrationDone = reinterpret_cast<fn_CheckMigrationDone_t>(
        ResolveAddr(Module::Exe, 0x140121980));

    s_pWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pHerdTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7858));

    s_fSixty = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_fFoodThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_fFoodScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172174));

    s_helpersResolved = true;
}

// ============================================================
// Helper: check if tile at data4C is valid on the map
// Returns true if the tile's terrain type value is nonzero
// ============================================================
static bool IsTileValid(HerdComponent* self) {
    s_fn_CheckPtr(reinterpret_cast<uint8_t*>(self) + 0x28);
    int64_t entityRef = static_cast<int64_t>(self->data28);
    if (entityRef == 0) return false;

    void* gameWorld = *s_pWorldSingleton;
    int mapWidth = **reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(gameWorld) + 0xF8);

    int tileIdx = self->data4C;

    // Signed division for row/col
    int64_t quotient = static_cast<int64_t>(tileIdx) / static_cast<int64_t>(mapWidth);
    int64_t remainder = static_cast<int64_t>(tileIdx) % static_cast<int64_t>(mapWidth);

    // Read grid info from entity ref
    int64_t mapData = *reinterpret_cast<int64_t*>(entityRef + 0x28);
    int cellSize = *reinterpret_cast<int*>(mapData + 0x28);
    int stride = *reinterpret_cast<int*>(entityRef + 0x30);
    int64_t tileArray = *reinterpret_cast<int64_t*>(entityRef + 0x38);

    // Compute grid row/col (matching decompiler's division pattern)
    int row = static_cast<int>(
        static_cast<int64_t>(
            (static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int>(quotient) >> 31)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(quotient))
        ) / static_cast<int64_t>(cellSize));
    int col = static_cast<int>(
        static_cast<int64_t>(
            (static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int>(remainder) >> 31)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(remainder))
        ) / static_cast<int64_t>(cellSize));

    // Each tile entry is 0xC (12) bytes. Check the int at offset 0.
    int64_t tileOffset = (static_cast<int64_t>(row) * static_cast<int64_t>(stride) +
                          static_cast<int64_t>(col)) * 0xC;
    int terrainType = *reinterpret_cast<int*>(tileArray + tileOffset);

    return terrainType != 0;
}

// ============================================================
// HerdComponent::Update (vtable slot [9], 0x140121bf0)
// ============================================================

void HerdComponent_Update(HerdComponent* self, int64_t* deltaTime) {
    EnsureHelpers();

    // Get config object via IsDerivedFrom check
    void* unk18 = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(unk18, *s_pHerdTypeId);
    uint8_t* config = isDerived ? reinterpret_cast<uint8_t*>(unk18) : nullptr;

    // === Timer tick: accumulate time at +0x58 ===
    int64_t* spawnTimer = reinterpret_cast<int64_t*>(&self->data58);
    *spawnTimer += *deltaTime;

    // Compute interval from config+0x3C (in game minutes, convert to ticks)
    float intervalMinutes = *reinterpret_cast<float*>(config + 0x3C);
    int64_t intervalTicks;
    fn_Time_Ctor_Float(&intervalTicks, intervalMinutes * *s_fSixty);

    if (intervalTicks < *spawnTimer) {
        // Timer fired -- subtract interval
        *spawnTimer -= intervalTicks;

        // Check food availability
        s_fn_CheckPtr(reinterpret_cast<uint8_t*>(self) + 0x28);
        float foodScore = s_fn_CheckFood(
            static_cast<int64_t>(self->data28),
            *reinterpret_cast<int32_t*>(&self->data30));

        // Compute food modifier: if food < threshold, scale it
        float foodModifier = *s_fOne;
        if (foodScore < *s_fFoodThreshold) {
            foodModifier = foodScore * *s_fFoodScale;
        }

        // Target size: current * 2 (minimum 2)
        int currentTarget = *reinterpret_cast<int32_t*>(&self->data40) * 2;
        if (currentTarget == 0) {
            currentTarget = 2;
        }

        // Max size from config+0x28, scaled by food modifier
        int maxSize = static_cast<int>(
            static_cast<float>(*reinterpret_cast<int32_t*>(config + 0x28)) *
            foodModifier + *s_fFoodThreshold);
        if (maxSize < currentTarget) {
            currentTarget = maxSize;
        }

        s_fn_AdjustSize(self, currentTarget);
    }

    // === Phase logic ===
    if (self->data48 == 0) {
        // Grazing phase: countdown timer at +0x50
        int64_t* grazeTimer = reinterpret_cast<int64_t*>(&self->data50);
        *grazeTimer -= *deltaTime;

        if (*grazeTimer > 0) {
            // Timer still running -- check terrain validity
            if (IsTileValid(self)) {
                return;  // Valid terrain, stay grazing
            }
        }

        // Timer expired or invalid terrain -- start migration
        s_fn_StartMigration(reinterpret_cast<int64_t>(self));
        *reinterpret_cast<int64_t*>(&self->data50) = 0;
        self->data48 = 1;  // Switch to migrating phase
    } else {
        // Migrating phase: check if done
        char done = s_fn_CheckMigrationDone(reinterpret_cast<int64_t>(self));

        if (done != 0) {
            // Migration complete -- randomize graze timer and switch back
            void* world = *s_pWorldSingleton;
            void* random = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(world) + 0xD8);

            float randVal = s_fn_RandFloat(random);
            float baseTime = *reinterpret_cast<float*>(config + 0x30);
            float randRange = *reinterpret_cast<float*>(config + 0x34);
            float grazeSeconds = (randVal * randRange + baseTime) * *s_fSixty;

            int64_t grazeTicks;
            fn_Time_Ctor_Float(&grazeTicks, grazeSeconds);

            self->data48 = 0;  // Back to grazing
            *reinterpret_cast<int64_t*>(&self->data50) = grazeTicks;
            return;
        }

        // Still migrating -- check terrain validity at current position
        if (!IsTileValid(self)) {
            // Tile became invalid -- restart migration
            s_fn_StartMigration(reinterpret_cast<int64_t>(self));
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HerdComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HerdComponent] Funcs resolved: CheckFood=%p AdjustSize=%p "
           "StartMigration=%p CheckDone=%p\n",
           (void*)s_fn_CheckFood, (void*)s_fn_AdjustSize,
           (void*)s_fn_StartMigration, (void*)s_fn_CheckMigrationDone);
    printf("[HerdComponent]   HerdTypeId=%p -> 0x%x\n",
           (void*)s_pHerdTypeId, s_pHerdTypeId ? *s_pHerdTypeId : 0);
#endif
}
