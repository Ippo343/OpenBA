// code/Game/Work/foresterwork.cpp
// Clean C++ reimplementation of ForesterWork::Slot4 — main work-assignment logic.
// Source: Game/Work/ForesterWork.cpp (from profile build debug strings)
//
// ForesterWork (0x60 bytes) — Forester resource gathering work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> ForesterWork
// Vtable: 0x1401613a0
//
// Slot4 (0x1400EAEC0) — 599 instructions, the largest single function across all Work classes.
// Finds nearby trees, checks terrain, assigns forester to cut/plant tasks.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/exe_bodies.h"
#include "Work/ForesterWork.h"
#include "Work/foresterwork_funcs.h"
#include "options.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/ForesterComponent.h"             // ENDGAME B.3.9 D5
#include "Component/ForesterDescription.h"           // ENDGAME B.3.9 D5
#include "Component/RadiusDescription.h"             // ENDGAME B.3.9 D5
#include "Component/NaturalResourceDescription.h"    // ENDGAME B.3.9 D5

// ============================================================
// Local function pointers (resolved once by ForesterWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with FW_ to avoid conflicts with engine_funcs.h

// BST serialization
typedef void  (*FW_BST_Serialize_t)(void* bstRoot, void* serializer);
// FUN_1400f7100 -- validate work entity
typedef char  (*FW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*FW_MemAlloc_t)(int64_t size, int align);
typedef void  (*FW_MemFreeFunc_t)(void* ptr);
// BST node remove
typedef void  (*FW_BSTRemove_t)(void* bstRoot, void* node);
// BST find-and-remove by key
typedef void  (*FW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// Cancel all work items (FUN_1400c5400)
typedef void  (*FW_CancelAllWork_t)(void* smPtr);
// Find sub-entity (FUN_140002e10)
typedef void* (*FW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// Tree node cleanup (FUN_140014c30)
typedef void  (*FW_TreeNodeCleanup_t)(void* bstRoot, void* node);

typedef int   (*FW_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
typedef void* (*FW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*FW_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void  (*FW_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
typedef int   (*FW_CalcMaxWorkers_t)(uintptr_t matDesc);
typedef float (*FW_Sqrtf_t)(float val);
typedef void* (*FW_FindComp_NatResDesc_t)(uintptr_t compList, void* outPtr);
typedef int   (*FW_Entity_GetGridCell_t)(void* entityIBP, float* outPos);
typedef void  (*FW_Map_Invalidate_t)(int* mapGrid);
typedef int   (*FW_Map_CalcDistance_t)(int* mapGrid, uint32_t cellA, uint32_t cellB);
typedef int   (*FW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*FW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
typedef void* (*FW_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);
typedef void* (*FW_FindComp_RawMaterial_t)(uintptr_t compList, void* outPtr);
typedef bool  (*FW_CheckTerrainValid_t)(int* mapGrid, int cellX, int cellY, uint32_t terrainType,
                                        float minHeight, float maxHeight, float elevation,
                                        char allowOccupied, char allowRoad, uintptr_t entityFilter);
typedef float*(*FW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
typedef bool  (*FW_CheckResourceDensity_t)(void* natResDesc, int cellX, int cellY);
typedef void  (*FW_CreatePlantAction_t)(void* entityIBP, int priority, uintptr_t matDescRef,
                                         uint32_t param4, uint32_t* pathPos, uint32_t cellIdx,
                                         int param7, uintptr_t* resourcePtr, uint32_t* resCountPtr,
                                         uint64_t param10, int param11);
typedef void  (*FW_PlayAnimation_Ctor_t)(void* entityIBP, int priority, int animIndex);
typedef void  (*FW_PerformWork_Ctor3_t)(void* entityIBP, int priority, void* targetIBP,
                                         float* position, int flag, uint8_t param6);
typedef void  (*FW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
typedef void  (*FW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
typedef void* (*FW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
typedef int   (*FW_StorageQueryCount_t)(void* storageComp);

static FW_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;
static FW_FindComp_MatDesc_t            s_fn_FindComp_MatDesc = nullptr;
static FW_FindComp_MeshInstance_t       s_fn_FindComp_MeshInstance = nullptr;
static FW_MeshInstance_SetVisible_t     s_fn_MeshInstance_SetVisible = nullptr;
static FW_CalcMaxWorkers_t              s_fn_CalcMaxWorkers = nullptr;
static FW_Sqrtf_t                       s_fn_Sqrtf = nullptr;
static FW_FindComp_NatResDesc_t         s_fn_FindComp_NatResDesc = nullptr;
static FW_Entity_GetGridCell_t          s_fn_Entity_GetGridCell = nullptr;
static FW_Map_Invalidate_t             s_fn_Map_Invalidate = nullptr;
static FW_Map_CalcDistance_t            s_fn_Map_CalcDistance = nullptr;
static FW_Map_FindNearestPassable_t    s_fn_Map_FindNearestPassable = nullptr;
static FW_Map_CanPathTo_t              s_fn_Map_CanPathTo = nullptr;
static FW_FindComp_NatRes_t             s_fn_FindComp_NatRes = nullptr;
static FW_FindComp_RawMaterial_t        s_fn_FindComp_RawMaterial = nullptr;
static FW_CheckTerrainValid_t           s_fn_CheckTerrainValid = nullptr;
static FW_GetTerrainPos_t              s_fn_GetTerrainPos = nullptr;
static FW_CheckResourceDensity_t       s_fn_CheckResourceDensity = nullptr;
static FW_CreatePlantAction_t          s_fn_CreatePlantAction = nullptr;
static FW_PlayAnimation_Ctor_t         s_fn_PlayAnimation_Ctor = nullptr;
static FW_PerformWork_Ctor3_t          s_fn_PerformWork_Ctor3 = nullptr;
static FW_CitizenAI_RegisterWork_t     s_fn_CitizenAI_RegisterWork = nullptr;
static FW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static FW_FindComp_Storage_t           s_fn_FindComp_Storage = nullptr;
static FW_StorageQueryCount_t          s_fn_StorageQueryCount = nullptr;
static FW_BST_Serialize_t             s_fn_BST_Serialize = nullptr;
static FW_ValidateEntity_t            s_fn_ValidateEntity = nullptr;
static FW_MemAlloc_t                  s_fn_MemAlloc = nullptr;
static FW_MemFreeFunc_t               s_fn_MemFreeFunc = nullptr;
static FW_BSTRemove_t                 s_fn_BSTRemove = nullptr;
static FW_BSTFindRemove_t             s_fn_BSTFindRemove = nullptr;
static FW_CancelAllWork_t             s_fn_CancelAllWork = nullptr;
static FW_FindSubEntity_t             s_fn_FindSubEntity = nullptr;
static FW_TreeNodeCleanup_t           s_fn_TreeNodeCleanup = nullptr;

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: ForesterWork_Ctor
// disassembles to `0x1400ebac9: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.

// fn_InternalBasePtr_Ctor, fn_InternalBasePtr_CheckPtr, fn_IsDerivedFrom
// all from engine_funcs.h (no local copies needed)

// Type ID constants
// ENDGAME B.3.9 D5: the cell caches above are gone -- every site below calls the
// class's TypeId() directly (BY CELL: DAT_1401c7740=ForesterComponent,
// DAT_1401c7778=ForesterDescription, DAT_1401c4870=RadiusDescription,
// DAT_1401c7b30=NaturalResourceComponent, DAT_1401c7b68=NaturalResourceDescription,
// DAT_1401c7cb8=RawMaterialDescription, DAT_1401c77e8=GrowthDescription,
// DAT_1401c80a8=WorkDescription; several legacy variable names above named a
// different class than the cell actually held).

// Float constants from .rdata
static float* s_fPI = nullptr;             // DAT_140172184 = 3.14159265f (PI)
static float* s_fMaxHeight = nullptr;      // DAT_140172248 = 1000.0f
static float* s_fMinGrowth = nullptr;      // DAT_14017213c ≈ 0.97f
static float* s_fHalfCellOffset = nullptr; // DAT_140172114 = 0.5f

// Game world singleton (from engine_funcs.h: g_pGameWorldSingleton)

static bool s_forester_funcs_resolved = false;
static bool s_forester_exec_ready = false;

// ============================================================
// Resolution — called once during hook installation
// ============================================================
bool ForesterWork_ResolveFuncs() {
    if (s_forester_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<FW_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<FW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<FW_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<FW_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<FW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Sqrtf = reinterpret_cast<FW_Sqrtf_t>(
        ResolveAddr(Module::Exe, 0x140132010));
    s_fn_FindComp_NatResDesc = reinterpret_cast<FW_FindComp_NatResDesc_t>(
        ResolveAddr(Module::Exe, 0x14000c3c0));
    s_fn_Entity_GetGridCell = reinterpret_cast<FW_Entity_GetGridCell_t>(
        ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_Map_Invalidate = reinterpret_cast<FW_Map_Invalidate_t>(
        ResolveAddr(Module::Exe, 0x14009c480));
    s_fn_Map_CalcDistance = reinterpret_cast<FW_Map_CalcDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    s_fn_Map_FindNearestPassable = reinterpret_cast<FW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<FW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_FindComp_NatRes = reinterpret_cast<FW_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_FindComp_RawMaterial = reinterpret_cast<FW_FindComp_RawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140006330));
    s_fn_CheckTerrainValid = reinterpret_cast<FW_CheckTerrainValid_t>(
        ResolveAddr(Module::Exe, 0x14009f5d0));
    s_fn_GetTerrainPos = reinterpret_cast<FW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_CheckResourceDensity = reinterpret_cast<FW_CheckResourceDensity_t>(
        ResolveAddr(Module::Exe, 0x140120c30));
    s_fn_CreatePlantAction = reinterpret_cast<FW_CreatePlantAction_t>(
        ResolveAddr(Module::Exe, 0x1400be6e0));
    s_fn_PlayAnimation_Ctor = reinterpret_cast<FW_PlayAnimation_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400c2490));
    s_fn_PerformWork_Ctor3 = reinterpret_cast<FW_PerformWork_Ctor3_t>(
        ResolveAddr(Module::Exe, 0x1400c1eb0));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<FW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<FW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Storage = reinterpret_cast<FW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<FW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));

    // New function pointers for deferred vtable slots
    s_fn_BST_Serialize = reinterpret_cast<FW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));  // verified from original disasm at 0x1400eb9c6
    s_fn_ValidateEntity = reinterpret_cast<FW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_BSTRemove = reinterpret_cast<FW_BSTRemove_t>(
        // Level-1 caller-bug fix: 0x140024f30 is MID-FUNCTION (inside Toolbar_vslot14
        // 0x140024f00). The real BST single-node remove is 0x14001a6e0 — confirmed by
        // xref: ForesterWork_Update@0x1400eae3f CALLs 0x14001a6e0 (already registered).
        ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_BSTFindRemove = reinterpret_cast<FW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_CancelAllWork = reinterpret_cast<FW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<FW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_TreeNodeCleanup = reinterpret_cast<FW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Vtable pointers for dtor chain

    // MemAlloc / MemFree — resolve via the canonical Runtime exports. The exe IAT
    // dispatch slots these used (0x1401527e8/0x1401527f0) are UNREGISTERED in the
    // standalone exe (ResolveAddr -> 0, so *slot never ran and these stayed null, failing
    // ResolveFuncs); the MemAlloc/MemFree slot addresses were also wrong (they index the
    // _rng/Lerp imports, not MemoryAllocator). ResolveExport reads the Runtime DLL exports
    // directly, which is correct for the standalone exe.
    s_fn_MemAlloc = reinterpret_cast<FW_MemAlloc_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));     // static (size,align)
    s_fn_MemFreeFunc = reinterpret_cast<FW_MemFreeFunc_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));        // static (ptr)

    // Float constants from .rdata. In the standalone exe ResolveAddr(Module::Exe,...) on a
    // raw .rdata constant returns NULL (the exe's .rdata isn't mapped at the ghidra VA), so
    // fall back to a static holding the real value (reference_standalone_rdata_null_fallback).
    static float s_fb_PI = 3.14159265f;        // 0x140172184
    static float s_fb_MaxHeight = 1000.0f;     // 0x140172248
    static float s_fb_MinGrowth = 0.97f;       // 0x14017213c
    static float s_fb_HalfCell = 0.5f;         // 0x140172114
    s_fPI = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172184));
    if (!s_fPI) s_fPI = &s_fb_PI;
    s_fMaxHeight = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172248));
    if (!s_fMaxHeight) s_fMaxHeight = &s_fb_MaxHeight;
    s_fMinGrowth = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017213c));
    if (!s_fMinGrowth) s_fMinGrowth = &s_fb_MinGrowth;
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    if (!s_fHalfCellOffset) s_fHalfCellOffset = &s_fb_HalfCell;

    // Verify critical pointers
    bool execReady = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_Sqrtf &&
              s_fn_FindComp_NatResDesc && s_fn_Entity_GetGridCell &&
              s_fn_Map_Invalidate && s_fn_Map_CalcDistance &&
              s_fn_Map_FindNearestPassable && s_fn_Map_CanPathTo &&
              s_fn_FindComp_NatRes && s_fn_FindComp_RawMaterial &&
              s_fn_CheckTerrainValid && s_fn_GetTerrainPos &&
              s_fn_CheckResourceDensity && s_fn_CreatePlantAction &&
              s_fn_PlayAnimation_Ctor && s_fn_PerformWork_Ctor3 &&
              s_fn_CitizenAI_RegisterWork && s_fn_CitizenAI_LongTermPlan &&
              s_fn_FindComp_Storage && s_fn_StorageQueryCount &&
              s_fn_BST_Serialize &&
              s_fn_ValidateEntity && s_fn_MemAlloc && s_fn_MemFreeFunc &&
              s_fn_BSTRemove && s_fn_BSTFindRemove &&
              s_fn_CancelAllWork && s_fn_FindSubEntity && s_fn_TreeNodeCleanup &&
              fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_fPI && s_fMaxHeight && s_fMinGrowth && s_fHalfCellOffset;

    // The parent vtables (AutoPickupWork/Work) are only WRITTEN by the Dtor (never called
    // through) -- Execute derefs none of them. GrowthDescription's cell (c77e8) is a
    // registered type-id cell (ENDGAME B.3.9 D5: GrowthDescription::TypeId() is always
    // available, no separate resolve to gate on).
    bool ok = execReady;

    s_forester_exec_ready = execReady;
    s_forester_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[ForesterWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: s_fn_AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: s_fn_FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: s_fn_FindComp_MeshInstance\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: s_fn_MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: s_fn_CalcMaxWorkers\n");
        if (!s_fn_Sqrtf) printf("  MISSING: s_fn_Sqrtf\n");
        if (!s_fn_FindComp_NatResDesc) printf("  MISSING: s_fn_FindComp_NatResDesc\n");
        if (!s_fn_Entity_GetGridCell) printf("  MISSING: s_fn_Entity_GetGridCell\n");
        if (!s_fn_Map_Invalidate) printf("  MISSING: s_fn_Map_Invalidate\n");
        if (!s_fn_Map_CalcDistance) printf("  MISSING: s_fn_Map_CalcDistance\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: s_fn_Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: s_fn_Map_CanPathTo\n");
        if (!s_fn_FindComp_NatRes) printf("  MISSING: s_fn_FindComp_NatRes\n");
        if (!s_fn_FindComp_RawMaterial) printf("  MISSING: s_fn_FindComp_RawMaterial\n");
        if (!s_fn_CheckTerrainValid) printf("  MISSING: s_fn_CheckTerrainValid\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: s_fn_GetTerrainPos\n");
        if (!s_fn_CheckResourceDensity) printf("  MISSING: s_fn_CheckResourceDensity\n");
        if (!s_fn_CreatePlantAction) printf("  MISSING: s_fn_CreatePlantAction\n");
        if (!s_fn_PlayAnimation_Ctor) printf("  MISSING: s_fn_PlayAnimation_Ctor\n");
        if (!s_fn_PerformWork_Ctor3) printf("  MISSING: s_fn_PerformWork_Ctor3\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: s_fn_CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: s_fn_CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: s_fn_FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: s_fn_StorageQueryCount\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: s_fn_BST_Serialize\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: s_fn_ValidateEntity\n");
        if (!s_fn_MemAlloc) printf("  MISSING: s_fn_MemAlloc\n");
        if (!s_fn_MemFreeFunc) printf("  MISSING: s_fn_MemFreeFunc\n");
        if (!s_fn_BSTRemove) printf("  MISSING: s_fn_BSTRemove\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: s_fn_BSTFindRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: s_fn_CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: s_fn_FindSubEntity\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: s_fn_TreeNodeCleanup\n");
        if (!s_fPI) printf("  MISSING: s_fPI\n");
        if (!s_fMaxHeight) printf("  MISSING: s_fMaxHeight\n");
        if (!s_fMinGrowth) printf("  MISSING: s_fMinGrowth\n");
        if (!s_fHalfCellOffset) printf("  MISSING: s_fHalfCellOffset\n");
    }
