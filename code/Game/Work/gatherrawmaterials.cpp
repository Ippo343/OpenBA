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

namespace Object { uint32_t GetTypeIdByName(const char* name); }  // factory_registration.h

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

// Tree node cleanup (FUN_140014c30)
typedef void (*GRM_TreeNodeCleanup_t)(void* bstRoot, void* node);
static GRM_TreeNodeCleanup_t s_fn_TreeNodeCleanup = nullptr;

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

// Vtable pointers for dtor chain
static void** s_vtable_GatherRawMaterials = nullptr;  // 0x140161420
static void** s_vtable_Work = nullptr;                 // 0x1401612e0

static bool s_resolved = false;

// Gated Execute-decision trace (BANISHED_BUILD_TRACE), to pinpoint where a construction
// gather work bails before enqueuing a real town->site haul. Diagnostic only.
#include <cstdlib>
#include <cstring>
static int s_grm_trace = -1;
static bool GRM_Trace() {
    if (s_grm_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_grm_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_grm_trace == 1;
}
#define GRM_TRACEF(...) do { if (GRM_Trace()) { std::fprintf(stderr, "[GRMExec] " __VA_ARGS__); std::fflush(stderr); } } while (0)

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
        // Level-1 caller-bug fix: 0x140025630 is MID-FUNCTION (inside TradeUI_impl_55a0
        // 0x1400255a0). The real simple-BST serialize is 0x1400296b0 — confirmed by
        // xref: GatherRawMaterials_Shutdown@0x1400ec62a CALLs 0x1400296b0 (already registered),
        // the same BST-serialize ForesterWork_Shutdown uses.
        ResolveAddr(Module::Exe, 0x1400296b0));
    s_fn_TreeRemoveByKey = reinterpret_cast<GRM_TreeRemoveByKey_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_TreeRemoveNode = reinterpret_cast<GRM_TreeRemoveNode_t>(
        // Level-1 caller-bug fix: 0x140024f30 is MID-FUNCTION (inside Toolbar_vslot14
        // 0x140024f00). The real BST single-node remove is 0x14001a6e0 — confirmed by
        // xref: GatherRawMaterials_Update@0x1400ebd24 CALLs 0x14001a6e0 (already registered).
        ResolveAddr(Module::Exe, 0x14001a6e0));
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
    s_fn_TreeNodeCleanup = reinterpret_cast<GRM_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Vtable pointers for dtor chain
    s_vtable_GatherRawMaterials = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161420));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401612e0));

    // Dispatch slots (indirect through IAT)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<GRM_SerializerCtor_t*>(iatSerCtor);

    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<GRM_SerializerDtor_t*>(iatSerDtor);

    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt)
        s_fn_SerializeInt = *reinterpret_cast<GRM_SerializeInt_t*>(iatSerInt);

    uintptr_t iatSerIntBulk = ResolveAddr(Module::Exe, 0x1401528c0);
    if (iatSerIntBulk)
        s_fn_SerializeIntBulk = *reinterpret_cast<GRM_SerializeIntBulk_t*>(iatSerIntBulk);

    // MemAlloc/MemFree — resolve via the canonical Runtime exports (static size/align &
    // ptr forms). The exe IAT dispatch slots (0x1401527e8/0x1401527f0) are UNREGISTERED in
    // the standalone exe AND mislabelled (they index the _rng/Lerp imports, not
    // MemoryAllocator) — see ForesterWork's identical fix. Without this s_fn_MemAlloc stays
    // NULL and GatherRawMaterials::Validate crashes (EXEC of 0) at its removal-array grow
    // when a citizen's Idle-state work-tree walk validates the posted gather work.
    // ResolveExport reads the Runtime DLL exports directly and is correct in BOTH modes.
    s_fn_MemAlloc = reinterpret_cast<GRM_MemAlloc_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));     // static (size,align)
    if (!s_fn_MemAlloc) {
        uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x1401527e8);
        if (iatMemAlloc) s_fn_MemAlloc = *reinterpret_cast<GRM_MemAlloc_t*>(iatMemAlloc);
    }
    s_fn_MemFree = reinterpret_cast<GRM_MemFree_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));        // static (ptr)
    if (!s_fn_MemFree) {
        uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x1401527f0);
        if (iatMemFree) s_fn_MemFree = *reinterpret_cast<GRM_MemFree_t*>(iatMemFree);
    }

    // Type IDs. In the DLL build these come from raw .rdata; in standalone the .rdata
    // globals are never seeded (ResolveAddr(Exe,.rdata)==NULL) so we fall back to the
    // runtime type registry by name — otherwise Execute derefs *s_typeId_* (NULL) and
    // crashes the moment a citizen claims the gather work.
    s_typeId_GatherDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7040));
    s_typeId_RawMatComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_typeId_PosDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));
    static uint32_t s_tidGatherDescVal = 0, s_tidRawMatCompVal = 0, s_tidPosDescVal = 0;
    if (!s_typeId_GatherDesc) {
        if (!s_tidGatherDescVal) s_tidGatherDescVal = ::Object::GetTypeIdByName("BuildDescription");
        s_typeId_GatherDesc = &s_tidGatherDescVal;
    }
    if (!s_typeId_RawMatComp) {
        if (!s_tidRawMatCompVal) s_tidRawMatCompVal = ::Object::GetTypeIdByName("RawMaterialDescription");
        s_typeId_RawMatComp = &s_tidRawMatCompVal;
    }
    if (!s_typeId_PosDesc) {
        if (!s_tidPosDescVal) s_tidPosDescVal = ::Object::GetTypeIdByName("MapDescription");
        s_typeId_PosDesc = &s_tidPosDescVal;
    }

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
              s_fn_TreeNodeCleanup &&
              s_vtable_GatherRawMaterials && s_vtable_Work &&
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
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: s_fn_TreeNodeCleanup\n");
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

    // Ensure the exe-helper function pointers are resolved. The factory path calls
    // ResolveFuncs at construction, but the construction gather work is built via
    // GatherRawMaterials_Ctor_Impl (state-1 spawner), which does not — leaving every
    // s_fn_* NULL and the first `call *s_fn_FindComp_Position` faulting at RIP=0.
    GatherRawMaterials_ResolveFuncs();
    GRM_TRACEF("ENTER self=%p entityIBP=%p\n", (void*)self, entityIBP);

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
        GRM_TRACEF("BAIL: workplace disabled (wpExt+0x50==0)\n");
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
    GRM_TRACEF("past wp-check; numSlots=%d citizenCell=%u\n", numSlots, citizenCell);
    if (numSlots < 1) {
        GRM_TRACEF("BAIL: numSlots<1\n");
        return 1;
    }

    uintptr_t gatherDescAddr = (uintptr_t)gatherDesc;
    uintptr_t slotOffset = 0;      // +0x00, +0x10, +0x20, ...  (stride 0x10 per slot)
    uintptr_t intOffset = 0;       // +0x00, +0x04, +0x08, ...  (stride 4 per slot)

    // [DELIVERED-SYNC BRIDGE] (BANISHED_BUILD_SYNC_DELIVERED, default-off / faithful).
    // In retail, field_48[slot] (delivered-per-material) is credited by the work's
    // OnComplete (vtable +0x30), invoked when a citizen's haul CompleteTask action runs
    // through the task manager (gameState+0x118). In the standalone exe the citizen's
    // action executor never advances from DropOff to the CompleteTask node (deep
    // CitizenAI state-machine gap, phase3_keystone_d), so OnComplete never fires and
    // field_48 stays 0 forever — GRM re-hauls slot 0 (already over-delivered) and never
    // advances to slot 1, so the site never receives the 2nd material and state 1 never
    // reaches 8. The site StorageComponent IS the ground truth (physical DropOff deposits
    // into it, and BuildComponent_HaveAllMaterials reads it for the 1->8 gate). This
    // bridge re-syncs field_48[slot] from the live site storage each Execute — exactly
    // what GatherRawMaterials_Ctor_Impl seeds it to at construction, and what OnComplete
    // would keep it at — so GRM's per-slot "already fulfilled" check reflects reality and
    // advances through materials as they are physically delivered.
    static int s_syncDelivered = -1;
    if (s_syncDelivered < 0) {
        const char* e = std::getenv("BANISHED_BUILD_SYNC_DELIVERED");
        s_syncDelivered = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    uintptr_t siteStorageIBP = 0;
    if (s_syncDelivered) {
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
        s_fn_FindComp_Storage(selfQ[2], &siteStorageIBP);
    }

    for (int slotIdx = 0; slotIdx < numSlots; slotIdx++) {
        // Get material description data for this slot
        uintptr_t matDescBase = *(uintptr_t*)(gatherDescAddr + 0x20);

        // Calculate target amount: materialCount * multiplier from descComp
        fn_InternalBasePtr_CheckPtr(&descCompIBP);
        int targetAmount = *(int*)(slotOffset + matDescBase + 8) * *(int*)(descCompIBP + 0x4c);

        // [DELIVERED-SYNC BRIDGE] re-seed field_48[slot] from the live site storage
        // delivered amount (total - reserved) for this slot's material type, mirroring
        // GatherRawMaterials_Ctor_Impl's seeding. Keeps GRM's slot accounting in step with
        // what the citizens have physically deposited, standing in for the OnComplete credit
        // the inert standalone task manager never delivers.
        if (s_syncDelivered && siteStorageIBP) {
            uint64_t matTypeSync = *(uint64_t*)(slotOffset + *(uintptr_t*)(gatherDescAddr + 0x20));
            fn_InternalBasePtr_CheckPtr(&siteStorageIBP);
            uintptr_t st = siteStorageIBP;
            int deliveredSync = 0;
            int slotCountSync = *(int*)(st + 0x30);
            uintptr_t* slotsSync = *(uintptr_t**)(st + 0x28);
            if (slotsSync) {
                for (int s2 = 0; s2 < slotCountSync; s2++) {
                    if (slotsSync[s2 * 2] == matTypeSync) {
                        int total = (int)slotsSync[s2 * 2 + 1];
                        int reserved = *(int*)((uintptr_t)slotsSync + (uintptr_t)s2 * 0x10 + 0xc);
                        deliveredSync = total - reserved;
                        break;
                    }
                }
            }
            // [DELIVERED-SYNC + IN-TRANSIT RECONCILE] The fulfilled check below is
            // `field_58[slot] (in-transit) + field_48[slot] (delivered) >= target`. In retail
            // OnComplete moves a completed haul FROM in-transit TO delivered (field_58 -= amt;
            // field_48 += amt), keeping the sum == total committed. The standalone executor
            // never reaches OnComplete, so field_58 keeps every enqueued amount forever while
            // this bridge also credits the SAME physical units into field_48 — double-counting
            // them (e.g. enqueue 9 -> build=9; deliver 9 -> assign=9; 9+9=18 >= 16 target, so
            // GRM stops at 9/16 and never hauls the remaining 7). Reconcile here exactly as
            // OnComplete would: as delivered rises, subtract the delta from in-transit so the
            // gather keeps enqueuing until the site PHYSICALLY holds the full requirement.
            {
                int oldDelivered = *(int*)(intOffset + selfQ[9]);       // field_48[slot] (prev)
                int delta = deliveredSync - oldDelivered;
                if (delta > 0) {
                    int* inTransit = (int*)(intOffset + selfQ[0xb]);    // field_58[slot]
                    *inTransit -= delta;
                    if (*inTransit < 0) *inTransit = 0;
                }
            }
            *(int*)(intOffset + selfQ[9]) = deliveredSync;  // field_48[slot] = site delivered
        }

        // Check: buildCount[slotIdx] + field_48[slotIdx] < targetAmount
        int buildCount = *(int*)(intOffset + selfQ[0xb]);      // self->field_58[slotIdx]
        int assignCount = *(int*)(intOffset + selfQ[9]);        // self->field_48[slotIdx]
        GRM_TRACEF("slot[%d] target=%d build=%d assign=%d\n", slotIdx, targetAmount, buildCount, assignCount);
        if (buildCount + assignCount >= targetAmount) {
            // This slot is already fulfilled
            GRM_TRACEF("slot[%d] SKIP already-fulfilled\n", slotIdx);
            goto next_slot;
        }

        {
            // --- Find RawMaterialComponent in the material description ---
            uintptr_t matDescEntity = *(uintptr_t*)(slotOffset + matDescBase);
            void* rawMatComp = FindRawMaterialInList(matDescEntity, *s_typeId_RawMatComp);

            // --- Search for a storage entity with this resource ---
            uintptr_t foundEntity = 0;
            float searchPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            uintptr_t excludeIBP = 0;  // Original: local null-initialized IBP (NOT &self->field_10)

            s_fn_FindStorageForResource(worldEntityList, &foundEntity, citizenCell,
                *(uint64_t*)(slotOffset + matDescBase), 1, '\0', searchPos,
                (void*)&excludeIBP);

            GRM_TRACEF("slot[%d] FindStorageForResource -> foundEntity=%p\n", slotIdx, (void*)foundEntity);
            if (foundEntity == 0) {
                // No storage found for this slot
                GRM_TRACEF("slot[%d] SKIP no-source-storage\n", slotIdx);
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

                // [CARRY-DIAG] when capacity is 0, dump the carry-storage's StorageDescription
                // state to tell "no description" from "description present, Draw never ran".
                if (GRM_Trace() && capacity == 0) {
                    uintptr_t stComp = citizenStorageIBP;
                    uintptr_t stDesc = stComp ? *(uintptr_t*)(stComp + 0x18) : 0;
                    std::fprintf(stderr,
                        "[GRMExec] CARRY-DIAG storageComp=%p carryEntity=%p desc=%p avail(+0x21)=%d maxPerSlot(+0x24)=%d areaBased(+0x20)=%d slotCount(+0x30)=%d\n",
                        (void*)stComp, (void*)*(uintptr_t*)(citizenEnt + 0x10), (void*)stDesc,
                        stDesc ? *(uint8_t*)(stDesc + 0x21) : -1,
                        stDesc ? *(int*)(stDesc + 0x24) : -1,
                        stDesc ? *(uint8_t*)(stDesc + 0x20) : -1,
                        stComp ? *(int*)(stComp + 0x30) : -1);
                    std::fflush(stderr);
                }

                int carryWeight = *(int*)((uintptr_t)rawMatComp + 0x48);
                if (capacity - currentStored < carryWeight * remaining) {
                    // Not enough carry capacity -- recalculate
                    fn_InternalBasePtr_CheckPtr(entityIBP);
                    int remCapacity = s_fn_GetRemainingCapacity(*(uintptr_t*)entityIBP);
                    remaining = remCapacity / carryWeight;
                }

                GRM_TRACEF("slot[%d] remaining(pre-clamp)=%d carryWeight=%d cap=%d stored=%d\n",
                           slotIdx, remaining, carryWeight, capacity, currentStored);
                if (remaining == 0) {
                    // Can't carry anything -- replan
                    GRM_TRACEF("slot[%d] BAIL: remaining==0 -> LongTermPlan replan\n", slotIdx);
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

                // Search storage slots for matching material type
                // R8D=slotIdx starts at 0, increments on miss, used on match
                if (storageSlotCount > 0) {
                    uintptr_t* slotArray = *(uintptr_t**)(storageComp + 0x28);
                    for (int si = 0; si < storageSlotCount; si++) {
                        if (slotArray[si * 2] == *(uintptr_t*)(slotOffset + matDescBase)) {
                            if (si >= 0) {  // R8D >= 0 (always true for unsigned, but matches original TEST R8D,R8D / JS)
                                availableAmount = (int)slotArray[si * 2 + 1] -
                                    *(int*)((uintptr_t)slotArray + si * 0x10 + 0xc);
                            }
                            break;
                        }
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

                bool isDerivedPos = s_typeId_PosDesc && *s_typeId_PosDesc &&
                                    fn_IsDerivedFrom(posDescBase, *s_typeId_PosDesc);
                void* posDesc = isDerivedPos ? posDescBase : nullptr;

                // [STANDALONE GUARD] posDesc may be null if the position-description
                // type-id (0x1401c7938) does not resolve by name in standalone. The real
                // game always resolves it. Skip the primary target-pos calc when null
                // (the child-target loop below still contributes a position); never pass
                // null+0x38 into CalcBestTargetPos (it would read from 0x38).
                uint8_t found = posDesc ? s_fn_CalcBestTargetPos(posCompIBP,
                    (uintptr_t)posDesc + 0x38, searchPos, targetPos, &bestDist) : 0;

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
                    GRM_TRACEF("slot[%d] BAIL: no target pos (found==0, posDesc=%p)\n", slotIdx, posDesc);
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

                GRM_TRACEF("slot[%d] validateWorkAssign=%d availAmt=%d remaining(clamped)=%d targetCell=%d\n",
                           slotIdx, (int)(char)validated, availableAmount, remaining, targetCell);
                if ((char)validated == '\0') {
                    GRM_TRACEF("slot[%d] BAIL: validateWorkAssign failed\n", slotIdx);
                    return 0;
                }

                // --- Find path and assign work ---
                // Original uses a 3-qword stack buffer; [0]=worldEntityList
                uintptr_t pathBuf[3] = { (uintptr_t)worldEntityList, 0, 0 };
                uintptr_t destEntity = foundEntity;
                int* mapGridPtr = (int*)selfQ[2];  // Reread from self
                void* citizenPtr = *(void**)entityIBP;

                s_fn_PathfindingFindPath(
                    pathBuf,            // stack buffer (original: local_58[3])
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

                GRM_TRACEF("slot[%d] ENQUEUED HAUL remaining=%d buildCount->%d (return 2)\n",
                           slotIdx, remaining, *buildCountPtr);

                // [CHAIN-DIAG] Did PathfindingFindPath actually push the pickup->dropoff->
                // CompleteTask chain onto this citizen's action list? CanStartWork walks
                // citizenAIComp(+0x278)+0x48 (count +0x60) for a CompleteTask whose taskId
                // (+0x30) == work+0x20. Dump the list size + the work taskId so we can tell
                // "chain never pushed" (count 0) from "pushed but state machine won't run it".
                if (GRM_Trace()) {
                    uintptr_t cz = *(uintptr_t*)entityIBP;
                    uintptr_t aic = cz ? *(uintptr_t*)(cz + 0x278) : 0;
                    int actCount = aic ? *(int*)(aic + 0x60) : -999;
                    void* actList = aic ? *(void**)(aic + 0x48) : nullptr;
                    std::fprintf(stderr, "[GRMExec] CHAIN-DIAG citizen=%p citizenAIComp=%p actList=%p actCount(+0x60)=%d workTaskId(self+0x20)=%d citEnt+0x30=%d\n",
                                 (void*)cz, (void*)aic, actList, actCount, (int)selfQ[4], *(int32_t*)(citizenFinal + 0x30));
                    std::fflush(stderr);
                }
                return 2;
            }
        }

    next_slot:
        intOffset += 4;
        slotOffset += 0x10;
    }

    GRM_TRACEF("EXIT: no work enqueued (all slots skipped) return 1\n");
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

    GRM_TRACEF("OnComplete self=%p slot=%u amount=%d -> field_58[slot]=%d field_48[slot]=%d\n",
               (void*)self, slotIdx, amount, *buildCountPtr, *assignCountPtr);

    return (uintptr_t)selfQ[9] & 0xFFFFFF00;  // Original returns uVar3 & 0xffffffffffffff00
}

// ============================================================
// OnCancel (0x1400ebdb0) -- vtable slot 7
//
// Cancel handler. Removes from tree, decrements build count only
// (does NOT increment assigned count).
// ============================================================
int GatherRawMaterials_OnCancel_Impl(GatherRawMaterials* self, void* param2,
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
    GRM_TRACEF("OnCancel self=%p slot=%u amount=%d -> field_58[slot]=%d (NO field_48 credit)\n",
               (void*)self, slotIdx, amount, *buildCountPtr);
    return 0;
}

// ============================================================
// Cleanup (0x1400ec480) -- vtable slot 8
//
// Release all assigned citizens from tree at +0x68.
// Walks tree from leftmost node, for each citizen calls
// CitizenAI::StateMachineRelease.
// ============================================================
int GatherRawMaterials_Cleanup_Impl(GatherRawMaterials* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    while (true) {
        uintptr_t root = *(uintptr_t*)(selfAddr + 0x68);
        if (root == 0) return 0;

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
        if (node == 0) return 0;

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
                            fn_MemFreeStatic(invalidArray);
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
            GRM_TRACEF("Validate self=%p REMOVED %u invalid task(s) -> field_58[0..%d] RESET to 0\n",
                       (void*)self, invalidCount, numSlots - 1);
        }
    }

    fn_MemFreeStatic(invalidArray);
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


// ============================================================
// GatherRawMaterials::DtorBody (0x1400ec650)
// Sets vtable to GatherRawMaterials, frees BST at +0x68,
// frees int arrays at +0x58 and +0x48, sets vtable to Work,
// then calls Object::Internal::~Internal.
// ============================================================
void GatherRawMaterials_DtorBody(GatherRawMaterials* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to GatherRawMaterials
    *(void**)selfAddr = s_vtable_GatherRawMaterials;

    // Free BST nodes at +0x68
    void* bstRoot = *(void**)(selfAddr + 0x68);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x68), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x68), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        fn_MemFreeStatic(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x68) = nullptr;
    *(uint32_t*)(selfAddr + 0x70) = 0;

    // Free second int array at +0x58
    fn_MemFreeStatic(*(void**)(selfAddr + 0x58));
    *(void**)(selfAddr + 0x58) = nullptr;
    *(uint64_t*)(selfAddr + 0x60) = 0;

    // Free first int array at +0x48
    fn_MemFreeStatic(*(void**)(selfAddr + 0x48));
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint64_t*)(selfAddr + 0x50) = 0;

    // Set vtable to Work (no AutoPickupWork in GatherRawMaterials' chain)
    *(void**)selfAddr = s_vtable_Work;

    // Call Object::Internal::~Internal (base class cleanup)
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// GatherRawMaterials::ScalarDtor (0x14002a410)
// Calls dtor body, then conditional free.
// ============================================================
void* GatherRawMaterials_ScalarDtor_Impl(GatherRawMaterials* self, uint32_t flags) {
    GatherRawMaterials_DtorBody(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161420 (13 slots), size 0x78, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// FLAGS:
//   - slot0  ScalarDtor : GatherRawMaterials_ScalarDtor_Impl  (present)
//   - slot3  Serialize  : GatherRawMaterials_Serialize_Impl
//   - slot4  Execute    : GatherRawMaterials_Execute_Impl (4-arg: self, entityIBP, p3, p4)
//   - slot6  OnComplete : GatherRawMaterials_OnComplete_Impl
//   - slot7  OnCancel   : GatherRawMaterials_OnCancel_Impl
//   - slot8  Cleanup    : GatherRawMaterials_Cleanup_Impl
//   - slot9  Validate   : GatherRawMaterials_Validate_Impl
//   - slot10 GetTarget  : GatherRawMaterials_GetTarget_Impl
//   - slot11 GetPriority: NO GatherRawMaterials_GetPriority_Impl defined -> /*FLAG*/ nullptr
//   - Ctor              : NO GatherRawMaterials_Ctor_Impl defined (only exe addr
//                         Ctor 0x1400ec8a0) -> factory cannot construct ->
//                         /*FLAG*/ returns null until a Ctor_Impl is written.
//   - ResolveFuncs      : GatherRawMaterials_ResolveFuncs() (does NOT resolve a vtable).
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t GatherRawMaterials_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("GatherRawMaterials");
    return c;
}

static void* g_GatherRawMaterials_Vtable[13] = {
    (void*)&GatherRawMaterials_ScalarDtor_Impl, // +0x00 ScalarDtor
    (void*)&Work_Slot1,                         // +0x08 slot1 (null)
    (void*)&GatherRawMaterials_GetTypeId,       // +0x10 GetTypeId
    (void*)&GatherRawMaterials_Serialize_Impl,  // +0x18 Serialize
    (void*)&GatherRawMaterials_Execute_Impl,    // +0x20 Execute
    (void*)&Work_Slot5,                         // +0x28 slot5 (returns 0)
    (void*)&GatherRawMaterials_OnComplete_Impl, // +0x30 OnComplete
    (void*)&GatherRawMaterials_OnCancel_Impl,   // +0x38 OnCancel
    (void*)&GatherRawMaterials_Cleanup_Impl,    // +0x40 Cleanup
    (void*)&GatherRawMaterials_Validate_Impl,   // +0x48 Validate
    (void*)&GatherRawMaterials_GetTarget_Impl,  // +0x50 GetTarget
    (void*)&Work_SlotRet0,                           // +0x58 GetPriority (NO _GetPriority_Impl)
    (void*)&Work_Slot12,                        // +0x60 slot12 (void nop)
};

extern "C" void RegisterGatherRawMaterialsVtable() {
    RegisterExeAddr(0x140161420, (uintptr_t)g_GatherRawMaterials_Vtable);
}
extern "C" void* GatherRawMaterials_CreateInPlace(void* mem) {
    GatherRawMaterials_ResolveFuncs();
    return Work_GenericCtor(mem, 0x78, g_GatherRawMaterials_Vtable);
}
extern "C" void* GatherRawMaterials_Factory(void) {
    GatherRawMaterials_ResolveFuncs();
    void* p = Work_Alloc(0x78);
    return p ? Work_GenericCtor(p, 0x78, g_GatherRawMaterials_Vtable) : nullptr;
}
REGISTER_TYPE(GatherRawMaterials, "Work", &GatherRawMaterials_Factory, &GatherRawMaterials_CreateInPlace);

// ---- Construction force-harness wrappers (construction_force_harness.cpp) ----
// Mirror the ForesterWork_Factory/ForceDrive/ExecuteReady trio so the generic
// ForceWork harness can drive construction gathering.
extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP);  // gatherrawmaterials_ctor.cpp

// Build a GatherRawMaterials bound to a construction SITE entity using the FAITHFUL
// site ctor (seeds per-material slots from the site's BuildDescription + the amount
// already in the site StorageComponent) rather than the generic factory. buildEntityIBP
// is an InternalBasePtr* whose *value is the site entity pointer.
extern "C" void* GatherRawMaterials_ConstructForSite(void* buildEntityIBP) {
    GatherRawMaterials_ResolveFuncs();
    void* mem = Work_Alloc(0x78);
    if (!mem) return nullptr;
    return GatherRawMaterials_Ctor_Impl(mem, buildEntityIBP);
}

// Drive Execute with the WORK-DRIVER ABI (self, citizenIBP). Execute's param3 is a
// scratch WpExt out-IBP (written internally) and param4 is unused, so 0/0 is safe.
extern "C" int GatherRawMaterials_ForceDrive(void* self, void* citizenIBP) {
    return GatherRawMaterials_Execute_Impl(reinterpret_cast<GatherRawMaterials*>(self),
                                           citizenIBP, 0, 0);
}

// Execute-readiness gate for the force harness. GatherRawMaterials_ResolveFuncs() also
// requires the SERIALIZE-only fns (Work_Serialize / Serializer_* / SerializeInt /
// SerializeIntBulk / TreeSerialize) and the dtor-only parent vtable — none of which
// Execute or its pushed action chain touch, and SerializeIntBulk's IAT dispatch slot
// (0x1401528c0) is unpopulated in standalone, which would falsely block the driver. So we
// resolve everything, then gate on only the Execute-critical subset (mirrors
// ForesterWork_ExecuteReady). Serialize never runs during standalone gameplay.
extern "C" bool GatherRawMaterials_DepsReady() {
    GatherRawMaterials_ResolveFuncs();   // populate all resolvable fns
    return s_fn_ResizeIntArray && s_fn_TreeRemoveByKey && s_fn_TreeRemoveNode &&
           s_fn_StateMachineRelease && s_fn_CanStartWork &&
           s_fn_FindComp_Position && s_fn_FindComp_Unk && s_fn_FindComp_WpExt &&
           s_fn_FindStorageForResource && s_fn_FindComp_Storage &&
           s_fn_StorageQueryCount && s_fn_GetRemainingCapacity && s_fn_LongTermPlan &&
           s_fn_CalcBestTargetPos && s_fn_CalcChildTargetPos && s_fn_ValidateWorkAssign &&
           s_fn_PathfindingFindPath && s_fn_RegisterWork && s_fn_MemAlloc && s_fn_MemFree &&
           s_fn_TreeNodeCleanup && s_vtable_GatherRawMaterials &&
           fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom && g_pGameWorldSingleton &&
           s_typeId_GatherDesc && s_typeId_RawMatComp && s_typeId_PosDesc;
}
