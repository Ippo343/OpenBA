// code/Game/Work/harvestarea.cpp
// Clean C++ reimplementation of HarvestArea Work class.
// Source: Game/Work/HarvestArea.cpp (from profile build debug strings)
//
// HarvestArea (0x68 bytes) -- Crop/field harvest work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestArea
// Vtable: 0x1401614a0
//
// Struct layout:
//   +0x00: vtable
//   +0x08: Work/AutoPickupWork base (0x40 bytes of fields)
//   +0x48: BST root (tree of assigned work items, int-keyed)
//   +0x50: BST count (int)
//   +0x58: validity array ptr (byte array, one per field cell)
//   +0x60: field cell count (int)
//
// Methods implemented:
//   Slot4 (0x1400ECCD0) -- main crop harvest assignment logic
//   Slot6 (0x1400ECC20) -- tree insert with paired slot removal
//   Slot7 (0x1400ECB60) -- mark cell valid, remove paired entry
//   Slot8 (0x1400ED270) -- CancelAll
//   Slot9 (0x1400EC930) -- RemoveInvalid (hook disabled, bug class 2)
//   Slot10 (0x1400ED2F0) -- GetWorkEntity
//   Serialize (0x1400ED350) -- serialization
//   ScalarDtor body (0x1400ED410) -- destructor
//

#include "Work/harvestarea_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Local function pointers (resolved once by HarvestArea_ResolveFuncs)
// ============================================================

