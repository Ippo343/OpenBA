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
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// Sub-helpers used by HerdComponent_AdjustSize (deep spawn path).
// Random::Rand / RandFloatTol go through exe dispatch slots (not direct Runtime addrs).
typedef int (*fn_Random_Rand_slot_t)(void* rng, int range);
static fn_Random_Rand_slot_t s_fn_Rand = nullptr;
typedef float (*fn_Random_RandFloatTol_t)(void* rng);
static fn_Random_RandFloatTol_t s_fn_RandFloatTol = nullptr;
typedef void* (*fn_ComponentEntity_CopyCtor_t)(void* mem, int64_t descr);
static fn_ComponentEntity_CopyCtor_t s_fn_ComponentEntity_CopyCtor = nullptr;
typedef void* (*fn_Vec3Ctor_local_t)(void* out, float x, float y, float z);
static fn_Vec3Ctor_local_t s_fn_Vec3Ctor_local = nullptr;
typedef void (*fn_CharComp_f3e0_t)(void* mapPtr, void* outVec3, float* inVec3, char flag, void* opt);
static fn_CharComp_f3e0_t s_fn_CharComp_f3e0 = nullptr;
typedef void (*fn_Herd_helper7400_t)(int64_t ref, void* pos, float scale, char flag);
static fn_Herd_helper7400_t s_fn_Herd_helper7400 = nullptr;
typedef void (*fn_Herd_imple510_t)(void* refA, void* refB);
static fn_Herd_imple510_t s_fn_Herd_imple510 = nullptr;
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityRef, int id);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;
// HerdComponent_helper_6b10 @0x140106b10 — per-animal "migration done?" test.
typedef char (*fn_Herd_helper6b10_t)(int64_t ref);
static fn_Herd_helper6b10_t s_fn_Herd_helper6b10 = nullptr;

// Type-id globals used by the helpers.
static uint32_t* s_pTypeId_1c8000 = nullptr;  // animal model/family component
static uint32_t* s_pTypeId_1c6f98 = nullptr;  // pasture/herd sub-component
static uint32_t* s_pTypeId_1c7078 = nullptr;  // behavior component
static float* s_fSpread = nullptr;            // DAT_140172220 spawn spread scale

static bool s_herdExtraInit = false;
static void EnsureHerdExtras() {
    if (s_herdExtraInit) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401512e8);            // System::Random::Rand
    if (slot) s_fn_Rand = *reinterpret_cast<fn_Random_Rand_slot_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151400);            // System::Random::RandFloatTol
    if (slot) s_fn_RandFloatTol = *reinterpret_cast<fn_Random_RandFloatTol_t*>(slot);
    s_fn_ComponentEntity_CopyCtor = reinterpret_cast<fn_ComponentEntity_CopyCtor_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    slot = ResolveAddr(Module::Exe, 0x140152a30);            // Math::Vector3::Vector3
    if (slot) s_fn_Vec3Ctor_local = *reinterpret_cast<fn_Vec3Ctor_local_t*>(slot);
    s_fn_CharComp_f3e0 = reinterpret_cast<fn_CharComp_f3e0_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_Herd_helper7400 = reinterpret_cast<fn_Herd_helper7400_t>(
        ResolveAddr(Module::Exe, 0x140107400));
    s_fn_Herd_imple510 = reinterpret_cast<fn_Herd_imple510_t>(
        ResolveAddr(Module::Exe, 0x14012e510));
    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_Herd_helper6b10 = reinterpret_cast<fn_Herd_helper6b10_t>(
        ResolveAddr(Module::Exe, 0x140106b10));
    s_pTypeId_1c8000 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8000));
    s_pTypeId_1c6f98 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6f98));
    s_pTypeId_1c7078 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7078));
    s_fSpread = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172220));
    s_herdExtraInit = true;
}

