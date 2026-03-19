// code/Game/Work/gatherrawmaterials.cpp
// Clean C++ reimplementation of GatherRawMaterials Work class.
// Source: Game/Work/GatherRawMaterials.cpp (from profile build debug strings)
//
// GatherRawMaterials -- raw material gathering work. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> GatherRawMaterials
// Ctor: 0x1400ec8a0, Vtable: 0x140161420
//
// Methods implemented:
//   Serialize (vtable slot 3, 0x1400ec560)
//   Execute   (vtable slot 4, 0x1400ebe40)
//   OnComplete(vtable slot 6, 0x1400ebdf0)
//   OnCancel  (vtable slot 7, 0x1400ebdb0)
//   Cleanup   (vtable slot 8, 0x1400ec480)
//   Validate  (vtable slot 9, 0x1400ebb50)
//   GetTarget (vtable slot 10, 0x1400ec510)
//
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/gatherrawmaterials_funcs.h"
#include "BuildActions.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// Local function pointers resolved at init time
// (only those NOT already declared in engine_funcs.h)
// All typedefs prefixed with GRM_ to avoid conflicts.
// ============================================================

// Work::Serialize(this, stream) -- base class serialize at 0x1400f7400
typedef void (*GRM_WorkSerialize_t)(void* self, void* stream);
static GRM_WorkSerialize_t s_fn_Work_Serialize = nullptr;

// IO::Serializer ctor/dtor
typedef void (*GRM_SerializerCtor_t)(void* self, void* stream, int version);
static GRM_SerializerCtor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*GRM_SerializerDtor_t)(void* self);
static GRM_SerializerDtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize for int
typedef void (*GRM_SerializeInt_t)(void* self, void* field, const char* name);
static GRM_SerializeInt_t s_fn_SerializeInt = nullptr;

// IO::Serializer::Serialize for int bulk (count, ptr, name)
typedef void (*GRM_SerializeIntBulk_t)(void* self, int count, void* ptr, const char* name);
static GRM_SerializeIntBulk_t s_fn_SerializeIntBulk = nullptr;

// Resize int array: FUN_1400021d0(arrayPtrField, newCount)
typedef void (*GRM_ResizeIntArray_t)(void* arrayPtrField, int newCount);
static GRM_ResizeIntArray_t s_fn_ResizeIntArray = nullptr;

// CitizenAI tree set serialize: CitizenAI_impl_96b0(treePtr, serializer)
typedef void (*GRM_TreeSerialize_t)(void* treePtr, void* serializer);
static GRM_TreeSerialize_t s_fn_TreeSerialize = nullptr;

// CitizenAI tree remove by key: CitizenAI_impl_5520 @ 0x140025520
typedef void (*GRM_TreeRemoveByKey_t)(void* treePtr, void* entityIBP);
static GRM_TreeRemoveByKey_t s_fn_TreeRemoveByKey = nullptr;

// CitizenAI tree remove node: CitizenAI_impl_a6e0(treePtr, nodePtr)
typedef void (*GRM_TreeRemoveNode_t)(void* treePtr, void* nodePtr);
static GRM_TreeRemoveNode_t s_fn_TreeRemoveNode = nullptr;

// CitizenAI state machine release: FUN_1400c5400(stateMachinePtr)
typedef void (*GRM_StateMachineRelease_t)(void* smPtr);
static GRM_StateMachineRelease_t s_fn_StateMachineRelease = nullptr;

// Work::CanStartWork (FUN_1400f7100)
typedef char (*GRM_CanStartWork_t)(void* self, void* citizenAI, void* taskIBP, void* outTask);
static GRM_CanStartWork_t s_fn_CanStartWork = nullptr;

// MemAlloc/MemFree from System::MemoryAllocator
typedef void* (*GRM_MemAlloc_t)(int64_t size, int alignment);
static GRM_MemAlloc_t s_fn_MemAlloc = nullptr;

typedef void (*GRM_MemFree_t)(void* ptr);
static GRM_MemFree_t s_fn_MemFree = nullptr;

// Entity_FindComponent_Position: FUN_140002ed0
typedef void (*GRM_FindCompPosition_t)(uintptr_t entity, void* retIBP);
static GRM_FindCompPosition_t s_fn_FindComp_Position = nullptr;

// FUN_14000d6c0: FindComponent by type (unknown component)
typedef void (*GRM_FindCompUnk_t)(uintptr_t entity, void* retIBP);
static GRM_FindCompUnk_t s_fn_FindComp_Unk = nullptr;

// FUN_140002e10: FindComponent_WorkplaceExt (returns IBP)
typedef void* (*GRM_FindCompWpExt_t)(uintptr_t entity, void* retIBP);
static GRM_FindCompWpExt_t s_fn_FindComp_WpExt = nullptr;

// FUN_14008d590: FindStorageForResource
typedef void (*GRM_FindStorageForResource_t)(void* worldEntityList, void* outEntity,
    uint32_t srcCell, uint64_t materialType, int amount, char param6,
    float* searchPos, void* srcEntityIBP);