// AutoPickupWork::CheckAssignments (base class work check)
typedef int (*HA_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
static HA_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;

// FindComponent helpers
typedef void* (*HA_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_CropDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);

static HA_FindComp_MatDesc_t s_fn_FindComp_MatDesc = nullptr;         // 0x140003050
static HA_FindComp_MeshInstance_t s_fn_FindComp_MeshInstance = nullptr; // 0x140002f90
static HA_FindComp_CropDesc_t s_fn_FindComp_CropDesc = nullptr;       // 0x140005380
static HA_FindComp_NatRes_t s_fn_FindComp_NatRes = nullptr;            // 0x1400052c0

// MeshInstance::SetVisible(mesh, group, visible)
typedef void (*HA_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
static HA_MeshInstance_SetVisible_t s_fn_MeshInstance_SetVisible = nullptr; // 0x14012c3f0

// CalcMaxWorkers(matDesc) -> int
typedef int (*HA_CalcMaxWorkers_t)(uintptr_t matDesc);
static HA_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr; // 0x14012e5c0

// Map functions
typedef int (*HA_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool (*HA_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
static HA_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr; // 0x14009ccf0
static HA_Map_CanPathTo_t s_fn_Map_CanPathTo = nullptr;                     // 0x1400a00f0

// GetTerrainPos(mapGrid, outPos, gridPos, param4, outNormal)
typedef float* (*HA_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
static HA_GetTerrainPos_t s_fn_GetTerrainPos = nullptr; // 0x14009f3e0

// Math::Vector3::Vector3 -- IAT dispatch
typedef float* (*HA_Vector3_Ctor_t)(float* self, float x, float y, float z);
static HA_Vector3_Ctor_t s_fn_Vector3_Ctor = nullptr;

// ShowTool_Action_Ctor4 -- creates harvest action chain for crop areas
// Signature: (citizenIBP, priority, harvestMode, targetIBP, position, packedCellInfo, ???)
typedef void (*HA_ShowTool_Action_Ctor4_t)(void* citizenIBP, int priority, int harvestMode,
                                           void* targetIBP, float* position,
                                           uint32_t packedCellInfo, uint32_t param7);
static HA_ShowTool_Action_Ctor4_t s_fn_ShowTool_Action_Ctor4 = nullptr; // ShowTool_Action_Ctor4

// BST insert: TradeDescription_impl_9a40(treeRoot, outResult, keyData)
typedef void* (*HA_BST_Insert_t)(void* treeRoot, void* outResult, uint32_t* keyData);
static HA_BST_Insert_t s_fn_BST_Insert = nullptr; // TradeDescription_impl_9a40

// BST remove: TradeDescription_impl_ca60(treeRoot, node)
typedef void (*HA_BST_Remove_t)(void* treeRoot, void* node);
static HA_BST_Remove_t s_fn_BST_Remove = nullptr; // TradeDescription_impl_ca60

// BST serialize: TradeDescription_impl_9de0(treeRoot, serializer)
typedef void (*HA_BST_Serialize_t)(void* treeRoot, void* serializer);
static HA_BST_Serialize_t s_fn_BST_Serialize = nullptr; // TradeDescription_impl_9de0

// FUN_1400f7100 -- Work validate task helper
typedef uint8_t (*HA_ValidateTask_t)(uintptr_t self, uintptr_t param2, void* ibp, void** outTask);
static HA_ValidateTask_t s_fn_ValidateTask = nullptr;

// FUN_1400c5400 -- cancel work/task cleanup
typedef void (*HA_TaskCancel_t)(void* taskList);
static HA_TaskCancel_t s_fn_TaskCancel = nullptr;

// FUN_140002e10 -- FindPositionComponent on entity
typedef void* (*HA_FindPosComp_t)(uintptr_t entity, void* outIBP);
static HA_FindPosComp_t s_fn_FindPosComp = nullptr;

// Work::Serialize base
typedef void (*HA_Work_Serialize_t)(void* self, void* stream);
static HA_Work_Serialize_t s_fn_Work_Serialize = nullptr;

// Serializer ctor/dtor via IAT
typedef void (*HA_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void (*HA_Serializer_Dtor_t)(void* self);
static HA_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
static HA_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Serializer::Serialize(ser, intBuf, name) for int fields
typedef void (*HA_Serializer_SerializeInt_t)(void* ser, int* buf, const char* name);
static HA_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// Serializer::Serialize(ser, count, boolBuf, name) for bool array
typedef void (*HA_Serializer_SerializeBoolArray_t)(void* ser, int count, void* buf, const char* name);
static HA_Serializer_SerializeBoolArray_t s_fn_Serializer_SerializeBoolArray = nullptr;

// Resize byte array (FUN_14000a590)
typedef void (*HA_ResizeByteArray_t)(void* arrayField, int newCount);
static HA_ResizeByteArray_t s_fn_ResizeByteArray = nullptr;

// BST recursive free (FUN_14000e320)
typedef void (*HA_BST_RecursiveFree_t)(void* treeRoot, void* node);
static HA_BST_RecursiveFree_t s_fn_BST_RecursiveFree = nullptr;

// Object::Internal::~Internal -- tail call of dtor chain
typedef void (*HA_ObjectInternal_Dtor_t)(void* self);
static HA_ObjectInternal_Dtor_t s_fn_ObjectInternal_Dtor = nullptr;

// MemAlloc / MemFree
typedef void* (*HA_MemAlloc_t)(int64_t size, int align);
static HA_MemAlloc_t s_fn_MemAlloc = nullptr;

// Type IDs
static uint32_t* s_typeId_CropDesc = nullptr;    // DAT_1401c7548 (FindComp_CropDesc type)
// (s_typeId_NatResComp not needed -- NatRes lookup done via s_fn_FindComp_NatRes)

// Float constants
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain
static void** s_vtable_HarvestArea = nullptr;
static void** s_vtable_AutoPickupWork = nullptr;
static void** s_vtable_Work = nullptr;

static bool s_ha_funcs_resolved = false;

// ============================================================
// Resolution
// ============================================================
bool HarvestArea_ResolveFuncs() {
    if (s_ha_funcs_resolved) return true;

    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<HA_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<HA_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<HA_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_FindComp_CropDesc = reinterpret_cast<HA_FindComp_CropDesc_t>(
        ResolveAddr(Module::Exe, 0x140005380));
    s_fn_FindComp_NatRes = reinterpret_cast<HA_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<HA_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<HA_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HA_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HA_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<HA_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_ShowTool_Action_Ctor4 = reinterpret_cast<HA_ShowTool_Action_Ctor4_t>(
        ResolveAddr(Module::Exe, 0x1400c1530));  // was 0x1400c1720 (mid-function!)
    s_fn_BST_Insert = reinterpret_cast<HA_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140029a40));
    s_fn_BST_Remove = reinterpret_cast<HA_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));  // was 0x140029a60 (wrong function!)
    s_fn_BST_Serialize = reinterpret_cast<HA_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140029de0));
    s_fn_ValidateTask = reinterpret_cast<HA_ValidateTask_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TaskCancel = reinterpret_cast<HA_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindPosComp = reinterpret_cast<HA_FindPosComp_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_Work_Serialize = reinterpret_cast<HA_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ResizeByteArray = reinterpret_cast<HA_ResizeByteArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_BST_RecursiveFree = reinterpret_cast<HA_BST_RecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x14000e320));
    // Object::Internal::~Internal via dispatch slot (JMP [0x140152770] in dtor)
    uintptr_t iatInternalDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatInternalDtor)
        s_fn_ObjectInternal_Dtor = *reinterpret_cast<HA_ObjectInternal_Dtor_t*>(iatInternalDtor);

    // Dispatch slot IATs
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<HA_Vector3_Ctor_t*>(iatVec3Ctor);

    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<HA_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<HA_Serializer_Dtor_t*>(iatSerDtor);
    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt)
        s_fn_Serializer_SerializeInt = *reinterpret_cast<HA_Serializer_SerializeInt_t*>(iatSerInt);
    s_fn_Serializer_SerializeBoolArray = reinterpret_cast<HA_Serializer_SerializeBoolArray_t>(
        s_fn_Serializer_SerializeInt); // same fn, different usage (count, buf, name)

    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140152780);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<HA_MemAlloc_t*>(iatMemAlloc);

    // Type IDs
    s_typeId_CropDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7548));

    // Float constants
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable addresses
    s_vtable_HarvestArea = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401614a0));
    // ⚠ Were 0x140161380 / 0x140161300 — RTTI type descriptors ("ForesterWork\0" /
    // "FishingWork\0"), not vtables; same adjacency error harvestorchard.cpp carried.
    // The real base vtables are AutoPickupWorkAddr::Vtable / WorkAddr::Vtable.
    s_vtable_AutoPickupWork = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140160fa0));
    s_vtable_Work = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_FindComp_CropDesc &&
              s_fn_FindComp_NatRes && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_GetTerrainPos &&
              s_fn_Vector3_Ctor && s_fn_ShowTool_Action_Ctor4 &&
              s_fn_BST_Insert && s_fn_BST_Remove && s_fn_BST_Serialize &&
              s_fn_ValidateTask && s_fn_TaskCancel && s_fn_FindPosComp &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_ResizeByteArray && s_fn_BST_RecursiveFree &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_typeId_CropDesc && s_fHalfCellOffset &&
              s_vtable_HarvestArea && s_vtable_AutoPickupWork && s_vtable_Work;

    s_ha_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[HarvestArea] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
        if (!s_fn_FindComp_CropDesc) printf("  MISSING: FindComp_CropDesc\n");
        if (!s_fn_FindComp_NatRes) printf("  MISSING: FindComp_NatRes\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_Vector3_Ctor) printf("  MISSING: Vector3_Ctor\n");
        if (!s_fn_ShowTool_Action_Ctor4) printf("  MISSING: ShowTool_Action_Ctor4\n");
        if (!s_fn_BST_Insert) printf("  MISSING: BST_Insert\n");
        if (!s_fn_BST_Remove) printf("  MISSING: BST_Remove\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_ValidateTask) printf("  MISSING: ValidateTask\n");
        if (!s_fn_TaskCancel) printf("  MISSING: TaskCancel\n");
        if (!s_fn_FindPosComp) printf("  MISSING: FindPosComp\n");
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_ResizeByteArray) printf("  MISSING: ResizeByteArray\n");
        if (!s_fn_BST_RecursiveFree) printf("  MISSING: BST_RecursiveFree\n");
    }