// Find a component of `typeId` on a ComponentEntity (list at +0x30, count at +0x38).
// Mirrors the inlined search loop used throughout these helpers.
static void* HerdFindComponent(int64_t entity, uint32_t* pTypeId) {
    if (!pTypeId) return nullptr;
    int n = *(int*)(entity + 0x38);
    uintptr_t list = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = (void**)(list + (int64_t)i * 8);
        if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
        void* comp = *slot;
        uint32_t t = (*(uint32_t(**)(void*))(*(uintptr_t*)comp + 0x10))(comp);
        if (t == *pTypeId) {
            if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
            void* c = *slot;
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) ? c : nullptr;
        }
    }
    return nullptr;
}

// Entity_FindComponent_Family (0x1400012d0) — find the herd-member sub-component
// (returns an IBP). Resolved lazily here for the 6b10 body.
typedef void* (*fn_FindFamily_t)(int64_t entity, void* outIBP);
static fn_FindFamily_t s_fn_FindFamily = nullptr;

// Reimpl of FUN_140106b10 (HerdComponent_helper_6b10): per-animal "has this animal
// reached its migration target tile?" test. Finds the animal's herd-member component
// off its owning entity (+0x10 -> Entity_FindComponent_Family), reads the herd centre
// world position (+0x158/+0x15c), converts it to a clamped tile index, then returns
// true iff that tile falls inside the animal's recorded bounds rect (+0x38..+0x3e).
// Closure: Entity_FindComponent_Family (0x1400012d0, registered) + CheckPtr (engine).
char HerdComponent_Helper6b10(int64_t param_1) {
    EnsureHelpers();
    EnsureHerdExtras();
    if (!s_fn_FindFamily)
        s_fn_FindFamily = reinterpret_cast<fn_FindFamily_t>(
            ResolveAddr(Module::Exe, 0x1400012d0));

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    int* piMapDim = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(param_1 + 0x10));
    uint64_t ibp = 0;
    if (s_fn_FindFamily) s_fn_FindFamily(*(int64_t*)(param_1 + 0x10), &ibp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&ibp);
    int64_t comp = *(int64_t*)&ibp;
    if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(comp + 0x68));
    int64_t pos = *(int64_t*)(comp + 0x68);

    int mapDim = *piMapDim;
    int x = (int)(*(float*)(pos + 0x158) + (float)(mapDim >> 1));
    int y = (int)(*(float*)(pos + 0x15c) + (float)(mapDim >> 1));
    if (x < 0) x = 0;
    if (mapDim <= x) x = mapDim - 1;
    if (y < 0) y = 0;
    if (mapDim <= y) y = mapDim - 1;

    // Original folds (x,y) into mapDim*y+x then re-splits it; net effect recovers
    // the same clamped x,y. Keep that for byte-faithfulness.
    int idx = mapDim * y + x;
    int ry = idx / mapDim;
    int rx = idx % mapDim;

    short bx = *(short*)(param_1 + 0x38);
    short by = *(short*)(param_1 + 0x3a);
    short bw = *(short*)(param_1 + 0x3c);
    short bh = *(short*)(param_1 + 0x3e);
    if (rx < bx || ry < by ||
        (int)bw + (int)bx <= rx ||
        (int)bh + (int)by <= ry) {
        return 0;
    }
    return 1;
}

// Reimpl of FUN_140053f20 (count fertile/food tiles in the herd's grazing cell)
// Sub-samples the (param_2) macro-cell into a (gridDim x gridDim) block of map tiles
// (gridDim = mapDim/cellDim), counts tiles whose first int (food value) is > 0, and
// returns the fraction (countPositive / gridDim^2).
float HerdComponent_CheckFood(int64_t entityComp, int gridCell) {
    int cellDim = *(int*)(entityComp + 0x48);
    int gridDim = *(int*)(entityComp + 0x30) / cellDim;
    int mapStride = *(int*)(entityComp + 0x30);
    uintptr_t tiles = *(uintptr_t*)(entityComp + 0x38);

    int positive = 0;
    for (int64_t row = 0; row < gridDim; row++) {
        int* pTile = (int*)(tiles +
            (((gridCell / cellDim) * gridDim + row) * (int64_t)mapStride +
             (int64_t)((gridCell % cellDim) * gridDim)) * 0xc);
        for (int64_t col = gridDim; col != 0; col--) {
            positive += (*pTile > 0) ? 1 : 0;
            pTile += 3;
        }
    }
    return (float)positive / (float)(gridDim * gridDim);
}

