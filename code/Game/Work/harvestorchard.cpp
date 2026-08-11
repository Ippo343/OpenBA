// code/Game/Work/harvestorchard.cpp
// Clean C++ reimplementation of HarvestOrchard Work class.
// Source: Game/Work/HarvestOrchard.cpp (from profile build debug strings)
//
// HarvestOrchard (0x68 bytes) -- Orchard harvest work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestOrchard
// Vtable: 0x140161520
//
// Same struct layout as HarvestArea but different harvest logic:
// - Orchards use a different description component (OrchardDescription at DAT_1401c46b0)
// - Slot4 only harvests single trees (no paired cell logic like HarvestArea)
// - Slot4 uses FUN_1400bb7b0 to create harvest action (different from ShowTool_Action_Ctor4)
// - Slot4 also looks up RawMaterial description and passes extra params
// - Slot6/Slot7 use simple int key (not packed high/low 16-bit)
//
// Methods implemented:
//   Slot4 (0x1400ED8C0), Slot6 (0x1400ED880), Slot7 (0x1400ED810)
//   Slot8 (0x1400EDDC0), Slot9 (0x1400ED5E0), Slot10 (0x1400EDE40)
//   Serialize (0x1400EDEA0), ScalarDtor body (0x1400EDF60)
//

#include "Work/harvestorchard_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// Diagnostics-only counters (standalone/orchard_diag.cpp). No-op unless
// BANISHED_ORCHARD_DIAG is set; never change behaviour.
extern "C" void OrchardDiag_NoteHarvestConstructed();
extern "C" void OrchardDiag_NoteHarvestExecuted();

// ============================================================
// Local function pointers (resolved once by HarvestOrchard_ResolveFuncs)
// ============================================================