#endif

    return ok;
}

// True once every pointer ForesterWork::Execute (Slot4) actually derefs is resolved. Excludes
// the Dtor-only parent vtables + the serialize-only NatResDesc2 id, which Execute never
// touches and which are unregistered in the standalone build. The force-driver gates on this.
bool ForesterWork_ExecuteReady() {
    ForesterWork_ResolveFuncs();
    return s_forester_exec_ready;
}

// extern "C" wrapper so the force harness can drive ForesterWork::Execute directly (the Work
// driver ABI: self + the worker citizen's IBP) without needing the ForesterWork type/mangling.
extern "C" int ForesterWork_ForceDrive(void* self, void* citizenIBP) {
    return reinterpret_cast<ForesterWork*>(self)->ForesterWork::Execute(citizenIBP, nullptr);   // B.3.8 bridge
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
        uintptr_t obj = *ibpPtr;
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
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
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
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
// Common tail code for both plant and cut paths.
// Sets entity flags, stores priority, registers work with citizen AI.
// ============================================================
static void AssignWorkToEntity(ForesterWork* self, void* citizenIBP, int priority) {
    uintptr_t citizen = *(uintptr_t*)citizenIBP;

    // Call vtable slot +0x50 (GetWorkEntity) on self
    uintptr_t workEntityIBP[2] = {0};
    uintptr_t* result = static_cast<uintptr_t*>(self->GetTarget(workEntityIBP));
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
// ForesterWork::Slot4 — Main work-assignment logic
// ============================================================
//
// Algorithm:
// 1. Check if citizen already has auto-pickup assignments
// 2. Find WorkplaceComponent, RadiusComponent, MaterialDescription
// 3. Check if material count has reached capacity (return 1 = done if so)
// 4. Calculate search radius from workplace radius
// 5. Random iteration over grid cells in radius
// 6. For each valid cell:
//    a. If empty cell and planting enabled: check terrain, create plant action
//    b. If occupied cell and cutting enabled: check resource, create cut action
// 7. If no work found after all attempts:
//    a. If enough failures, check storage and replan
//    b. Otherwise play idle animation and increment fail counter
//
int ForesterWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;  // for qword-aligned field access

    // Bring-up trace (first few ticks): confirms ForesterWork::Execute is actually being
    // ticked by the executor and reports the work-area entity + workplace cell it operates on.
    { static int s_tc = 0;
      if (s_tc < 6) { ++s_tc;
        fprintf(stderr, "[ForesterWork::Execute] tick#%d this=%p workArea+0x10=%p cell+0x28=%d\n",
                s_tc, (void*)this, (void*)selfQ[2], (int)selfQ[5]);
        fflush(stderr); } }

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
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(selfQ[2], &matDescIBP);

    // Find ForesterComponent on workplace entity (DAT_1401c7740, ENDGAME B.3.9 D5:
    // by cell -- not the legacy variable name "WorkplaceComponent").
    uintptr_t entity = selfQ[2];
    void* workplaceComp = FindComponentInList(entity, ForesterComponent::TypeId());

    // Wrap in InternalBasePtr
    uintptr_t workplaceIBP = 0;
    new (&workplaceIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(workplaceComp));   // B.3.3 bridge

    // Get ForesterDescription from workplace description (+0x18) (DAT_1401c7778,
    // ENDGAME B.3.9 D5: by cell -- not the legacy variable name "RadiusComponent").
    void* workplaceDesc = *(void**)(workplaceIBP + 0x18);
    void* radiusComp = nullptr;
    if (fn_IsDerivedFrom(workplaceDesc, ForesterDescription::TypeId())) {
        radiusComp = workplaceDesc;
    }

    // Find RadiusDescription on workplace's sub-object (+0x28) (DAT_1401c4870,
    // ENDGAME B.3.9 D5: by cell -- not the legacy variable name "MaterialComponent").
    uintptr_t entitySub = *(uintptr_t*)(selfQ[2] + 0x28);
    void* materialComp = FindSubComponentInList(entitySub, RadiusDescription::TypeId());

    // -------------------------------------------------------
    // Step 3: Check capacity — is material count at limit?
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)radiusComp + 0x28);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // Set mesh visibility based on capacity (visGroup 2)
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(selfQ[2], meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    // Get tree count (self+0x50 as int from qword at offset 10)
    int treeCount = (int)selfQ[10]; // self->_field50

    // If material description count is 0, calculate it
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    // If treeCount exceeds max workers, we're done (return 1)
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
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

    // Get NaturalResourceDescription from the material description's ref
    uintptr_t matDescCompRef = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);
    void* natResDesc = nullptr;
    s_fn_FindComp_NatResDesc(matDescCompRef, &natResDesc);

    // Get citizen's current position from citizen entity
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    // Navigate: citizen -> +0x278 -> CheckPtr -> deref -> +0x68 -> CheckPtr -> deref
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    (void)*(float*)(posComp + 0x160);  // citizenYaw at +0x160, unused in this path
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));

    // Clamp citizen cell to map bounds
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;

    uint32_t citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Get entity's grid cell for distance calculations
    uintptr_t entityIBPBuf[2] = { selfQ[2], 0 };
    uint32_t entityGridCell = s_fn_Entity_GetGridCell(entityIBPBuf, nullptr);

    // -------------------------------------------------------
    // Step 5: Random search for tree spots
    // -------------------------------------------------------
    float fMaxHeight = *s_fMaxHeight;
    float fMinGrowthVal = *s_fMinGrowth;
    // Note: XMM7 = g_fUVScale (DAT_14017214c = 1.0f) used for growth comparison.
    // .rdata constant -> NULL in standalone; fall back to the real value (see ResolveFuncs).
    float* pGrowthThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    static float s_fb_UVScale = 1.0f;
    float fGrowthThreshold = pGrowthThreshold ? *pGrowthThreshold : s_fb_UVScale;

    int entityCellSigned = (int)entityGridCell;

    if (maxAttempts > 0) {
        for (int attempt = 0; attempt < maxAttempts; attempt++) {
            int randX, randY;
            int testCellX, testCellY;

            // Generate random position within radius (rejection sampling)
            do {
                do {
                    randX = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    randY = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    testCellX = cellCol + randX;
                    testCellY = cellRow + randY;
                } while (randY * randY + randX * randX > radiusSq);
            } while (testCellX < 0 || testCellX >= mapSize ||
                     testCellY < 0 || testCellY >= mapSize);

            uint32_t testCell = (uint32_t)(*mapGrid) * testCellY + testCellX;
            uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20); // piVar3[0x20] = mapGrid+0x80
            uintptr_t cellData = gridData + (uintptr_t)testCell * 0x30;

            // Check terrain type bitmask — skip "forest-incompatible" terrain types
            // bit 5..8 (0x1e0 mask) indicates forest/grass terrain
            uint8_t terrainByte = *(uint8_t*)(cellData + 0x29);
            uint32_t terrainBit = 1u << (terrainByte & 0x1f);
            if ((terrainBit & 0x1e0u) != 0) {
                // Terrain already has resources (trees), skip
                goto next_attempt;
            }

            {
                // Map invalidation for connectivity
                s_fn_Map_Invalidate(mapGrid);

                // Read fresh grid data after invalidation
                uintptr_t gridData2 = *(uintptr_t*)(mapGrid + 0x20);
                uintptr_t entityCellData = gridData2 + (uintptr_t)entityCellSigned * 0x30;
                uintptr_t testCellData2 = gridData2 + (uintptr_t)(int)testCell * 0x30;

                uint32_t entityTerrainBit = 1u << (*(uint8_t*)(entityCellData + 0x29) & 0x1f);
                uint32_t testTerrainBit = 1u << (*(uint8_t*)(testCellData2 + 0x29) & 0x1f);
                uint32_t combinedBits = testTerrainBit | entityTerrainBit;
                uint32_t fullMask = combinedBits | 0xe1f;

                // Both cells must be passable
                if ((fullMask & testTerrainBit) == 0) goto next_attempt;
                if ((fullMask & entityTerrainBit) == 0) goto next_attempt;

                // Either both are "forest terrain" (0x1e0), or they share the same zone
                if ((combinedBits & 0x1e0) == 0) {
                    int16_t zone1 = *(int16_t*)(testCellData2 + 0x26);
                    if (zone1 == 0) goto next_attempt;
                    int16_t zone2 = *(int16_t*)(entityCellData + 0x26);
                    if (zone2 == 0) goto next_attempt;
                    if (zone1 != zone2) goto next_attempt;
                }

                // Cell must be unoccupied (no entity at cell)
                if (*(uintptr_t*)(testCellData2) != 0) goto next_attempt;

                // Check distance is within 2x workplace radius
                int dist = s_fn_Map_CalcDistance(mapGrid, entityGridCell, testCell);
                if (dist > workplaceRadius * 2) goto next_attempt;

                // Get resource entity at this cell
                uintptr_t resourceEntity = *(uintptr_t*)(gridData2 + (uintptr_t)(int)testCell * 0x30 + 8);

                // -------------------------------------------------------
                // Branch: Check workplace flags for cut vs plant
                // -------------------------------------------------------
                // PORT-BUG FIX: the original (0x1400eaec0) enters PLANT only when the
                // ForesterComponent's plant-enabled flag (+0x50) is NON-zero and the cell
                // is empty:  else-branch of `if (cmp[0x50]==0 || resource!=0)` ==
                // `(+0x50 != 0 && resource==0)`. The prior port had `+0x50 == 0`, which
                // (with the inert lodge's flag defaulting to 0) ALWAYS entered the plant
                // path -> CheckResourceDensity(natResDesc=null)+0x5c null-deref. CUT is
                // unchanged: `+0x51 != 0 && resource!=0`. After this fix the if/else
                // matches the original's nested branches exactly for all four cases.
                bool plantEnabled = (*(uint8_t*)(workplaceIBP + 0x50) != 0);  // plant-trees enabled
                bool cutEnabled = (*(uint8_t*)(workplaceIBP + 0x51) != 0);    // cut-trees enabled

                if (plantEnabled && resourceEntity == 0) {
                    // PLANT PATH: empty cell, check if we can plant here
                    bool canPath = s_fn_Map_CanPathTo(mapGrid, 0xe1f, testCell, citizenGridCell);
                    if (canPath) {
                        // Check terrain suitability for planting
                        bool terrainOk = s_fn_CheckTerrainValid(
                            mapGrid, testCellX, testCellY, 4,
                            fMinGrowthVal, fMaxHeight, 0.0f, 0, 0, 0);
                        if (terrainOk) {
                            // Check natural resource density limits
                            bool densityOk = s_fn_CheckResourceDensity(
                                natResDesc, testCellX, testCellY);
                            if (densityOk) {
                                // Reset fail counter
                                *(uint32_t*)(selfAddr + 0x58) = 0;

                                // Calculate world position for target cell
                                float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                                Math::Vector3 targetPos(
                                    (float)(testCell % *mapGrid) - halfOffset,
                                    (float)(testCell / *mapGrid) - halfOffset,
                                    0.0f);

                                // Get terrain height at position
                                float pathPos[3];
                                s_fn_GetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

                                // Get material description ref for plant action
                                uintptr_t matDescRef = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);

                                // Create plant action chain
                                uintptr_t entityRef = *(uintptr_t*)citizenIBP;
                                uintptr_t plantBuf[2] = { entityRef, 0 };
                                uintptr_t resourceVal = 0;
                                uintptr_t resCountBuf[2] = { 0, 0 };
                                s_fn_CreatePlantAction(
                                    plantBuf,                          // entityIBP
                                    (int)selfQ[4],                     // priority (workplaceId)
                                    matDescRef,                        // matDescRef
                                    0,                                 // param4
                                    (uint32_t*)pathPos,                // pathPos
                                    testCell,                          // cellIdx
                                    0,                                 // param7
                                    &resourceVal,                      // resourcePtr
                                    (uint32_t*)resCountBuf,            // resCountPtr
                                    0,                                 // param10
                                    0);                                // param11

                                // Assign work and register with AI
                                AssignWorkToEntity(this, citizenIBP, (int)selfQ[4]);
                                return 2;
                            }
                        }
                    }
                }
                else if (cutEnabled && resourceEntity != 0) {
                    // CUT PATH: cell has a resource, check if we can cut it

                    // Find NaturalResourceComponent on the resource entity
                    uintptr_t natResCompIBP[4] = {0};
                    s_fn_FindComp_NatRes(resourceEntity, natResCompIBP);

                    if (*(uintptr_t*)natResCompIBP != 0) {
                        // Check capacity — can we still add more resources?
                        int matIdx2 = *(int*)((uintptr_t)radiusComp + 0x28);
                        uintptr_t resEntry2 = resourceTable + (uintptr_t)matIdx2 * 0x20;
                        if (*(int*)(resEntry2) < *(int*)(resEntry2 + 8)) {
                            // Find NaturalResourceDescription on the resource
                            uintptr_t natResCompIBP2[4] = {0};
                            s_fn_FindComp_NatRes(resourceEntity, natResCompIBP2);

                            void* natResDescObj = *(void**)(*(uintptr_t*)natResCompIBP2 + 0x18);
                            void* natResDescFound = nullptr;
                            if (fn_IsDerivedFrom(natResDescObj, NaturalResourceDescription::TypeId())) {
                                natResDescFound = natResDescObj;
                            }

                            // Find RawMaterialComponent
                            uintptr_t rawMatPtr = 0;
                            s_fn_FindComp_RawMaterial(
                                *(uintptr_t*)((uintptr_t)natResDescFound + 0x20), &rawMatPtr);

                            // Check resource flags:
                            // bit 5 of +0x5c = "being harvested"
                            // bit 13 of +0x5c = "reserved"
                            uint32_t resFlags = *(uint32_t*)(rawMatPtr + 0x5c);
                            if ((resFlags >> 5) & 1) goto next_attempt;
                            if ((resFlags >> 13) & 1) goto next_attempt;

                            // Check growth is sufficient (>= g_fUVScale threshold)
                            float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + 0x28);
                            if (growth < fGrowthThreshold) goto next_attempt;

                            // Check bit 20 of +0x20 = "flagged for removal"
                            uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResCompIBP2 + 0x20);
                            if ((flags20 >> 20) & 1) goto next_attempt;

                            // Find nearest passable cell for pathfinding
                            int nearestCell = s_fn_Map_FindNearestPassable(
                                mapGrid, testCell, citizenGridCell, 0xe07);
                            if (nearestCell < 0) goto next_attempt;

                            // Check path to the nearest passable cell
                            bool canPath2 = s_fn_Map_CanPathTo(
                                mapGrid, 0xe1f, nearestCell, citizenGridCell);
                            if (!canPath2) goto next_attempt;

                            // Reset fail counter
                            *(uint32_t*)(selfAddr + 0x58) = 0;

                            // Calculate world position for nearest cell
                            float halfOffset2 = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                            Math::Vector3 cutPos(
                                (float)(nearestCell % *mapGrid) - halfOffset2,
                                (float)(nearestCell / *mapGrid) - halfOffset2,
                                0.0f);

                            // Get terrain height
                            float cutPathPos[3];
                            s_fn_GetTerrainPos(mapGrid, cutPathPos, &cutPos.x, 0, nullptr);

                            // ── PHASE 5 PRODUCE VERIFICATION (env BANISHED_FORCE_HARVEST, default OFF) ──
                            // The organic delivery of a citizen to the tree is gated on the pathfinder
                            // MoveTo/arrival keystone (project_citizen_pathfinding_zero_dest) — citizens
                            // currently drift via walk-anim root-motion and never arrive. To verify the
                            // Phase-5 PRODUCTION chain (RemoveNaturalResource::Main → FUN_1401260f0 →
                            // RegisterRawMaterial) end-to-end independent of that movement blocker, this
                            // block invokes the real produce helper directly on the tree the real forester
                            // scan just found + validated. It mirrors RemoveNaturalResource::Main's flag
                            // setup (natResComp+0x2d harvestable, +0x2c mature) then calls 0x1401260f0.
                            if (Opt().forceHarvest) {
                                typedef void (*RemoveNatRes_t)(void* natResComp, void* workIBP, char flag);
                                static RemoveNatRes_t s_forceProduce =
                                    reinterpret_cast<RemoveNatRes_t>(ResolveAddr(Module::Exe, 0x1401260f0));
                                static int s_harvestCount = 0;
                                if (s_forceProduce && s_harvestCount < 6) {
                                    ++s_harvestCount;
                                    uintptr_t nrc = *(uintptr_t*)natResCompIBP2;
                                    *(uint8_t*)(nrc + 0x2d) = 1;   // harvestable
                                    *(uint8_t*)(nrc + 0x2c) = 1;   // mature
                                    uintptr_t czEnt = *(uintptr_t*)citizenIBP;
                                    uintptr_t residence = czEnt ? *(uintptr_t*)(czEnt + 0x248) : 0;
                                    uintptr_t workIBPbuf[2] = { residence, 0 };
                                    fprintf(stderr, "[FORCE-HARVEST] #%d invoking produce on tree natResComp=%p "
                                            "(mirrors RemoveNaturalResource::Main; bypasses MoveTo arrival)\n",
                                            s_harvestCount, (void*)nrc);
                                    fflush(stderr);
                                    s_forceProduce((void*)nrc, workIBPbuf, 1);
                                    // With BANISHED_PRODUCE_SPAWN_ENTITY the force-produce just
                                    // REMOVED the tree (dirty-list -> freed next EntityList tick).
                                    // Do NOT also queue the real cut sequence on it: its pending
                                    // AnimateNaturalResource/PerformWork children would execute
                                    // against the freed entity (retail order runs all tree-touching
                                    // actions BEFORE RemoveNaturalResource, so this ordering only
                                    // exists under the harness). Scan on for a different tree.
                                    goto next_attempt;
                                }
                            }

                            // Create PerformWork action (cut tree)
                            // Get target entity IBP
                            uintptr_t targetRef = *(uintptr_t*)natResCompIBP2;
                            uintptr_t cutEntityRef = *(uintptr_t*)citizenIBP;
                            uintptr_t cutBuf[2] = { cutEntityRef, 0 };
                            uintptr_t cutTargetBuf[2] = { targetRef, 0 };
                            s_fn_PerformWork_Ctor3(
                                cutBuf,                            // entityIBP
                                (int)selfQ[4],                     // priority (workplaceId)
                                cutTargetBuf,                      // targetIBP
                                cutPathPos,                        // position
                                0,                                 // flag
                                1);                                // param6

                            // Assign work and register with AI
                            AssignWorkToEntity(this, citizenIBP, (int)selfQ[4]);
                            return 2;
                        }
                    }
                }
            }

        next_attempt:
            ;  // Continue to next random attempt
        }
    }

    // -------------------------------------------------------
    // Step 7: No work found — handle failure
    // -------------------------------------------------------
    int failCount = (int)selfQ[0xb]; // self->_field58

    if (failCount > maxAttempts) {
        // Too many failures — check if citizen's storage has items, then replan
        uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

        uintptr_t storageIBP[2] = {0};
        s_fn_FindComp_Storage(*(uintptr_t*)(citizenEnt + 0x10), storageIBP);

        int storageCount = s_fn_StorageQueryCount((void*)*(uintptr_t*)storageIBP);
        if (storageCount < 1) {
            return 0;
        }

        // Citizen has items in storage — replan long term
        s_fn_CitizenAI_LongTermPlan(*(void**)citizenIBP);
    }
    else {
        // Play idle animation and increment fail counter
        uintptr_t idleEntityRef = *(uintptr_t*)citizenIBP;
        uintptr_t idleBuf[2] = { idleEntityRef, 0 };
        s_fn_PlayAnimation_Ctor(idleBuf, (int)selfQ[4], 0);

        // Assign work (idle animation) and register with AI
        int priority = (int)selfQ[4];
        uintptr_t citizenEnt2 = *(uintptr_t*)citizenIBP;

        // Call vtable slot +0x50 (GetWorkEntity)
        uintptr_t workEntIBP[2] = {0};
        uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
        uintptr_t workEnt = *wResult;

        *(uint32_t*)(citizenEnt2 + 0x54) |= 4;
        *(int32_t*)(citizenEnt2 + 0x30) = priority;
        *(uint32_t*)(citizenEnt2 + 0xb0) = 0;

        if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && priority >= 0) {
            *(uint32_t*)(citizenEnt2 + 0x88) |= 0x20000;
        }

        uintptr_t citizenAISub = (uintptr_t)this + 0x48;
        s_fn_CitizenAI_RegisterWork((void*)citizenAISub, workEntIBP, citizenIBP);

        // Increment fail counter
        *(int32_t*)(selfAddr + 0x58) = failCount + 1;
    }

    return 2;
}


