// code/Game/Work/pasturework.cpp
// Clean C++ reimplementation of PastureWork -- all methods.
// Source: Game/Work/PastureWork.cpp (from profile build debug strings)
//
// PastureWork (0x58 bytes) -- Pasture animal work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> PastureWork
// Vtable: 0x1401617a0
//
// PastureWork is the most complex Work subclass. Slot4 handles:
// - Checking animal count vs max workers
// - Resource capacity checks
// - Removing animals from pasture (via RemoveAnimal action)
// - Finding empty pasture positions for animals
// - Creating grazing work actions
//
// Slot7 is unique: tree insertion + clearing "flagged for removal" entity bits.
// Slot8/Slot9/Slot10/Serialize/Dtor are identical patterns to HuntWork.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/PastureWork.h"
#include "Work/pasturework_funcs.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>


// ============================================================
// Local function pointers (resolved once by PastureWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with PW_ to avoid conflicts

typedef int   (*PW_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
typedef void* (*PW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*PW_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void  (*PW_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
typedef int   (*PW_CalcMaxWorkers_t)(uintptr_t matDesc);
typedef void* (*PW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
typedef int   (*PW_StorageQueryCount_t)(void* storageComp);
typedef float*(*PW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                    char param4, uint32_t* outNormal);
typedef void  (*PW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
typedef void  (*PW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
typedef void* (*PW_FindComp_Pasture_t)(uintptr_t entity, void* outIBP);
typedef void* (*PW_GetAreaComponent_t)(uintptr_t entityPtr, void* outIBP);
typedef int   (*PW_Map_CalcDistance_t)(int* mapGrid, uint32_t cellA, uint32_t cellB);
typedef int   (*PW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*PW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
typedef void* (*PW_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);
typedef void* (*PW_FindComp_Position_t)(uintptr_t entity, void* outIBP);
typedef void* (*PW_FindComp_Family_t)(uintptr_t entity, void* outIBP);
typedef void* (*PW_FindComp_Family2_t)(uintptr_t entity, void* outIBP);  // Entity_FindComponent_Family @ 0x1400012d0

// Work::Serialize
typedef void  (*PW_Work_Serialize_t)(void* self, void* stream);
// BST serialization
typedef void  (*PW_BST_Serialize_t)(void* bstRoot, void* serializer);

// FUN_1400f7100 -- validate work entity
typedef bool  (*PW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*PW_MemAlloc_t)(int64_t size, int align);
typedef void  (*PW_MemFree_t)(void* ptr);
// BST node remove
typedef void  (*PW_BSTRemove_t)(void* bstRoot, void* node);
// BST find-and-remove by key
typedef void  (*PW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// Cancel all work items in BST
typedef void  (*PW_CancelAllWork_t)(void* bstRoot);
// Find sub-entity
typedef void* (*PW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// Tree node cleanup
typedef void  (*PW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// fn_MemFreeStatic
typedef void  (*PW_MemFreeStatic_t)(void* ptr);

// PastureWork-specific helpers
typedef int   (*PW_PastureHelper8350_t)(uintptr_t pastureComp);
typedef void  (*PW_ActionCtor_GrazingWork_t)(void* entityIBP, int priority, void* targetIBP,
                                              uint32_t param4, float* position, float* position2,
                                              uint32_t typeId, int param8, int param9);
// RemoveAnimal_Action_Ctor (0x1400bbea0)
typedef void  (*PW_RemoveAnimalCtor_t)(void* entityIBP, int priority, void* posPtr, int cell,
                                        void* natResIBP, uintptr_t descPtr, uint32_t param7);
// Entity_impl_2e20 -- get entity position by index from position component
typedef void* (*PW_Entity_GetPosition_t)(uintptr_t posComp, float* outPos, int group, int index, float* outHeading);
// FUN_140123070 -- count positions in a position component
typedef int   (*PW_PositionCount_t)(uintptr_t posComp, int group);
// FUN_1400f8620 -- storage overflow work check
typedef uint64_t (*PW_StorageOverflowCheck_t)(uintptr_t storageWorkMgr, void* citizenIBP, int storageId, char flag);
// FUN_1400c3470 -- update parent entity after flag change
typedef void  (*PW_UpdateParentEntity_t)(uintptr_t parentEntity);

static PW_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;
static PW_FindComp_MatDesc_t            s_fn_FindComp_MatDesc = nullptr;
static PW_FindComp_MeshInstance_t       s_fn_FindComp_MeshInstance = nullptr;
static PW_MeshInstance_SetVisible_t     s_fn_MeshInstance_SetVisible = nullptr;
static PW_CalcMaxWorkers_t              s_fn_CalcMaxWorkers = nullptr;
static PW_FindComp_Storage_t            s_fn_FindComp_Storage = nullptr;
static PW_StorageQueryCount_t           s_fn_StorageQueryCount = nullptr;
static PW_GetTerrainPos_t              s_fn_GetTerrainPos = nullptr;
static PW_CitizenAI_RegisterWork_t     s_fn_CitizenAI_RegisterWork = nullptr;
static PW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static PW_FindComp_Pasture_t           s_fn_FindComp_Pasture = nullptr;
static PW_GetAreaComponent_t           s_fn_GetAreaComponent = nullptr;
static PW_Map_CalcDistance_t            s_fn_Map_CalcDistance = nullptr;
static PW_Map_FindNearestPassable_t    s_fn_Map_FindNearestPassable = nullptr;
static PW_Map_CanPathTo_t              s_fn_Map_CanPathTo = nullptr;
static PW_FindComp_NatRes_t             s_fn_FindComp_NatRes = nullptr;
static PW_FindComp_Position_t          s_fn_FindComp_Position = nullptr;
static PW_FindComp_Family_t            s_fn_FindComp_Family = nullptr;   // FUN_1400223c0
static PW_FindComp_Family2_t           s_fn_FindComp_Family2 = nullptr;  // Entity_FindComponent_Family @ 0x1400012d0
static PW_BST_Serialize_t              s_fn_BST_Serialize = nullptr;
static PW_ValidateEntity_t             s_fn_ValidateEntity = nullptr;
static PW_MemAlloc_t                   s_fn_MemAlloc = nullptr;
static PW_MemFree_t                    s_fn_MemFree = nullptr;
static PW_BSTRemove_t                  s_fn_BSTRemove = nullptr;
static PW_BSTFindRemove_t              s_fn_BSTFindRemove = nullptr;
static PW_CancelAllWork_t              s_fn_CancelAllWork = nullptr;
static PW_FindSubEntity_t              s_fn_FindSubEntity = nullptr;
static PW_TreeNodeCleanup_t            s_fn_TreeNodeCleanup = nullptr;
static PW_MemFreeStatic_t              s_fn_MemFreeStatic = nullptr;
static PW_PastureHelper8350_t          s_fn_PastureHelper8350 = nullptr;
static PW_ActionCtor_GrazingWork_t     s_fn_ActionCtor_GrazingWork = nullptr;
static PW_RemoveAnimalCtor_t           s_fn_RemoveAnimalCtor = nullptr;
static PW_Entity_GetPosition_t         s_fn_Entity_GetPosition = nullptr;
static PW_PositionCount_t             s_fn_PositionCount = nullptr;
static PW_StorageOverflowCheck_t      s_fn_StorageOverflowCheck = nullptr;
static PW_UpdateParentEntity_t         s_fn_UpdateParentEntity = nullptr;

// Type ID constants.
// ⚠ The THREE names below are all wrong — verified against
// standalone/factory_registrations.csv:230,284,285 on 2026-07-31. The ADDRESSES are
// correct and the code behaves correctly, so do NOT "fix" any deref to match a name.
// Renaming the variables is the right follow-up; leaving the lie labelled is the
// minimum. (Wrong names of exactly this kind caused two separate bugs today: a
// GetTypeIdByName("AreaDescription") that matched nothing, and a whole investigation
// misdirected by a finder named for the wrong component.)
static uint32_t* s_typeId_PastureComponent = nullptr;  // DAT_1401c7bd8  = PastureDescription
static uint32_t* s_typeId_ComponentEntity = nullptr;    // DAT_1401c6fd0  = AnimalDescription
static uint32_t* s_typeId_NatResComponent = nullptr;    // DWORD_1401c7ba0 = PastureComponent

// Float constants from .rdata
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: PastureWork_Ctor
// disassembles to `0x1400f2da9: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.

// Original Slot4 exe function pointer for fallback (complex function)

static bool s_pasture_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool PastureWork_ResolveFuncs() {
    if (s_pasture_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<PW_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<PW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<PW_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<PW_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<PW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_FindComp_Storage = reinterpret_cast<PW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<PW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_GetTerrainPos = reinterpret_cast<PW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<PW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<PW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Pasture = reinterpret_cast<PW_FindComp_Pasture_t>(
        ResolveAddr(Module::Exe, 0x140004fe0));
    s_fn_GetAreaComponent = reinterpret_cast<PW_GetAreaComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_Map_CalcDistance = reinterpret_cast<PW_Map_CalcDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    s_fn_Map_FindNearestPassable = reinterpret_cast<PW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<PW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_FindComp_NatRes = reinterpret_cast<PW_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x140005500));
    s_fn_FindComp_Position = reinterpret_cast<PW_FindComp_Position_t>(
        ResolveAddr(Module::Exe, 0x140002ed0));  // Entity_FindComponent_Position
    s_fn_FindComp_Family = reinterpret_cast<PW_FindComp_Family_t>(
        ResolveAddr(Module::Exe, 0x1400223c0));
    s_fn_FindComp_Family2 = reinterpret_cast<PW_FindComp_Family2_t>(
        ResolveAddr(Module::Exe, 0x1400012d0));  // Entity_FindComponent_Family
    s_fn_BST_Serialize = reinterpret_cast<PW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));  // verified from original disasm
    s_fn_ValidateEntity = reinterpret_cast<PW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_BSTRemove = reinterpret_cast<PW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_BSTFindRemove = reinterpret_cast<PW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_CancelAllWork = reinterpret_cast<PW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<PW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_TreeNodeCleanup = reinterpret_cast<PW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));
    s_fn_PastureHelper8350 = reinterpret_cast<PW_PastureHelper8350_t>(
        ResolveAddr(Module::Exe, 0x140128350));
    s_fn_ActionCtor_GrazingWork = reinterpret_cast<PW_ActionCtor_GrazingWork_t>(
        ResolveAddr(Module::Exe, 0x1400bb0a0));
    s_fn_RemoveAnimalCtor = reinterpret_cast<PW_RemoveAnimalCtor_t>(
        ResolveAddr(Module::Exe, 0x1400bbea0));
    s_fn_Entity_GetPosition = reinterpret_cast<PW_Entity_GetPosition_t>(
        ResolveAddr(Module::Exe, 0x140122e20));   // Entity_impl_2e20
    s_fn_PositionCount = reinterpret_cast<PW_PositionCount_t>(
        ResolveAddr(Module::Exe, 0x140123070));   // FUN_140123070
    s_fn_StorageOverflowCheck = reinterpret_cast<PW_StorageOverflowCheck_t>(
        ResolveAddr(Module::Exe, 0x1400f8620));   // FUN_1400f8620
    s_fn_UpdateParentEntity = reinterpret_cast<PW_UpdateParentEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));

    // Static MemAlloc/MemFree. These previously read exe dispatch slots 0x140151460 /
    // 0x140151468; 0x140151460 is NOT MemAlloc, it is Graphics::Geometry::GetTransform
    // (see placementtool.cpp:78). Since that slot is registered, the pointer was
    // non-null and WRONG, so the first array grow called GetTransform(size, align) and
    // took an ACCESS_VIOLATION. Diagnosed in tradework.cpp, where a TradeWork actually
    // reached its grow path; the same wrong pair was copied into five work classes.
    s_fn_MemAlloc = reinterpret_cast<PW_MemAlloc_t>(fn_MemAllocStatic);
    s_fn_MemFree  = reinterpret_cast<PW_MemFree_t>(fn_MemFreeStatic);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<PW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID pointers
    s_typeId_PastureComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7bd8));
    s_typeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6fd0));
    s_typeId_NatResComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7ba0));

    // Float constants from .rdata
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable pointers for dtor

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_FindComp_Storage &&
              s_fn_StorageQueryCount &&
              s_fn_GetTerrainPos &&
              s_fn_CitizenAI_RegisterWork && s_fn_CitizenAI_LongTermPlan &&
              s_fn_FindComp_Pasture && s_fn_GetAreaComponent &&
              s_fn_Map_CalcDistance && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_FindComp_NatRes &&
              s_fn_BST_Serialize &&
              s_fn_ValidateEntity && s_fn_BSTRemove &&
              s_fn_BSTFindRemove && s_fn_CancelAllWork &&
              s_fn_FindSubEntity && s_fn_TreeNodeCleanup &&
              s_fn_UpdateParentEntity &&
              s_fn_PastureHelper8350 && s_fn_RemoveAnimalCtor &&
              s_fn_Entity_GetPosition && s_fn_PositionCount &&
              s_fn_StorageOverflowCheck && s_fn_FindComp_Family2 &&
              fn_IsDerivedFrom &&
              g_pGameWorldSingleton &&
              s_typeId_PastureComponent && s_typeId_ComponentEntity &&
              s_typeId_NatResComponent &&
              s_fHalfCellOffset;

    s_pasture_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[PastureWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: StorageQueryCount\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Pasture) printf("  MISSING: FindComp_Pasture\n");
        if (!s_fn_GetAreaComponent) printf("  MISSING: GetAreaComponent\n");
        if (!s_fn_Map_CalcDistance) printf("  MISSING: Map_CalcDistance\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_FindComp_NatRes) printf("  MISSING: FindComp_NatRes\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
        if (!s_fn_UpdateParentEntity) printf("  MISSING: UpdateParentEntity\n");
        if (!s_fn_PastureHelper8350) printf("  MISSING: PastureHelper8350\n");
        if (!s_fn_RemoveAnimalCtor) printf("  MISSING: RemoveAnimalCtor\n");
        if (!s_fn_Entity_GetPosition) printf("  MISSING: Entity_GetPosition\n");
        if (!s_fn_PositionCount) printf("  MISSING: PositionCount\n");
        if (!s_fn_StorageOverflowCheck) printf("  MISSING: StorageOverflowCheck\n");
    }