typedef int (*HO_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
typedef void* (*HO_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HO_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void* (*HO_FindComp_OrchardDesc_t)(uintptr_t entity, void* outIBP);  // FUN_140005440
typedef void* (*HO_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);
typedef void* (*HO_FindComp_DescComp_t)(uintptr_t entity, void* outIBP);     // FUN_14000a0b0
typedef void  (*HO_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
typedef int   (*HO_CalcMaxWorkers_t)(uintptr_t matDesc);
typedef int   (*HO_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*HO_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
typedef float*(*HO_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
typedef float*(*HO_Vector3_Ctor_t)(float* self, float x, float y, float z);
typedef void* (*HO_FindComp_RawMaterial_t)(uintptr_t compList, void* outPtr);

// FUN_1400bb7b0 -- builds the gather-raw-material action chain on the citizen's AI task list.
// Signature taken from the PORTED body (code/Game/Work/work_act_rawmat.cpp, which was
// reversed from 0x1400bb7b0 itself), not from this call site:
//   (citizenIBP, workId u32, workEntity int64*, worldPos float*, rawMatP2 u64,
//    flag6 char, rawMatP3 FLOAT, rawMatP8 u32, param9 u32)
// ⚠ The last three used to be declared (u32, int, float) here — one slot out of step with
// the callee on both type and meaning. Args 5+ travel in 8-byte stack slots, so nothing
// crashed; the callee simply read a float's bit pattern as a count and an int slot as a
// float. Fixed 2026-08-10 together with the three call-site arguments below.
typedef void (*HO_OrchardHarvest_Ctor_t)(void* citizenIBP, uint32_t workId, void* workEntity,
                                          float* worldPos, uintptr_t rawMatP2, char flag6,
                                          float rawMatP3, uint32_t rawMatP8, uint32_t param9);
static HO_OrchardHarvest_Ctor_t s_fn_OrchardHarvest_Ctor = nullptr;

// BST operations
typedef void* (*HO_BST_Insert_t)(void* treeRoot, void* outResult, uint32_t* keyData);
typedef void  (*HO_BST_Remove_t)(void* treeRoot, void* node);
typedef void  (*HO_BST_Serialize_t)(void* treeRoot, void* serializer);
typedef uint8_t (*HO_ValidateTask_t)(uintptr_t self, uintptr_t param2, void* ibp, void** outTask);
typedef void  (*HO_TaskCancel_t)(void* taskList);
typedef void* (*HO_FindPosComp_t)(uintptr_t entity, void* outIBP);
typedef void  (*HO_Work_Serialize_t)(void* self, void* stream);
typedef void  (*HO_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*HO_Serializer_Dtor_t)(void* self);
typedef void  (*HO_Serializer_SerializeInt_t)(void* ser, int* buf, const char* name);
typedef void  (*HO_Serializer_SerializeBoolArray_t)(void* ser, int count, void* buf, const char* name);
typedef void  (*HO_ResizeByteArray_t)(void* arrayField, int newCount);
typedef void  (*HO_BST_RecursiveFree_t)(void* treeRoot, void* node);
typedef void  (*HO_ObjectInternal_Dtor_t)(void* self);
typedef void* (*HO_MemAlloc_t)(int64_t size, int align);

static HO_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;
static HO_FindComp_MatDesc_t s_fn_FindComp_MatDesc = nullptr;
static HO_FindComp_MeshInstance_t s_fn_FindComp_MeshInstance = nullptr;
static HO_FindComp_OrchardDesc_t s_fn_FindComp_OrchardDesc = nullptr;
static HO_FindComp_NatRes_t s_fn_FindComp_NatRes = nullptr;
static HO_FindComp_DescComp_t s_fn_FindComp_DescComp = nullptr;
static HO_MeshInstance_SetVisible_t s_fn_MeshInstance_SetVisible = nullptr;
static HO_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr;
static HO_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr;
static HO_Map_CanPathTo_t s_fn_Map_CanPathTo = nullptr;
static HO_GetTerrainPos_t s_fn_GetTerrainPos = nullptr;
static HO_Vector3_Ctor_t s_fn_Vector3_Ctor = nullptr;
static HO_FindComp_RawMaterial_t s_fn_FindComp_RawMaterial = nullptr;
static HO_BST_Insert_t s_fn_BST_Insert = nullptr;
static HO_BST_Remove_t s_fn_BST_Remove = nullptr;
static HO_BST_Serialize_t s_fn_BST_Serialize = nullptr;
static HO_ValidateTask_t s_fn_ValidateTask = nullptr;
static HO_TaskCancel_t s_fn_TaskCancel = nullptr;
static HO_FindPosComp_t s_fn_FindPosComp = nullptr;
static HO_Work_Serialize_t s_fn_Work_Serialize = nullptr;
static HO_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
static HO_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;
static HO_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;
static HO_Serializer_SerializeBoolArray_t s_fn_Serializer_SerializeBoolArray = nullptr;
static HO_ResizeByteArray_t s_fn_ResizeByteArray = nullptr;
static HO_BST_RecursiveFree_t s_fn_BST_RecursiveFree = nullptr;
static HO_ObjectInternal_Dtor_t s_fn_ObjectInternal_Dtor = nullptr;
static HO_MemAlloc_t s_fn_MemAlloc = nullptr;

// Type IDs
static uint32_t* s_typeId_OrchardDesc = nullptr;  // DAT_1401c46e8
static uint32_t* s_typeId_DescComp = nullptr;      // DAT_1401c4288

// Float constants
static float* s_fHalfCellOffset = nullptr;

// Vtable pointers
static void** s_vtable_HarvestOrchard = nullptr;
static void** s_vtable_AutoPickupWork = nullptr;
static void** s_vtable_Work = nullptr;

static bool s_ho_funcs_resolved = false;

// ============================================================
// Resolution
// ============================================================
bool HarvestOrchard_ResolveFuncs() {
    if (s_ho_funcs_resolved) return true;

    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<HO_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<HO_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<HO_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_FindComp_OrchardDesc = reinterpret_cast<HO_FindComp_OrchardDesc_t>(
        ResolveAddr(Module::Exe, 0x140005440));
    s_fn_FindComp_NatRes = reinterpret_cast<HO_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_FindComp_DescComp = reinterpret_cast<HO_FindComp_DescComp_t>(
        ResolveAddr(Module::Exe, 0x14000a0b0));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<HO_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<HO_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HO_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HO_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<HO_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_FindComp_RawMaterial = reinterpret_cast<HO_FindComp_RawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140006330));
    s_fn_OrchardHarvest_Ctor = reinterpret_cast<HO_OrchardHarvest_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bb7b0));
    s_fn_BST_Insert = reinterpret_cast<HO_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140029a40));
    s_fn_BST_Remove = reinterpret_cast<HO_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));  // was 0x140029a60 (mid-function; true remove entry is 0x14001ca60)
    s_fn_BST_Serialize = reinterpret_cast<HO_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140029de0));
    s_fn_ValidateTask = reinterpret_cast<HO_ValidateTask_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TaskCancel = reinterpret_cast<HO_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindPosComp = reinterpret_cast<HO_FindPosComp_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_Work_Serialize = reinterpret_cast<HO_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ResizeByteArray = reinterpret_cast<HO_ResizeByteArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_BST_RecursiveFree = reinterpret_cast<HO_BST_RecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x14000e320));
    // Level-1 caller-bug fix: 0x14014e3a4 is MID-FUNCTION (inside Tool_StaticInit
    // 0x14014e390) — a bad code address. The "ObjectInternal_Dtor" is the base
    // Object::Internal::~Internal tail-called at the end of the HarvestOrchard dtor body
    // (0x1400edf60). It is reached via the dispatch slot 0x140152770, exactly as the
    // sibling HarvestArea resolves it (harvestarea.cpp). Resolve through the slot, not a
    // code address.
    uintptr_t iatInternalDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatInternalDtor)
        s_fn_ObjectInternal_Dtor = *reinterpret_cast<HO_ObjectInternal_Dtor_t*>(iatInternalDtor);

    // Dispatch slot IATs
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<HO_Vector3_Ctor_t*>(iatVec3Ctor);

    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<HO_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<HO_Serializer_Dtor_t*>(iatSerDtor);
    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt)
        s_fn_Serializer_SerializeInt = *reinterpret_cast<HO_Serializer_SerializeInt_t*>(iatSerInt);
    s_fn_Serializer_SerializeBoolArray = reinterpret_cast<HO_Serializer_SerializeBoolArray_t>(
        s_fn_Serializer_SerializeInt);

    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140152780);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<HO_MemAlloc_t*>(iatMemAlloc);

    // Type IDs
    s_typeId_OrchardDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c46e8));
    s_typeId_DescComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4288));

    // Float constants
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable pointers
    s_vtable_HarvestOrchard = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161520));
    s_vtable_AutoPickupWork = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161380));
    s_vtable_Work = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161300));

    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_FindComp_OrchardDesc &&
              s_fn_FindComp_NatRes && s_fn_FindComp_DescComp &&
              s_fn_MeshInstance_SetVisible && s_fn_CalcMaxWorkers &&
              s_fn_Map_FindNearestPassable && s_fn_Map_CanPathTo &&
              s_fn_GetTerrainPos && s_fn_Vector3_Ctor &&
              s_fn_FindComp_RawMaterial && s_fn_OrchardHarvest_Ctor &&
              s_fn_BST_Insert && s_fn_BST_Remove && s_fn_BST_Serialize &&
              s_fn_ValidateTask && s_fn_TaskCancel && s_fn_FindPosComp &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_ResizeByteArray && s_fn_BST_RecursiveFree &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_typeId_OrchardDesc && s_typeId_DescComp && s_fHalfCellOffset &&
              s_vtable_HarvestOrchard && s_vtable_AutoPickupWork && s_vtable_Work;

    s_ho_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[HarvestOrchard] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    // The MISSING list must cover EVERY term of `ok`. It used to print 9 of 35, so a
    // failure in any of the other 26 reported "FAILED" with no reason at all — which is
    // exactly what state 11's first live run hit (see reference_diagnostics_must_report_outcomes).
    if (!ok) {
        #define HO_MISS(cond, name) do { if (!(cond)) printf("  MISSING: %s\n", name); } while (0)
        HO_MISS(s_fn_AutoPickup_CheckAssignments, "AutoPickup_CheckAssignments");
        HO_MISS(s_fn_FindComp_MatDesc, "FindComp_MatDesc");
        HO_MISS(s_fn_FindComp_MeshInstance, "FindComp_MeshInstance");
        HO_MISS(s_fn_FindComp_OrchardDesc, "FindComp_OrchardDesc");
        HO_MISS(s_fn_FindComp_NatRes, "FindComp_NatRes");
        HO_MISS(s_fn_FindComp_DescComp, "FindComp_DescComp");
        HO_MISS(s_fn_MeshInstance_SetVisible, "MeshInstance_SetVisible");
        HO_MISS(s_fn_CalcMaxWorkers, "CalcMaxWorkers");
        HO_MISS(s_fn_Map_FindNearestPassable, "Map_FindNearestPassable");
        HO_MISS(s_fn_Map_CanPathTo, "Map_CanPathTo");
        HO_MISS(s_fn_GetTerrainPos, "GetTerrainPos");
        HO_MISS(s_fn_Vector3_Ctor, "Vector3_Ctor (slot 0x140152a30)");
        HO_MISS(s_fn_FindComp_RawMaterial, "FindComp_RawMaterial");
        HO_MISS(s_fn_OrchardHarvest_Ctor, "OrchardHarvest_Ctor");
        HO_MISS(s_fn_BST_Insert, "BST_Insert");
        HO_MISS(s_fn_BST_Remove, "BST_Remove");
        HO_MISS(s_fn_BST_Serialize, "BST_Serialize");
        HO_MISS(s_fn_ValidateTask, "ValidateTask");
        HO_MISS(s_fn_TaskCancel, "TaskCancel");
        HO_MISS(s_fn_FindPosComp, "FindPosComp");
        HO_MISS(s_fn_Work_Serialize, "Work_Serialize");
        HO_MISS(s_fn_Serializer_Ctor, "Serializer_Ctor (slot 0x140152ad8)");
        HO_MISS(s_fn_Serializer_Dtor, "Serializer_Dtor (slot 0x140152ad0)");
        HO_MISS(s_fn_ResizeByteArray, "ResizeByteArray");
        HO_MISS(s_fn_BST_RecursiveFree, "BST_RecursiveFree");
        HO_MISS(fn_InternalBasePtr_CheckPtr, "fn_InternalBasePtr_CheckPtr");
        HO_MISS(fn_InternalBasePtr_Ctor, "fn_InternalBasePtr_Ctor");
        HO_MISS(fn_IsDerivedFrom, "fn_IsDerivedFrom");
        HO_MISS(g_pGameWorldSingleton, "g_pGameWorldSingleton");
        HO_MISS(s_typeId_OrchardDesc, "typeId_OrchardDesc (0x1401c46e8)");
        HO_MISS(s_typeId_DescComp, "typeId_DescComp (0x1401c4288)");
        HO_MISS(s_fHalfCellOffset, "fHalfCellOffset (0x140172114)");
        HO_MISS(s_vtable_HarvestOrchard, "vtable_HarvestOrchard (0x140161520)");
        HO_MISS(s_vtable_AutoPickupWork, "vtable_AutoPickupWork (0x140161380)");
        HO_MISS(s_vtable_Work, "vtable_Work (0x140161300)");
        #undef HO_MISS
    }