#endif

    return ok;
}


// ============================================================
// Helper: BST search for int key
// Returns node pointer if found, nullptr otherwise.
// BST node layout: [0]=int key, [+8]=IBP data, [+0x10]=parent, [+0x18]=left, [+0x20]=right
// ============================================================
static void* BST_Find(void* root, int key) {
    int* node = (int*)root;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);  // left child at +0x18
        } else if (key > *node) {
            node = *(int**)(node + 8);  // right child at +0x20
        } else {
            return node;  // found
        }
    }
    return nullptr;
}


// ============================================================
// HarvestArea::Slot4 -- Main crop harvest assignment logic
// ============================================================
//
// Algorithm:
// 1. Check auto-pickup assignments
// 2. Find MaterialDescription, CropDescription on workplace entity
// 3. Check capacity
// 4. Iterate field cells (0..cellCount), find unharvested crops
// 5. For each valid cell: check pathing, check NaturalResource status
// 6. If pairable cell available, harvest two at once; else harvest one
// 7. Create harvest action, assign work, register with citizen AI
//
// Harvest-assignment census counters. State 11 posts this work and its Execute runs
// every frame; "runs but produces nothing" has EIGHT possible causes here and they need
// different fixes, so each is counted separately. Two of them (atCapacity, workerCap)
// return before the cell loop is even reached.
static long long g_ha_calls = 0, g_ha_auto = 0, g_ha_cap = 0, g_ha_wcap = 0,
                 g_ha_nomark = 0, g_ha_inbst = 0, g_ha_noent = 0, g_ha_nopass = 0,
                 g_ha_nopath = 0, g_ha_ok = 0;

