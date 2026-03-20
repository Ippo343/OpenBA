// code/Game/Work/herbalistwork.cpp
// Clean C++ reimplementation of HerbalistWork — all methods.
// Source: Game/Work/HerbalistWork.cpp (from profile build debug strings)
//
// HerbalistWork (0x60 bytes) — Herbalist gathering work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HerbalistWork
// Vtable: 0x1401615a0
// Same layout as ForesterWork (different vtable, different work logic).
//
// Key difference from ForesterWork: no plant/cut branching. Herbalist only
// searches for existing natural resources (herbs) that match the description
// list, checks growth threshold, then creates a PerformWork action to harvest.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "Work/herbalistwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// ============================================================
// Local function pointers (resolved once by HerbalistWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with HW_ to avoid conflicts

typedef int   (*HW_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
typedef void* (*HW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HW_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void  (*HW_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
typedef int   (*HW_CalcMaxWorkers_t)(uintptr_t matDesc);
typedef float (*HW_Sqrtf_t)(float val);
typedef int   (*HW_Entity_GetGridCell_t)(void* entityIBP, float* outPos);
typedef int   (*HW_Map_CalcDistance_t)(int* mapGrid, uint32_t cellA, uint32_t cellB);
typedef int   (*HW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*HW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
typedef void* (*HW_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);
typedef float*(*HW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
typedef void  (*HW_PerformWork_Ctor3_t)(void* entityIBP, int priority, void* targetIBP,
                                         float* position, int flag, uint8_t param6);
typedef void  (*HW_PlayAnimation_Ctor_t)(void* entityIBP, int priority, int animIndex);
typedef void  (*HW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
typedef void  (*HW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
typedef void* (*HW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
typedef int   (*HW_StorageQueryCount_t)(void* storageComp);
typedef int   (*HW_Random_RandTol_t)(void* rng, int range);
typedef float*(*HW_Vector3_Ctor_t)(float* self, float x, float y, float z);

// Work::Serialize
typedef void  (*HW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor
typedef void* (*HW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*HW_Serializer_Dtor_t)(void* self);
// CitizenAI_impl_96b0 — BST serialization
typedef void  (*HW_BST_Serialize_t)(void* bstRoot, void* serializer);
// IO::Serializer::Serialize(int*)
typedef void  (*HW_Serializer_SerializeInt_t)(void* serializer, int* value, char* name);

// FUN_1400f7100 — validate work entity
typedef bool  (*HW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// System::MemoryAllocator::MemAlloc / MemFree
typedef void* (*HW_MemAlloc_t)(int64_t size, int align);
typedef void  (*HW_MemFree_t)(void* ptr);
// CitizenAI_impl_a6e0 — BST node remove
typedef void  (*HW_BSTRemove_t)(void* bstRoot, void* node);
// CitizenAI_impl_5520 — BST find-and-remove by key
typedef void  (*HW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// FUN_1400c5400 — cancel all work items in BST
typedef void  (*HW_CancelAllWork_t)(void* bstRoot);
// FUN_140002e10 — find sub-entity
typedef void* (*HW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// Object::Internal::~Internal (dtor tail call)
typedef void  (*HW_Internal_Dtor_t)(void* self);
// Tree node cleanup
typedef void  (*HW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// fn_MemFreeStatic
typedef void  (*HW_MemFreeStatic_t)(void* ptr);

static HW_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;
static HW_FindComp_MatDesc_t            s_fn_FindComp_MatDesc = nullptr;
static HW_FindComp_MeshInstance_t       s_fn_FindComp_MeshInstance = nullptr;
static HW_MeshInstance_SetVisible_t     s_fn_MeshInstance_SetVisible = nullptr;
static HW_CalcMaxWorkers_t              s_fn_CalcMaxWorkers = nullptr;
static HW_Sqrtf_t                       s_fn_Sqrtf = nullptr;
static HW_Entity_GetGridCell_t          s_fn_Entity_GetGridCell = nullptr;
static HW_Map_CalcDistance_t            s_fn_Map_CalcDistance = nullptr;
static HW_Map_FindNearestPassable_t    s_fn_Map_FindNearestPassable = nullptr;
static HW_Map_CanPathTo_t              s_fn_Map_CanPathTo = nullptr;
static HW_FindComp_NatRes_t             s_fn_FindComp_NatRes = nullptr;
static HW_GetTerrainPos_t              s_fn_GetTerrainPos = nullptr;
static HW_PerformWork_Ctor3_t          s_fn_PerformWork_Ctor3 = nullptr;
static HW_PlayAnimation_Ctor_t         s_fn_PlayAnimation_Ctor = nullptr;
static HW_CitizenAI_RegisterWork_t     s_fn_CitizenAI_RegisterWork = nullptr;
static HW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static HW_FindComp_Storage_t           s_fn_FindComp_Storage = nullptr;
static HW_StorageQueryCount_t          s_fn_StorageQueryCount = nullptr;
static HW_Random_RandTol_t             s_fn_Random_RandTol = nullptr;
static HW_Vector3_Ctor_t              s_fn_Vector3_Ctor = nullptr;
static HW_Work_Serialize_t             s_fn_Work_Serialize = nullptr;
static HW_Serializer_Ctor_t            s_fn_Serializer_Ctor = nullptr;
static HW_Serializer_Dtor_t            s_fn_Serializer_Dtor = nullptr;
static HW_BST_Serialize_t              s_fn_BST_Serialize = nullptr;
static HW_Serializer_SerializeInt_t    s_fn_Serializer_SerializeInt = nullptr;
static HW_ValidateEntity_t             s_fn_ValidateEntity = nullptr;
static HW_MemAlloc_t                   s_fn_MemAlloc = nullptr;
static HW_MemFree_t                    s_fn_MemFree = nullptr;
static HW_BSTRemove_t                  s_fn_BSTRemove = nullptr;
static HW_BSTFindRemove_t              s_fn_BSTFindRemove = nullptr;
static HW_CancelAllWork_t              s_fn_CancelAllWork = nullptr;
static HW_FindSubEntity_t              s_fn_FindSubEntity = nullptr;
static HW_TreeNodeCleanup_t            s_fn_TreeNodeCleanup = nullptr;
static HW_MemFreeStatic_t              s_fn_MemFreeStatic = nullptr;

// Type ID constants
static uint32_t* s_typeId_WorkplaceComponent = nullptr;  // DAT_1401c43a0
static uint32_t* s_typeId_RadiusComponent = nullptr;      // DAT_1401c43d8
static uint32_t* s_typeId_MaterialComponent = nullptr;     // DAT_1401c4870
// Note: NatResComponent type ID not needed — FindComp_NatRes handles it internally

// Float constants from .rdata
static float* s_fPI = nullptr;             // DAT_140172184 = 3.14159265f
static float* s_fHalfCellOffset = nullptr; // DAT_140172114 = 0.5f

// Growth threshold constant (g_fUVScale)
static float* s_fGrowthThreshold = nullptr; // DAT_14017214c = 1.0f

// Vtable pointers for dtor chain
static void** s_vtable_HerbalistWork = nullptr;  // 0x1401615a0
static void** s_vtable_AutoPickupWork = nullptr;  // 0x140161340 (from dtor body)
static void** s_vtable_Work = nullptr;             // 0x1401612e0 (from dtor body)

static bool s_herbalist_funcs_resolved = false;

// ============================================================
// Resolution — called once during hook installation
// ============================================================
bool HerbalistWork_ResolveFuncs() {
    if (s_herbalist_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<HW_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<HW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<HW_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<HW_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<HW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Sqrtf = reinterpret_cast<HW_Sqrtf_t>(
        ResolveAddr(Module::Exe, 0x140132010));
    s_fn_Entity_GetGridCell = reinterpret_cast<HW_Entity_GetGridCell_t>(
        ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_Map_CalcDistance = reinterpret_cast<HW_Map_CalcDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_FindComp_NatRes = reinterpret_cast<HW_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_GetTerrainPos = reinterpret_cast<HW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_PerformWork_Ctor3 = reinterpret_cast<HW_PerformWork_Ctor3_t>(
        ResolveAddr(Module::Exe, 0x1400c1eb0));
    s_fn_PlayAnimation_Ctor = reinterpret_cast<HW_PlayAnimation_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400c2490));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<HW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<HW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Storage = reinterpret_cast<HW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<HW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_Work_Serialize = reinterpret_cast<HW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize = reinterpret_cast<HW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14001a6b0));  // CitizenAI_impl_96b0 renamed
    s_fn_ValidateEntity = reinterpret_cast<HW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_BSTRemove = reinterpret_cast<HW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_BSTFindRemove = reinterpret_cast<HW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_CancelAllWork = reinterpret_cast<HW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<HW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_TreeNodeCleanup = reinterpret_cast<HW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatRandTol = ResolveAddr(Module::Exe, 0x1401512f0);
    if (iatRandTol)
        s_fn_Random_RandTol = *reinterpret_cast<HW_Random_RandTol_t*>(iatRandTol);
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<HW_Vector3_Ctor_t*>(iatVec3Ctor);
    // CheckPtr already from engine_funcs.h (no local copy needed)

    // Serializer ctor/dtor from dispatch slots
    uintptr_t iatSerCtorSlot = ResolveAddr(Module::Exe, 0x140151500);
    if (iatSerCtorSlot)
        s_fn_Serializer_Ctor = *reinterpret_cast<HW_Serializer_Ctor_t*>(iatSerCtorSlot);
    uintptr_t iatSerDtorSlot = ResolveAddr(Module::Exe, 0x140151508);
    if (iatSerDtorSlot)
        s_fn_Serializer_Dtor = *reinterpret_cast<HW_Serializer_Dtor_t*>(iatSerDtorSlot);
    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140151528);
    if (iatSerInt)
        s_fn_Serializer_SerializeInt = *reinterpret_cast<HW_Serializer_SerializeInt_t*>(iatSerInt);

    // MemAlloc/MemFree from dispatch slots
    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140151460);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<HW_MemAlloc_t*>(iatMemAlloc);
    uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x140151468);
    if (iatMemFree)
        s_fn_MemFree = *reinterpret_cast<HW_MemFree_t*>(iatMemFree);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<HW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID pointers
    s_typeId_WorkplaceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c43a0));
    s_typeId_RadiusComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c43d8));
    s_typeId_MaterialComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4870));

    // Float constants from .rdata
    s_fPI = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172184));
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_fGrowthThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));

    // Vtable pointers for dtor
    s_vtable_HerbalistWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401615a0));
    s_vtable_AutoPickupWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161340));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401612e0));

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_Sqrtf &&
              s_fn_Entity_GetGridCell &&
              s_fn_Map_CalcDistance && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_FindComp_NatRes &&
              s_fn_GetTerrainPos && s_fn_PerformWork_Ctor3 &&
              s_fn_PlayAnimation_Ctor && s_fn_CitizenAI_RegisterWork &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Storage &&
              s_fn_StorageQueryCount && s_fn_Random_RandTol &&
              s_fn_Vector3_Ctor && s_fn_Work_Serialize &&
              s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_SerializeInt &&
              s_fn_ValidateEntity && s_fn_BSTRemove &&
              s_fn_BSTFindRemove && s_fn_CancelAllWork &&
              s_fn_FindSubEntity && s_fn_TreeNodeCleanup &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_typeId_WorkplaceComponent && s_typeId_RadiusComponent &&
              s_typeId_MaterialComponent &&
              s_fPI && s_fHalfCellOffset && s_fGrowthThreshold;

    s_herbalist_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[HerbalistWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_Sqrtf) printf("  MISSING: Sqrtf\n");
        if (!s_fn_Entity_GetGridCell) printf("  MISSING: Entity_GetGridCell\n");
        if (!s_fn_Map_CalcDistance) printf("  MISSING: Map_CalcDistance\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_FindComp_NatRes) printf("  MISSING: FindComp_NatRes\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_PerformWork_Ctor3) printf("  MISSING: PerformWork_Ctor3\n");
        if (!s_fn_PlayAnimation_Ctor) printf("  MISSING: PlayAnimation_Ctor\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: StorageQueryCount\n");
        if (!s_fn_Random_RandTol) printf("  MISSING: Random_RandTol\n");
        if (!s_fn_Vector3_Ctor) printf("  MISSING: Vector3_Ctor\n");
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_SerializeInt) printf("  MISSING: Serializer_SerializeInt\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
    }
