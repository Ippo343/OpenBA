// code/Game/Action/huntanimal.cpp
// Clean C++ reimplementation of HuntAnimal action methods.
// Source: Game/Action/HuntAnimal.cpp (from profile build debug strings)
//
// HuntAnimal -- 0x40 bytes. Handles hunting behavior: finding nearby animal,
// spawning a corpse entity, starting herd migration.
// Inherits: Object::Internal -> Action (0x20) -> HuntAnimal
// Factory: 0x140022700, Vtable: 0x14015fde0
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022770
//   [1] +0x08 VectorDtor    = 0x1400226f0 (returns type ptr, unhookable)
//   [2] +0x10 BaseDtor      = 0x1400226e0 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400c8260
//   [4] +0x20 AbstractImpl  = 0x1400c8250 (9 bytes, unhookable)
//   [5] +0x28 Execute       = 0x1400c8240 (9 bytes, unhookable)
//   [6] +0x30 Enter         = 0x1400c7de0 (~250 instructions, grid math + entity queries)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c4a30 (base Action)
//   [9] +0x48 GetTarget     = 0x1400c7d60
//
// Field layout (0x40 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: void*    animalEntity   (InternalBasePtr -- the animal being hunted)
//   +0x28: void*    herdEntity     (InternalBasePtr -- the herd the animal belongs to)
//   +0x30: void*    corpseEntity   (InternalBasePtr -- unused/zeroed field)
//   +0x38: uint8_t  huntStarted    (flag: 0=not started, 1=hunt completed)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "HuntAnimal.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Object::Internal::~Internal(this) -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// HuntAnimal_helper_2a80 (0x140122a80) -- checks if animal has been held long enough
// Returns true if entity's time at +0x50 >= description's holdTime * 300.0
static bool (*s_fn_IsAnimalReady)(void* animalEntity) = nullptr;

// FUN_140092260 -- spatial entity query (finds nearby entity matching criteria)
// Params: gridPtr, outResult, position, radius, typeId, outIBP
typedef void* (*s_fn_SpatialQuery_t)(void* gridPtr, void* outResult, float* position,
                                      float* radius, int typeId, void* outIBP);
static s_fn_SpatialQuery_t s_fn_SpatialQuery = nullptr;

// FUN_140005500 -- get model component from entity
static void (*s_fn_GetModelComp)(void* entity, void* outIBP) = nullptr;

// FUN_14009ccf0 -- find path cost between two grid cells
// Params: mapPtr, fromCell, toCell, flags(0xe1f)
typedef int (*s_fn_FindPathCost_t)(int* mapPtr, int fromCell, int toCell, uint32_t flags);
static s_fn_FindPathCost_t s_fn_FindPathCost = nullptr;

// FUN_1400a00f0 -- check path validity
// Params: mapPtr, flags(0xe1f), cell, destCell
typedef uint64_t (*s_fn_CheckPathValid_t)(int* mapPtr, uint64_t flags, uint64_t cell, int destCell);
static s_fn_CheckPathValid_t s_fn_CheckPathValid = nullptr;

// CharacterComponent_impl_f3e0 -- convert grid pos to world pos
// Params: mapPtr, outWorldPos, inGridPos, flag, extra
typedef float* (*s_fn_GridToWorld_t)(int* mapPtr, float* outWorldPos, float* inGridPos,
                                     char flag, void* extra);
static s_fn_GridToWorld_t s_fn_GridToWorld = nullptr;

// RemoveAnimal_Action_Ctor (0x1400bbea0) -- creates the corpse/removal action
// Params: herdIBP, animIndex(-1), worldPos, fromCell, corpseIBP, descPtr, flags
typedef void (*s_fn_SpawnCorpse_t)(void* herdIBP, int animIndex, float* worldPos,
                                   int fromCell, void* corpseIBP, void* descPtr, uint32_t flags);
static s_fn_SpawnCorpse_t s_fn_SpawnCorpse = nullptr;