// ============================================================
// ForesterWork::DtorBody (0x1400EBA00)
// Sets vtables down ForesterWork -> AutoPickupWork -> Work chain,
// frees BST nodes at +0x48, zeros count at +0x50, then calls
// Object::Internal::~Internal via tail-call.
// ============================================================
// ForesterWork dtor (retail slot 0 0x14002A370 inlines it). The stamps and the `~Internal` call are the compiler's chain.
ForesterWork::~ForesterWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        fn_MemFreeStatic(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}



// ============================================================
// ForesterWork::Serialize (0x1400EB970)
// Work::Serialize + version block + BST serialization + int field
// ============================================================
void ForesterWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create first serializer (version block, version=0)
    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Create second serializer (version=0) for BST + int
    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at self+0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializer2);

    // Serialize int field at self+0x58
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((selfAddr + 0x58)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// ForesterWork::Slot6 (0x1400EAEA0) — NOT HOOKABLE
// 20-byte thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520;
//                xor al,al; add rsp,0x28; ret
// Removes entity from BST at self+0x48, returns false.
// Standalone exe only.
// ============================================================
int ForesterWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);
    return 0;
}


// ============================================================
// ForesterWork::Slot7 (0x1400EAE90) — 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// Standalone exe only.
// ============================================================
int ForesterWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);
    return 0;
}