#endif

    return ok;
}


// ============================================================
// PastureWork::Slot4 -- Pasture work assignment (0x1400F2370)
// ============================================================
//
// This is the most complex Slot4 in the Work hierarchy (509 instructions, 279 lines).
// Three main branches:
//   A) If pasture resource NOT at capacity -> find empty animal slot, create RemoveAnimal action
//   B) If at capacity -> iterate position component for overflow storage work
//   C) Final fallback -> random position via Area component, create grazing work action
//
// Decompiled from Ghidra at 0x1400F2370, all control flow matched to original.
//
int PastureWork::Execute(void* entityIBP, void* /*context*/, uint64_t /*arg3*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;
    void* citizenIBP = entityIBP;

    // local_res20 = *(longlong*)param_2;  local_res10 = param_2
    uintptr_t local_res20 = *(uintptr_t*)entityIBP;
    void* local_res10 = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments (FUN_1400e3db0)
    // -------------------------------------------------------
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &local_res20);
    if (checkResult == 2) {
        return 2;
    }

    // plVar17 = param_1 + 2 => selfAddr + 0x10
    uintptr_t* plVar17 = (uintptr_t*)(selfAddr + 0x10);

    // -------------------------------------------------------
    // Step 2: Find StorageLocation and Pasture components
    // -------------------------------------------------------
    uintptr_t local_58 = 0;  // matDescIBP (StorageLocation component)
    s_fn_FindComp_MatDesc(*plVar17, &local_58);

    uintptr_t local_a8 = 0;  // pastureIBP
    s_fn_FindComp_Pasture(*plVar17, &local_a8);

    // -------------------------------------------------------
    // Step 3: Get pasture description and worker count
    // -------------------------------------------------------
    void* pvVar20 = *(void**)(local_a8 + 0x18);  // pasture component description
    bool bVar8 = fn_IsDerivedFrom(pvVar20, *s_typeId_PastureComponent);  // DAT_1401c7bd8
    uintptr_t lVar18 = selfQ[10];  // worker count at self+0x50
    void* pvVar19 = nullptr;
    if (bVar8) {
        pvVar19 = pvVar20;
    }

    // -------------------------------------------------------
    // Step 4: Check max workers
    // -------------------------------------------------------
    if (*(int*)(local_58 + 0x30) == 0) {
        int iVar9 = s_fn_CalcMaxWorkers(local_58);
        *(int*)(local_58 + 0x30) = iVar9;
    }

    // Original: if ((int)lVar18 <= *(int*)(local_58 + 0x30)) { ... whole body ... }
    // Inverted: if workerCount > maxWorkers, skip to return 1 at end
    if ((int)lVar18 > *(int*)(local_58 + 0x30)) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 5: Check resource capacity via game world resource table
    //   lVar18 = *(int*)(pvVar19 + 0x20) * 0x20
    //   table = *(*(g_pGameWorldSingleton + 0xf0) + 0x170)
    //   bVar8 = table[lVar18 + 8] <= table[lVar18]
    // -------------------------------------------------------
    {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        int descIndex = *(int*)((uintptr_t)pvVar19 + 0x20);
        uintptr_t offset = (uintptr_t)descIndex * 0x20;
        uintptr_t tableBase = *(uintptr_t*)(*(uintptr_t*)(gameWorld + 0xf0) + 0x170);
        int currentCount = *(int*)(tableBase + offset);
        int maxCount = *(int*)(tableBase + offset + 8);
        bVar8 = (maxCount <= currentCount);  // at capacity?

        // Set mesh visibility based on capacity
        uintptr_t meshInstIBP = 0;
        s_fn_FindComp_MeshInstance(*plVar17, &meshInstIBP);
        s_fn_MeshInstance_SetVisible(*(void**)&meshInstIBP, 2, bVar8 ? 1 : 0);
    }

    // -------------------------------------------------------
    // Branch A: NOT at capacity (bVar8 == false) -> find empty slot, remove animal
    // -------------------------------------------------------
    if (!bVar8) {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        int* piVar4 = *(int**)(gameWorld + 0xf8);  // mapGrid

        // Get citizen position for pathfinding
        uintptr_t citEnt = *(uintptr_t*)citizenIBP;
        uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);
        uintptr_t charComp = *(uintptr_t*)(aiComp + 0x68);

        int iVar21 = *piVar4;  // map size
        (void)*(uint32_t*)(charComp + 0x160);  // local_48: read but unused (matches original)
        int iVar9 = (int)(*(float*)(charComp + 0x158) + (float)(iVar21 >> 1));
        int iVar10 = (int)(*(float*)(charComp + 0x15c) + (float)(iVar21 >> 1));
        if (iVar9 < 0) iVar9 = 0;
        if (iVar21 <= iVar9) iVar9 = iVar21 - 1;
        if (iVar10 < 0) iVar10 = 0;
        if (iVar21 <= iVar10) iVar10 = iVar21 - 1;
        uint32_t uVar23 = iVar21 * iVar10 + iVar9;  // citizen cell index

        // Check if pasture has room for more animals (field+0x40 < field+0x38)
        int pastureCount = *(int*)(local_a8 + 0x38);
        if (*(int*)(local_a8 + 0x40) < pastureCount) {
            // Find empty animal slot via helper
            int slotIdx = s_fn_PastureHelper8350(local_a8);
            if (slotIdx >= 0) {
                // Get entity from pasture slot array
                local_res20 = *(uintptr_t*)(*(uintptr_t*)(local_a8 + 0x30) + (uintptr_t)slotIdx * 8);

                // Navigate entity chain to get NatRes component
                uintptr_t local_a0_0 = *(uintptr_t*)(local_res20 + 0x10);
                uintptr_t natResIBP = 0;
                s_fn_FindComp_NatRes(local_a0_0, &natResIBP);

                if (natResIBP != 0) {
                    // Check if not flagged for removal (bit 20)
                    if (((*(uint32_t*)(natResIBP + 0x20) >> 0x14) & 1) == 0) {
                        // Get entity from natRes to find Family component
                        local_a0_0 = *(uintptr_t*)(natResIBP + 0x10);
                        uintptr_t familyIBP[2] = {0};
                        s_fn_FindComp_Family2(local_a0_0, familyIBP);  // Entity_FindComponent_Family @ 0x1400012d0
                        uintptr_t familyComp = *(uintptr_t*)familyIBP;
                        uintptr_t familyCharComp = *(uintptr_t*)(familyComp + 0x68);

                        // Get animal position as cell index
                        iVar21 = *piVar4;
                        (void)*(uint32_t*)(familyCharComp + 0x160);  // local_48: read but unused
                        iVar9 = (int)(*(float*)(familyCharComp + 0x158) + (float)(iVar21 >> 1));
                        iVar10 = (int)(*(float*)(familyCharComp + 0x15c) + (float)(iVar21 >> 1));
                        if (iVar9 < 0) iVar9 = 0;
                        if (iVar21 <= iVar9) iVar9 = iVar21 - 1;
                        if (iVar10 < 0) iVar10 = 0;
                        if (iVar21 <= iVar10) iVar10 = iVar21 - 1;
                        int animalCell = iVar21 * iVar10 + iVar9;

                        // Pathfinding check
                        uint32_t uVar11 = (uint32_t)s_fn_Map_FindNearestPassable(piVar4, animalCell, uVar23, 0xe1f);
                        uint64_t uVar16 = (uint64_t)s_fn_Map_CanPathTo(piVar4, 0xe1f, (uint32_t)uVar11, uVar23);

                        if ((char)uVar16 != '\0') {
                            // Get description from natRes for RemoveAnimal action
                            void* descPtr = *(void**)(natResIBP + 0x18);
                            bool isDerived = fn_IsDerivedFrom(descPtr, *s_typeId_ComponentEntity);
                            pvVar20 = nullptr;
                            if (isDerived) {
                                pvVar20 = descPtr;
                            }

                            // Compute world position from cell index
                            float halfOff = (float)(*piVar4 >> 1) - *s_fHalfCellOffset;
                            Math::Vector3 local_80(
                                (float)((int)uVar11 % *piVar4) - halfOff,
                                (float)((int)uVar11 / *piVar4) - halfOff,
                                0.0f);
                            float local_70[4];  // Vector3 + padding
                            s_fn_GetTerrainPos(piVar4, local_70, &local_80.x, '\0', nullptr);

                            // Build RemoveAnimal action
                            uintptr_t local_90 = *(uintptr_t*)citizenIBP;
                            uintptr_t local_a0_ibp = natResIBP;
                            s_fn_RemoveAnimalCtor(
                                &local_90,              // citizenIBP
                                (int)selfQ[4],          // priority
                                local_70,               // position
                                animalCell,             // cell index
                                &local_a0_ibp,          // natRes IBP
                                (uintptr_t)pvVar20,     // description ptr
                                1);                     // flag (CONCAT31 result)

                            // Assign work to citizen
                            int priority = (int)selfQ[4];
                            uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

                            // Call vtable slot +0x50 (GetWorkEntity)
                            uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(&local_90));
                            uintptr_t workEnt = *wResult;

                            *(uint32_t*)(citizenEnt + 0x54) |= 4;
                            *(int32_t*)(citizenEnt + 0x30) = priority;
                            *(uint32_t*)(citizenEnt + 0xb0) = 0;

                            if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                                *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                            }
                            return 2;
                        }
                    }
                }
            }
            // Slot search failed
            return 1;
        }

        // -------------------------------------------------------
        // Branch B: At animal capacity -> try position-based overflow storage work
        // -------------------------------------------------------
        uintptr_t posCompIBP = 0;
        s_fn_FindComp_Position(*plVar17, &posCompIBP);

        int posCount = s_fn_PositionCount(posCompIBP, 2);

        if (posCount > 0) {
            int iPvVar20 = 0;
            do {
                float local_80[3];
                s_fn_Entity_GetPosition(posCompIBP, local_80, 2, iPvVar20, nullptr);

                int iVar10map = *piVar4;
                int iVar21pos = (int)(local_80[0] + (float)(iVar10map >> 1));
                int iVar12 = (int)(local_80[1] + (float)(iVar10map >> 1));
                if (iVar21pos < 0) iVar21pos = 0;
                if (iVar10map <= iVar21pos) iVar21pos = iVar10map - 1;
                if (iVar12 < 0) iVar12 = 0;
                if (iVar10map <= iVar12) iVar12 = iVar10map - 1;

                // Read entity from map grid cell
                uintptr_t mapDataBase = *(uintptr_t*)((uintptr_t)piVar4 + 0x80);  // piVar4 + 0x20 as int* => byte offset 0x80
                uintptr_t cellOffset = ((uintptr_t)iVar12 * (uintptr_t)iVar10map + (uintptr_t)iVar21pos) * 0x30;
                local_res20 = *(uintptr_t*)(mapDataBase + 8 + cellOffset);

                if (local_res20 != 0) {
                    uintptr_t familyIBP2[2] = {0};
                    s_fn_FindComp_Family(local_res20, familyIBP2);

                    if (*(uintptr_t*)familyIBP2 != 0) {
                        uintptr_t storageIBP2[4] = {0};  // local_70
                        s_fn_FindComp_MatDesc(local_res20, storageIBP2);

                        uintptr_t local_90_val = *(uintptr_t*)citizenIBP;
                        uintptr_t storageWorkMgr = *(uintptr_t*)(gameWorld + 0x118);
                        uint64_t overflowResult = s_fn_StorageOverflowCheck(
                            storageWorkMgr, &local_90_val,
                            *(int*)(*(uintptr_t*)storageIBP2 + 0x28), '\x01');

                        if ((char)overflowResult != '\0') {
                            return 2;
                        }
                    }
                }

                iPvVar20++;
            } while (iPvVar20 < posCount);
        }

        // -------------------------------------------------------
        // Branch C: Final fallback -> random area position, create grazing work
        // -------------------------------------------------------
        uintptr_t areaIBP = 0;
        s_fn_GetAreaComponent(*plVar17, &areaIBP);

        void* rng = *(void**)(gameWorld + 0xd8);  // Random*

        short sVar1 = *(short*)(areaIBP + 0x28);
        short sVar2 = *(short*)(areaIBP + 0x2a);
        short sVar3 = *(short*)(areaIBP + 0x2c);

        uint32_t uVar11 = (uint32_t)reinterpret_cast<System::Random*>(rng)->RandTol((int)*(short*)(areaIBP + 0x2e));   // B.3.4b bridge
        uint32_t uVar13 = (uint32_t)reinterpret_cast<System::Random*>(rng)->RandTol((int)sVar3);   // B.3.4b bridge
        int randCell = ((int)sVar2 + uVar11) * *piVar4 + uVar13 + (int)sVar1;

        // Update local_res20 with randCell (low 32 bits)
        local_res20 = (local_res20 & 0xFFFFFFFF00000000ULL) | (uint32_t)randCell;

        int nearCell = s_fn_Map_FindNearestPassable(piVar4, randCell, uVar23, 0xe1f);
        if (nearCell >= 0) {
            uint64_t canPath = (uint64_t)s_fn_Map_CanPathTo(piVar4, 0xe1f, uVar23, nearCell);
            uint32_t dVar7 = *s_typeId_NatResComponent;  // DWORD_1401c7ba0

            if ((char)canPath != '\0') {
                float halfOff = (float)(*piVar4 >> 1) - *s_fHalfCellOffset;
                Math::Vector3 local_80(
                    (float)((int)local_res20 % *piVar4) - halfOff,
                    (float)((int)local_res20 / *piVar4) - halfOff,
                    0.0f);
                float local_a0_pos[4];  // position buffer
                s_fn_GetTerrainPos(piVar4, local_a0_pos, &local_80.x, '\0', nullptr);

                halfOff = (float)(*piVar4 >> 1) - *s_fHalfCellOffset;
                Math::Vector3 local_70_pos(
                    (float)(nearCell % *piVar4) - halfOff,
                    (float)(nearCell / *piVar4) - halfOff,
                    0.0f);
                // Retail 0x1400F2370: this local is a Vector3 (12 bytes) that GetTerrainPos
                // fills; the port had it as float[2], 4 bytes short -- a stack overwrite.
                Math::Vector3 local_50;  // dest position
                s_fn_GetTerrainPos(piVar4, &local_50.x, &local_70_pos.x, '\0', nullptr);

                // Create grazing work action (FUN_1400bb0a0)
                local_res20 = *plVar17;
                uintptr_t local_90_val = *(uintptr_t*)local_res10;
                s_fn_ActionCtor_GrazingWork(
                    &local_90_val,          // citizenIBP value
                    (int)selfQ[4],          // priority
                    &local_res20,           // work entity IBP
                    0,                      // param4
                    &local_50.x,            // dest position
                    local_a0_pos,           // source position
                    dVar7,                  // type ID (DWORD_1401c7ba0)
                    0,                      // param8
                    0);                     // param9

                // Assign work to citizen
                int priority = (int)selfQ[4];
                uintptr_t citizenEnt = *(uintptr_t*)local_res10;

                // ⚠ Retail saves the citizen IBP pointer (pIVar15) BEFORE the
                // vtable+0x50 call: slot 10 writes its result (the workplace's
                // profession key, wpc+0x48) THROUGH the out param — i.e. it
                // OVERWRITES local_res10. The register call below must use the
                // SAVED pointer; passing the clobbered local made the BST key
                // *(Profession*) = g_Prof_Vtable, and the next Slot9 validate
                // crashed IsA on it (organic herder, first completed cycle).
                void* savedCitizenIBP = local_res10;

                // Call vtable slot +0x50 (GetWorkEntity — returns the profession key)
                uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(&local_res10));
                uintptr_t workEnt = *wResult;

                *(uint32_t*)(citizenEnt + 0x54) |= 4;
                *(int32_t*)(citizenEnt + 0x30) = priority;
                *(uint32_t*)(citizenEnt + 0xb0) = 0;

                // Profession match: slot 10's result vs citizen+0x1F0 (Profession*).
                if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                    *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                }

                // Register work with CitizenAI (CitizenAI_impl_1a30):
                // (tree root = self+0x48, outResult = &local_res10, key = citizen IBP)
                s_fn_CitizenAI_RegisterWork((void*)(selfAddr + 0x48), &local_res10, savedCitizenIBP);
                return 2;
            }
        }
        return 0;
    }

    // Worker count at or below max but resource at capacity -> return 1
    return 1;
}


// ============================================================
// PastureWork::Slot6 -- NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// ============================================================
int PastureWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);
    return false;
}


// ============================================================
// PastureWork::Slot7 (0x1400F22C0) -- Tree insertion with validation
// Unlike other Work classes, this does more than BSTFindRemove:
// 1. Insert citizen into BST at self+0x48
// 2. If param3 entity is non-null:
//    a. Check if it's derived from ComponentEntity
//    b. Find NatRes component
//    c. Clear "flagged for removal" bit (0x100000) in entity flags at +0x20
//    d. If flag changed and entity has parent (+0x10), call UpdateParentEntity
// ============================================================
int PastureWork::OnCancel(void* param2, void* param3, int /*arg3*/) {
    // Insert citizen into BST
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);

    // Check if param3 entity is valid
    if (*(uintptr_t*)param3 == 0) {
        return 0;
    }

    uintptr_t entity = *(uintptr_t*)param3;

    // Check if derived from ComponentEntity
    if (!fn_IsDerivedFrom((void*)entity, *s_typeId_ComponentEntity)) {
        return 0;
    }

    // Find NatRes component
    uintptr_t natResIBP[2] = {0};
    s_fn_FindComp_NatRes(entity, natResIBP);

    if (*(uintptr_t*)natResIBP == 0) {
        return 0;
    }

    // Clear "flagged for removal" bit (0x100000) in entity flags at +0x20
    uintptr_t natResObj = *(uintptr_t*)natResIBP;
    uint32_t oldFlags = *(uint32_t*)(natResObj + 0x20);
    uint32_t newFlags = oldFlags & ~0x100000u;

    if (newFlags != oldFlags) {
        *(uint32_t*)(natResObj + 0x20) = newFlags;

        // If entity has parent, update it
        uintptr_t parentEntity = *(uintptr_t*)(natResObj + 0x10);
        if (parentEntity != 0) {
            s_fn_UpdateParentEntity(parentEntity);
        }
    }
    return 0;
}


