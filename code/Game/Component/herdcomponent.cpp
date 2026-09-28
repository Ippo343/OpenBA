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
// Struct size: 0x60, vtable at 0x140163680. Verified slot indices (from the
// .rdata table at 0x140163680): [3] Serialize 0x140121f90, [4] Startup (base
// stub 0x140118a50), [6] OnAdd 0x140121f40, [7] OnRemove 0x140121e20,
// [9] Update 0x140121bf0.
//
// Herds are created by PopulationComponent's Startup (vtable slot 4,
// 0x140054220 — see populationcomponent.cpp), which instantiates
// (mapDim/popDesc+0x2c)^2 herd entities and seeds each one through
// HerdComponent_Populate (0x140120f50).
//
// Decompiled from Application-x64.exe using Ghidra.

// graphics/Mesh.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include "Component/herdcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/HerdDescription.h"        // ENDGAME B.3.9 C1: 0x1401c7858 cell class
#include "Component/WildAnimalComponent.h"    // ENDGAME B.3.9 C1: 0x1401c8000 cell class
#include "Component/AnimalComponent.h"        // ENDGAME B.3.9 C1: 0x1401c6f98 cell class
#include "Component/CharacterComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7078 cell class
#include "Component/AnimalDescription.h"      // ENDGAME B.3.9 C1: 0x1401c6fd0 cell class
#include "Component/ModelDescription.h"       // ENDGAME B.3.9 C1: 0x1401c7a88 cell class
#include "Component/exe_bodies.h"

// ============================================================
// Resolved helpers
// ============================================================

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
// DAT_140172234 = 300.0f — the minutes->seconds scale every herd/growth timer
// multiplies its description interval by. (Old comments here said 60.0f; the
// .rdata word is 0x43960000 = 300.0f and PopulationComponent_Update uses the
// same constant as a literal 300-second smoothing period.)
static float* s_f300 = nullptr;              // DAT_140172234 = 300.0f
static float* s_fOne = nullptr;              // DAT_14017214c = 1.0f
static float* s_fFoodThreshold = nullptr;    // DAT_140172114 = 0.5f
static float* s_fFoodScale = nullptr;        // DAT_140172174 = 2.0f

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

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

    s_f300 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_fFoodThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_fFoodScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172174));

    s_helpersResolved = true;
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// Sub-helpers used by HerdComponent_AdjustSize (deep spawn path).
typedef void* (*fn_ComponentEntity_CopyCtor_t)(void* mem, int64_t descr);
static fn_ComponentEntity_CopyCtor_t s_fn_ComponentEntity_CopyCtor = nullptr;
typedef void* (*fn_Vec3Ctor_local_t)(void* out, float x, float y, float z);
static fn_Vec3Ctor_local_t s_fn_Vec3Ctor_local = nullptr;
typedef void (*fn_CharComp_f3e0_t)(void* mapPtr, void* outVec3, float* inVec3, char flag, void* opt);
static fn_CharComp_f3e0_t s_fn_CharComp_f3e0 = nullptr;
typedef void (*fn_Herd_imple510_t)(void* refA, void* refB);
static fn_Herd_imple510_t s_fn_Herd_imple510 = nullptr;
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityRef, int id);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;
// HerdComponent_helper_6b10 @0x140106b10 — per-animal "migration done?" test.
typedef char (*fn_Herd_helper6b10_t)(int64_t ref);
static fn_Herd_helper6b10_t s_fn_Herd_helper6b10 = nullptr;

// Type-id globals used by the helpers.
static float* s_fSpread = nullptr;            // DAT_140172220 = 180.0f, spawn yaw scale
static float* s_fScatter = nullptr;           // DAT_140172110 = 0.4f, populate jitter

// Deps of HerdComponent_Helper7400 (0x140107400).
typedef void* (*fn_FindModelComp_t)(int64_t entity, void* outIbp);        // 0x140003f60
static fn_FindModelComp_t s_fn_FindModelComp = nullptr;
typedef void (*fn_ModelSetTransform_t)(int64_t model, const void* pos, float yaw); // 0x140003870
static fn_ModelSetTransform_t s_fn_ModelSetTransform = nullptr;
typedef void (*fn_ModelHelper2b70_t)(int64_t ownerIbp, int arg);          // 0x1400c2b70
static fn_ModelHelper2b70_t s_fn_ModelHelper2b70 = nullptr;
typedef void (*fn_MarkEntityDirty_t)(void* entity);                       // 0x1400c3470
static fn_MarkEntityDirty_t s_fn_MarkEntityDirty = nullptr;
typedef void (*fn_AnimalUpdateBehavior_t)(void* animalComp);              // 0x140107320
static fn_AnimalUpdateBehavior_t s_fn_AnimalUpdateBehavior = nullptr;
// DynArray<InternalBasePtr> ops used by OnAdd/OnRemove.
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call sites below.
typedef void (*fn_DynArray8_EraseByValue_t)(int64_t* arr, void* ibp);            // 0x140002cc0
static fn_DynArray8_EraseByValue_t s_fn_DynArray8_EraseByValue = nullptr;