// Reimpl of FUN_140121980 (is the herd's migration complete?)
// For each migrating animal entity, finds its herd-member sub-component (type
// 0x1401c6f98) and asks helper_6b10 whether that animal has reached its target.
// Returns 0 as soon as any animal is still en route; 1 if all are done.
char HerdComponent_CheckMigrationDone(int64_t herdComp) {
    EnsureHerdExtras();
    uint8_t* self = reinterpret_cast<uint8_t*>(herdComp);
    if (fn_IsDerivedFrom && s_pHerdTypeId)
        fn_IsDerivedFrom(*(void**)(self + 0x18), *s_pHerdTypeId);

    int count = *(int*)(self + 0x40);
    for (int64_t i = 0; i < count; i++) {
        uintptr_t arr = *(uintptr_t*)(self + 0x38);
        if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(arr + i * 8));
        int64_t entity = *(int64_t*)(*(int64_t*)(arr + i * 8) + 0x10);
        if (s_fn_CheckPtr) s_fn_CheckPtr(&entity);

        void* member = HerdFindComponent(entity, s_pTypeId_1c6f98);
        uint64_t memberRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&memberRef, member);
        if (s_fn_CheckPtr) s_fn_CheckPtr(&memberRef);
        char done = s_fn_Herd_helper6b10 ? s_fn_Herd_helper6b10((int64_t)memberRef) : 1;
        if (done == 0) return 0;
    }
    return 1;
}

// Reimpl of FUN_140121450 (grow the herd toward targetSize by spawning animals)
// While current population (+0x40) < target: clone the herd's animal template
// (ComponentEntity_CopyCtor), locate its model/behavior/member sub-components,
// pick a randomized spawn position around the herd centre tile (+0x4c), place the
// new animal (helper_7400), link it to the herd (impl_e510), and register the entity
// with the world entity list (+0xe8). Spawn flag alternates each iteration.
void HerdComponent_AdjustSize(void* herdComp, int targetSize) {
    EnsureHerdExtras();
    uint8_t* self = reinterpret_cast<uint8_t*>(herdComp);

    void* cfgObj = (fn_IsDerivedFrom && s_pHerdTypeId &&
                    fn_IsDerivedFrom(*(void**)(self + 0x18), *s_pHerdTypeId))
                       ? *(void**)(self + 0x18) : nullptr;
    if (!cfgObj) return;
    int64_t descr = *(int64_t*)((uintptr_t)cfgObj + 0x20);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* rng = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
    int* mapDimPtr = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    char altFlag = (s_fn_Rand && s_fn_Rand(rng, 2) == 0) ? 1 : 0;
    float spread = s_fSpread ? *s_fSpread : 0.0f;
    float half = s_fFoodThreshold ? *s_fFoodThreshold : 0.5f;

    while (*(int*)(self + 0x40) < targetSize) {
        // Clone the animal template into a fresh ComponentEntity.
        void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
        void* ent = (mem && s_fn_ComponentEntity_CopyCtor)
                        ? s_fn_ComponentEntity_CopyCtor(mem, descr) : nullptr;
        uint64_t entRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entRef, ent);
        if (s_fn_CheckPtr) s_fn_CheckPtr(&entRef);
        int64_t entObj = (int64_t)entRef;

        void* modelComp = HerdFindComponent(entObj, s_pTypeId_1c8000);
        uint64_t modelRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&modelRef, modelComp);

        void* memberComp = HerdFindComponent(entObj, s_pTypeId_1c6f98);
        uint64_t memberRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&memberRef, memberComp);

        void* behaviorComp = HerdFindComponent(entObj, s_pTypeId_1c7078);
        uint64_t behaviorRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&behaviorRef, behaviorComp);

        // Spread radius from the herd config animal count.
        void* cfg2 = (fn_IsDerivedFrom && s_pHerdTypeId &&
                      fn_IsDerivedFrom(*(void**)(self + 0x18), *s_pHerdTypeId))
                         ? *(void**)(self + 0x18) : nullptr;
        int animalArea = *(int*)((uintptr_t)cfg2 + 0x38);
        int mapDim = *mapDimPtr;
        float origin = (float)(mapDim >> 1) - half;

        uint8_t centreVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(centreVec,
                (float)(*(int*)(self + 0x4c) % mapDim) - origin,
                (float)(*(int*)(self + 0x4c) / mapDim) - origin, 0.0f);

        uint8_t worldPos[16] = {0};   // local_e8/e4/e0
        if (s_fn_CharComp_f3e0)
            s_fn_CharComp_f3e0(mapDimPtr, worldPos, (float*)centreVec, 0, nullptr);

        float rx = s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
        float ry = s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
        uint8_t offVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(offVec, ry * (float)animalArea, rx * (float)animalArea, 0.0f);

        uint8_t spawnVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(spawnVec,
                *(float*)(worldPos + 0) + *(float*)(offVec + 0),
                *(float*)(worldPos + 4) + *(float*)(offVec + 4),
                *(float*)(worldPos + 8) + *(float*)(offVec + 8));

        if (s_fn_CheckPtr) s_fn_CheckPtr(&behaviorRef);
        float rz = s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
        if (s_fn_Herd_helper7400)
            s_fn_Herd_helper7400((int64_t)behaviorRef, spawnVec, rz * spread, altFlag);
        altFlag = (altFlag == 0) ? 1 : 0;

        if (fn_IsDerivedFrom && s_pHerdTypeId)
            fn_IsDerivedFrom(*(void**)(self + 0x18), *s_pHerdTypeId);
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfRef, self);
        if (s_fn_CheckPtr) s_fn_CheckPtr(&modelRef);
        if (s_fn_Herd_imple510) s_fn_Herd_imple510((void*)modelRef, &selfRef);

        if (s_fn_RegisterEntity) {
            void* entityList = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xe8);
            uint64_t regRef = entRef;
            s_fn_RegisterEntity(entityList, &regRef, *(int*)(descr + 0x20));
        }
    }
}