// ============================================================
// ForesterWork::Slot8 / CancelAll (0x1400EB8A0)
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
// ============================================================
int ForesterWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find leftmost node in BST (following +0x10 = left child)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// ForesterWork::Slot9 / RemoveInvalid (0x1400EAC60) — 164 instructions
// Iterates BST at +0x48, validates each entry via FUN_1400f7100.
// Collects invalid ones in a dynamic array, removes them from tree.
// Returns true if all valid (removeCount == 0), false otherwise.
// ============================================================
int ForesterWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root != nullptr) {
        // Find leftmost node (minimum)
        void* node = root;
        while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // In-order traversal
        while (node != nullptr) {
            uintptr_t entityIBP = *(uintptr_t*)node;

            // Validate this entity
            char stillValid = s_fn_ValidateEntity(selfAddr, nullptr, (void*)&entityIBP, nullptr);

            if (stillValid == '\0') {
                // Add to removal list -- grow array if needed
                uint32_t newCount = removeCount + 1;
                if (newCount != removeCount) {
                    uintptr_t* newArr = removeArr;
                    if ((int)removeCapacity < (int)newCount) {
                        uint32_t newCap = newCount;
                        if (removeCount != 0) {
                            newCap = newCount * 3;
                        }
                        removeCapacity = newCap;
                        newArr = (uintptr_t*)s_fn_MemAlloc((int64_t)(int)newCap * 8, 8);
                    }

                    // Copy existing entries if buffer changed
                    int copyStart = (removeArr == newArr) ? (int)removeCount : 0;
                    for (int ci = copyStart; ci < (int)newCount; ci++) {
                        uintptr_t* dst = &newArr[ci];
                        *dst = 0;
                        if (ci < (int)removeCount) {
                            *dst = removeArr[ci];
                        } else {
                            *dst = entityIBP;
                        }
                    }

                    if (removeArr != newArr) {
                        fn_MemFreeStatic(removeArr);
                    }
                    removeArr = newArr;
                    removeCount = newCount;
                }
            }

            // Get next node in tree (in-order traversal)
            void* right = *(void**)((uintptr_t)node + 0x18);
            if (right != nullptr) {
                // Go right, then leftmost
                node = right;
                while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                    node = *(void**)((uintptr_t)node + 0x10);
                }
            } else {
                // Go up until we come from a left child
                void* parent = *(void**)((uintptr_t)node + 0x8);
                while (parent != nullptr) {
                    if (*(void**)((uintptr_t)parent + 0x10) == node) {
                        // node is left child of parent -- parent is next
                        break;
                    }
                    // node is right child, keep going up
                    node = parent;
                    parent = *(void**)((uintptr_t)node + 0x8);
                }
                node = parent;
            }
        }
    }

    // Remove invalid entries from tree
    if ((int)removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            uintptr_t searchKey = removeArr[i];
            void* treeNode = *bstRootPtr;

            while (treeNode != nullptr) {
                uintptr_t nodeKey = *(uintptr_t*)treeNode;
                if (searchKey < nodeKey) {
                    treeNode = *(void**)((uintptr_t)treeNode + 0x10);
                } else {
                    if (searchKey <= nodeKey) {
                        // Found -- remove this node
                        s_fn_BSTRemove(bstRootPtr, treeNode);
                        break;
                    }
                    treeNode = *(void**)((uintptr_t)treeNode + 0x18);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return (removeCount == 0) ? 1 : 0;
}


// ============================================================
// ForesterWork::Slot10 / GetWorkEntity (0x1400EB910)
// Looks up entity from this->_field10, gets sub-object via
// FUN_140002e10, returns entity +0x48 through output pointer.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
// ============================================================
void* ForesterWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}



// [11] 0x140029860 (shared by 13 tables): `XOR EAX,EAX; CMP [RCX+0x50],EAX; SETG AL`
// — "is anyone assigned". The old table pointed at Work_Slot11_HasActiveTasks.
int ForesterWork::GetPriority() {
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's. ForesterWork_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
ForesterWork::ForesterWork() : AutoPickupWork(), _field48(nullptr), _field50(0), _field58(0) {
    ForesterWork_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_ForesterWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over ForesterWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a310: MemAlloc(sizeof, 8), then the ctor.
void* ForesterWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ForesterWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ForesterWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* ForesterWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ForesterWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// The factory the forester force harness (forester_force_harness.cpp) names.
extern "C" void* ForesterWork_Factory(void) { return ForesterWork::CreateNewObject(); }
DEFINE_EXE_TYPE(ForesterWork, "AutoPickupWork")