static GRM_FindStorageForResource_t s_fn_FindStorageForResource = nullptr;

// Entity_FindComponent_Storage: FUN_140006270
typedef void* (*GRM_FindCompStorage_t)(uintptr_t entity, void* retIBP);
static GRM_FindCompStorage_t s_fn_FindComp_Storage = nullptr;

// StorageComponent_QueryCount: 0x14012cf10
typedef int (*GRM_StorageQueryCount_t)(void* storageComp);
static GRM_StorageQueryCount_t s_fn_StorageQueryCount = nullptr;

// FUN_140116540: GetRemainingCapacity
typedef int (*GRM_GetRemainingCapacity_t)(uintptr_t citizenEntity);
static GRM_GetRemainingCapacity_t s_fn_GetRemainingCapacity = nullptr;

// CitizenAI_State_LongTermPlan: 0x140115e60
typedef void (*GRM_LongTermPlan_t)(void* citizenEntity);
static GRM_LongTermPlan_t s_fn_LongTermPlan = nullptr;

// FUN_140122be0: CalcBestTargetPos (position lookup from positionComp + material desc)
typedef uint8_t (*GRM_CalcBestTargetPos_t)(uintptr_t posComp, uintptr_t matDescField,
    float* posOut, float* posOut2, int* distOut);
static GRM_CalcBestTargetPos_t s_fn_CalcBestTargetPos = nullptr;

// FUN_140122b00: CalcChildTargetPos (iterate sub-components for position)
typedef uint8_t (*GRM_CalcChildTargetPos_t)(uintptr_t entity, int param2,
    float* posOut, float* posOut2, int* distOut);
static GRM_CalcChildTargetPos_t s_fn_CalcChildTargetPos = nullptr;

// FUN_1400f6ef0: BuildStructure_impl_6ef0 -- validate work assignment
typedef uint64_t (*GRM_ValidateWorkAssign_t)(uintptr_t self, void* citizenIBP,
    int targetCell, uint32_t srcCell, void* param5);
static GRM_ValidateWorkAssign_t s_fn_ValidateWorkAssign = nullptr;

// FUN_1400bf790: Pathfinding_FindPath
typedef void (*GRM_PathfindingFindPath_t)(void* outResult, void* citizenPtr,
    int pathType, void* mapGridPtr, float* targetPos, void* destEntityPtr,
    float* srcPos, int64_t materialType, int amount,
    int packedSlotInfo, int param11, char param12, uint32_t param13, int param14);
static GRM_PathfindingFindPath_t s_fn_PathfindingFindPath = nullptr;