// ============================================================
// HERD cluster (bottom-up land): helper_1ad0 (0x140121ad0) + impl_e510 (0x14012e510)
// ============================================================
// Their full exe FUN_ closure is registered (FUN_140003110, FUN_140002cc0,
// FUN_140005500), so both are now closure-safe to port.
typedef void (*fn_Herd_AddToList_t)(int64_t* listField, int newCount, void* ibp); // 0x140003110
typedef void (*fn_Herd_RemoveFromList_t)(int64_t* listField, void* ibp);          // 0x140002cc0
typedef void* (*fn_Herd_FindMember_t)(int64_t entity, void* outIbp);              // 0x140005500
static fn_Herd_AddToList_t      s_fn_Herd_AddToList = nullptr;
static fn_Herd_RemoveFromList_t s_fn_Herd_RemoveFromList = nullptr;
static fn_Herd_FindMember_t     s_fn_Herd_FindMember = nullptr;
static bool s_herdClusterInit = false;
static void EnsureHerdCluster() {
    if (s_herdClusterInit) return;
    EnsureHelpers();
    s_fn_Herd_AddToList      = reinterpret_cast<fn_Herd_AddToList_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fn_Herd_RemoveFromList = reinterpret_cast<fn_Herd_RemoveFromList_t>(ResolveAddr(Module::Exe, 0x140002cc0));
    s_fn_Herd_FindMember     = reinterpret_cast<fn_Herd_FindMember_t>(ResolveAddr(Module::Exe, 0x140005500));
    s_herdClusterInit = true;
}