static bool s_herdExtraInit = false;
static void EnsureHerdExtras() {
    if (s_herdExtraInit) return;
    s_fn_ComponentEntity_CopyCtor = reinterpret_cast<fn_ComponentEntity_CopyCtor_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152a30);  // Math::Vector3::Vector3
    if (slot) s_fn_Vec3Ctor_local = *reinterpret_cast<fn_Vec3Ctor_local_t*>(slot);
    s_fn_CharComp_f3e0 = reinterpret_cast<fn_CharComp_f3e0_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    // 0x140107400 is HerdComponent_Helper7400 in this file — called directly.
    s_fn_Herd_imple510 = reinterpret_cast<fn_Herd_imple510_t>(
        ResolveAddr(Module::Exe, 0x14012e510));
    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_Herd_helper6b10 = reinterpret_cast<fn_Herd_helper6b10_t>(
        ResolveAddr(Module::Exe, 0x140106b10));
    s_fSpread = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172220));
    s_fScatter = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172110));

    s_fn_FindModelComp = reinterpret_cast<fn_FindModelComp_t>(
        ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_ModelSetTransform = reinterpret_cast<fn_ModelSetTransform_t>(
        ResolveAddr(Module::Exe, 0x140003870));
    s_fn_ModelHelper2b70 = reinterpret_cast<fn_ModelHelper2b70_t>(
        ResolveAddr(Module::Exe, 0x1400c2b70));
    s_fn_MarkEntityDirty = reinterpret_cast<fn_MarkEntityDirty_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_AnimalUpdateBehavior = reinterpret_cast<fn_AnimalUpdateBehavior_t>(
        ResolveAddr(Module::Exe, 0x140107320));
    s_fn_DynArray8_EraseByValue = reinterpret_cast<fn_DynArray8_EraseByValue_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    s_herdExtraInit = true;
}

// Find a component of `typeId` on a ComponentEntity (list at +0x30, count at +0x38).
// Mirrors the inlined search loop used throughout these helpers.
static void* HerdFindComponent(int64_t entity, uint32_t typeId) {
    if (!typeId) return nullptr;
    int n = *(int*)(entity + 0x38);
    uintptr_t list = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = (void**)(list + (int64_t)i * 8);
        void* comp = *slot;
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (t == typeId) {
            void* c = *slot;
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, typeId)) ? c : nullptr;
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

    uint64_t ibp = 0;
    if (s_fn_FindFamily) s_fn_FindFamily(*(int64_t*)(param_1 + 0x10), &ibp);
    int64_t comp = *(int64_t*)&ibp;
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