// CitizenAI_impl_1a30: RegisterWork
typedef void (*GRM_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
static GRM_RegisterWork_t s_fn_RegisterWork = nullptr;

// Type IDs
static uint32_t* s_typeId_GatherDesc = nullptr;    // DWORD_1401c7040
static uint32_t* s_typeId_RawMatComp = nullptr;     // DWORD_1401c7cb8
static uint32_t* s_typeId_PosDesc = nullptr;        // DAT_1401c7938

static bool s_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool GatherRawMaterials_ResolveFuncs() {
    if (s_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<GRM_WorkSerialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ResizeIntArray = reinterpret_cast<GRM_ResizeIntArray_t>(
        ResolveAddr(Module::Exe, 0x1400021d0));
    s_fn_TreeSerialize = reinterpret_cast<GRM_TreeSerialize_t>(
        ResolveAddr(Module::Exe, 0x140025630));
    s_fn_TreeRemoveByKey = reinterpret_cast<GRM_TreeRemoveByKey_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_TreeRemoveNode = reinterpret_cast<GRM_TreeRemoveNode_t>(
        ResolveAddr(Module::Exe, 0x140024f30));
    s_fn_StateMachineRelease = reinterpret_cast<GRM_StateMachineRelease_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_CanStartWork = reinterpret_cast<GRM_CanStartWork_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_FindComp_Position = reinterpret_cast<GRM_FindCompPosition_t>(
        ResolveAddr(Module::Exe, 0x140002ed0));
    s_fn_FindComp_Unk = reinterpret_cast<GRM_FindCompUnk_t>(
        ResolveAddr(Module::Exe, 0x14000d6c0));
    s_fn_FindComp_WpExt = reinterpret_cast<GRM_FindCompWpExt_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_FindStorageForResource = reinterpret_cast<GRM_FindStorageForResource_t>(
        ResolveAddr(Module::Exe, 0x14008d590));
    s_fn_FindComp_Storage = reinterpret_cast<GRM_FindCompStorage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<GRM_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_GetRemainingCapacity = reinterpret_cast<GRM_GetRemainingCapacity_t>(
        ResolveAddr(Module::Exe, 0x140116540));
    s_fn_LongTermPlan = reinterpret_cast<GRM_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_CalcBestTargetPos = reinterpret_cast<GRM_CalcBestTargetPos_t>(
        ResolveAddr(Module::Exe, 0x140122be0));
    s_fn_CalcChildTargetPos = reinterpret_cast<GRM_CalcChildTargetPos_t>(
        ResolveAddr(Module::Exe, 0x140122b00));
    s_fn_ValidateWorkAssign = reinterpret_cast<GRM_ValidateWorkAssign_t>(
        ResolveAddr(Module::Exe, 0x1400f6ef0));
    s_fn_PathfindingFindPath = reinterpret_cast<GRM_PathfindingFindPath_t>(
        ResolveAddr(Module::Exe, 0x1400bf790));
    s_fn_RegisterWork = reinterpret_cast<GRM_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));

    // Dispatch slots (indirect through IAT)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152a00);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<GRM_SerializerCtor_t*>(iatSerCtor);

    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152a08);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<GRM_SerializerDtor_t*>(iatSerDtor);

    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152878);
    if (iatSerInt)
        s_fn_SerializeInt = *reinterpret_cast<GRM_SerializeInt_t*>(iatSerInt);

    uintptr_t iatSerIntBulk = ResolveAddr(Module::Exe, 0x1401528c0);
    if (iatSerIntBulk)
        s_fn_SerializeIntBulk = *reinterpret_cast<GRM_SerializeIntBulk_t*>(iatSerIntBulk);

    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x1401527e8);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<GRM_MemAlloc_t*>(iatMemAlloc);

    uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x1401527f0);
    if (iatMemFree)
        s_fn_MemFree = *reinterpret_cast<GRM_MemFree_t*>(iatMemFree);

    // Type IDs
    s_typeId_GatherDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7040));
    s_typeId_RawMatComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_typeId_PosDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));

    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_SerializeInt && s_fn_SerializeIntBulk &&
              s_fn_ResizeIntArray && s_fn_TreeSerialize &&
              s_fn_TreeRemoveByKey && s_fn_TreeRemoveNode &&
              s_fn_StateMachineRelease && s_fn_CanStartWork &&
              s_fn_FindComp_Position && s_fn_FindComp_Unk &&
              s_fn_FindComp_WpExt && s_fn_FindStorageForResource &&
              s_fn_FindComp_Storage && s_fn_StorageQueryCount &&
              s_fn_GetRemainingCapacity && s_fn_LongTermPlan &&
              s_fn_CalcBestTargetPos && s_fn_CalcChildTargetPos &&
              s_fn_ValidateWorkAssign && s_fn_PathfindingFindPath &&
              s_fn_RegisterWork && s_fn_MemAlloc && s_fn_MemFree &&
              fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              g_pGameWorldSingleton &&
              s_typeId_GatherDesc && s_typeId_RawMatComp && s_typeId_PosDesc;

#ifdef _DEBUG
    printf("[GatherRawMaterials] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: s_fn_Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: s_fn_Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: s_fn_Serializer_Dtor\n");
        if (!s_fn_SerializeInt) printf("  MISSING: s_fn_SerializeInt\n");
        if (!s_fn_SerializeIntBulk) printf("  MISSING: s_fn_SerializeIntBulk\n");
        if (!s_fn_ResizeIntArray) printf("  MISSING: s_fn_ResizeIntArray\n");
        if (!s_fn_TreeSerialize) printf("  MISSING: s_fn_TreeSerialize\n");
        if (!s_fn_TreeRemoveByKey) printf("  MISSING: s_fn_TreeRemoveByKey\n");
        if (!s_fn_TreeRemoveNode) printf("  MISSING: s_fn_TreeRemoveNode\n");
        if (!s_fn_StateMachineRelease) printf("  MISSING: s_fn_StateMachineRelease\n");
        if (!s_fn_CanStartWork) printf("  MISSING: s_fn_CanStartWork\n");
        if (!s_fn_FindComp_Position) printf("  MISSING: s_fn_FindComp_Position\n");
        if (!s_fn_FindComp_Unk) printf("  MISSING: s_fn_FindComp_Unk\n");
        if (!s_fn_FindComp_WpExt) printf("  MISSING: s_fn_FindComp_WpExt\n");
        if (!s_fn_FindStorageForResource) printf("  MISSING: s_fn_FindStorageForResource\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: s_fn_FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: s_fn_StorageQueryCount\n");
        if (!s_fn_GetRemainingCapacity) printf("  MISSING: s_fn_GetRemainingCapacity\n");
        if (!s_fn_LongTermPlan) printf("  MISSING: s_fn_LongTermPlan\n");
        if (!s_fn_CalcBestTargetPos) printf("  MISSING: s_fn_CalcBestTargetPos\n");
        if (!s_fn_CalcChildTargetPos) printf("  MISSING: s_fn_CalcChildTargetPos\n");
        if (!s_fn_ValidateWorkAssign) printf("  MISSING: s_fn_ValidateWorkAssign\n");
        if (!s_fn_PathfindingFindPath) printf("  MISSING: s_fn_PathfindingFindPath\n");
        if (!s_fn_RegisterWork) printf("  MISSING: s_fn_RegisterWork\n");
        if (!s_fn_MemAlloc) printf("  MISSING: s_fn_MemAlloc\n");
        if (!s_fn_MemFree) printf("  MISSING: s_fn_MemFree\n");
    }