// Reimpl of FUN_140121ad0 (HerdComponent_helper_1ad0): register a new animal into the
// herd. Append its IBP to the member list (+0x38, count +0x40), then compute a clamped
// bounds rect (from the herd centre tile +0x4c and the config animal-spread +0x38) and
// write it into the animal's herd-member sub-component (+0x38..+0x3e via FUN_140005500).
// Faithful 1:1 port; closure: FUN_140003110 + FUN_140005500 (both registered).
void HerdComponent_Helper1ad0(int64_t herdComp, void* animalIbp) {
    EnsureHerdCluster();
    uint8_t* self = reinterpret_cast<uint8_t*>(herdComp);

    if (s_fn_Herd_AddToList)
        s_fn_Herd_AddToList((int64_t*)(self + 0x38), *(int*)(self + 0x40) + 1, animalIbp);

    void* cfgObj = *(void**)(self + 0x18);
    void* cfg = (fn_IsDerivedFrom && s_pHerdTypeId &&
                 fn_IsDerivedFrom(cfgObj, *s_pHerdTypeId)) ? cfgObj : nullptr;
    int spread = *(int*)((uintptr_t)cfg + 0x38);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int mapDim = **(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    int bx = *(int*)(self + 0x4c) % mapDim - (spread >> 1);
    int by = *(int*)(self + 0x4c) / mapDim - (spread >> 1);
    if (bx < 1) bx = 1;
    if (by < 1) by = 1;
    int bw = spread;
    int bh = spread;
    if (mapDim - 1 <= bx + spread) bw = (mapDim - bx) - 2;
    if (mapDim - 1 <= by + bw)     bh = (mapDim - by) - 2;

    if (s_fn_CheckPtr) s_fn_CheckPtr(animalIbp);
    uint64_t entity = *(uint64_t*)(*(int64_t*)animalIbp + 0x10);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&entity);
    uint64_t memberIbp = 0;
    if (s_fn_Herd_FindMember) s_fn_Herd_FindMember((int64_t)entity, &memberIbp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&memberIbp);
    int64_t member = *(int64_t*)&memberIbp;
    *(int16_t*)(member + 0x38) = (int16_t)bx;
    *(int16_t*)(member + 0x3a) = (int16_t)by;
    *(int16_t*)(member + 0x3c) = (int16_t)bw;
    *(int16_t*)(member + 0x3e) = (int16_t)bh;
}

// Reimpl of FUN_14012e510 (HerdComponent_impl_e510): assign the herd's owning-herd
// reference (+0x28). If it changes: detach the animal from the old herd's member list
// (FUN_140002cc0 on oldHerd+0x38), store the new herd ptr, and if non-null attach it
// (HerdComponent_helper_1ad0). param_1 = animal's herd-member component, param_2 = new
// herd IBP. Faithful 1:1 port; closure: FUN_140002cc0 + helper_1ad0 (registered).
void HerdComponent_Impl_e510(void* memberComp, void* newHerdIbp) {
    EnsureHerdCluster();
    uint8_t* self = reinterpret_cast<uint8_t*>(memberComp);

    if (s_fn_CheckPtr) s_fn_CheckPtr(newHerdIbp);
    if (*(int64_t*)(self + 0x28) == *(int64_t*)newHerdIbp) return;

    if (*(int64_t*)(self + 0x28) != 0) {
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfRef, self);
        if (s_fn_CheckPtr) s_fn_CheckPtr(self + 0x28);
        if (s_fn_Herd_RemoveFromList)
            s_fn_Herd_RemoveFromList((int64_t*)(*(int64_t*)(self + 0x28) + 0x38), &selfRef);
    }

    int64_t newHerd = *(int64_t*)newHerdIbp;
    *(int64_t*)(self + 0x28) = newHerd;
    if (newHerd != 0) {
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfRef, self);
        if (s_fn_CheckPtr) s_fn_CheckPtr(self + 0x28);
        HerdComponent_Helper1ad0(*(int64_t*)(self + 0x28), &selfRef);
    }
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

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x60 (HerdComponent_Factory MemAlloc), flags 0x8b (Ctor 0x140122150:
// Component base sets +0x20=0x80, leaf OR 0xb => 0x8b).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(HerdComponent, 0x60, 0x8b)
REGISTER_TYPE(HerdComponent, "Component",
              &HerdComponent_Factory, &HerdComponent_CreateInPlace);