// ============================================================
// PastureWork::Slot8 / CancelAll (0x1400F2B90)
// Identical to HuntWork::Slot8 -- loops tree, cancels each.
// ============================================================
int PastureWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// PastureWork::Slot9 / RemoveInvalid (0x1400F2090)
// Identical to HuntWork::Slot9 -- BST traversal, validates, removes.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP).
// ============================================================
int PastureWork::Validate(void* param2) {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find leftmost node
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        bool stillValid = s_fn_ValidateEntity(selfAddr, param2, (void*)&entityIBP, nullptr);

        if (!stillValid) {
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

        void* rightChild = *(void**)((uintptr_t)node + 0x18);
        if (rightChild != nullptr) {
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
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

    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                } else {
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
// PastureWork::Slot10 / GetWorkEntity (0x1400F2C00)
// Identical to HuntWork::Slot10.
// ============================================================
void* PastureWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// PastureWork::GetPriority [11] 0x140029860 (shared by 13 tables) / HasWorkers.
// Returns (this->_field50 > 0).
// ============================================================
int PastureWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}




// ============================================================
// PastureWork::Serialize (0x1400F2C60)
// Work::Serialize + Serializer(version=0) + BST serialize.
// Identical to HuntWork::Serialize (no extra int field).
// ============================================================
void PastureWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    Work::Persist(stream);   // 0x1400f7400, a direct call in retail — QUALIFIED here

    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializer2);

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// PastureWork::DtorBody (0x1400F2CE0)
// Identical pattern to HuntWork, uses vtable_PastureWork.
// ============================================================
// PastureWork dtor (retail 0x1400f2ce0, called by slot 0 0x14002bf70). The stamps and the `~Internal` call are the compiler's chain.
PastureWork::~PastureWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}