#endif

    s_resolved = ok;
    return ok;
}

// ============================================================
// Helper: Find RawMaterialComponent by type ID in entity's component list.
// Iterates array at (entity+0x28), count at (entity+0x30).
// ============================================================
static void* FindRawMaterialInList(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x30);
    uintptr_t arr = *(uintptr_t*)(entity + 0x28);
    for (int i = 0; i < count; i++) {
        uintptr_t obj = *(uintptr_t*)(arr + (uintptr_t)i * 8);
        uintptr_t vtable = *(uintptr_t*)obj;
        typedef uint32_t (*GetTypeId_t)();
        auto getTypeId = *(GetTypeId_t*)(vtable + 0x10);
        if (getTypeId() == typeId) {
            void* comp = *(void**)(arr + (uintptr_t)i * 8);
            if (fn_IsDerivedFrom(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: Tree iteration -- find leftmost node
// ============================================================
static uintptr_t TreeLeftmost(uintptr_t node) {
    if (node == 0) return 0;
    while (*(uintptr_t*)(node + 0x10) != 0) {
        node = *(uintptr_t*)(node + 0x10);
    }
    return node;
}

// ============================================================
// Helper: Tree iteration -- get next node (in-order)
// (Currently unused but kept for future reference.)
// ============================================================
__attribute__((unused))
static uintptr_t TreeNext(uintptr_t node) {
    if (node == 0) return 0;
    // If right child exists, go right then leftmost
    uintptr_t right = *(uintptr_t*)(node + 0x18);
    if (right != 0) {
        return TreeLeftmost(right);
    }
    // Otherwise go up until we come from a left child
    uintptr_t parent = *(uintptr_t*)(node + 0x8);
    while (parent != 0 && *(uintptr_t*)(parent + 0x18) == node) {
        node = parent;
        parent = *(uintptr_t*)(node + 0x8);
    }
    return parent;
}

// ============================================================
// Serialize (0x1400ec560)
// Work::Serialize + 2 int arrays + tree serialization
// ============================================================
void GatherRawMaterials_Serialize_Impl(GatherRawMaterials* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call base Work::Serialize
    s_fn_Work_Serialize(self, stream);

    // Create IO::Serializer with version 0
    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Check if loading (stream+8 == 0 means loading)
    uintptr_t serStreamPtr = *(uintptr_t*)serializerBuf;
    bool isLoading = (*(int*)(serStreamPtr + 8) == 0);

    // --- Serialize first int array count (+0x50) ---
    int count50 = self->field_50;
    s_fn_SerializeInt(serializerBuf, &count50, nullptr);

    if (isLoading) {
        s_fn_ResizeIntArray((void*)(selfAddr + 0x48), count50);
        self->field_50 = count50;
    }

    // Bulk-serialize the first int array
    s_fn_SerializeIntBulk(serializerBuf, self->field_50, self->field_48, nullptr);

    // --- Serialize second int array count (+0x60) ---
    int count60 = self->field_60;
    s_fn_SerializeInt(serializerBuf, &count60, nullptr);

    if (isLoading) {
        s_fn_ResizeIntArray((void*)(selfAddr + 0x58), count60);
        self->field_60 = count60;
    }

    // Bulk-serialize the second int array
    s_fn_SerializeIntBulk(serializerBuf, self->field_60, self->field_58, nullptr);

    // --- Serialize tree at +0x68 ---
    s_fn_TreeSerialize((void*)(selfAddr + 0x68), serializerBuf);

    s_fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Execute (0x1400ebe40) -- vtable slot 4
//
// Main gather logic. Iterates material description slots to find
// resources that need gathering. For each slot with capacity, searches
// for a storage entity with the resource, calculates path, and assigns
// the citizen to fetch it.
//
// Returns: 0=fail, 1=retry/done, 2=action pushed.
// ============================================================
int GatherRawMaterials_Execute_Impl(GatherRawMaterials* self, void* entityIBP,
                                     uint64_t param3, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // --- Find PositionComponent on work entity (self+0x10) ---
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t posCompIBP = 0;
    s_fn_FindComp_Position(selfQ[2], &posCompIBP);

    // --- Find description component on work entity ---
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t descCompIBP = 0;
    s_fn_FindComp_Unk(selfQ[2], &descCompIBP);

    // Get GatherDescription from desc component +0x18
    fn_InternalBasePtr_CheckPtr(&descCompIBP);
    void* descBase = *(void**)(descCompIBP + 0x18);
    bool isDerived = fn_IsDerivedFrom(descBase, *s_typeId_GatherDesc);
    void* gatherDesc = isDerived ? descBase : nullptr;

    // --- Check workplace enable flag via WpExt component ---
    fn_InternalBasePtr_CheckPtr(&descCompIBP);
    uintptr_t descComp = descCompIBP;
    fn_InternalBasePtr_CheckPtr((void*)(descComp + 0x10));
    void* wpExtIBP = s_fn_FindComp_WpExt(*(uintptr_t*)(descComp + 0x10), (void*)&param3);
    fn_InternalBasePtr_CheckPtr(wpExtIBP);
    if (*(char*)(*(uintptr_t*)wpExtIBP + 0x50) == '\0') {
        return 1;  // Workplace disabled
    }

    // --- Get game world data ---
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void* worldEntityList = *(void**)(gameWorld + 0xf0);

    // --- Get citizen position for cell calculation ---
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;
    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t citizenAIComp = *(uintptr_t*)(citizenEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(citizenAIComp + 0x68));
    uintptr_t citizenPosComp = *(uintptr_t*)(citizenAIComp + 0x68);

    int mapSize = *mapGrid;
    int halfMap = mapSize >> 1;
    int cellX = (int)(*(float*)(citizenPosComp + 0x158) + (float)halfMap);
    int cellY = (int)(*(float*)(citizenPosComp + 0x15c) + (float)halfMap);
    if (cellX < 0) cellX = 0;
    if (cellX >= mapSize) cellX = mapSize - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= mapSize) cellY = mapSize - 1;
    uint32_t citizenCell = (uint32_t)(cellY * mapSize + cellX);

    // --- Iterate material slots ---
    int numSlots = (int)selfQ[10]; // self->field_50 count
    if (numSlots < 1) {
        return 1;
    }

    uintptr_t gatherDescAddr = (uintptr_t)gatherDesc;
    uintptr_t slotOffset = 0;      // +0x00, +0x10, +0x20, ...  (stride 0x10 per slot)
    uintptr_t intOffset = 0;       // +0x00, +0x04, +0x08, ...  (stride 4 per slot)

    for (int slotIdx = 0; slotIdx < numSlots; slotIdx++) {
        // Get material description data for this slot
        uintptr_t matDescBase = *(uintptr_t*)(gatherDescAddr + 0x20);

        // Calculate target amount: materialCount * multiplier from descComp
        fn_InternalBasePtr_CheckPtr(&descCompIBP);
        int targetAmount = *(int*)(slotOffset + matDescBase + 8) * *(int*)(descCompIBP + 0x4c);

        // Check: buildCount[slotIdx] + field_48[slotIdx] < targetAmount
        int buildCount = *(int*)(intOffset + selfQ[0xb]);      // self->field_58[slotIdx]
        int assignCount = *(int*)(intOffset + selfQ[9]);        // self->field_48[slotIdx]
        if (buildCount + assignCount >= targetAmount) {
            // This slot is already fulfilled
            goto next_slot;
        }

        {
            // --- Find RawMaterialComponent in the material description ---
            uintptr_t matDescEntity = *(uintptr_t*)(slotOffset + matDescBase);
            void* rawMatComp = FindRawMaterialInList(matDescEntity, *s_typeId_RawMatComp);

            // --- Search for a storage entity with this resource ---
            uintptr_t foundEntity = 0;
            float searchPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            s_fn_FindStorageForResource(worldEntityList, &foundEntity, citizenCell,
                *(uint64_t*)(slotOffset + matDescBase), 1, '\0', searchPos,
                (void*)(selfAddr + 0x10));

            if (foundEntity == 0) {
                // No storage found for this slot
                goto next_slot;
            }

            {
                // --- Calculate how much to gather ---
                int remaining = targetAmount - buildCount - assignCount;

                // Check citizen's carrying capacity
                fn_InternalBasePtr_CheckPtr(entityIBP);
                uintptr_t citizenEnt = *(uintptr_t*)entityIBP;
                fn_InternalBasePtr_CheckPtr((void*)(citizenEnt + 0x10));
                uintptr_t citizenStorageIBP = 0;
                s_fn_FindComp_Storage(*(uintptr_t*)(citizenEnt + 0x10), &citizenStorageIBP);

                fn_InternalBasePtr_CheckPtr(&citizenStorageIBP);
                int capacity = *(int*)(citizenStorageIBP + 0x38);
                fn_InternalBasePtr_CheckPtr(&citizenStorageIBP);
                int currentStored = s_fn_StorageQueryCount((void*)citizenStorageIBP);

                int carryWeight = *(int*)((uintptr_t)rawMatComp + 0x48);
                if (capacity - currentStored < carryWeight * remaining) {
                    // Not enough carry capacity -- recalculate
                    fn_InternalBasePtr_CheckPtr(entityIBP);
                    int remCapacity = s_fn_GetRemainingCapacity(*(uintptr_t*)entityIBP);
                    remaining = remCapacity / carryWeight;
                }

                if (remaining == 0) {
                    // Can't carry anything -- replan
                    fn_InternalBasePtr_CheckPtr(entityIBP);
                    s_fn_LongTermPlan(*(void**)entityIBP);
                    return 2;
                }

                // --- Find storage on the found entity and match material slot ---
                fn_InternalBasePtr_CheckPtr(&foundEntity);
                uintptr_t storageCompIBP2 = 0;
                void* retIBP = s_fn_FindComp_Storage((uintptr_t)foundEntity, &storageCompIBP2);
                fn_InternalBasePtr_CheckPtr(retIBP);

                uintptr_t storageComp = *(uintptr_t*)retIBP;
                int storageSlotCount = *(int*)(storageComp + 0x30);
                int foundSlotIdx = -1;
                int availableAmount = 0;

                if (storageSlotCount > 0) {
                    uintptr_t* slotArray = *(uintptr_t**)(storageComp + 0x28);
                    for (int si = 0; si < storageSlotCount; si++) {
                        if (slotArray[si * 2] == *(uintptr_t*)(slotOffset + matDescBase)) {
                            if (foundSlotIdx >= 0) {
                                // Found matching slot -- calc available
                                availableAmount = (int)slotArray[foundSlotIdx * 2 + 1] -
                                    *(int*)((uintptr_t)slotArray + foundSlotIdx * 0x10 + 0xc);
                            }
                            break;
                        }
                        foundSlotIdx = si;
                    }
                }

                // Clamp remaining to available
                if (availableAmount < remaining) {
                    remaining = availableAmount;
                }

                // --- Calculate target position ---
                float targetPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};

                fn_InternalBasePtr_CheckPtr(&posCompIBP);
                void* posDescBase = *(void**)((uintptr_t)posCompIBP + 0x18);
                int bestDist = 0x7fffffff;

                bool isDerivedPos = fn_IsDerivedFrom(posDescBase, *s_typeId_PosDesc);
                void* posDesc = isDerivedPos ? posDescBase : nullptr;

                uint8_t found = s_fn_CalcBestTargetPos(posCompIBP,
                    (uintptr_t)posDesc + 0x38, searchPos, targetPos, &bestDist);

                void* posComp2 = (void*)posCompIBP;
                if (*(int*)((uintptr_t)posComp2 + 0x30) > 0) {
                    uintptr_t childOffset = 0;
                    int childCount = 0;
                    do {
                        uintptr_t* childPtr = (uintptr_t*)(*(uintptr_t*)((uintptr_t)posComp2 + 0x28) + childOffset);
                        fn_InternalBasePtr_CheckPtr(childPtr);
                        uint8_t childFound = s_fn_CalcChildTargetPos(*childPtr, 1,
                            searchPos, targetPos, &bestDist);
                        childCount++;
                        found |= childFound;
                        childOffset += 8;
                    } while (childCount < *(int*)((uintptr_t)posComp2 + 0x30));
                }

                if (found == 0) {
                    return 1;
                }

                // --- Convert target position to grid cell ---
                int mapSz = *mapGrid;
                int tgtCellX = (int)(targetPos[0] + (float)(mapSz >> 1));
                int tgtCellY = (int)(targetPos[1] + (float)(mapSz >> 1));
                if (tgtCellX < 0) tgtCellX = 0;
                if (tgtCellX >= mapSz) tgtCellX = mapSz - 1;
                if (tgtCellY < 0) tgtCellY = 0;
                if (tgtCellY >= mapSz) tgtCellY = mapSz - 1;
                int targetCell = tgtCellY * mapSz + tgtCellX;

                // --- Validate work assignment ---
                uintptr_t citizenRef = *(uintptr_t*)entityIBP;
                uint64_t validated = s_fn_ValidateWorkAssign(selfAddr, &citizenRef,
                    targetCell, citizenCell, (void*)param3);

                if ((char)validated == '\0') {
                    return 0;
                }

                // --- Find path and assign work ---
                uintptr_t destEntity = foundEntity;
                int* mapGridPtr = (int*)selfQ[2];  // Reread from self
                void* citizenPtr = *(void**)entityIBP;

                s_fn_PathfindingFindPath(
                    worldEntityList,    // outResult (reused)
                    &citizenPtr,        // citizenPtr
                    (int)selfQ[4],      // pathType (priority/workplaceId)
                    &mapGridPtr,        // mapGridPtr
                    targetPos,          // targetPos
                    &destEntity,        // destEntityPtr
                    searchPos,          // srcPos
                    (int64_t)*(uint64_t*)(slotOffset + matDescBase),  // materialType
                    remaining,          // amount
                    (remaining << 8) | slotIdx,  // packed slot info
                    0, '\0', 0, 0);

                // --- Finalize: set citizen state and register work ---
                uintptr_t priority = selfQ[4];
                fn_InternalBasePtr_CheckPtr(entityIBP);
                uintptr_t citizenFinal = *(uintptr_t*)entityIBP;

                // Call vtable slot +0x50 (GetWorkEntity)
                uintptr_t vtable = *(uintptr_t*)self;
                typedef uintptr_t* (*VtSlot50_t)(GatherRawMaterials*, void*);
                VtSlot50_t getWorkEntity = *(VtSlot50_t*)(vtable + 0x50);
                uintptr_t workEntIBP[2] = {0};
                uintptr_t* wResult = getWorkEntity(self, workEntIBP);
                uintptr_t workEntity = *wResult;

                *(uint32_t*)(citizenFinal + 0x54) |= 4;
                *(int32_t*)(citizenFinal + 0x30) = (int)priority;
                *(uint32_t*)(citizenFinal + 0xb0) = 0;

                if (workEntity == *(uintptr_t*)(citizenFinal + 0x1f0) && (int)priority >= 0) {
                    *(uint32_t*)(citizenFinal + 0x88) |= 0x20000;
                }

                s_fn_RegisterWork((void*)(selfAddr + 0x68), workEntIBP, entityIBP);

                // Increment build count for this slot
                int* buildCountPtr = (int*)(selfQ[0xb] + (int64_t)slotIdx * 4);
                *buildCountPtr += remaining;

                return 2;
            }
        }

    next_slot:
        intOffset += 4;
        slotOffset += 0x10;
    }

    return 1;  // No work found
}

// ============================================================
// OnComplete (0x1400ebdf0) -- vtable slot 6
//
// Completion handler. Removes citizen from tree at +0x68,
// updates build counts (decrement field_58, increment field_48).
// Returns: value from field_48 masking.
// ============================================================
int GatherRawMaterials_OnComplete_Impl(GatherRawMaterials* self, void* param2,
                                        void* /*param3*/, uint32_t param4) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Remove entity from tree at +0x68
    s_fn_TreeRemoveByKey((void*)(selfAddr + 0x68), param2);

    // Extract slot index (low byte) and amount (high 24 bits, shifted right by 8)
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] (build counts) by amount
    uintptr_t* selfQ = (uintptr_t*)self;
    int* buildCountPtr = (int*)(selfQ[0xb] + (uintptr_t)slotIdx * 4);
    *buildCountPtr -= amount;

    // Increment field_48[slotIdx] (assigned counts) by amount
    int* assignCountPtr = (int*)(selfQ[9] + (uintptr_t)slotIdx * 4);
    *assignCountPtr += amount;

    return (uintptr_t)selfQ[9] & 0xFFFFFF00;  // Original returns uVar3 & 0xffffffffffffff00
}

// ============================================================
// OnCancel (0x1400ebdb0) -- vtable slot 7
//
// Cancel handler. Removes from tree, decrements build count only
// (does NOT increment assigned count).
// ============================================================
void GatherRawMaterials_OnCancel_Impl(GatherRawMaterials* self, void* param2,
                                       void* /*param3*/, uint32_t param4) {
    (void)self; // selfAddr used below via cast
    uintptr_t selfAddr = (uintptr_t)self;

    // Remove entity from tree at +0x68
    s_fn_TreeRemoveByKey((void*)(selfAddr + 0x68), param2);

    // Extract slot index and amount
    uintptr_t* selfQ = (uintptr_t*)self;
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] only (cancel = don't credit the work)
    int* buildCountPtr = (int*)(selfQ[0xb] + (uintptr_t)slotIdx * 4);
    *buildCountPtr -= amount;
}

// ============================================================
// Cleanup (0x1400ec480) -- vtable slot 8
//
// Release all assigned citizens from tree at +0x68.
// Walks tree from leftmost node, for each citizen calls
// CitizenAI::StateMachineRelease.
// ============================================================
void GatherRawMaterials_Cleanup_Impl(GatherRawMaterials* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    while (true) {
        uintptr_t root = *(uintptr_t*)(selfAddr + 0x68);
        if (root == 0) return;

        // Find leftmost node
        uintptr_t node = root;
        while (*(uintptr_t*)(node + 0x10) != 0) {
            node = *(uintptr_t*)(node + 0x10);
        }

        // Also try walking up from leftmost
        uintptr_t current = root;
        while (current != 0) {
            uintptr_t next = *(uintptr_t*)(current + 0x10);
            if (next == 0) break;
            current = next;
        }

        // node is now leftmost
        if (node == 0) return;

        // Get the citizen entity IBP from this node
        fn_InternalBasePtr_CheckPtr((void*)node);
        uintptr_t citizenEntity = *(uintptr_t*)node;

        // Release citizen's state machine via +0x278 -> +0x48
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        uintptr_t citizenAI = *(uintptr_t*)(citizenEntity + 0x278);
        s_fn_StateMachineRelease((void*)(citizenAI + 0x48));
    }
}

// ============================================================
// Validate (0x1400ebb50) -- vtable slot 9
//
// Check all assigned tasks in tree at +0x68 are still valid.
// Walks tree, calls Work::CanStartWork for each. Invalid ones
// are collected and removed, then cleanup is triggered.
//
// Returns: true (nonzero) if nothing was removed, false if any removed.
// ============================================================
int GatherRawMaterials_Validate_Impl(GatherRawMaterials* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Walk tree at +0x68 (field_68), collect invalid entries
    uintptr_t root = selfQ[0xd]; // +0x68
    uint32_t invalidCount = 0;
    void* invalidArray = nullptr;
    uint32_t invalidCapacity = 0;

    if (root != 0) {
        // Find leftmost node
        uintptr_t node = root;
        while (*(uintptr_t*)(node + 0x10) != 0) {
            node = *(uintptr_t*)(node + 0x10);
        }

        while (node != 0) {
            // Check if this task is still valid
            uintptr_t taskIBP[1];
            taskIBP[0] = *(uintptr_t*)node;

            char valid = s_fn_CanStartWork((void*)self, param2, taskIBP, nullptr);

            if (valid == '\0') {
                // Invalid -- add to removal list
                uint32_t newCount = invalidCount + 1;
                if (newCount != invalidCount) {
                    if ((int)invalidCapacity < (int)newCount) {
                        uint32_t newCap = newCount;
                        if (invalidCount != 0) {
                            newCap = newCount * 3;
                        }
                        invalidCapacity = newCap;
                        void* newArray = s_fn_MemAlloc((int64_t)(int)newCap << 3, 8);

                        // Copy old entries
                        int copyStart = (invalidArray == newArray) ? (int)invalidCount : 0;
                        for (int ci = copyStart; ci < (int)newCount; ci++) {
                            uintptr_t* dst = (uintptr_t*)((uintptr_t)newArray + (int64_t)ci * 8);
                            *dst = 0;
                            if (ci < (int)invalidCount) {
                                *dst = *((uintptr_t*)((uintptr_t)invalidArray + (int64_t)ci * 8));
                            } else {
                                *dst = *(uintptr_t*)node;
                            }
                        }

                        if (invalidArray != newArray) {
                            s_fn_MemFree(invalidArray);
                        }
                        invalidArray = newArray;
                    } else {
                        // Space available, just add
                        *((uintptr_t*)((uintptr_t)invalidArray + (int64_t)invalidCount * 8)) = *(uintptr_t*)node;
                    }
                    invalidCount = newCount;
                }
            }

            // Get next node in tree (in-order traversal)
            uintptr_t right = *(uintptr_t*)(node + 0x18);
            if (right != 0) {
                // Go right, then leftmost
                node = right;
                while (*(uintptr_t*)(node + 0x10) != 0) {
                    node = *(uintptr_t*)(node + 0x10);
                }
            } else {
                // Go up until we come from a left child
                uintptr_t parent = *(uintptr_t*)(node + 0x8);
                while (parent != 0) {
                    if (*(uintptr_t*)(parent + 0x10) == node) {
                        // node is left child of parent -- parent is next
                        break;
                    }
                    // node is right child, keep going up
                    node = parent;
                    parent = *(uintptr_t*)(node + 0x8);
                }
                node = parent;
            }
        }
    }

    // Remove invalid entries from tree
    if ((int)invalidCount > 0) {
        uintptr_t* invArr = (uintptr_t*)invalidArray;
        for (uint32_t i = 0; i < invalidCount; i++) {
            // Find node in tree by key and remove
            uintptr_t searchKey = invArr[i];
            uintptr_t* treeRoot = (uintptr_t*)(selfAddr + 0x68);
            uintptr_t treeNode = *treeRoot;

            while (treeNode != 0) {
                fn_InternalBasePtr_CheckPtr((void*)treeNode);
                uintptr_t nodeKey = *(uintptr_t*)treeNode;
                if (searchKey < nodeKey) {
                    treeNode = *(uintptr_t*)(treeNode + 0x10);
                } else {
                    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)invArr + i * 8));
                    if (searchKey <= nodeKey) {
                        // Found -- remove this node
                        s_fn_TreeRemoveNode((void*)(selfAddr + 0x68), (void*)treeNode);
                        break;
                    }
                    treeNode = *(uintptr_t*)(treeNode + 0x18);
                }
            }
        }

        // If any were removed, call vtable slot +0x40 (Cleanup) and reset build counts
        if ((int)invalidCount > 0) {
            // Call vtable slot +0x40
            uintptr_t vtable = *(uintptr_t*)self;
            typedef void (*VtSlot40_t)(GatherRawMaterials*);
            VtSlot40_t cleanupSlot = *(VtSlot40_t*)(vtable + 0x40);
            cleanupSlot(self);

            // Reset all build counts to 0
            int numSlots = (int)selfQ[0xc]; // field_60 count
            for (int i = 0; i < numSlots; i++) {
                *(int*)(selfQ[0xb] + (uintptr_t)i * 4) = 0;
            }
        }
    }

    s_fn_MemFree(invalidArray);
    return (invalidCount == 0) ? 1 : 0;
}

// ============================================================
// GetTarget (0x1400ec510) -- vtable slot 10
//
// Returns target position from game world singleton.
// Hidden return ptr: RCX=self, RDX=retbuf.
// ============================================================
void* GatherRawMaterials_GetTarget_Impl(GatherRawMaterials* /*self*/, void* retbuf) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t something = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)retbuf = *(uintptr_t*)(something + 0x30);
    return retbuf;
}