// ============================================================
// Reimpl of FUN_140107400 (HerdComponent_helper_7400): place one animal.
// ============================================================
// param_1 is the animal's AnimalComponent (type 0x1401c6f98) — NOT the herd.
// Called once per spawned animal from AdjustSize and Populate:
//   * find the ModelComponent on the animal's entity and write its world
//     transform (position + yaw) via 0x140003870
//   * record the pose flag at +0x40; when set, and the model's variant entry
//     has more than one mesh, swap the MeshInstance to mesh index 1 (the
//     alternate "grazing" pose) and re-register the model with the scene
//   * reset the behavior timer at +0x50 to 0 and OR in capability bit 0x400000
//   * tail-call AnimalComponent::UpdateBehavior (0x140107320)
//
// The model variant table lives at ModelDescription+0x20 with 0x18-byte
// entries; +0x10 is the mesh count and +0x08 the mesh pointer array. Model
// component +0x164 is the variant index, +0x166 the current mesh index.
//
// Note the retail dirty-flag test `((new ^ old) & 0xffff) != 0` can never be
// true here (the only changed bit is 0x400000, above the mask), so the
// 0x1400c3470 re-register is dead on this path. Kept for fidelity.
void HerdComponent_Helper7400(int64_t animalComp, void* pos, float yaw, char pose) {
    EnsureHelpers();
    EnsureHerdExtras();
    if (!s_fn_FindModelComp || !s_fn_ModelSetTransform) return;

    uint64_t modelIbp = 0;
    s_fn_FindModelComp(*(int64_t*)(animalComp + 0x10), &modelIbp);
    int64_t model = *(int64_t*)&modelIbp;
    if (!model) return;

    s_fn_ModelSetTransform(model, pos, yaw);
    *(char*)(animalComp + 0x40) = pose;

    if (pose != 0) {
        // Re-resolve the model exactly as retail does (a second find call).
        modelIbp = 0;
        s_fn_FindModelComp(*(int64_t*)(animalComp + 0x10), &modelIbp);
        model = *(int64_t*)&modelIbp;

        void* descObj = *(void**)(model + 0x18);
        void* modelDesc = (fn_IsDerivedFrom &&
                           fn_IsDerivedFrom(descObj, ModelDescription::TypeId()))
                              ? descObj : nullptr;
        int variant = *(int16_t*)(model + 0x164);
        uintptr_t variantTable = modelDesc ? *(uintptr_t*)((uintptr_t)modelDesc + 0x20) : 0;
        int meshCount = variantTable
            ? *(int*)(variantTable + (int64_t)variant * 0x18 + 0x10) : 0;

        if (meshCount > 1) {
            modelIbp = 0;
            s_fn_FindModelComp(*(int64_t*)(animalComp + 0x10), &modelIbp);
            model = *(int64_t*)&modelIbp;

            if (*(int16_t*)(model + 0x166) != 1) {
                // Detach from the scene while the mesh is swapped (slot +0x38),
                // then re-add (slot +0x30).
                if (*(int16_t*)(model + 0x166) >= 0) {
                    reinterpret_cast<Component*>(model)->OnRemove();   // B.3.x bridge (slot 7)
                }
                *(int16_t*)(model + 0x166) = 1;

                void* d2 = *(void**)(model + 0x18);
                void* md2 = (fn_IsDerivedFrom &&
                             fn_IsDerivedFrom(d2, ModelDescription::TypeId())) ? d2 : nullptr;
                int meshIdx = *(int16_t*)(model + 0x166);
                int v2 = *(int16_t*)(model + 0x164);
                uintptr_t table2 = md2 ? *(uintptr_t*)((uintptr_t)md2 + 0x20) : 0;
                if (table2) {
                    uintptr_t meshList = *(uintptr_t*)(table2 + (int64_t)v2 * 0x18 + 0x8);
                    void* mesh = meshList
                        ? *(void**)(meshList + (int64_t)meshIdx * 8) : nullptr;
                    reinterpret_cast<Graphics::MeshInstance*>(model + 0x28)
                        ->SetMesh(Graphics::ExternalPtrHandle{mesh});   // B.3.5 bridge
                }

                reinterpret_cast<Component*>(model)->OnAdded();   // B.3.x bridge (slot 6)
            }
            if (s_fn_ModelHelper2b70) s_fn_ModelHelper2b70(*(int64_t*)(model + 0x10), 0);
        }
    }

    int64_t zeroTime = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&zeroTime, 0.0f);
    uint32_t oldFlags = *(uint32_t*)(animalComp + 0x20);
    *(int64_t*)(animalComp + 0x50) = zeroTime;
    uint32_t newFlags = oldFlags | 0x400000u;
    *(uint32_t*)(animalComp + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*)(animalComp + 0x10) != 0) {
        if (s_fn_MarkEntityDirty) s_fn_MarkEntityDirty(*(void**)(animalComp + 0x10));
    }

    if (s_fn_AnimalUpdateBehavior) s_fn_AnimalUpdateBehavior((void*)animalComp);
}