#endif

    return ok;
}


// ============================================================
// Helper: BST search for int key
// ============================================================
static void* BST_Find(void* root, int key) {
    int* node = (int*)root;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);
        } else if (key > *node) {
            node = *(int**)(node + 8);
        } else {
            return node;
        }
    }
    return nullptr;
}


// ============================================================
// HarvestOrchard::Slot4 -- Main orchard harvest assignment logic
// ============================================================
//
// Key differences from HarvestArea:
// - Uses OrchardDescription component (DAT_1401c46b0) instead of CropDescription
// - No paired cell harvesting (single tree per assignment)
// - Checks NaturalResource growth flags + description type
// - Uses FUN_1400bb7b0 to create harvest action (orchard-specific)
// - Passes raw material ref and material count to action constructor
// - Orchard cell data at +0x50 with stride 0x28 (not +0x58 with stride 0x20)
//
int HarvestOrchard_Slot4_Impl(HarvestOrchard* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // Step 1: Check auto-pickup
    uintptr_t checkRef = citizenRef;
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &checkRef);
    if (checkResult == 2) {
        return 2;
    }

    // Step 2: Find components
    uintptr_t* workEntityPtr = (uintptr_t*)(selfAddr + 0x10);
    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*workEntityPtr, &matDescIBP);

    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t orchardDescIBP[2] = {0};
    s_fn_FindComp_OrchardDesc(*workEntityPtr, orchardDescIBP);

    // Get OrchardDescription object
    fn_InternalBasePtr_CheckPtr(orchardDescIBP);
    void* descObj = *(void**)(orchardDescIBP[0] + 0x18);
    bool isDerived = fn_IsDerivedFrom(descObj, *s_typeId_OrchardDesc);
    void* orchardDesc = isDerived ? descObj : nullptr;

    // Check tree count vs max workers
    int treeCount = (int)selfQ[10]; // _field50
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Check capacity
    int materialIdx = *(int*)((uintptr_t)orchardDesc + 0x20);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*workEntityPtr, meshIBP);
    fn_InternalBasePtr_CheckPtr(meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // Step 3: Get map and citizen position
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;
    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(aiComp + 0x68));
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;
    uint32_t citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Step 4: Iterate orchard cells
    int fieldCellCount = (int)selfQ[0xc]; // _field60 as cell count
    float cellFloat = 0.0f;

    if (fieldCellCount > 0) {
        for (int i = 0; i < fieldCellCount; i++) {
            char* validityArray = (char*)selfQ[0xb]; // _field58
            if (validityArray[i] == 0) {
                goto next_cell;
            }

            // Check if cell is already in BST
            {
                int* bstRoot = (int*)selfQ[9]; // _field48
                void* found = BST_Find(bstRoot, i);
                if (found != nullptr) {
                    goto next_cell;
                }
            }

            // Look up entity at this cell
            {
                fn_InternalBasePtr_CheckPtr(orchardDescIBP);
                uintptr_t orchDescVal = orchardDescIBP[0];
                uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20);
                // Orchard cell data stride is 0x28 (different from HarvestArea's 0x20)
                int cellGridIdx = *(int*)(*(uintptr_t*)(orchDescVal + 0x50) + (uintptr_t)i * 0x28);
                uintptr_t entityAtCell = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx * 0x30 + 8);

                if (entityAtCell == 0) {
                    validityArray[i] = 0;
                } else {
                    // Check pathfinding
                    fn_InternalBasePtr_CheckPtr(orchardDescIBP);
                    int srcCellIdx = *(int*)(*(uintptr_t*)(orchardDescIBP[0] + 0x50) + (uintptr_t)i * 0x28);
                    int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);

                    if (nearestCell >= 0) {
                        uint64_t canPath = (uint64_t)s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
                        if ((char)canPath != 0) {
                            // Check NaturalResource component
                            fn_InternalBasePtr_CheckPtr(&entityAtCell);
                            uintptr_t natResIBP = 0;
                            s_fn_FindComp_NatRes(entityAtCell, &natResIBP);

                            if (natResIBP == 0) {
                                validityArray[i] = 0;
                            } else {
                                // Check flag bit 20 (flagged for removal)
                                fn_InternalBasePtr_CheckPtr(&natResIBP);
                                if ((*(uint32_t*)(natResIBP + 0x20) >> 20) & 1) {
                                    goto next_cell;
                                }

                                // Get description component from entity
                                fn_InternalBasePtr_CheckPtr(&entityAtCell);
                                uintptr_t descCompIBP = 0;
                                s_fn_FindComp_DescComp(entityAtCell, &descCompIBP);

                                fn_InternalBasePtr_CheckPtr(&descCompIBP);
                                void* descCompObj = *(void**)(descCompIBP + 0x18);
                                bool isDescDerived = fn_IsDerivedFrom(descCompObj, *s_typeId_DescComp);
                                void* descComp = isDescDerived ? descCompObj : nullptr;

                                // Get raw material reference
                                uintptr_t rawMatRef = *(uintptr_t*)((uintptr_t)descComp + 0x30);
                                uintptr_t rawMatIBP = 0;
                                s_fn_FindComp_RawMaterial(rawMatRef, &rawMatIBP);

                                // Arg 7 of the action ctor. Read from the DESCRIPTION-COMPONENT
                                // holder (RBP-0x39, the 0x14000a0b0 finder's out-param), NOT the
                                // orchard component, and as a FLOAT:
                                //   1400edc74  MOV RAX,[RBP-0x39] ; 1400edc7f MOVSS XMM6,[RAX+0x30]
                                //   1400edd04  MOVSS [RSP+0x30],XMM6      <- 7th argument slot
                                // It used to be read as a uint32 off orchardDescIBP — wrong object
                                // and wrong type, so the callee (which takes a float here, and
                                // forwards it to CreateRawMaterial in XMM2) got an integer bit
                                // pattern from an unrelated field.
                                float rawMatAmount = *(float*)(descCompIBP + 0x30);

                                // Calculate world position
                                float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                                float targetPos[3];
                                s_fn_Vector3_Ctor(targetPos,
                                    (float)(nearestCell % *mapGrid) - halfOffset,
                                    (float)(nearestCell / *mapGrid) - halfOffset,
                                    0.0f);

                                float pathPos[4];
                                s_fn_GetTerrainPos(mapGrid, pathPos, targetPos, 0, nullptr);

                                // Get entity ref from orchard description
                                fn_InternalBasePtr_CheckPtr(orchardDescIBP);
                                uintptr_t entityRef = *(uintptr_t*)(orchardDescIBP[0] + 0x10);

                                // Create orchard harvest action
                                uintptr_t citizenVal = *(uintptr_t*)citizenIBP;
                                uintptr_t actionBuf[2] = { citizenVal, 0 };
                                // Arg 6 is the PARITY of the slot index, not "is this the first
                                // cell". Retail: 1400edd00 MOVZX EAX,SIL ; 1400edd0a NOT AL ;
                                // 1400edd14 AND AL,1 — with ESI = i, that is `!(i & 1)`. The old
                                // `(i == 0) ? 0 : 1` was both the wrong test and inverted.
                                char altCellFlag = (char)(~i & 1);

                                s_fn_OrchardHarvest_Ctor(
                                    actionBuf,                     // citizenIBP
                                    (uint32_t)selfQ[4],            // work id / priority (self+0x20)
                                    &entityRef,                    // entity ref ptr
                                    pathPos,                       // position
                                    rawMatRef,                     // raw material ref
                                    altCellFlag,                   // alternating-cell flag
                                    rawMatAmount,                  // float, -> CreateRawMaterial XMM2
                                    (uint32_t)nearestCell,         // map cell index
                                    (uint32_t)i);                  // SLOT index — the value that
                                                                   // comes back as the harvested-slot
                                                                   // id (CompleteTask p5 /
                                                                   // SendComponentMessage p6). This
                                                                   // used to be `cellFloat`, a float
                                                                   // in an integer slot, so every
                                                                   // completion reported the same
                                                                   // wrong slot. See
                                                                   // reference_int_arg_typed_as_float.

                                // Assign work and register with AI
                                int priority = (int)selfQ[4];
                                fn_InternalBasePtr_CheckPtr(citizenIBP);
                                uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

                                uintptr_t vtable = *(uintptr_t*)self;
                                typedef uintptr_t* (*GetWorkEntity_t)(HarvestOrchard*, void*);
                                auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
                                uintptr_t workEntIBP[2] = {0};
                                uintptr_t* wResult = getWorkEntity(self, workEntIBP);
                                uintptr_t workEnt = *wResult;

                                *(uint32_t*)(citizenEnt + 0x54) |= 4;
                                *(int32_t*)(citizenEnt + 0x30) = priority;
                                *(uint32_t*)(citizenEnt + 0xb0) = 0;

                                if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                                    *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                                }

                                // Insert into BST
                                uintptr_t insertResult[2] = {0};
                                uint32_t keyBuf[4] = { (uint32_t)i, 0, 0, 0 };
                                uintptr_t citizenIBPVal = *(uintptr_t*)citizenIBP;
                                keyBuf[2] = (uint32_t)citizenIBPVal;
                                keyBuf[3] = (uint32_t)(citizenIBPVal >> 32);
                                s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, keyBuf);

                                // Mark cell as used
                                validityArray[i] = 0;

                                OrchardDiag_NoteHarvestExecuted();
                                return 2;
                            }
                        }
                    }
                }
            }

        next_cell:
            cellFloat = (float)(i + 1);
        }
    }

    return 1;
}


