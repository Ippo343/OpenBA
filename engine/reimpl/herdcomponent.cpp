// engine/reimpl/herdcomponent.cpp
// Reimplementation of HerdComponent::Update from Application-x64.exe.
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

#include "HerdComponent.h"
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
// Local helper function pointers
// ============================================================

// Object::InternalBasePtr::CheckPtr @ Runtime 0x1800c0290
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// System::Time::Time(float) @ Runtime 0x1800078c0
typedef void* (*fn_Time_Ctor_Float_t)(void* outTime, float seconds);
static fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;

// System::Random::RandFloat(Random*) @ Runtime 0x1800c7d40
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t fn_RandFloat = nullptr;

// FUN_140053f20 -- food availability check helper
// Returns float food score for a component at a grid cell
typedef float (*fn_CheckFood_t)(int64_t entityComp, int gridCell);
static fn_CheckFood_t fn_CheckFood = nullptr;

// HerdComponent_AdjustSize (0x140121450) -- adjusts herd population
typedef void (*fn_AdjustSize_t)(void* herdComp, int targetSize);
static fn_AdjustSize_t fn_AdjustSize = nullptr;

// HerdComponent_StartMigration (0x140122340) -- starts herd migration
typedef void (*fn_StartMigration_t)(int64_t herdComp);
static fn_StartMigration_t fn_StartMigration = nullptr;

// HerdComponent_CheckMigrationDone (0x140121980) -- checks if migration complete
typedef char (*fn_CheckMigrationDone_t)(int64_t herdComp);
static fn_CheckMigrationDone_t fn_CheckMigrationDone = nullptr;

// Globals
static void** g_pWorldSingleton = nullptr;   // DAT_1401c3dd8
static uint32_t* g_pHerdTypeId = nullptr;    // DAT_1401c7858 -- type ID for herd config
static float* g_fSixty = nullptr;            // DAT_140172234 = 60.0f (game minutes to seconds)
static float* g_fOne = nullptr;              // DAT_14017214c = 1.0f
static float* g_fFoodThreshold = nullptr;    // DAT_140172114 = 0.5f
static float* g_fFoodScale = nullptr;        // DAT_140172174 = 2.0f

// ============================================================
// Helper: check if tile at data4C is valid on the map
// Returns true if the tile's terrain type value is nonzero
// ============================================================
static bool IsTileValid(HerdComponent* self) {
    fn_CheckPtr(reinterpret_cast<uint8_t*>(self) + 0x28);
    int64_t entityRef = static_cast<int64_t>(self->data28);
    if (entityRef == 0) return false;

    void* gameWorld = *g_pWorldSingleton;
    int mapWidth = **reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(gameWorld) + 0xF8);

    int tileIdx = self->data4C;

    // Signed division for row/col
    int64_t quotient = (int64_t)tileIdx / (int64_t)mapWidth;
    int64_t remainder = (int64_t)tileIdx % (int64_t)mapWidth;

    // Read grid info from entity ref
    int64_t mapData = *reinterpret_cast<int64_t*>(entityRef + 0x28);
    int cellSize = *reinterpret_cast<int*>(mapData + 0x28);
    int stride = *reinterpret_cast<int*>(entityRef + 0x30);
    int64_t tileArray = *reinterpret_cast<int64_t*>(entityRef + 0x38);

    // Compute grid row/col (matching decompiler's division pattern)
    int row = (int)((int64_t)((uint64_t)(uint32_t)((int)quotient >> 31) << 32 |
              (uint64_t)((uint32_t)quotient)) / (int64_t)cellSize);
    int col = (int)((int64_t)((uint64_t)(uint32_t)((int)remainder >> 31) << 32 |
              (uint64_t)((uint32_t)remainder)) / (int64_t)cellSize);

    // Each tile entry is 0xC (12) bytes. Check the int at offset 0.
    int64_t tileOffset = ((int64_t)row * (int64_t)stride + (int64_t)col) * 0xC;
    int terrainType = *reinterpret_cast<int*>(tileArray + tileOffset);

    return terrainType != 0;
}

// ============================================================
// HerdComponent::Update reimplementation
// ============================================================

static void Reimpl_Update(HerdComponent* self, int64_t* deltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[HerdComponent::Update] self=%p dt=%lld phase=%d\n",
               (void*)self, *deltaTime, (int)self->data48);
        callCount++;
    }