// HuntAnimal_helper_27e0 (0x1400227e0) -- finds HerdComponent from entity's component list
// Params: entity, outIBP
typedef void* (*s_fn_FindHerdComp_t)(void* entity, void* outIBP);
static s_fn_FindHerdComp_t s_fn_FindHerdComp = nullptr;

// HerdComponent_StartMigration (0x140122340) -- triggers herd migration after kill
static void (*s_fn_StartMigration)(void* herdComp) = nullptr;

// Type IDs
static uint32_t* s_typeId_AnimalEntity = nullptr;     // DAT_1401c7890 -- for animal entity check
static uint32_t* s_typeId_HerdEntity = nullptr;        // DWORD_1401c7158 -- for herd entity check
static uint32_t* s_typeId_ModelDesc = nullptr;          // DWORD_1401c6fd0 -- ModelDescription type
static int* s_typeId_HuntAnimalSearch = nullptr;       // DAT_1401c8000 -- search type for spatial query

// TypeInfo for slots 1/2
static uint64_t* s_dat_VectorDtorReturn = nullptr;    // DAT_1401c5f70
static uint32_t* s_typeInfo_HuntAnimal = nullptr;      // DAT_1401c5f68

// Vtable addresses for dtor chain
static uintptr_t s_vtable_HuntAnimal = 0;
static uintptr_t s_vtable_Action = 0;