// Reimpl of FUN_140121980 (is the herd's migration complete?)
// For each migrating animal entity, finds its herd-member sub-component (type
// 0x1401c6f98) and asks helper_6b10 whether that animal has reached its target.
// Returns 0 as soon as any animal is still en route; 1 if all are done.
char HerdComponent_CheckMigrationDone(int64_t herdComp) {
    EnsureHerdExtras();
    uint8_t* self = reinterpret_cast<uint8_t*>(herdComp);
    if (fn_IsDerivedFrom)
        fn_IsDerivedFrom(*(void**)(self + 0x18), HerdDescription::TypeId());

    int count = *(int*)(self + 0x40);
    for (int64_t i = 0; i < count; i++) {
        uintptr_t arr = *(uintptr_t*)(self + 0x38);
        int64_t entity = *(int64_t*)(*(int64_t*)(arr + i * 8) + 0x10);

        void* member = HerdFindComponent(entity, AnimalComponent::TypeId());
        uint64_t memberRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&memberRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(member));   // B.3.3 bridge
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

    void* cfgObj = (fn_IsDerivedFrom && fn_IsDerivedFrom(*(void**)(self + 0x18), HerdDescription::TypeId()))
                       ? *(void**)(self + 0x18) : nullptr;
    if (!cfgObj) return;
    int64_t descr = *(int64_t*)((uintptr_t)cfgObj + 0x20);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* rng = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
    int* mapDimPtr = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    char altFlag = (reinterpret_cast<System::Random*>(rng)->Rand(2u) == 0) ? 1 : 0;   // B.3.4b bridge
    float spread = s_fSpread ? *s_fSpread : 0.0f;
    float half = s_fFoodThreshold ? *s_fFoodThreshold : 0.5f;

    while (*(int*)(self + 0x40) < targetSize) {
        // The loop's only exit is the member count at +0x40 growing, which
        // happens inside impl_e510 -> helper_1ad0. If that link is missing the
        // retail shape spins forever while allocating an entity per pass, so
        // bail on a pass that made no progress rather than hanging the world.
        const int countBefore = *(int*)(self + 0x40);

        // Clone the animal template into a fresh ComponentEntity.
        void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
        void* ent = (mem && s_fn_ComponentEntity_CopyCtor)
                        ? s_fn_ComponentEntity_CopyCtor(mem, descr) : nullptr;
        uint64_t entRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&entRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ent));   // B.3.3 bridge
        int64_t entObj = (int64_t)entRef;

        // Retail looks up three components in this order and uses two of them:
        // the WildAnimalComponent is what gets linked back to the herd, and the
        // AnimalComponent is what gets placed. The CharacterComponent lookup is
        // performed and discarded — kept so the search order matches.
        void* wildComp = HerdFindComponent(entObj, WildAnimalComponent::TypeId());
        uint64_t wildRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&wildRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wildComp));   // B.3.3 bridge

        void* animalComp = HerdFindComponent(entObj, AnimalComponent::TypeId());
        uint64_t animalRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&animalRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(animalComp));   // B.3.3 bridge

        void* charComp = HerdFindComponent(entObj, CharacterComponent::TypeId());
        uint64_t charRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&charRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(charComp));   // B.3.3 bridge
        (void)charRef;

        // Spread radius from the herd config animal count.
        void* cfg2 = (fn_IsDerivedFrom &&
                      fn_IsDerivedFrom(*(void**)(self + 0x18), HerdDescription::TypeId()))
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

        float rx = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        float ry = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        uint8_t offVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(offVec, ry * (float)animalArea, rx * (float)animalArea, 0.0f);

        uint8_t spawnVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(spawnVec,
                *(float*)(worldPos + 0) + *(float*)(offVec + 0),
                *(float*)(worldPos + 4) + *(float*)(offVec + 4),
                *(float*)(worldPos + 8) + *(float*)(offVec + 8));

        float rz = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        HerdComponent_Helper7400((int64_t)animalRef, spawnVec, rz * spread, altFlag);
        altFlag = (altFlag == 0) ? 1 : 0;

        if (fn_IsDerivedFrom)
            fn_IsDerivedFrom(*(void**)(self + 0x18), HerdDescription::TypeId());
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        if (s_fn_Herd_imple510) s_fn_Herd_imple510((void*)wildRef, &selfRef);

        if (s_fn_RegisterEntity) {
            void* entityList = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xe8);
            uint64_t regRef = entRef;
            s_fn_RegisterEntity(entityList, &regRef, *(int*)(descr + 0x20));
        }
        if (*(int*)(self + 0x40) <= countBefore) {
            fprintf(stderr, "[Herd] AdjustSize aborted: member count stuck at %d "
                    "(target %d) — the herd back-link never ran\n",
                    countBefore, targetSize);
            fflush(stderr);
            return;
        }
    }
}

// ============================================================
// HERD cluster (bottom-up land): helper_1ad0 (0x140121ad0) + impl_e510 (0x14012e510)
// ============================================================
// Their full exe FUN_ closure is registered (FUN_140003110, FUN_140002cc0,
// FUN_140005500), so both are now closure-safe to port.
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.
typedef void (*fn_Herd_RemoveFromList_t)(int64_t* listField, void* ibp);          // 0x140002cc0
typedef void* (*fn_Herd_FindMember_t)(int64_t entity, void* outIbp);              // 0x140005500
static fn_Herd_RemoveFromList_t s_fn_Herd_RemoveFromList = nullptr;
static fn_Herd_FindMember_t     s_fn_Herd_FindMember = nullptr;
static bool s_herdClusterInit = false;
static void EnsureHerdCluster() {
    if (s_herdClusterInit) return;
    EnsureHelpers();
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

    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(self + 0x38))
        .SetSizeFill(*(int*)(self + 0x40) + 1, animalIbp);

    void* cfgObj = *(void**)(self + 0x18);
    void* cfg = (fn_IsDerivedFrom &&
                 fn_IsDerivedFrom(cfgObj, HerdDescription::TypeId())) ? cfgObj : nullptr;
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

    uint64_t entity = *(uint64_t*)(*(int64_t*)animalIbp + 0x10);
    uint64_t memberIbp = 0;
    if (s_fn_Herd_FindMember) s_fn_Herd_FindMember((int64_t)entity, &memberIbp);
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

    if (*(int64_t*)(self + 0x28) == *(int64_t*)newHerdIbp) return;

    if (*(int64_t*)(self + 0x28) != 0) {
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        if (s_fn_Herd_RemoveFromList)
            s_fn_Herd_RemoveFromList((int64_t*)(*(int64_t*)(self + 0x28) + 0x38), &selfRef);
    }

    int64_t newHerd = *(int64_t*)newHerdIbp;
    *(int64_t*)(self + 0x28) = newHerd;
    if (newHerd != 0) {
        uint64_t selfRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        HerdComponent_Helper1ad0(*(int64_t*)(self + 0x28), &selfRef);
    }
}