// ============================================================
// PastureWork::ScalarDtor (0x14002BF70)
// Calls dtor body, then conditional free.
// ============================================================
// [0] 0x14002bf70: ~PastureWork, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* PastureWork::DeletingDtor(unsigned flags) {
    this->~PastureWork();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}



// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's. PastureWork_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
PastureWork::PastureWork() : AutoPickupWork(), _field48(nullptr), _field50(0) {
    PastureWork_ResolveFuncs();
}

// ============================================================
// PastureWork::PastureWork (0x1400f2d80) — the retail ctor Slot14 cmd-2 calls
// when it turns the herder on. Base = Work_Ctor25 (0x1400f7250 pattern):
//   +0x10 = *entityIBP (raw 8-byte copy of the pasture entity ref)
//   +0x18 = WorkComponent ref (finder 0x140003050 on the same entity)
//   +0x20 = -1, +0x28 = -1 OR the Area component's center cell:
//           ((h>>1)+y)*mapDim + (w>>1)+x  from area +0x28/+0x2a/+0x2c/+0x2e
//   +0x24/+0x2c/+0x30/+0x38/+0x40 = 0
// PastureWork tail: vtable, +0x48 BST root = 0, +0x50 count = 0.
// The tree's port starts from the default shape (Work(): the zero fields and the two
// -1s) and adds the entity-dependent fields on top; that is kept.
// ============================================================
PastureWork::PastureWork(void* entityIBP) : AutoPickupWork(), _field48(nullptr), _field50(0) {
    PastureWork_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    *reinterpret_cast<uint64_t*>(s + 0x10) = *reinterpret_cast<uint64_t*>(entityIBP);
    if (*reinterpret_cast<uint64_t*>(s + 0x10) != 0 &&
        g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t areaIBP[2] = {0};
        s_fn_GetAreaComponent(*reinterpret_cast<uintptr_t*>(s + 0x10), areaIBP);
        if (areaIBP[0] != 0) {
            uintptr_t a = areaIBP[0];
            uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;
            int mapDim = **reinterpret_cast<int**>(gw + 0xf8);
            *reinterpret_cast<int32_t*>(s + 0x28) =
                (((int)*reinterpret_cast<int16_t*>(a + 0x2e) >> 1) +
                  (int)*reinterpret_cast<int16_t*>(a + 0x2a)) * mapDim +
                ((int)*reinterpret_cast<int16_t*>(a + 0x2c) >> 1) +
                 (int)*reinterpret_cast<int16_t*>(a + 0x28);
        }
        uintptr_t wcIBP[2] = {0};
        s_fn_FindComp_MatDesc(*reinterpret_cast<uintptr_t*>(entityIBP), wcIBP);  // 0x140003050 = WorkComponent finder
        *reinterpret_cast<uint64_t*>(s + 0x18) = wcIBP[0];
    }
}