int HarvestArea_Slot4_Impl(HarvestArea* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    // Harvest-assignment census. State 11 posts this work; whether a citizen ever
    // ACCEPTS a harvest cell is the last unmeasured link before a deposit, and an
    // Execute that finds nothing looks exactly like an Execute that is never called.
    {
        static int s_t = -1;
        if (s_t < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                       s_t = (e && e[0] && e[0] != '0') ? 1 : 0; }
        ++g_ha_calls;
        if (s_t && ((g_ha_calls % 20) == 0)) {
            std::fprintf(stderr, "[HarvestArea] self=%p work-entity=%p | calls=%lld | autopickup=%lld atCapacity=%lld "
                         "workerCap=%lld noMark=%lld inBst=%lld noEntity=%lld noPass=%lld "
                         "noPath=%lld ACCEPTED=%lld\n",
                         (void*)self, (void*)*(uintptr_t*)((uintptr_t)self + 0x10),
                         g_ha_calls, g_ha_auto, g_ha_cap, g_ha_wcap, g_ha_nomark,
                         g_ha_inbst, g_ha_noent, g_ha_nopass, g_ha_nopath, g_ha_ok);
            std::fflush(stderr);
        }
    }
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // Step 1: Check auto-pickup
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &citizenRef);
    if (checkResult == 2) {
        ++g_ha_auto;
        return 2;
    }

    // Step 2: Find components
    uintptr_t* workEntityPtr = (uintptr_t*)(selfAddr + 0x10);
    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*workEntityPtr, &matDescIBP);

    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t cropDescIBP = 0;
    s_fn_FindComp_CropDesc(*workEntityPtr, &cropDescIBP);

    // Check CropDescription -> get description object
    fn_InternalBasePtr_CheckPtr(&cropDescIBP);
    void* descObj = *(void**)(cropDescIBP + 0x18);
    bool isDerived = fn_IsDerivedFrom(descObj, *s_typeId_CropDesc);
    void* cropDesc = isDerived ? descObj : nullptr;

    // Material index for resource tracking
    int materialIdx = *(int*)((uintptr_t)cropDesc + 0x20);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // THE DEPOSIT METER. resEntry+0 is the town's current stock of this crop's material
    // and resEntry+8 its limit (that is exactly what the atCapacity test above compares).
    // Harvest is confirmed running -- 61 crops through the harvestFlag=1 arm -- so the
    // only question left is whether the raw material reaches STORAGE. Printing the stock
    // here answers it directly instead of by inference: a rising number IS the deposit.
    {
        static int s_dt = -1;
        if (s_dt < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                        s_dt = (e && e[0] && e[0] != '0') ? 1 : 0; }
        static int s_last = -1;
        const int cur = *(int*)(resEntry);
        if (s_dt && cur != s_last) {
            s_last = cur;
            std::fprintf(stderr, "[CropStock] material %d: town stock = %d (limit %d)\n",
                         materialIdx, cur, *(int*)(resEntry + 8));
            std::fflush(stderr);
        }
    }

    // Set mesh visibility (group 2) based on capacity
    fn_InternalBasePtr_CheckPtr(workEntityPtr);
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*workEntityPtr, meshIBP);
    fn_InternalBasePtr_CheckPtr(meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        ++g_ha_cap;
        return 1;
    }

    // Check tree count vs max workers
    int treeCount = (int)selfQ[10]; // _field50
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
        ++g_ha_wcap;
        return 1;
    }

    // Step 3: Setup iteration
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    // Get citizen position
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

    int fieldCellCount = (int)selfQ[0xc]; // self->_field60 as int

    if (fieldCellCount > 0) {
        int cellIdx2 = 1; // paired cell index starts at 1
        for (int i = 0; i < fieldCellCount; i++) {
            // Check if this cell's validity flag is set
            char* validityArray = (char*)selfQ[0xb]; // self->_field58
            if (validityArray[i] == 0) {
                ++g_ha_nomark;
                goto next_cell;
            }

            // Check if cell index is already in the BST (already assigned)
            {
                int* bstRoot = (int*)selfQ[9]; // self->_field48
                void* found = BST_Find(bstRoot, i);
                if (found != nullptr) {
                    ++g_ha_inbst;
                    goto next_cell;
                }
            }

            // Check if paired cell is also available
            {
                bool pairedAvailable = false;
                if (cellIdx2 < fieldCellCount && validityArray[cellIdx2] != 0) {
                    int* bstRoot = (int*)selfQ[9];
                    void* found2 = BST_Find(bstRoot, cellIdx2);
                    if (found2 == nullptr) {
                        pairedAvailable = true;
                    }
                }

                // Look up entity at this cell in the map grid
                uintptr_t entityAtCell1 = 0;
                uintptr_t entityAtCell2 = 0;

                fn_InternalBasePtr_CheckPtr(&cropDescIBP);
                uintptr_t cropDescVal = cropDescIBP;
                uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20); // +0x80 byte offset
                int cellGridIdx = *(int*)((uintptr_t)i * 0x20 + *(uintptr_t*)(cropDescVal + 0x58));
                entityAtCell1 = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx * 0x30 + 8);

                if (entityAtCell1 == 0) {
                    // No entity at cell -- mark as unavailable
                    ++g_ha_noent;
                    // Print the CELL INDEX harvest is reading. CreateNaturalResource
                    // verifies tile+8 is SET at its own cellIndex immediately after
                    // registering, so if both sides are healthy they must be reading the
                    // SAME cell. Compare these against [CNR-tile]'s "last cell=".
                    {
                        static int s_ct = -1;
                        if (s_ct < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                                        s_ct = (e && e[0] && e[0] != '0') ? 1 : 0; }
                        static int s_shown = 0;
                        if (s_ct && s_shown < 8) {
                            ++s_shown;
                            // Recompute the SAME tile address the independent way the
                            // crop field's own trace uses (gw+0xf8 -> +0x80), and count
                            // how many of the field's cells hold an entity right now. If
                            // this says the tile is non-null, or the field is full, then
                            // the read above is looking at the wrong place rather than
                            // the crops being gone.
                            const uintptr_t gwc = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
                            const uintptr_t mpc = gwc ? *(uintptr_t*)(gwc + 0xf8) : 0;
                            const uintptr_t gdc = mpc ? *(uintptr_t*)(mpc + 0x80) : 0;
                            const uintptr_t viaB = gdc ? *(uintptr_t*)(gdc + 8 + (uintptr_t)cellGridIdx * 0x30) : 0;
                            int liveC = 0;
                            if (gdc) for (int k = 0; k < fieldCellCount; ++k) {
                                const int ck = *(int*)((uintptr_t)k * 0x20 + *(uintptr_t*)(cropDescIBP + 0x58));
                                if (*(uintptr_t*)(gdc + 8 + (uintptr_t)ck * 0x30)) ++liveC;
                            }
                            std::fprintf(stderr, "[HarvestCell] slot=%d cell=%d viaHarvest=%p "
                                         "viaGameWorld=%p gridSame=%d fieldLive=%d/%d\n",
                                         i, cellGridIdx, (void*)entityAtCell1, (void*)viaB,
                                         (int)(gridData == gdc), liveC, fieldCellCount);
                            std::fflush(stderr);
                        }
                    }
                    validityArray[i] = 0;
                } else {
                    if (pairedAvailable) {
                        fn_InternalBasePtr_CheckPtr(&cropDescIBP);
                        uintptr_t cropDescVal2 = cropDescIBP;
                        int cellGridIdx2 = *(int*)((uintptr_t)cellIdx2 * 0x20 + *(uintptr_t*)(cropDescVal2 + 0x58));
                        entityAtCell2 = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx2 * 0x30 + 8);
                        if (entityAtCell2 == 0) {
                            pairedAvailable = false;
                        }
                    }

                    // Check pathfinding from cell to citizen
                    fn_InternalBasePtr_CheckPtr(&cropDescIBP);
                    int srcCellIdx = *(int*)(*(uintptr_t*)(cropDescIBP + 0x58) + (uintptr_t)i * 0x20);
                    int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);

                    if (nearestCell >= 0) {
                        uint64_t canPath = (uint64_t)s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
                        if ((char)canPath != 0) {
                            // Check NaturalResourceComponent on entity
                            // natResIBPArray must be contiguous: ShowTool_Action_Ctor4
                            // iterates param_4 as an array of harvestMode entries
                            uintptr_t natResIBPArray[2] = {0, 0};
                            uintptr_t& natResIBP1 = natResIBPArray[0];
                            uintptr_t& natResIBP2 = natResIBPArray[1];

                            fn_InternalBasePtr_CheckPtr(&entityAtCell1);
                            uintptr_t natResCompIBP[2] = {0};
                            s_fn_FindComp_NatRes(entityAtCell1, natResCompIBP);
                            natResIBP1 = *(uintptr_t*)natResCompIBP;

                            if (pairedAvailable) {
                                fn_InternalBasePtr_CheckPtr(&entityAtCell2);
                                uintptr_t natResCompIBP2[2] = {0};
                                s_fn_FindComp_NatRes(entityAtCell2, natResCompIBP2);
                                natResIBP2 = *(uintptr_t*)natResCompIBP2;
                            }

                            // Check flag bit 20 on NatRes (flagged for removal)
                            fn_InternalBasePtr_CheckPtr(&natResIBP1);
                            if (natResIBP1 == 0) {
                                // NatRes component gone — skip this cell
                                validityArray[i] = 0;
                                goto next_cell;
                            }
                            if ((*(uint32_t*)(natResIBP1 + 0x20) >> 20) & 1) {
                                goto handle_removal;
                            }

                            if (natResIBP2 != 0) {
                                fn_InternalBasePtr_CheckPtr(&natResIBP2);
                                if (natResIBP2 != 0 && (*(uint32_t*)(natResIBP2 + 0x20) >> 20) & 1) {
                                    pairedAvailable = false;
                                }
                            }

                            if (!pairedAvailable) {
                                // Single cell harvest -- need cell count for packed info
                                cellIdx2 = fieldCellCount;
                            }

                        handle_removal:
                            if (!((*(uint32_t*)(natResIBP1 + 0x20) >> 20) & 1)) {
                                // Valid cell -- create harvest action
                                float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                                float targetPos[3];
                                s_fn_Vector3_Ctor(targetPos,
                                    (float)(nearestCell % *mapGrid) - halfOffset,
                                    (float)(nearestCell / *mapGrid) - halfOffset,
                                    0.0f);

                                float pathPos[4]; // extra padding for GetTerrainPos
                                s_fn_GetTerrainPos(mapGrid, pathPos, targetPos, 0, nullptr);

                                // Create the harvest action
                                void* pCitizenIBP = citizenIBP;
                                uintptr_t citizenVal = *(uintptr_t*)pCitizenIBP;
                                uintptr_t actionBuf[2] = { citizenVal, 0 };

                                int harvestMode = pairedAvailable ? 2 : 1;
                                uint32_t packedCellInfo = ((uint32_t)cellIdx2 << 16) | (uint32_t)i;


                                s_fn_ShowTool_Action_Ctor4(
                                    actionBuf,                    // citizenIBP
                                    (int)selfQ[4],                // priority
                                    harvestMode,                  // 1=single, 2=paired
                                    natResIBPArray,               // target NatRes IBP array (harvestMode entries)
                                    pathPos,                      // position
                                    packedCellInfo,               // packed cell indices
                                    0);                           // param7

                                // Assign work and register with AI
                                int priority = (int)selfQ[4];
                                fn_InternalBasePtr_CheckPtr(citizenIBP);
                                uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

                                uintptr_t vtable = *(uintptr_t*)self;
                                typedef uintptr_t* (*GetWorkEntity_t)(HarvestArea*, void*);
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

                                // Insert cell index into BST
                                uintptr_t insertResult[2] = {0};
                                uint32_t keyBuf[4] = { (uint32_t)i, 0, 0, 0 };
                                uintptr_t citizenIBPBuf = *(uintptr_t*)citizenIBP;
                                keyBuf[2] = (uint32_t)citizenIBPBuf;
                                keyBuf[3] = (uint32_t)(citizenIBPBuf >> 32);
                                s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, keyBuf);

                                // Mark cell as used
                                validityArray[i] = 0;

                                // If paired, also insert and mark
                                if (pairedAvailable) {
                                    uintptr_t insertResult2[2] = {0};
                                    uint32_t keyBuf2[4] = { (uint32_t)cellIdx2, 0, 0, 0 };
                                    uintptr_t citizenIBPBuf2 = *(uintptr_t*)citizenIBP;
                                    keyBuf2[2] = (uint32_t)citizenIBPBuf2;
                                    keyBuf2[3] = (uint32_t)(citizenIBPBuf2 >> 32);
                                    s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult2, keyBuf2);
                                    validityArray[cellIdx2] = 0;
                                }

                                return 2;
                            }
                        }
                    }

                    // Restore citizenGridCell for next iteration
                    citizenGridCell = (uint32_t)(mapSz * citizenCellY + citizenCellX);
                }
            }

        next_cell:
            cellIdx2 = i + 2; // paired cell is always i+1 in iteration
            // Note: cellIdx2 tracks as fVar13 in decompilation (i+1 each iteration)
        }
    }

    return 1;
}