// ============================================================
// Reimpl of FUN_140120f50 — HerdComponent::Populate(count).
// ============================================================
// Ghidra names this PopulationComponent_helper_0f50, but param_1 is the
// HerdComponent: it reads the herd's description through +0x18 against
// HerdDescription (0x1401c7858) and writes the herd's own +0x4c/+0x50/+0x48.
//
// This is the herd's initial seeding pass, called once per herd from
// PopulationComponent's Startup. It picks the grazing cell (StartMigration),
// derives the clamped bounds rect the same way helper_1ad0 does, then spawns
// `count` animals at random tiles inside that rect, and finally arms the graze
// timer and drops the herd into the grazing phase.
//
// Unlike AdjustSize (which scatters around the herd centre with a radius from
// herdDesc+0x38), each animal here gets its own random tile in the rect plus a
// sub-tile jitter of DAT_140172110 (0.4f).
void HerdComponent_Populate(int64_t herdComp, int count) {
    EnsureHelpers();
    EnsureHerdExtras();
    uint8_t* self = reinterpret_cast<uint8_t*>(herdComp);

    void* cfgObj = *(void**)(self + 0x18);
    void* herdDesc = (fn_IsDerivedFrom &&
                      fn_IsDerivedFrom(cfgObj, HerdDescription::TypeId())) ? cfgObj : nullptr;

    if (s_fn_StartMigration) s_fn_StartMigration(herdComp);

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton || !herdDesc) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* mapDimPtr = *(int**)(gw + 0xf8);
    void* rng = *(void**)(gw + 0xd8);

    // StartMigration picks the grazing tile into +0x4c, or leaves -1 when the
    // herd's cell has no habitable ground at all (a water/mountain cell — a
    // normal outcome, and also what an unregistered StartMigration looks like).
    // Retail never guards this because its StartMigration always runs; without
    // the check every tile computed below would be negative.
    int mapDim = mapDimPtr ? *mapDimPtr : 0;
    if (mapDim <= 0 || *(int*)(self + 0x4c) < 0) {
        return;
    }

    // Clamped bounds rect around the herd centre tile (+0x4c).
    int spread = *(int*)((uintptr_t)herdDesc + 0x38);
    int bx = *(int*)(self + 0x4c) % mapDim - (spread >> 1);
    int by = *(int*)(self + 0x4c) / mapDim - (spread >> 1);
    if (bx < 1) bx = 1;
    if (by < 1) by = 1;
    int bw = spread;
    int bh = spread;
    if (mapDim - 1 <= bx + spread) bw = (mapDim - bx) - 2;
    if (mapDim - 1 <= by + bw)     bh = (mapDim - by) - 2;

    // Retail feeds bw/bh straight to Random::Rand. A herd whose centre lands
    // within two tiles of the map edge makes them <= 0, and Rand's modulo would
    // fault, so floor them at one tile.
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    char altPose = (reinterpret_cast<System::Random*>(rng)->Rand(2u) == 0) ? 1 : 0;   // B.3.4b bridge
    float scatter = s_fScatter ? *s_fScatter : 0.0f;
    float yawScale = s_fSpread ? *s_fSpread : 0.0f;
    float half = s_fFoodThreshold ? *s_fFoodThreshold : 0.5f;
    int64_t descr = *(int64_t*)((uintptr_t)herdDesc + 0x20);
    if (!descr) {
        // Skip the spawn loop but still arm the graze timer below, exactly as a
        // count of zero would.
        fprintf(stderr, "[Herd] Populate: herd=%p has no animal template "
                "(HerdDescription+0x20 is null) — no deer will spawn\n", (void*)self);
        fflush(stderr);
        count = 0;
    }

    int spawned = 0;
    for (int n = 0; n < count; n++) {
        int rndX = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(bw));   // B.3.4b bridge
        int rndY = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(bh));   // B.3.4b bridge

        void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
        void* ent = (mem && s_fn_ComponentEntity_CopyCtor)
                        ? s_fn_ComponentEntity_CopyCtor(mem, descr) : nullptr;
        uint64_t entRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&entRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ent));   // B.3.3 bridge
        int64_t entObj = (int64_t)entRef;
        if (!entObj) continue;

        void* animalComp = HerdFindComponent(entObj, AnimalComponent::TypeId());
        uint64_t animalRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&animalRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(animalComp));   // B.3.3 bridge

        void* wildComp = HerdFindComponent(entObj, WildAnimalComponent::TypeId());
        uint64_t wildRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&wildRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wildComp));   // B.3.3 bridge

        mapDim = *mapDimPtr;
        int tile = mapDim * (by + rndY) + bx + rndX;
        float origin = (float)(mapDim >> 1) - half;

        uint8_t cellVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(cellVec,
                (float)(tile % mapDim) - origin,
                (float)(tile / mapDim) - origin, 0.0f);

        uint8_t worldPos[16] = {0};
        if (s_fn_CharComp_f3e0)
            s_fn_CharComp_f3e0(mapDimPtr, worldPos, (float*)cellVec, 0, nullptr);

        float j1 = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        float j2 = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        uint8_t offVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(offVec, j2 * scatter, j1 * scatter, 0.0f);

        uint8_t spawnVec[16] = {0};
        if (s_fn_Vec3Ctor_local)
            s_fn_Vec3Ctor_local(spawnVec,
                *(float*)(worldPos + 0) + *(float*)(offVec + 0),
                *(float*)(worldPos + 4) + *(float*)(offVec + 4),
                *(float*)(worldPos + 8) + *(float*)(offVec + 8));

        float yaw = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        HerdComponent_Helper7400((int64_t)animalRef, spawnVec, yaw * yawScale, altPose);
        altPose = (altPose == 0) ? 1 : 0;

        // Randomize the animal's starting appetite from AnimalDescription+0x28.
        if (animalRef) {
            void* aDescObj = *(void**)((uintptr_t)animalRef + 0x18);
            void* animalDesc = (fn_IsDerivedFrom &&
                                fn_IsDerivedFrom(aDescObj, AnimalDescription::TypeId()))
                                   ? aDescObj : nullptr;
            float r = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
            if (animalDesc)
                *(float*)((uintptr_t)animalRef + 0x44) =
                    r * *(float*)((uintptr_t)animalDesc + 0x28);
            if (s_fn_AnimalUpdateBehavior) s_fn_AnimalUpdateBehavior((void*)animalRef);
        }

        uint64_t herdRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&herdRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        if (s_fn_Herd_imple510) s_fn_Herd_imple510((void*)wildRef, &herdRef);

        if (s_fn_RegisterEntity) {
            void* entityList = *(void**)(gw + 0xe8);
            uint64_t regRef = entRef;
            s_fn_RegisterEntity(entityList, &regRef, *(int*)(descr + 0x20));
        }
        spawned++;
    }

    // Arm the graze timer and enter the grazing phase.
    float r = reinterpret_cast<System::Random*>(*(void**)(gw + 0xd8))->RandFloat();   // B.3.4b bridge
    int64_t grazeTicks = 0;
    if (fn_Time_Ctor_Float)
        fn_Time_Ctor_Float(&grazeTicks,
            r * (*(float*)((uintptr_t)herdDesc + 0x34) +
                 *(float*)((uintptr_t)herdDesc + 0x30)) * (s_f300 ? *s_f300 : 300.0f));
    *(int64_t*)(self + 0x50) = grazeTicks;
    *(char*)(self + 0x48) = 0;
}