// ============================================================
// HarvestOrchard::Slot6 -- Insert/remove work entry
// ============================================================
// Simpler than HarvestArea -- single int key, no paired cell.
int HarvestOrchard_Slot6_Impl(HarvestOrchard* self, void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Search BST for key and remove if found
    int* node = (int*)selfQ[9];
    while (node != nullptr) {
        if (param4 < *node) {
            node = *(int**)(node + 6);
        } else if (param4 > *node) {
            node = *(int**)(node + 8);
        } else {
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
            break;
        }
    }

    // Mark cell as valid
    char* validityArray = (char*)selfQ[0xb];
    validityArray[param4] = 1;
    return 0;
}


// ============================================================
// HarvestOrchard::Slot7 -- Tree node lookup with validation
// ============================================================
int HarvestOrchard_Slot7_Impl(HarvestOrchard* self, void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Search BST for key and remove if found
    int* node = (int*)selfQ[9];
    while (node != nullptr) {
        if (param4 < *node) {
            node = *(int**)(node + 6);
        } else if (param4 > *node) {
            node = *(int**)(node + 8);
        } else {
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
            break;
        }
    }

    // Mark cell as valid
    char* validityArray = (char*)selfQ[0xb];
    validityArray[param4] = 1;
    return 0;
}


// ============================================================
// HarvestOrchard::Slot8 -- CancelAll
// ============================================================
int HarvestOrchard_Slot8_Impl(HarvestOrchard* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18);
        }

        fn_InternalBasePtr_CheckPtr((void*)(rightmost + 8));
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HarvestOrchard::Slot9 -- RemoveInvalid
// ============================================================
// Identical structure to HarvestArea::Slot9.
// Hook DISABLED (bug class 2: MOV RAX,RSP in prologue).
int HarvestOrchard_Slot9_Impl(HarvestOrchard* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    int* collectArray = nullptr;
    int* currentArray = nullptr;
    uint32_t collectCount = 0;
    uint32_t collectCapacity = 0;

    int* root = (int*)(selfAddr + 0x48);
    int* node = *(int**)root;

    if (node == nullptr) {
        goto cleanup;
    }

    // Find leftmost node
    {
        int* cur = node;
        while (cur != nullptr) {
            node = cur;
            cur = *(int**)(cur + 6);
        }
    }

    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2);

        uint8_t valid = s_fn_ValidateTask(selfAddr, (uintptr_t)param2, (void*)&ibpData, nullptr);

        if (valid == 0) {
            uint32_t newCount = collectCount + 1;
            if (newCount != collectCount) {
                if ((int)collectCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (collectCount != 0) {
                        newCap = newCount * 3;
                    }
                    collectCapacity = newCap;
                    int* newArray = (int*)s_fn_MemAlloc((int64_t)newCap << 2, 4);
                    currentArray = newArray;

                    uint32_t copyStart = 0;
                    if (collectArray == newArray) {
                        copyStart = collectCount;
                    }
                    for (uint32_t j = copyStart; j < newCount; j++) {
                        if (j < collectCount) {
                            newArray[j] = collectArray[j];
                        } else {
                            newArray[j] = *node;
                        }
                    }
                    if (collectArray != newArray) {
                        fn_MemFreeStatic(collectArray);
                    }
                    collectArray = newArray;
                } else {
                    currentArray = collectArray;
                    collectArray[collectCount] = *node;
                }
                collectCount = newCount;
            }

            char* validityArray = (char*)selfQ[0xb];
            validityArray[*node] = 1;
        }

        // Move to next in-order node
        int* right = *(int**)(node + 8);
        if (right != nullptr) {
            int* cur = right;
            while (cur != nullptr) {
                node = cur;
                cur = *(int**)(cur + 6);
            }
        } else {
            int* parent = *(int**)(node + 4);
            if (parent != nullptr) {
                if (*(int**)(parent + 6) != node) {
                    while (parent != nullptr && *(int**)(parent + 8) == node) {
                        node = parent;
                        parent = *(int**)(parent + 4);
                    }
                }
                node = parent;
            } else {
                node = nullptr;
            }
        }
    }