// ============================================================
// HarvestArea::Slot6 -- Insert/remove work entry
// ============================================================
// Called when citizen completes/cancels work. Removes entry from BST,
// marks cell validity, and handles paired cell removal.
// param4 is packed: low 16 bits = primary cell index, high 16 bits = paired cell index.
int HarvestArea_Slot6_Impl(HarvestArea* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    uint32_t primaryIdx = param4 & 0xFFFF;
    uint32_t pairedIdx = (param4 >> 16) & 0xFFFF;

    // Search BST for primary cell index and remove if found
    int* node = (int*)selfQ[9]; // BST root at +0x48
    while (node != nullptr) {
        if ((int)primaryIdx < *node) {
            node = *(int**)(node + 6);
        } else if ((int)primaryIdx > *node) {
            node = *(int**)(node + 8);
        } else {
            // Found -- remove from BST
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
            break;
        }
    }

    // Mark primary cell as valid (available for reassignment)
    char* validityArray = (char*)selfQ[0xb];
    validityArray[primaryIdx] = 1;

    // Handle paired cell (if within bounds)
    if ((int)pairedIdx < *(int*)(selfAddr + 0x60)) {
        validityArray[pairedIdx] = 1;

        // Remove paired cell from BST too
        int* node2 = (int*)selfQ[9];
        while (node2 != nullptr) {
            if ((int)pairedIdx < *node2) {
                node2 = *(int**)(node2 + 6);
            } else if ((int)pairedIdx > *node2) {
                node2 = *(int**)(node2 + 8);
            } else {
                s_fn_BST_Remove((void*)(selfAddr + 0x48), node2);
                break;
            }
        }
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot7 -- Tree node lookup with validation
// ============================================================
// Called to validate and clean up work entries. Sets validity flag
// and removes stale paired entry.
// param4 is packed: low 16 = primary cell, high 16 = paired cell.
int HarvestArea_Slot7_Impl(HarvestArea* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    uint32_t primaryIdx = param4 & 0xFFFF;
    uint32_t pairedIdx = (param4 >> 16) & 0xFFFF;

    // Mark primary cell validity
    char* validityArray = (char*)selfQ[0xb];
    validityArray[primaryIdx] = 1;

    // Handle paired cell
    if ((int)pairedIdx < *(int*)(selfAddr + 0x60)) {
        validityArray[pairedIdx] = 1;

        // Remove paired cell from BST
        int* node = (int*)selfQ[9];
        while (node != nullptr) {
            if ((int)pairedIdx < *node) {
                node = *(int**)(node + 6);
            } else if ((int)pairedIdx > *node) {
                node = *(int**)(node + 8);
            } else {
                s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
                return 0;
            }
        }
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot8 -- CancelAll
// ============================================================
// Loops through BST (rightmost traversal), cancels each work item
// via citizen AI task cleanup.
int HarvestArea_Slot8_Impl(HarvestArea* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find rightmost node in BST
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18); // left child (finds leftmost actually)
        }

        // Cancel work via citizen AI
        fn_InternalBasePtr_CheckPtr((void*)(rightmost + 8));
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot9 -- RemoveInvalid
// ============================================================
// Hook DISABLED (bug class 2: MOV RAX,RSP in prologue).
// Implemented for standalone exe.
//
// Iterates BST in-order, validates each entry via FUN_1400f7100,
// collects invalid ones, then removes them from BST.
int HarvestArea_Slot9_Impl(HarvestArea* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    int* collectArray = nullptr;
    int* currentArray = nullptr;
    uint32_t collectCount = 0;
    uint32_t collectCapacity = 0;

    // In-order traversal of BST
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
            cur = *(int**)(cur + 6); // left child
        }
    }

    // Traverse in-order
    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2); // IBP at +0x8

        uint8_t valid = s_fn_ValidateTask(selfAddr, (uintptr_t)param2, (void*)&ibpData, nullptr);

        if (valid == 0) {
            // Invalid -- add to collect list
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

            // Mark validity flag
            char* validityArray = (char*)selfQ[0xb];
            validityArray[*node] = 1;
        }

        // Move to next in-order node
        int* right = *(int**)(node + 8); // right child
        if (right != nullptr) {
            // Go to leftmost of right subtree
            int* cur = right;
            while (cur != nullptr) {
                node = cur;
                cur = *(int**)(cur + 6);
            }
        } else {
            // Go up until we find a node we're the left child of
            int* parent = *(int**)(node + 4); // parent
            if (parent != nullptr) {
                if (*(int**)(parent + 6) != node) {
                    // Not left child -- go up
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
    // Remove collected invalid nodes from BST
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
// HarvestArea::Slot10 -- GetWorkEntity
// ============================================================
// Returns entity pointer from workplace entity ref.
// Uses hidden return ptr ABI (RCX=self, RDX=outPtr).
void* HarvestArea_Slot10_Impl(HarvestArea* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on self->_field10 (work entity IBP)
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // Find position component on entity
    uintptr_t posCompIBP[2] = {0};
    s_fn_FindPosComp(entity, posCompIBP);

    // CheckPtr and read +0x48 (workplace sub-entity)
    fn_InternalBasePtr_CheckPtr(posCompIBP);
    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCompIBP + 0x48);

    return outPtr;
}


// ============================================================
// HarvestArea::Serialize
// ============================================================
// Work::Serialize + version block + BST serialize + array + int
void HarvestArea_Serialize_Impl(HarvestArea* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Serialize Work base class
    s_fn_Work_Serialize(self, stream);

    // Create Serializer with version=0
    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Immediately destroy (version block with no extra data)
    s_fn_Serializer_Dtor(serializerBuf);

    // Create new Serializer (version=0) for our data
    uint8_t serializerBuf2[16];
    s_fn_Serializer_Ctor(serializerBuf2, stream, 0);

    // Serialize BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf2);

    // Serialize field cell count at +0x60
    int cellCount = *(int*)(selfAddr + 0x60);
    int cellCountBuf[2] = { cellCount, 0 };
    s_fn_Serializer_SerializeInt(serializerBuf2, cellCountBuf, nullptr);

    // If reading (serializer mode == 0), resize validity array
    if (*(int*)(*(uintptr_t*)serializerBuf2 + 8) == 0) {
        s_fn_ResizeByteArray((void*)(selfAddr + 0x58), cellCountBuf[0]);
    }

    // Serialize validity array (byte array)
    s_fn_Serializer_SerializeBoolArray(serializerBuf2, cellCountBuf[0],
                                        *(void**)(selfAddr + 0x58), nullptr);

    // Destroy serializer
    s_fn_Serializer_Dtor(serializerBuf2);
}


// ============================================================
// HarvestArea::ScalarDtor body
// ============================================================
// Sets vtable chain down, frees validity array, frees BST,
// then calls Work/Internal dtor. If flags & 1, frees self.
void* HarvestArea_ScalarDtor_Impl(HarvestArea* self, uint32_t flags) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set HarvestArea vtable
    *(void**)selfAddr = s_vtable_HarvestArea;

    // Free validity array at +0x58
    fn_MemFreeStatic(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    // Free BST at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        fn_MemFreeStatic(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set AutoPickupWork vtable, then Work vtable, then call Internal dtor
    *(void**)selfAddr = s_vtable_AutoPickupWork;
    *(void**)selfAddr = s_vtable_Work;
    s_fn_ObjectInternal_Dtor((void*)selfAddr);

    // If flags & 1, free the object
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x1401614a0 (13 slots), size 0x68, parent "AutoPickupWork".
// Shared slot helpers: work_base_standalone.{h,cpp}.
// Slot map ground-truthed from the real exe vtable @ 0x1401614a0:
//   slot4 (Execute)=HarvestArea_Slot4_Impl (0x1400eccd0).
//   slot5 = shared Work_Slot5 (0x1400f7540).
//   slot11 (GetPriority) = 0x140029860 — the SHARED HasWorkers thunk;
//   HarvestArea has NO own GetPriority fn in harvestarea.cpp -> FLAG.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HarvestArea_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("HarvestArea");
    return c;
}

static void* g_HarvestArea_Vtable[13] = {
    (void*)&HarvestArea_ScalarDtor_Impl,  // +0x00 ScalarDtor (0x14002a4d0)
    (void*)&Work_Slot1,                   // +0x08 slot1 (null)
    (void*)&HarvestArea_GetTypeId,        // +0x10 GetTypeId
    (void*)&HarvestArea_Serialize_Impl,   // +0x18 Serialize (0x1400ed350)
    (void*)&HarvestArea_Slot4_Impl,       // +0x20 Execute (0x1400eccd0)
    (void*)&Work_Slot5,                   // +0x28 slot5 (returns 0)
    (void*)&HarvestArea_Slot6_Impl,       // +0x30 OnComplete (0x1400ecc20)
    (void*)&HarvestArea_Slot7_Impl,       // +0x38 OnCancel (0x1400ecb60)
    (void*)&HarvestArea_Slot8_Impl,       // +0x40 Cleanup (0x1400ed270)
    (void*)&HarvestArea_Slot9_Impl,       // +0x48 Validate (0x1400ec930)
    (void*)&HarvestArea_Slot10_Impl,      // +0x50 GetTarget (0x1400ed2f0)
    (void*)&Work_Slot11_HasActiveTasks,  // +0x58 slot11 (exe 0x140029860: self->+0x50 > 0 = "someone is working this"; was pinned to 0 by Work_SlotRet0)
    (void*)&Work_Slot12,                  // +0x60 slot12 (void nop)
};

extern "C" void RegisterHarvestAreaVtable() {
    RegisterExeAddr(0x1401614a0, (uintptr_t)g_HarvestArea_Vtable);
}
extern "C" void* HarvestArea_CreateInPlace(void* mem) {
    HarvestArea_ResolveFuncs();
    return Work_GenericCtor(mem, 0x68, g_HarvestArea_Vtable);
}
extern "C" void* HarvestArea_Factory(void) {
    HarvestArea_ResolveFuncs();
    void* p = Work_Alloc(0x68);
    return p ? Work_GenericCtor(p, 0x68, g_HarvestArea_Vtable) : nullptr;
}
REGISTER_TYPE(HarvestArea, "AutoPickupWork", &HarvestArea_Factory, &HarvestArea_CreateInPlace);