// ============================================================
// Reimpl of FUN_140121f40 — HerdComponent::OnAdd (vtable slot 6).
// Appends this herd (as an InternalBasePtr) to the town manager's herd list at
// (GameWorld+0xf0)+0xb0, count at +0xb8.
// ============================================================

// ============================================================
// Reimpl of FUN_140121e20 — HerdComponent::OnRemove (vtable slot 7).
// Detaches every member (the inlined impl_e510(member, null) — erasing the
// member from this herd's own +0x38 list is what drains the +0x40 count and
// terminates the loop), then removes the herd from the world herd list.
// ============================================================
void HerdComponent_OnRemove(void* selfV) {
    EnsureHelpers();
    EnsureHerdExtras();
    EnsureHerdCluster();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);

    while (*(int*)(self + 0x40) > 0) {
        uint64_t nullRef = 0;
        if (fn_InternalBasePtr_Ctor) new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        void** slot = reinterpret_cast<void**>(
            *(uintptr_t*)(self + 0x38) + (int64_t)(*(int*)(self + 0x40) - 1) * 8);
        uint8_t* member = reinterpret_cast<uint8_t*>(*slot);
        if (!member) break;

        if (*(int64_t*)(member + 0x28) == (int64_t)nullRef) break;  // no progress possible

        if (*(int64_t*)(member + 0x28) != 0) {
            uint64_t memberRef = 0;
            if (fn_InternalBasePtr_Ctor) new (&memberRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(member));   // B.3.3 bridge
            if (s_fn_DynArray8_EraseByValue)
                s_fn_DynArray8_EraseByValue(
                    reinterpret_cast<int64_t*>(*(int64_t*)(member + 0x28) + 0x38), &memberRef);
        }
        *(int64_t*)(member + 0x28) = (int64_t)nullRef;
        if (nullRef != 0) {
            uint64_t memberRef = 0;
            if (fn_InternalBasePtr_Ctor) new (&memberRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(member));   // B.3.3 bridge
            HerdComponent_Helper1ad0(*(int64_t*)(member + 0x28), &memberRef);
        }
    }

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    uint8_t* townMgr = *(uint8_t**)(gw + 0xf0);
    if (!townMgr || !s_fn_DynArray8_EraseByValue) return;
    uint64_t selfRef = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    s_fn_DynArray8_EraseByValue(reinterpret_cast<int64_t*>(townMgr + 0xb0), &selfRef);
}