cleanup:
    if ((int)collectCount > 0) {
        for (uint32_t k = 0; k < collectCount; k++) {
            int* bstNode = *(int**)root;
            while (bstNode != nullptr) {
                if (collectArray[k] < *bstNode) {
                    bstNode = *(int**)(bstNode + 6);
                } else if (collectArray[k] > *bstNode) {
                    bstNode = *(int**)(bstNode + 8);
                } else {
                    s_fn_BST_Remove(root, bstNode);
                    break;
                }
            }
        }
    }

    fn_MemFreeStatic(currentArray ? currentArray : collectArray);
    return collectCount == 0 ? 1 : 0;
}


// ============================================================
// HarvestOrchard::Slot10 -- GetWorkEntity
// ============================================================
void* HarvestOrchard_Slot10_Impl(HarvestOrchard* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t posCompIBP[2] = {0};
    s_fn_FindPosComp(entity, posCompIBP);

    fn_InternalBasePtr_CheckPtr(posCompIBP);
    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCompIBP + 0x48);

    return outPtr;
}


// ============================================================
// HarvestOrchard::Serialize
// ============================================================
void HarvestOrchard_Serialize_Impl(HarvestOrchard* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    s_fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);
    s_fn_Serializer_Dtor(serializerBuf);

    uint8_t serializerBuf2[16];
    s_fn_Serializer_Ctor(serializerBuf2, stream, 0);

    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf2);

    int cellCount = *(int*)(selfAddr + 0x60);
    int cellCountBuf[2] = { cellCount, 0 };
    s_fn_Serializer_SerializeInt(serializerBuf2, cellCountBuf, nullptr);

    if (*(int*)(*(uintptr_t*)serializerBuf2 + 8) == 0) {
        s_fn_ResizeByteArray((void*)(selfAddr + 0x58), cellCountBuf[0]);
    }

    s_fn_Serializer_SerializeBoolArray(serializerBuf2, cellCountBuf[0],
                                        *(void**)(selfAddr + 0x58), nullptr);

    s_fn_Serializer_Dtor(serializerBuf2);
}