// Float constants
static float* s_searchRadius = nullptr;     // DAT_1401721a8 = 8.0f
static float* s_halfOffset = nullptr;       // DAT_140172114 = 0.5f

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool HuntAnimal_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // Helpers
    s_fn_IsAnimalReady = (bool(*)(void*))ResolveAddr(Module::Exe, 0x140122a80);
    s_fn_SpatialQuery = (s_fn_SpatialQuery_t)ResolveAddr(Module::Exe, 0x140092260);
    s_fn_GetModelComp = (void(*)(void*, void*))ResolveAddr(Module::Exe, 0x140005500);
    s_fn_FindPathCost = (s_fn_FindPathCost_t)ResolveAddr(Module::Exe, 0x14009ccf0);
    s_fn_CheckPathValid = (s_fn_CheckPathValid_t)ResolveAddr(Module::Exe, 0x1400a00f0);
    s_fn_GridToWorld = (s_fn_GridToWorld_t)ResolveAddr(Module::Exe, 0x14009f3e0);
    s_fn_SpawnCorpse = (s_fn_SpawnCorpse_t)ResolveAddr(Module::Exe, 0x1400bbea0);
    s_fn_FindHerdComp = (s_fn_FindHerdComp_t)ResolveAddr(Module::Exe, 0x1400227e0);
    s_fn_StartMigration = (void(*)(void*))ResolveAddr(Module::Exe, 0x140122340);

    // Type IDs
    s_typeId_AnimalEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7890));
    s_typeId_HerdEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_typeId_ModelDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6fd0));
    s_typeId_HuntAnimalSearch = reinterpret_cast<int*>(ResolveAddr(Module::Exe, 0x1401c8000));

    // TypeInfo for slots 1/2
    s_dat_VectorDtorReturn = reinterpret_cast<uint64_t*>(ResolveAddr(Module::Exe, 0x1401c5f70));
    s_typeInfo_HuntAnimal = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5f68));

    // Vtable addresses
    s_vtable_HuntAnimal = ResolveAddr(Module::Exe, 0x14015fde0);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);

    // Float constants
    s_searchRadius = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721a8));
    s_halfOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_IBP_Serialize &&
              s_fn_IsAnimalReady && s_fn_SpatialQuery &&
              s_fn_GetModelComp && s_fn_FindPathCost &&
              s_fn_CheckPathValid && s_fn_GridToWorld &&
              s_fn_SpawnCorpse && s_fn_FindHerdComp && s_fn_StartMigration &&
              s_typeId_AnimalEntity && s_typeId_HerdEntity &&
              s_typeId_ModelDesc && s_typeId_HuntAnimalSearch &&
              s_dat_VectorDtorReturn && s_typeInfo_HuntAnimal &&
              s_vtable_HuntAnimal && s_vtable_Action &&
              s_searchRadius && s_halfOffset &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_DeletePtr &&
              fn_InternalBasePtr_Ctor && fn_IsDerivedFrom &&
              fn_MemFreeStatic && fn_Vector3_Ctor &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[HuntAnimal] ResolveFuncs: ObjDtor=%p IBPSer=%p IsReady=%p SpatialQ=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_IsAnimalReady, (void*)s_fn_SpatialQuery);
    printf("[HuntAnimal]   GetModel=%p PathCost=%p PathValid=%p GridToWorld=%p\n",
           (void*)s_fn_GetModelComp, (void*)s_fn_FindPathCost,
           (void*)s_fn_CheckPathValid, (void*)s_fn_GridToWorld);
    printf("[HuntAnimal]   SpawnCorpse=%p FindHerd=%p StartMig=%p\n",
           (void*)s_fn_SpawnCorpse, (void*)s_fn_FindHerdComp, (void*)s_fn_StartMigration);
    if (!ok) printf("[HuntAnimal] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Slot 0: ScalarDeletingDtor (0x140022770)
// Same pattern as RemoveAnimal: set vtables, cleanup +0x10, ~Internal, free.
// ============================================================
void* HuntAnimal_ScalarDtor_Impl(HuntAnimal* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set HuntAnimal vtable then Action vtable for proper dtor dispatch
    *(uintptr_t*)s = s_vtable_HuntAnimal;
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        fn_InternalBasePtr_DeletePtr(s + 0x10);
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Slot 1: VectorDeletingDtor (0x1400226f0) -- unhookable (7 bytes, RIP-relative)
// Returns DAT_1401c5f70 (type factory pointer).
// ============================================================
uint64_t HuntAnimal_VectorDtor_Impl() {
    return *s_dat_VectorDtorReturn;
}

// ============================================================
// Slot 2: BaseDtor (0x1400226e0) -- unhookable (7 bytes, RIP-relative)
// Returns TypeInfo for HuntAnimal (DAT_1401c5f68).
// ============================================================
uint32_t HuntAnimal_BaseDtor_Impl() {
    return *s_typeInfo_HuntAnimal;
}

// ============================================================
// Slot 3: Serialize (0x1400c8260)
// Serializes: status(+0x18), animal entity(+0x20) with type check,
//             herd entity(+0x28) with type check, huntStarted flag(+0x38).
// ============================================================
void HuntAnimal_Serialize_Impl(HuntAnimal* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs and hunt flag
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize animal entity ref (+0x20) with AnimalEntity type check
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_typeId_AnimalEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize herd entity ref (+0x28) with HerdEntity type check
    s_fn_IBP_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *s_typeId_HerdEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Serialize hunt started flag (+0x38)
    g_fn_Serialize_Bool(ser2, s + 0x38, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// Slot 4: AbstractImpl (0x1400c8250) -- unhookable (9 bytes)
// Returns 1 if hunt not started (keep running), 0 if hunt completed.
// ============================================================
int HuntAnimal_AbstractImpl_Impl(HuntAnimal* self, void* actionStack) {
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    return (*(uint8_t*)(s + 0x38) == 0) ? 1 : 0;
}

// ============================================================
// Slot 5: Execute (0x1400c8240) -- unhookable (9 bytes)
// Same logic as AbstractImpl: returns 1 if hunt not started, 0 if completed.
// ============================================================
int HuntAnimal_Execute_Impl(HuntAnimal* self, void* actionStack, void* time) {
    (void)actionStack;
    (void)time;
    uint8_t* s = (uint8_t*)self;
    return (*(uint8_t*)(s + 0x38) == 0) ? 1 : 0;
}

// ============================================================
// Slot 6: Enter (0x1400c7de0) -- complex hunting logic
//
// Algorithm:
// 1. If huntStarted is already set, return early
// 2. Check if the animal at +0x20 is ready (held long enough)
// 3. Get herd's CharacterComponent position from +0x28 -> +0x278 -> +0x68 -> +0x158
// 4. Do a spatial query to find a nearby entity of the hunt type
// 5. Verify the found entity's model component doesn't have bit 20 set (not in use)
// 6. Get the found entity's model component
// 7. Calculate grid cell positions for path finding
// 8. Check path cost and validity between animal and found entity
// 9. If path is valid:
//    a. Get model description from found entity's model component
//    b. Convert grid position to world position
//    c. Spawn a corpse action (RemoveAnimal_Action_Ctor)
//    d. Find HerdComponent from the found entity
//    e. Start herd migration
//    f. Clear animal entity's time at +0x50
//    g. Set huntStarted flag at +0x38
// ============================================================
void HuntAnimal_Enter_Impl(HuntAnimal* self) {
    uint8_t* s = (uint8_t*)self;

    // If hunt already started, return early
    if (*(uint8_t*)(s + 0x38) != 0) {
        return;
    }

    // Check if the animal at +0x20 is ready to be hunted
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* animalEntity = *(void**)(s + 0x20);
    if (!s_fn_IsAnimalReady(animalEntity)) {
        return;
    }

    // Set up search radius
    float searchRadius = *s_searchRadius;  // 8.0f

    // Initialize output IBP for spatial query
    void* queryOutIBP = nullptr;
    fn_InternalBasePtr_Ctor(&queryOutIBP, nullptr);

    // Get the search type ID
    int searchTypeId = *s_typeId_HuntAnimalSearch;

    // Get herd entity's CharacterComponent position
    // Follow chain: herdEntity(+0x28) -> +0x278 -> +0x68 -> position at +0x158
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    void* herdEntity = *(void**)(s + 0x28);

    fn_InternalBasePtr_CheckPtr((uint8_t*)herdEntity + 0x278);
    void* charComp = *(void**)((uint8_t*)herdEntity + 0x278);

    fn_InternalBasePtr_CheckPtr((uint8_t*)charComp + 0x68);
    void* posHolder = *(void**)((uint8_t*)charComp + 0x68);

    // Read position (3 floats at +0x158, +0x15c, +0x160)
    float searchPos[3];
    searchPos[0] = *(float*)((uint8_t*)posHolder + 0x158);
    searchPos[1] = *(float*)((uint8_t*)posHolder + 0x15c);
    searchPos[2] = *(float*)((uint8_t*)posHolder + 0x160);

    // Spatial query: find nearby entity
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    void* gridPtr = *(void**)(gameWorld + 0x88);
    void* queryResult = nullptr;

    s_fn_SpatialQuery(gridPtr, &queryResult, searchPos, &searchRadius, searchTypeId, &queryOutIBP);

    // If no result found, bail
    if (queryResult == nullptr) {
        return;
    }

    // Get the found entity from query result
    fn_InternalBasePtr_CheckPtr(&queryResult);
    void* foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);

    // Check if the found entity's model component has bit 20 set (in use)
    void* tempIBP = nullptr;
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);
    void* modelCompIBP1[2] = {nullptr, nullptr};
    s_fn_GetModelComp(tempIBP, modelCompIBP1);
    fn_InternalBasePtr_CheckPtr(modelCompIBP1);
    uint32_t modelFlags = *(uint32_t*)((uint8_t*)(modelCompIBP1[0]) + 0x20);

    if ((modelFlags >> 20) & 1) {
        // Entity already in use, bail
        return;
    }

    // Get model component from the found entity (second call for later use)
    fn_InternalBasePtr_CheckPtr(&queryResult);
    foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);
    void* modelCompIBP2[2] = {nullptr, nullptr};
    s_fn_GetModelComp(tempIBP, modelCompIBP2);

    // Get map pointer from game world for grid calculations
    int* mapPtr = *(int**)(gameWorld + 0xf8);

    // Get found entity's position for grid cell calculation
    fn_InternalBasePtr_CheckPtr(&queryResult);
    void* foundEntity2 = queryResult;
    fn_InternalBasePtr_CheckPtr((uint8_t*)foundEntity2 + 0x68);
    void* foundPosHolder = *(void**)((uint8_t*)foundEntity2 + 0x68);

    float foundPosX = *(float*)((uint8_t*)foundPosHolder + 0x158);
    float foundPosY = *(float*)((uint8_t*)foundPosHolder + 0x15c);

    // Get herd position again for the herd's grid cell
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    herdEntity = *(void**)(s + 0x28);
    fn_InternalBasePtr_CheckPtr((uint8_t*)herdEntity + 0x278);
    charComp = *(void**)((uint8_t*)herdEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((uint8_t*)charComp + 0x68);
    posHolder = *(void**)((uint8_t*)charComp + 0x68);

    int mapSize = *mapPtr;
    float herdPosX2 = *(float*)((uint8_t*)posHolder + 0x158);
    float herdPosY2 = *(float*)((uint8_t*)posHolder + 0x15c);

    // Convert world positions to grid cell indices
    float halfSize = (float)(mapSize >> 1);

    // Herd position -> grid cell (toCell)
    int herdCellX = (int)(herdPosX2 + halfSize);
    int herdCellY = (int)(herdPosY2 + halfSize);
    if (herdCellX < 0) herdCellX = 0;
    if (herdCellX >= mapSize) herdCellX = mapSize - 1;
    if (herdCellY < 0) herdCellY = 0;
    if (herdCellY >= mapSize) herdCellY = mapSize - 1;
    int toCell = mapSize * herdCellY + herdCellX;

    // Found entity position -> grid cell (fromCell)
    int foundCellX = (int)(foundPosX + halfSize);
    int foundCellY = (int)(foundPosY + halfSize);
    if (foundCellX < 0) foundCellX = 0;
    if (foundCellX >= mapSize) foundCellX = mapSize - 1;
    if (foundCellY < 0) foundCellY = 0;
    if (foundCellY >= mapSize) foundCellY = mapSize - 1;
    int fromCell = mapSize * foundCellY + foundCellX;

    // Check path cost between the two cells
    uint32_t pathCost = (uint32_t)s_fn_FindPathCost(mapPtr, fromCell, toCell, 0xe1f);
    if ((int)pathCost < 0) {
        // No valid path, bail
        return;
    }

    // Check path validity
    uint64_t pathValid = s_fn_CheckPathValid(mapPtr, 0xe1f, (uint64_t)pathCost, toCell);
    if ((char)pathValid == 0) {
        // Path not valid, bail
        return;
    }

    // Path is valid -- proceed with corpse spawning

    // Get description from the found entity's model component
    fn_InternalBasePtr_CheckPtr(modelCompIBP2);
    void* descObj = *(void**)((uint8_t*)(modelCompIBP2[0]) + 0x18);
    bool isModelDesc = fn_IsDerivedFrom(descObj, *s_typeId_ModelDesc);
    void* modelDesc = isModelDesc ? descObj : nullptr;

    // Store model component IBP for corpse entity ref
    void* corpseModelIBP = modelCompIBP2[0];

    // Convert grid cell to world position
    float gridPos[3];
    float worldPos[3];
    float halfFloat = (float)(mapSize >> 1) - *s_halfOffset;
    int cellIdx = (int)pathCost;
    gridPos[0] = (float)(cellIdx % mapSize) - halfFloat;  // x = col
    gridPos[1] = (float)(cellIdx / mapSize) - halfFloat;  // y = row
    gridPos[2] = 0.0f;

    // Use Vector3 ctor -- note: this is called at 0x140152a30 slot
    fn_Vector3_Ctor(gridPos, gridPos[0], gridPos[1], 0.0f);

    s_fn_GridToWorld(mapPtr, worldPos, gridPos, 0, nullptr);

    // Spawn the corpse -- call RemoveAnimal_Action_Ctor
    // Params: herdIBP(+0x28), animIndex(-1), worldPos, fromCell, corpseIBP, descPtr, flags(0)
    void* herdRef = *(void**)(s + 0x28);
    void* herdIBP_local[2];
    herdIBP_local[0] = herdRef;

    void* corpseIBP_local[2];
    corpseIBP_local[0] = corpseModelIBP;

    s_fn_SpawnCorpse(herdIBP_local, -1, worldPos, fromCell, corpseIBP_local, modelDesc, 0);

    // Find HerdComponent from the found entity and start migration
    fn_InternalBasePtr_CheckPtr(&queryResult);
    foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);

    void* herdCompIBP[2] = {nullptr, nullptr};
    s_fn_FindHerdComp(tempIBP, herdCompIBP);

    fn_InternalBasePtr_CheckPtr(herdCompIBP);

    // Read herd component IBP at +0x28 from the resolved object
    // herdCompIBP[0] IS the resolved object pointer after CheckPtr (no extra deref)
    void* migrationTarget = *(void**)((uintptr_t)herdCompIBP[0] + 0x28);

    fn_InternalBasePtr_CheckPtr(&migrationTarget);
    s_fn_StartMigration(migrationTarget);

    // Clear the animal entity's time at +0x50 and set hunt started flag
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    animalEntity = *(void**)(s + 0x20);
    *(uint64_t*)((uint8_t*)animalEntity + 0x50) = 0;

    *(uint8_t*)(s + 0x38) = 1;
}

// ============================================================
// Slot 9: GetTarget (0x1400c7d60)
// Returns true if param matches the herd entity's parent (+0x28->+0x10)
// or the animal entity's parent (+0x20->+0x10).
// ============================================================
bool HuntAnimal_GetTarget_Impl(HuntAnimal* self, void* param) {
    uint8_t* s = (uint8_t*)self;

    // Check herd entity's parent at +0x28 -> +0x10
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    void* herdEntity = *(void**)(s + 0x28);
    void* herdParent = *(void**)((uint8_t*)herdEntity + 0x10);

    fn_InternalBasePtr_CheckPtr(param);
    if (herdParent == *(void**)param) {
        return true;
    }

    // Check animal entity's parent at +0x20 -> +0x10
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* animalEntity = *(void**)(s + 0x20);
    void* animalParent = *(void**)((uint8_t*)animalEntity + 0x10);

    fn_InternalBasePtr_CheckPtr(param);
    if (animalParent == *(void**)param) {
        return true;
    }

    return false;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fde0 (10 slots), size 0x40, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HuntAnimal_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("HuntAnimal");
    return c;
}

static void* g_HuntAnimal_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&HuntAnimal_GetTypeId,           // +0x10 GetTypeId
    (void*)&HuntAnimal_Serialize_Impl,      // +0x18 Serialize
    (void*)&HuntAnimal_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&HuntAnimal_Execute_Impl,   // +0x28 Execute
    (void*)&HuntAnimal_Enter_Impl,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&HuntAnimal_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterHuntAnimalVtable() {
    RegisterExeAddr(0x14015fde0, (uintptr_t)g_HuntAnimal_Vtable);
}
extern "C" void* HuntAnimal_CreateInPlace(void* mem) {
    HuntAnimal_ResolveFuncs();
    return Action_InitInPlace(mem, 0x40, g_HuntAnimal_Vtable);
}
extern "C" void* HuntAnimal_Factory(void) {
    HuntAnimal_ResolveFuncs();
    return Action_Alloc(0x40, g_HuntAnimal_Vtable);
}
REGISTER_TYPE(HuntAnimal, "Action", &HuntAnimal_Factory, &HuntAnimal_CreateInPlace);
#endif // STANDALONE_EXE