// ============================================================
// Helper: check if tile at data4C is valid on the map
// Returns true if the tile's terrain type value is nonzero
// ============================================================
static bool IsTileValid(HerdComponent* self) {
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
    printf("[HerdComponent]   HerdTypeId=0x%x\n", HerdDescription::TypeId());
#endif
}

// ===== ENDGAME B.3.8 CIT — HerdComponent as a REAL class (HerdComponent.h) =====
// Before: a 48-slot hand-built HerdComponent_LiveVtable filled at every create
// (slots 3 Serialize, 4 Startup, 6 OnAdd, 9 Update), Inert_ScalarDtor in slot 0.
// After: the compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1,
// D2). A live herd does nothing on its own — PopulationComponent's Startup is
// what creates herd entities and calls Populate.
#include "Object/factory_registration.h"
#include "Component/HerdComponent.h"


// 0x140122150. Component() is the CALL 0x1401188b0; the 0x140163680 stamp is
// the compiler's; +0x28, +0x38, +0x40, +0x48 (byte), +0x50, +0x58 = 0; flags |=
// 0x0b (0x8b). Retail leaves +0x30 unwritten (zeroed here, as the tree did).
// +0x4c = -1 is the ctor's own store (0x14012218c). The tree used to write it from
// the Startup slot (HerdComponent_Startup) because the inert factory could not
// express it; retail's slot 4 is Component's `RET`, which the class now inherits.
HerdComponent::HerdComponent()
    : data28(0), data30(0), data38(0), data40(0), data48(0), _pad49{},
      data4C(-1), data50(0), data58(0) {
    OrFlagsInCtor(0x0b);
}

void HerdComponent::OnAdded() {
    EnsureHelpers();
    EnsureHerdExtras();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    uint8_t* townMgr = *(uint8_t**)(gw + 0xf0);
    if (!townMgr) return;

    uint64_t selfRef = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0xb0))
        .SetSizeFill(*(int*)(townMgr + 0xb8) + 1, &selfRef);
}

// [7] 0x140121e20. PORT GAP (retail 0x140121e20) — B38 D9: the body is ported
// (HerdComponent_OnRemove, registered at 0x140121e20 by reg_herd_live.cpp) but
// the tree's old table never installed it; the empty body is kept here. Installing
// it is its own titled commit.
void HerdComponent::OnRemove() {}

// [9] 0x140121bf0. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
// HerdComponent::Update's diagnostic counters (main thread only: the update walk),
// at namespace scope rather than function-local statics (B6-5).
static long long s_herdUpdCalls = 0, s_herdUpdNoPop = 0, s_herdUpdNoCfg = 0, s_herdUpdFired = 0;