#endif

    return ok;
}


// ============================================================
// Helper: Find component by type ID in entity's component list
// Iterates array at (entity+0x30), count at (entity+0x38).
// Each entry is an InternalBasePtr; vtable+0x10 = GetTypeId().
// Returns the component pointer, or nullptr if not found.
// ============================================================
static void* FindComponentInList(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t arr = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t* ibpPtr = (uintptr_t*)(arr + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(ibpPtr);
        uintptr_t obj = *ibpPtr;
        uintptr_t vtable = *(uintptr_t*)obj;
        typedef uint32_t (*GetTypeId_t)();
        auto getTypeId = *(GetTypeId_t*)(vtable + 0x10);
        if (getTypeId() == typeId) {
            ibpPtr = (uintptr_t*)(arr + (uintptr_t)i * 8);
            fn_InternalBasePtr_CheckPtr(ibpPtr);
            void* comp = (void*)*ibpPtr;
            if (fn_IsDerivedFrom(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: Find component by type ID in sub-object's component list
// Uses the +0x28/+0x30 layout (different from entity main list).
// ============================================================
static void* FindSubComponentInList(uintptr_t subObj, uint32_t typeId) {
    uintptr_t listBase = *(uintptr_t*)(subObj + 0x28);
    int count = *(int*)(subObj + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t obj = *(uintptr_t*)(listBase + (uintptr_t)i * 8);
        uintptr_t vtable = *(uintptr_t*)obj;
        typedef uint32_t (*GetTypeId_t)();
        auto getTypeId = *(GetTypeId_t*)(vtable + 0x10);
        if (getTypeId() == typeId) {
            void* comp = *(void**)(listBase + (uintptr_t)i * 8);
            if (fn_IsDerivedFrom(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}


// ============================================================
// Helper: Assign work and register with AI
// ============================================================
static void AssignWorkToEntity(HerbalistWork* self, void* citizenIBP, int priority) {
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizen = *(uintptr_t*)citizenIBP;

    // Call vtable slot +0x50 (GetWorkEntity) on self
    uintptr_t vtable = *(uintptr_t*)self;
    typedef uintptr_t* (*GetWorkEntity_t)(HerbalistWork* self, void* outIBP);
    auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
    uintptr_t workEntityIBP[2] = {0};
    uintptr_t* result = getWorkEntity(self, workEntityIBP);
    uintptr_t workEntity = *result;

    // Set entity flags
    *(uint32_t*)(citizen + 0x54) |= 4;
    *(uint32_t*)(citizen + 0xb0) = 0;
    *(int32_t*)(citizen + 0x30) = priority;

    // If work entity matches citizen's home entity (+0x1f0) and priority >= 0,
    // set the "owned work" flag
    if (workEntity == *(uintptr_t*)(citizen + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizen + 0x88) |= 0x20000;
    }

    // Register work with citizen AI subsystem at self+0x48
    uintptr_t citizenAISub = (uintptr_t)self + 0x48;
    s_fn_CitizenAI_RegisterWork((void*)citizenAISub, workEntityIBP, citizenIBP);
}


// ============================================================
// HerbalistWork::Slot4 — Main work-assignment logic
// ============================================================
//
// Algorithm (differs from ForesterWork — no plant/cut branching):
// 1. Check if citizen already has auto-pickup assignments
// 2. Find WorkplaceComponent, RadiusComponent, MaterialComponent
// 3. Check if material count has reached capacity (return 1 = done if so)
// 4. Calculate search radius from workplace radius
// 5. Random iteration over grid cells in radius
// 6. For each valid cell:
//    a. Skip if terrain is forest/resource terrain (0x1e0 mask)
//    b. Check distance within 2x radius
//    c. If cell has a resource entity, find NatRes component
//    d. For each herb type in the description list:
//       - Check growth >= threshold
//       - Check resource description matches
//       - Check not flagged for removal
//       - Find nearest passable cell, check pathfinding
//       - Create PerformWork action to harvest
// 7. If no work found, handle failure (idle animation or replan)
//
int HerbalistWork_Slot4_Impl(HerbalistWork* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Save citizen ref for later use
    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments
    // -------------------------------------------------------
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &citizenRef);
    if (checkResult == 2) {
        return 2;
    }

    // -------------------------------------------------------
    // Step 2: Find components on the workplace entity
    // -------------------------------------------------------

    // Get material description from workplace entity (self+0x10)
    uintptr_t* plVar14 = (uintptr_t*)(selfAddr + 0x10);
    fn_InternalBasePtr_CheckPtr(plVar14);
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*plVar14, &matDescIBP);

    // Find WorkplaceComponent on workplace entity
    fn_InternalBasePtr_CheckPtr(plVar14);
    uintptr_t entity = *plVar14;
    void* workplaceComp = FindComponentInList(entity, *s_typeId_WorkplaceComponent);

    // Wrap in InternalBasePtr
    uintptr_t workplaceIBP = 0;
    fn_InternalBasePtr_Ctor(&workplaceIBP, workplaceComp);

    // Get RadiusComponent from workplace description (+0x18)
    fn_InternalBasePtr_CheckPtr(&workplaceIBP);
    void* workplaceDesc = *(void**)(workplaceIBP + 0x18);
    void* radiusComp = nullptr;
    if (fn_IsDerivedFrom(workplaceDesc, *s_typeId_RadiusComponent)) {
        radiusComp = workplaceDesc;
    }

    // Find MaterialComponent on workplace's sub-object (+0x28)
    fn_InternalBasePtr_CheckPtr(plVar14);
    uintptr_t entitySub = *(uintptr_t*)(*plVar14 + 0x28);
    void* materialComp = FindSubComponentInList(entitySub, *s_typeId_MaterialComponent);

    // -------------------------------------------------------
    // Step 3: Check capacity — is material count at limit?
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)radiusComp + 0x30);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // Set mesh visibility based on capacity (visGroup 2)
    fn_InternalBasePtr_CheckPtr(plVar14);
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*plVar14, meshIBP);
    fn_InternalBasePtr_CheckPtr(meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    // Get herb count (self->_field50 via qword index 10)
    int herbCount = (int)selfQ[10]; // self->_field50
    fn_InternalBasePtr_CheckPtr(&matDescIBP);

    // If material description count is 0, calculate it
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    // If herbCount exceeds max workers OR at capacity, we're done (return 1)
    if (*(int*)(matDescIBP + 0x30) < herbCount || atCapacity) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 4: Calculate search parameters
    // -------------------------------------------------------
    void* rng = *(void**)(gameWorld + 0xd8);
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    int workplaceRadius = *(int*)((uintptr_t)materialComp + 0x28);
    int radiusSq = workplaceRadius * workplaceRadius;
    float searchArea = s_fn_Sqrtf((float)radiusSq * (*s_fPI));
    int mapSize = *mapGrid;
    int maxAttempts = (int)searchArea;

    // Workplace grid cell position
    int workplaceCell = (int)selfQ[5]; // self->_field28
    int cellRow = workplaceCell / mapSize;
    int cellCol = workplaceCell % mapSize;

    // Get citizen's current position
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(aiComp + 0x68));
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));

    // Clamp citizen cell to map bounds
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;

    int citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Get entity's grid cell for distance calculations
    uintptr_t entityIBPBuf[2] = { *plVar14, 0 };
    uint32_t entityGridCell = s_fn_Entity_GetGridCell(entityIBPBuf, nullptr);

    float fGrowthVal = *s_fGrowthThreshold;

    // -------------------------------------------------------
    // Step 5: Random search for herb spots
    // -------------------------------------------------------
    if (maxAttempts > 0) {
        for (int attempt = 0; attempt < maxAttempts; attempt++) {
            int randX, randY;
            int testCellX, testCellY;

            // Generate random position within radius (rejection sampling)
            do {
                do {
                    randX = s_fn_Random_RandTol(rng, workplaceRadius);
                    randY = s_fn_Random_RandTol(rng, workplaceRadius);
                    testCellX = cellCol + randX;
                    testCellY = cellRow + randY;
                } while (randY * randY + randX * randX > radiusSq);
            } while (testCellX < 0 || testCellX >= mapSize ||
                     testCellY < 0 || testCellY >= mapSize);

            uint32_t testCell = (uint32_t)(*mapGrid) * testCellY + testCellX;
            uintptr_t gridData = *(uintptr_t*)((uintptr_t)mapGrid + 0x80); // mapGrid+0x20 as qword ptr = offset 0x80

            // Check terrain type bitmask — skip "forest/resource" terrain (0x1e0 mask)
            uint8_t terrainByte = *(uint8_t*)(gridData + (uintptr_t)testCell * 0x30 + 0x29);
            uint32_t terrainBit = 1u << (terrainByte & 0x1f);
            if ((terrainBit & 0x1e0u) != 0) {
                continue;  // Terrain already has resources, skip
            }

            // Check distance is within 2x workplace radius
            int dist = s_fn_Map_CalcDistance(mapGrid, entityGridCell, testCell);
            if (dist > workplaceRadius * 2) {
                continue;
            }

            // Get resource entity at this cell (+8 in cell data)
            uintptr_t resourceEntity = *(uintptr_t*)(gridData + (uintptr_t)testCell * 0x30 + 8);
            if (resourceEntity == 0) {
                continue;  // No resource at this cell
            }

            // -------------------------------------------------------
            // Found a resource entity — check if it's a valid herb
            // -------------------------------------------------------
            fn_InternalBasePtr_CheckPtr(&resourceEntity);
            uintptr_t natResCompIBP[2] = {0};
            s_fn_FindComp_NatRes(resourceEntity, natResCompIBP);

            if (*(uintptr_t*)natResCompIBP == 0) {
                continue;  // No NatRes component
            }

            // Check NatRes component against the description list
            fn_InternalBasePtr_CheckPtr(&resourceEntity);
            uintptr_t natResCompIBP2[2] = {0};
            s_fn_FindComp_NatRes(resourceEntity, natResCompIBP2);

            // Iterate through herb types in the description list (radiusComp->+0x20, count at radiusComp->+0x28)
            int herbTypeCount = *(int*)((uintptr_t)radiusComp + 0x28);
            uintptr_t herbTypeList = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);

            for (int herbIdx = 0; herbIdx < herbTypeCount; herbIdx++) {
                // Check growth >= threshold
                fn_InternalBasePtr_CheckPtr(natResCompIBP2);
                float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + 0x28);
                if (growth < fGrowthVal) {
                    continue;
                }

                // Check resource description matches this herb type
                fn_InternalBasePtr_CheckPtr(&resourceEntity);
                uintptr_t resDescRef = *(uintptr_t*)(resourceEntity + 0x28);
                uintptr_t herbTypeRef = *(uintptr_t*)(herbTypeList + (uintptr_t)herbIdx * 8);
                if (resDescRef != herbTypeRef) {
                    continue;
                }

                // Check not flagged for removal (bit 20 of +0x20)
                fn_InternalBasePtr_CheckPtr(natResCompIBP2);
                uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResCompIBP2 + 0x20);
                if ((flags20 >> 20) & 1) {
                    continue;
                }

                // Find nearest passable cell for pathfinding
                int nearestCell = s_fn_Map_FindNearestPassable(
                    mapGrid, testCell, citizenGridCell, 0xe07);
                if (nearestCell < 0) {
                    continue;
                }

                // Check path to the nearest passable cell
                bool canPath = s_fn_Map_CanPathTo(
                    mapGrid, 0xe1f, nearestCell, citizenGridCell);
                if (!canPath) {
                    continue;
                }

                // -------------------------------------------------------
                // Valid herb found — create PerformWork action
                // -------------------------------------------------------

                // Calculate world position for nearest cell
                float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                float cutPos[3];
                s_fn_Vector3_Ctor(cutPos,
                    (float)(nearestCell % *mapGrid) - halfOffset,
                    (float)(nearestCell / *mapGrid) - halfOffset,
                    0.0f);

                // Get terrain height
                float pathPos[3];
                s_fn_GetTerrainPos(mapGrid, pathPos, cutPos, 0, nullptr);

                // Create PerformWork action (harvest herb)
                fn_InternalBasePtr_CheckPtr(natResCompIBP2);
                uintptr_t targetRef = *(uintptr_t*)natResCompIBP2;
                uintptr_t harvestEntityRef = *(uintptr_t*)citizenIBP;
                uintptr_t harvestBuf[2] = { harvestEntityRef, 0 };
                uintptr_t harvestTargetBuf[2] = { targetRef, 0 };
                s_fn_PerformWork_Ctor3(
                    harvestBuf,                         // entityIBP
                    (int)selfQ[4],                      // priority (workplaceId)
                    harvestTargetBuf,                    // targetIBP
                    pathPos,                             // position
                    0,                                   // flag
                    1);                                  // param6

                // Reset fail counter and assign work
                *(uint32_t*)(selfAddr + 0x58) = 0;
                AssignWorkToEntity(self, citizenIBP, (int)selfQ[4]);
                return 2;
            }
        }
    }

    // -------------------------------------------------------
    // Step 7: No work found — handle failure
    // -------------------------------------------------------
    int failCount = (int)selfQ[0xb]; // self->_field58

    if (failCount > maxAttempts) {
        // Too many failures — check if citizen's storage has items, then replan
        fn_InternalBasePtr_CheckPtr(citizenIBP);
        uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

        fn_InternalBasePtr_CheckPtr((void*)(citizenEnt + 0x10));
        uintptr_t storageIBP[2] = {0};
        s_fn_FindComp_Storage(*(uintptr_t*)(citizenEnt + 0x10), storageIBP);

        fn_InternalBasePtr_CheckPtr(storageIBP);
        int storageCount = s_fn_StorageQueryCount((void*)*(uintptr_t*)storageIBP);
        if (storageCount < 1) {
            return 0;
        }

        // Citizen has items in storage — replan long term
        fn_InternalBasePtr_CheckPtr(citizenIBP);
        s_fn_CitizenAI_LongTermPlan(*(void**)citizenIBP);
    }
    else {
        // Play idle animation and increment fail counter
        uintptr_t idleEntityRef = *(uintptr_t*)citizenIBP;
        uintptr_t idleBuf[2] = { idleEntityRef, 0 };
        s_fn_PlayAnimation_Ctor(idleBuf, (int)selfQ[4], 0);

        // Assign work (idle animation) and register with AI
        int priority = (int)selfQ[4];
        fn_InternalBasePtr_CheckPtr(citizenIBP);
        uintptr_t citizenEnt2 = *(uintptr_t*)citizenIBP;

        // Call vtable slot +0x50 (GetWorkEntity)
        uintptr_t vtable = *(uintptr_t*)self;
        typedef uintptr_t* (*GetWorkEntity_t)(HerbalistWork*, void*);
        auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
        uintptr_t workEntIBP[2] = {0};
        uintptr_t* wResult = getWorkEntity(self, workEntIBP);
        uintptr_t workEnt = *wResult;

        *(uint32_t*)(citizenEnt2 + 0x54) |= 4;
        *(int32_t*)(citizenEnt2 + 0x30) = priority;
        *(uint32_t*)(citizenEnt2 + 0xb0) = 0;

        if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && priority >= 0) {
            *(uint32_t*)(citizenEnt2 + 0x88) |= 0x20000;
        }

        uintptr_t citizenAISub = (uintptr_t)self + 0x48;
        s_fn_CitizenAI_RegisterWork((void*)citizenAISub, workEntIBP, citizenIBP);

        // Increment fail counter
        *(int32_t*)(selfAddr + 0x58) = failCount + 1;
    }

    return 2;
}