// The by-address door for 0x1400f2d80 (registered below): the real ctor on caller
// memory (B8-5).
extern "C" void* PastureWork_Ctor_Impl(void* mem, void* entityIBP) {
    if (!mem) return nullptr;
    return static_cast<Object::Base*>(new (mem) PastureWork(entityIBP));
}

// Convenience for PastureComponent Slot14 (pasturecomponent_states.cpp, block MI-B):
// MemAlloc(0x58,8) + the faithful ctor. `entityRefSlot` points at an 8-byte entity ref
// (the pasture's owner entity).
extern "C" void* PastureWork_CreateForEntity(void* entityRefSlot) {
    PastureWork_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureWork), 8);
    return p ? static_cast<Object::Base*>(new (p) PastureWork(entityRefSlot)) : nullptr;
}


// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_PastureWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over PastureWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.
// Nothing outside this block reads 0x1401617a0 back, so that row goes with the table.

// create 0x14002bf00: MemAlloc(sizeof, 8), then the ctor.
void* PastureWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PastureWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* PastureWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PastureWork());
}

DEFINE_EXE_VTABLE(PastureWork, "_ZTV11PastureWork")

#include "Object/factory_registration.h"
#include "address_registry.h"

// The ctor door 0x1400f2d80 stays registered (the function keeps its name because
// address_registry.cpp calls it by name).
extern "C" void RegisterPastureWorkVtable() {
    RegisterExeAddr(0x1400f2d80, reinterpret_cast<uintptr_t>(&PastureWork_Ctor_Impl));
}
DEFINE_EXE_TYPE(PastureWork, "AutoPickupWork")