#endif

    // Get config object via IsDerivedFrom check
    void* unk18 = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(unk18, *g_pHerdTypeId);
    uint8_t* config = isDerived ? (uint8_t*)unk18 : nullptr;

    // === Timer tick: accumulate time at +0x58 ===
    int64_t* spawnTimer = reinterpret_cast<int64_t*>(&self->data58);
    *spawnTimer += *deltaTime;

    // Compute interval from config+0x3C (in game minutes, convert to ticks)
    float intervalMinutes = *(float*)(config + 0x3C);
    int64_t intervalTicks;
    fn_Time_Ctor_Float(&intervalTicks, intervalMinutes * *g_fSixty);

    if (intervalTicks < *spawnTimer) {
        // Timer fired -- subtract interval
        *spawnTimer -= intervalTicks;

        // Check food availability
        fn_CheckPtr(reinterpret_cast<uint8_t*>(self) + 0x28);
        float foodScore = fn_CheckFood(
            static_cast<int64_t>(self->data28),
            *reinterpret_cast<int32_t*>(&self->data30));

        // Compute food modifier: if food < threshold, scale it
        float foodModifier = *g_fOne;
        if (foodScore < *g_fFoodThreshold) {
            foodModifier = foodScore * *g_fFoodScale;
        }

        // Target size: current * 2 (minimum 2)
        int currentTarget = *reinterpret_cast<int32_t*>(&self->data40) * 2;
        if (currentTarget == 0) {
            currentTarget = 2;
        }

        // Max size from config+0x28, scaled by food modifier
        int maxSize = (int)((float)(*(int32_t*)(config + 0x28)) * foodModifier + *g_fFoodThreshold);
        if (maxSize < currentTarget) {
            currentTarget = maxSize;
        }

        fn_AdjustSize(self, currentTarget);
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
        fn_StartMigration(reinterpret_cast<int64_t>(self));
        *reinterpret_cast<int64_t*>(&self->data50) = 0;
        self->data48 = 1;  // Switch to migrating phase
    } else {
        // Migrating phase: check if done
        char done = fn_CheckMigrationDone(reinterpret_cast<int64_t>(self));

        if (done != 0) {
            // Migration complete -- randomize graze timer and switch back
            void* world = *g_pWorldSingleton;
            void* random = *(void**)((uint8_t*)world + 0xD8);

            float randVal = fn_RandFloat(random);
            float baseTime = *(float*)(config + 0x30);
            float randRange = *(float*)(config + 0x34);
            float grazeSeconds = (randVal * randRange + baseTime) * *g_fSixty;

            int64_t grazeTicks;
            fn_Time_Ctor_Float(&grazeTicks, grazeSeconds);

            self->data48 = 0;  // Back to grazing
            *reinterpret_cast<int64_t*>(&self->data50) = grazeTicks;
            return;
        }

        // Still migrating -- check terrain validity at current position
        if (!IsTileValid(self)) {
            // Tile became invalid -- restart migration
            fn_StartMigration(reinterpret_cast<int64_t>(self));
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================

void InstallHerdComponentUpdate() {
    // Resolve Runtime functions
    fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        ResolveAddr(Module::Runtime, 0x1800078c0));
    fn_RandFloat = reinterpret_cast<fn_RandFloat_t>(
        ResolveAddr(Module::Runtime, 0x1800c7d40));

    // Resolve exe functions (direct calls)
    fn_CheckFood = reinterpret_cast<fn_CheckFood_t>(
        ResolveAddr(Module::Exe, 0x140053f20));
    fn_AdjustSize = reinterpret_cast<fn_AdjustSize_t>(
        ResolveAddr(Module::Exe, 0x140121450));
    fn_StartMigration = reinterpret_cast<fn_StartMigration_t>(
        ResolveAddr(Module::Exe, 0x140122340));
    fn_CheckMigrationDone = reinterpret_cast<fn_CheckMigrationDone_t>(
        ResolveAddr(Module::Exe, 0x140121980));

    // Resolve globals
    g_pWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    g_pHerdTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7858));

    // Resolve float constants
    g_fSixty = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    g_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    g_fFoodThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    g_fFoodScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172174));

    // Install hook
    uintptr_t target = ResolveAddr(Module::Exe, HerdComponentAddr::Update);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);

#ifdef _DEBUG
    printf("[HerdComponent] Update hooked at %p (prologue=13)\n", (void*)target);
    printf("[HerdComponent]   CheckFood=%p AdjustSize=%p StartMigration=%p CheckDone=%p\n",
           (void*)fn_CheckFood, (void*)fn_AdjustSize,
           (void*)fn_StartMigration, (void*)fn_CheckMigrationDone);
    printf("[HerdComponent]   HerdTypeId=%p -> 0x%x\n",
           (void*)g_pHerdTypeId, g_pHerdTypeId ? *g_pHerdTypeId : 0);
#endif
}

void RemoveHerdComponentUpdate() {
    RemoveHook(&g_hookUpdate);
}