// ============================================================
// HerbalistWork::Slot6 — NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// Calls BSTFindRemove on the BST at self+0x48, returns false.
// ============================================================
bool HerbalistWork_Slot6_Impl(HerbalistWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// HerbalistWork::Slot7 — 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// ============================================================
int HerbalistWork_Slot7_Impl(HerbalistWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return 0;
}


// ============================================================
// HerbalistWork::Slot8 / CancelAll
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each rightmost node, cancels the work item via engine call.
// ============================================================
int HerbalistWork_Slot8_Impl(HerbalistWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find rightmost node in BST
        void* node = *(void**)(selfAddr + 0x48);
        void* rightmost = node;
        while (node != nullptr) {
            rightmost = node;
            // Follow right child at +0x10 (descend to rightmost leaf)
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        fn_InternalBasePtr_CheckPtr(rightmost);
        uintptr_t citizenEntity = *(uintptr_t*)rightmost;

        // Navigate to citizen AI subsystem and cancel
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HerbalistWork::Slot9 / RemoveInvalid (164 instructions)
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if tree becomes empty, false otherwise.
// ============================================================
bool HerbalistWork_Slot9_Impl(HerbalistWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    void** bstRootPtr = (void**)((uintptr_t)self + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;  // Empty tree — nothing to validate
    }

    // Find leftmost node (minimum)
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Validate this entity
        bool stillValid = s_fn_ValidateEntity(selfAddr, param2, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Add to removal list — grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                uintptr_t* newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = (uintptr_t*)s_fn_MemAlloc((int64_t)newCap * 8, 8);
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    uintptr_t val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = entityIBP;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    fn_MemFreeStatic(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        void* rightChild = *(void**)((uintptr_t)node + 0x18);
        if (rightChild != nullptr) {
            // Go right, then all the way left
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            // Go up until we come from a left child
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
                // We came from the right — keep going up
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x08);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            // Find and remove the node with this key from BST
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                fn_InternalBasePtr_CheckPtr(searchNode);
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                } else {
                    fn_InternalBasePtr_CheckPtr(&removeArr[i]);
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_BSTRemove(bstRootPtr, searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return removeCount == 0;
}


// ============================================================
// HerbalistWork::Slot10 / GetWorkEntity
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* HerbalistWork_Slot10_Impl(HerbalistWork* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on self+0x10
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    // Find sub-entity via FUN_140002e10
    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    // CheckPtr on the result
    fn_InternalBasePtr_CheckPtr(result);

    // Read +0x48 from the resolved entity and store in output
    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// HerbalistWork::Serialize
// Work::Serialize + Serializer(version=0) + BST serialize + int field
// ============================================================
void HerbalistWork_Serialize_Impl(HerbalistWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create first serializer (version block, version=0)
    uint8_t serializer1[24] = {0};
    s_fn_Serializer_Ctor(serializer1, stream, 0);
    s_fn_Serializer_Dtor(serializer1);

    // Create second serializer (version=0) for BST + int
    uint8_t serializer2[24] = {0};
    s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Serialize BST at self+0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializer2);

    // Serialize int at self+0x58
    s_fn_Serializer_SerializeInt(serializer2, (int*)(selfAddr + 0x58), nullptr);

    s_fn_Serializer_Dtor(serializer2);
}


// ============================================================
// HerbalistWork::DtorBody (0x1400EEC40)
// Sets vtables down chain, frees tree nodes at +0x48, then calls
// AutoPickupWork -> Work -> Internal dtor chain.
// ============================================================
void HerbalistWork_DtorBody(HerbalistWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to HerbalistWork
    *(void**)selfAddr = s_vtable_HerbalistWork;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to AutoPickupWork, then Work, then call Internal dtor
    *(void**)selfAddr = s_vtable_AutoPickupWork;
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// HerbalistWork::ScalarDtor (0x14002A640)
// Calls dtor body, then conditional free based on flags.
// ============================================================
void* HerbalistWork_ScalarDtor_Impl(HerbalistWork* self, uint32_t flags) {
    HerbalistWork_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}