// ============================================================
// HarvestOrchard::ScalarDtor body
// ============================================================
void* HarvestOrchard_ScalarDtor_Impl(HarvestOrchard* self, uint32_t flags) {
    uintptr_t selfAddr = (uintptr_t)self;

    *(void**)selfAddr = s_vtable_HarvestOrchard;

    fn_MemFreeStatic(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        fn_MemFreeStatic(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    *(void**)selfAddr = s_vtable_AutoPickupWork;
    *(void**)selfAddr = s_vtable_Work;
    s_fn_ObjectInternal_Dtor((void*)selfAddr);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}


// ============================================================
// Standalone factory + vtable — AutoPickupWork family (factory cascade).
// Vtable @ 0x140161520 (13 slots), size 0x68, parent "AutoPickupWork".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// Slot map (this file's actual function names):
//   slot0  ScalarDtor : HarvestOrchard_ScalarDtor_Impl
//   slot3  Serialize  : HarvestOrchard_Serialize_Impl
//   slot4  Execute    : HarvestOrchard_Slot4_Impl
//   slot6  OnComplete : HarvestOrchard_Slot6_Impl
//   slot7  OnCancel   : HarvestOrchard_Slot7_Impl
//   slot8  Cleanup    : HarvestOrchard_Slot8_Impl
//   slot9  Validate   : HarvestOrchard_Slot9_Impl
//   slot10 GetTarget  : HarvestOrchard_Slot10_Impl
//   slot11 GetPriority: FLAG — NO HarvestOrchard_Slot11_Impl in this file
//   Ctor       : FLAG — NO HarvestOrchard_Ctor_Impl in this file -> factory null
//   ResolveFuncs: HarvestOrchard_ResolveFuncs()
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HarvestOrchard_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("HarvestOrchard");
    return c;
}

static void* g_HarvestOrchard_Vtable[13] = {
    (void*)&HarvestOrchard_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                      // +0x08 slot1 (null)
    (void*)&HarvestOrchard_GetTypeId,        // +0x10 GetTypeId
    (void*)&HarvestOrchard_Serialize_Impl,   // +0x18 Serialize
    (void*)&HarvestOrchard_Slot4_Impl,       // +0x20 Execute
    (void*)&Work_Slot5,                      // +0x28 slot5 (returns 0)
    (void*)&HarvestOrchard_Slot6_Impl,       // +0x30 OnComplete
    (void*)&HarvestOrchard_Slot7_Impl,       // +0x38 OnCancel
    (void*)&HarvestOrchard_Slot8_Impl,       // +0x40 Cleanup
    (void*)&HarvestOrchard_Slot9_Impl,       // +0x48 Validate
    (void*)&HarvestOrchard_Slot10_Impl,      // +0x50 GetTarget
    (void*)&Work_SlotRet0,            // +0x58 GetPriority (NO HarvestOrchard_Slot11_Impl)
    (void*)&Work_Slot12,                     // +0x60 slot12 (void nop)
};

extern "C" void RegisterHarvestOrchardVtable() {
    RegisterExeAddr(0x140161520, (uintptr_t)g_HarvestOrchard_Vtable);
}
extern "C" void* HarvestOrchard_CreateInPlace(void* mem) {
    HarvestOrchard_ResolveFuncs();
    void* r = Work_GenericCtor(mem, 0x68, g_HarvestOrchard_Vtable);
    if (r) OrchardDiag_NoteHarvestConstructed();
    return r;
}
extern "C" void* HarvestOrchard_Factory(void) {
    HarvestOrchard_ResolveFuncs();
    void* p = Work_Alloc(0x68);
    void* r = p ? Work_GenericCtor(p, 0x68, g_HarvestOrchard_Vtable) : nullptr;
    if (r) OrchardDiag_NoteHarvestConstructed();
    return r;
}
REGISTER_TYPE(HarvestOrchard, "AutoPickupWork", &HarvestOrchard_Factory, &HarvestOrchard_CreateInPlace);