void HerdComponent::Update(void* time) {
    int64_t* deltaTime = static_cast<int64_t*>(time);
    EnsureHelpers();

    // Every branch below reaches the owning PopulationComponent through +0x28
    // (food sampling, tile validity, migration retarget), which retail derefs
    // unconditionally. A herd whose Populate never ran has +0x28 == 0, so bail
    // rather than fault; the herd simply stays idle.
    // Update-entry census. The [Herd] census line below only prints once the
    // respawn timer fires, so its absence cannot distinguish "Update never ran"
    // from "ran but the timer never expired" from "bailed on a null +0x28".
    auto& g_hu_calls = s_herdUpdCalls; auto& g_hu_noPop = s_herdUpdNoPop;
    auto& g_hu_noCfg = s_herdUpdNoCfg; auto& g_hu_fired = s_herdUpdFired;
    ++g_hu_calls;
    if (data28 == 0) {
        ++g_hu_noPop;
        if ((g_hu_noPop % 2000) == 1) {
            fprintf(stderr, "[HerdUpd] calls=%lld noPop=%lld noCfg=%lld fired=%lld\n",
                    g_hu_calls, g_hu_noPop, g_hu_noCfg, g_hu_fired);
            fflush(stderr);
        }
        return;
    }

    // Get config object via IsDerivedFrom check
    void* unk18 = _description;
    bool isDerived = fn_IsDerivedFrom(unk18, HerdDescription::TypeId());
    uint8_t* config = isDerived ? reinterpret_cast<uint8_t*>(unk18) : nullptr;
    if (!config) ++g_hu_noCfg;

    // === Timer tick: accumulate time at +0x58 ===
    int64_t* spawnTimer = reinterpret_cast<int64_t*>(&data58);
    *spawnTimer += *deltaTime;

    // Compute interval from config+0x3C (in game minutes, convert to ticks)
    float intervalMinutes = *reinterpret_cast<float*>(config + 0x3C);
    int64_t intervalTicks;
    fn_Time_Ctor_Float(&intervalTicks, intervalMinutes * *s_f300);

    if ((g_hu_calls % 25) == 1) {
        fprintf(stderr, "[HerdUpd] calls=%lld noPop=%lld noCfg=%lld fired=%lld | "
                "herd=%p members=%d timer=%lld interval=%lld cfgMin=%.3f\n",
                g_hu_calls, g_hu_noPop, g_hu_noCfg, g_hu_fired, (void*)this,
                *reinterpret_cast<int32_t*>(&data40),
                (long long)*spawnTimer, (long long)intervalTicks, intervalMinutes);
        fflush(stderr);
    }

    if (intervalTicks < *spawnTimer) {
        ++g_hu_fired;
        fprintf(stderr, "[HerdUpd] FIRED #%lld herd=%p members=%d (calls=%lld)\n",
                g_hu_fired, (void*)this,
                *reinterpret_cast<int32_t*>(&data40), g_hu_calls);
        fflush(stderr);
        // Timer fired -- subtract interval
        *spawnTimer -= intervalTicks;

        // Check food availability
        float foodScore = s_fn_CheckFood(
            static_cast<int64_t>(data28),
            *reinterpret_cast<int32_t*>(&data30));

        // Compute food modifier: if food < threshold, scale it
        float foodModifier = *s_fOne;
        if (foodScore < *s_fFoodThreshold) {
            foodModifier = foodScore * *s_fFoodScale;
        }

        // Target size: current * 2 (minimum 2)
        int currentTarget = *reinterpret_cast<int32_t*>(&data40) * 2;
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

        s_fn_AdjustSize(this, currentTarget);
    }

    // === Phase logic ===
    if (data48 == 0) {
        // Grazing phase: countdown timer at +0x50
        int64_t* grazeTimer = reinterpret_cast<int64_t*>(&data50);
        *grazeTimer -= *deltaTime;

        if (*grazeTimer > 0) {
            // Timer still running -- check terrain validity
            if (IsTileValid(this)) {
                return;  // Valid terrain, stay grazing
            }
        }

        // Timer expired or invalid terrain -- start migration
        s_fn_StartMigration(reinterpret_cast<int64_t>(this));
        *reinterpret_cast<int64_t*>(&data50) = 0;
        data48 = 1;  // Switch to migrating phase
    } else {
        // Migrating phase: check if done
        char done = s_fn_CheckMigrationDone(reinterpret_cast<int64_t>(this));

        if (done != 0) {
            // Migration complete -- randomize graze timer and switch back
            void* world = *s_pWorldSingleton;
            void* random = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(world) + 0xD8);

            float randVal = reinterpret_cast<System::Random*>(random)->RandFloat();   // B.3.4b bridge
            float baseTime = *reinterpret_cast<float*>(config + 0x30);
            float randRange = *reinterpret_cast<float*>(config + 0x34);
            float grazeSeconds = (randVal * randRange + baseTime) * *s_f300;

            int64_t grazeTicks;
            fn_Time_Ctor_Float(&grazeTicks, grazeSeconds);

            data48 = 0;  // Back to grazing
            *reinterpret_cast<int64_t*>(&data50) = grazeTicks;
            return;
        }

        // Still migrating -- check terrain validity at current position
        if (!IsTileValid(this)) {
            // Tile became invalid -- restart migration
            s_fn_StartMigration(reinterpret_cast<int64_t>(this));
        }
    }
}

// create 0x140034cd0: MemAlloc(0x60, 8), then the ctor.
void* HerdComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HerdComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HerdComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* HerdComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HerdComponent());
}

DEFINE_EXE_TYPE(HerdComponent, "Component")
