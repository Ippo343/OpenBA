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

#include "options.h"
#include "Work/GatherRawMaterials.h"
#include "Work/gatherrawmaterials_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "Component/BuildDescription.h"   // ENDGAME B.3.8 CB3: BuildDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/InteractDescription.h"   // ENDGAME B.3.9 D5: InteractDescription::TypeId()


// ============================================================
// Local function pointers resolved at init time
// (only those NOT already declared in engine_funcs.h)
// All typedefs prefixed with GRM_ to avoid conflicts.
// ============================================================


// 0x1400021d0 (int-array SetSize) is the exe-side Collection::Array<int32>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its two call sites below to
// GameColl::AsArray<GameColl::I32NoInit> member calls, so its resolved-pointer stand-in
// is gone.

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


// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: GatherRawMaterials_Ctor
// disassembles to `0x1400ec8bd: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.

static bool s_resolved = false;

// Gated Execute-decision trace (BANISHED_BUILD_TRACE), to pinpoint where a construction
// gather work bails before enqueuing a real town->site haul. Diagnostic only.
#include <cstdlib>
#include <cstring>
#include <new>

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool GatherRawMaterials_ResolveFuncs() {
    if (s_resolved) return true;

    // Direct exe function addresses
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

    // MemAlloc/MemFree — resolve via the canonical Runtime exports (static size/align &
    // ptr forms). The exe IAT dispatch slots (0x1401527e8/0x1401527f0) are UNREGISTERED in
    // the standalone exe AND mislabelled (they index the _rng/Lerp imports, not
    // MemoryAllocator) — see ForesterWork's identical fix. Without this s_fn_MemAlloc stays
    // NULL and GatherRawMaterials::Validate crashes (EXEC of 0) at its removal-array grow
    // when a citizen's Idle-state work-tree walk validates the posted gather work.
    // ResolveExport reads the Runtime DLL exports directly, which is correct for the standalone exe.
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

    // ENDGAME B.3.9 D5: 0x1401c7040 (BuildDescription), 0x1401c7cb8
    // (RawMaterialDescription) and 0x1401c7938 (InteractDescription -- the healing
    // fallback here used to read MapDescription::TypeId(), a pre-existing bug this
    // batch also fixes) are registered type-id cells; each class's own TypeId() is
    // always available, no separate resolve/heal to gate on.

    bool ok = s_fn_TreeSerialize &&
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
              fn_IsDerivedFrom &&
              g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[GatherRawMaterials] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
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
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
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
void GatherRawMaterials::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create IO::Serializer with version 0
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Check if loading (stream+8 == 0 means loading)
    bool isLoading = (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)

    // --- Serialize first int array count (+0x50) ---
    int count50 = field_50;
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&count50), nullptr);   // B.3.4 bridge

    if (isLoading) {
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
        // GatherRawMaterials_Shutdown calls it at 1400ec5be). I32NoInit — a grown tail
        // is left as the allocation found it, like retail. Safe: load-only, and the bulk
        // int read below writes all count50 elements from the stream.
        GameColl::AsArray<GameColl::I32NoInit>((void*)(selfAddr + 0x48)).SetSize(count50);
        field_50 = count50;
    }

    // Bulk-serialize the first int array
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(field_50, reinterpret_cast<int*>(field_48), nullptr);   // B.3.4 bridge

    // --- Serialize second int array count (+0x60) ---
    int count60 = field_60;
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&count60), nullptr);   // B.3.4 bridge

    if (isLoading) {
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
        // GatherRawMaterials_Shutdown calls it at 1400ec606). I32NoInit — a grown tail
        // is left as the allocation found it, like retail. Safe: load-only, and the bulk
        // int read below writes all count60 elements from the stream.
        GameColl::AsArray<GameColl::I32NoInit>((void*)(selfAddr + 0x58)).SetSize(count60);
        field_60 = count60;
    }

    // Bulk-serialize the second int array
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(field_60, reinterpret_cast<int*>(field_58), nullptr);   // B.3.4 bridge

    // --- Serialize tree at +0x68 ---
    s_fn_TreeSerialize((void*)(selfAddr + 0x68), serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
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
int GatherRawMaterials::Execute(void* entityIBP, void* context) {
    // retail's param3 (R8) is a by-value scratch slot whose address the body takes
    uint64_t param3 = reinterpret_cast<uint64_t>(context);
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Ensure the exe-helper function pointers are resolved. The factory path calls
    // ResolveFuncs at construction, but the construction gather work is built via
    // GatherRawMaterials_Ctor_Impl (state-1 spawner), which does not — leaving every
    // s_fn_* NULL and the first `call *s_fn_FindComp_Position` faulting at RIP=0.
    GatherRawMaterials_ResolveFuncs();

    // --- Find PositionComponent on work entity (self+0x10) ---
    uintptr_t posCompIBP = 0;
    s_fn_FindComp_Position(selfQ[2], &posCompIBP);

    // --- Find description component on work entity ---
    uintptr_t descCompIBP = 0;
    s_fn_FindComp_Unk(selfQ[2], &descCompIBP);

    // Get GatherDescription from desc component +0x18
    void* descBase = *(void**)(descCompIBP + 0x18);
    bool isDerived = fn_IsDerivedFrom(descBase, BuildDescription::TypeId());
    void* gatherDesc = isDerived ? descBase : nullptr;

    // --- Check workplace enable flag via WpExt component ---
    uintptr_t descComp = descCompIBP;
    void* wpExtIBP = s_fn_FindComp_WpExt(*(uintptr_t*)(descComp + 0x10), (void*)&param3);
    if (*(char*)(*(uintptr_t*)wpExtIBP + 0x50) == '\0') {
        return 1;  // Workplace disabled
    }

    // --- Get game world data ---
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void* worldEntityList = *(void**)(gameWorld + 0xf0);

    // --- Get citizen position for cell calculation ---
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;
    uintptr_t citizenAIComp = *(uintptr_t*)(citizenEntity + 0x278);
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
            void* rawMatComp = FindRawMaterialInList(matDescEntity, RawMaterialDescription::TypeId());

            // --- Search for a storage entity with this resource ---
            uintptr_t foundEntity = 0;
            float searchPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            uintptr_t excludeIBP = 0;  // Original: local null-initialized IBP (NOT &self->field_10)

            s_fn_FindStorageForResource(worldEntityList, &foundEntity, citizenCell,
                *(uint64_t*)(slotOffset + matDescBase), 1, '\0', searchPos,
                (void*)&excludeIBP);

            if (foundEntity == 0) {
                // No storage found for this slot
                goto next_slot;
            }

            {
                // --- Calculate how much to gather ---
                int remaining = targetAmount - buildCount - assignCount;

                // Check citizen's carrying capacity
                uintptr_t citizenEnt = *(uintptr_t*)entityIBP;
                uintptr_t citizenStorageIBP = 0;
                s_fn_FindComp_Storage(*(uintptr_t*)(citizenEnt + 0x10), &citizenStorageIBP);

                int capacity = *(int*)(citizenStorageIBP + 0x38);
                int currentStored = s_fn_StorageQueryCount((void*)citizenStorageIBP);


                int carryWeight = *(int*)((uintptr_t)rawMatComp + 0x48);
                if (capacity - currentStored < carryWeight * remaining) {
                    // Not enough carry capacity -- recalculate
                    int remCapacity = s_fn_GetRemainingCapacity(*(uintptr_t*)entityIBP);
                    remaining = remCapacity / carryWeight;
                }

                if (remaining == 0) {
                    // Can't carry anything -- replan
                    s_fn_LongTermPlan(*(void**)entityIBP);
                    return 2;
                }

                // --- Find storage on the found entity and match material slot ---
                uintptr_t storageCompIBP2 = 0;
                void* retIBP = s_fn_FindComp_Storage((uintptr_t)foundEntity, &storageCompIBP2);

                uintptr_t storageComp = *(uintptr_t*)retIBP;
                int storageSlotCount = *(int*)(storageComp + 0x30);
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

                void* posDescBase = *(void**)((uintptr_t)posCompIBP + 0x18);
                int bestDist = 0x7fffffff;

                // DAT_1401c7938 = InteractDescription (ENDGAME B.3.9 D5: by cell -- the
                // registered cell's own class, not the legacy healing fallback which used
                // to read MapDescription::TypeId()).
                bool isDerivedPos = fn_IsDerivedFrom(posDescBase, InteractDescription::TypeId());
                void* posDesc = isDerivedPos ? posDescBase : nullptr;
                uint8_t found = posDesc ? s_fn_CalcBestTargetPos(posCompIBP,
                    (uintptr_t)posDesc + 0x38, searchPos, targetPos, &bestDist) : 0;

                void* posComp2 = (void*)posCompIBP;
                if (*(int*)((uintptr_t)posComp2 + 0x30) > 0) {
                    uintptr_t childOffset = 0;
                    int childCount = 0;
                    do {
                        uintptr_t* childPtr = (uintptr_t*)(*(uintptr_t*)((uintptr_t)posComp2 + 0x28) + childOffset);
                        uint8_t childFound = s_fn_CalcChildTargetPos(*childPtr, 1,
                            searchPos, targetPos, &bestDist);
                        childCount++;
                        found |= childFound;
                        childOffset += 8;
                    } while (childCount < *(int*)((uintptr_t)posComp2 + 0x30));
                }

                if (found == 0) {
                    // CalcBestTargetPos (0x140122be0) walks posDesc+0x38: {ptr, count} of
                    // local-space access offsets, and returns false when the count is 0 or
                    // every offset fails the 0xe1f walkability probe. The child loop above
                    // is the multi-part fallback (posComp+0x28/+0x30). Log both counts so a
                    // "site is unreachable" bail is distinguishable from "site has no
                    // access points at all".
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
                uintptr_t citizenFinal = *(uintptr_t*)entityIBP;

                // Call vtable slot +0x50 (GetWorkEntity)
                uintptr_t workEntIBP[2] = {0};
                uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
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
int GatherRawMaterials::OnComplete(void* param2, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    uintptr_t selfAddr = (uintptr_t)this;

    // Remove entity from tree at +0x68
    s_fn_TreeRemoveByKey((void*)(selfAddr + 0x68), param2);

    // Extract slot index (low byte) and amount (high 24 bits, shifted right by 8)
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] (build counts) by amount
    uintptr_t* selfQ = (uintptr_t*)this;
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
int GatherRawMaterials::OnCancel(void* param2, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    (void)this; // selfAddr used below via cast
    uintptr_t selfAddr = (uintptr_t)this;

    // Remove entity from tree at +0x68
    s_fn_TreeRemoveByKey((void*)(selfAddr + 0x68), param2);

    // Extract slot index and amount
    uintptr_t* selfQ = (uintptr_t*)this;
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] only (cancel = don't credit the work)
    int* buildCountPtr = (int*)(selfQ[0xb] + (uintptr_t)slotIdx * 4);
    *buildCountPtr -= amount;
    return 0;
}

// ============================================================
// Cleanup (0x1400ec480) -- vtable slot 8
//
// Release all assigned citizens from tree at +0x68.
// Walks tree from leftmost node, for each citizen calls
// CitizenAI::StateMachineRelease.
// ============================================================
int GatherRawMaterials::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;

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
        uintptr_t citizenEntity = *(uintptr_t*)node;

        // Release citizen's state machine via +0x278 -> +0x48
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
int GatherRawMaterials::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

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

            char valid = s_fn_CanStartWork((void*)this, nullptr, taskIBP, nullptr);

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
                uintptr_t nodeKey = *(uintptr_t*)treeNode;
                if (searchKey < nodeKey) {
                    treeNode = *(uintptr_t*)(treeNode + 0x10);
                } else {
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
            Cleanup();   // slot 8 (vtable +0x40), a virtual dispatch in retail

            // Reset all build counts to 0
            int numSlots = (int)selfQ[0xc]; // field_60 count
            for (int i = 0; i < numSlots; i++) {
                *(int*)(selfQ[0xb] + (uintptr_t)i * 4) = 0;
            }
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
void* GatherRawMaterials::GetTarget(void* retbuf) {
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
// GatherRawMaterials dtor 0x1400ec650. The stamps and the `~Internal` call are the compiler's chain.
GatherRawMaterials::~GatherRawMaterials() {
    uintptr_t selfAddr = (uintptr_t)this;

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

}


// ============================================================
// GatherRawMaterials::ScalarDtor (0x14002a410)
// Calls dtor body, then conditional free.
// ============================================================
// [0] 0x14002a410: ~GatherRawMaterials, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* GatherRawMaterials::DeletingDtor(unsigned flags) {
    this->~GatherRawMaterials();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}


// Slot11 (+0x58) — retail 0x14002a400 is `return 0 < *(int*)(self + 0x70)` (this class
// keeps its tree count at +0x70). Was pinned to 0 by Work_SlotRet0 (2026-09-05).
int GatherRawMaterials::GetPriority() {
    return field_70 > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's. GatherRawMaterials_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
GatherRawMaterials::GatherRawMaterials()
    : Work(), field_48(nullptr), field_50(0), field_54(0), field_58(nullptr),
      field_60(0), field_64(0), field_68(nullptr), field_70(0) {
    GatherRawMaterials_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_GatherRawMaterials_Vtable` (slot 1 null, slot 2 a
// lazily cached id) and REGISTER_TYPE over GatherRawMaterials_Factory / _CreateInPlace
// (both Work_GenericCtor). Nothing outside this block reads 0x140161420 back (sweep by
// hex and by GatherRawMaterialsAddr::Vtable; the site ctor that stamped it is the real
// ctor now), so its registration goes with the table.

// create 0x14002a3c0: MemAlloc(sizeof, 8), then the ctor.
void* GatherRawMaterials::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(GatherRawMaterials), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) GatherRawMaterials());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* GatherRawMaterials::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) GatherRawMaterials());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(GatherRawMaterials, "Work")

// ---- Construction force-harness wrappers (construction_force_harness.cpp) ----
// Mirror the ForesterWork_Factory/ForceDrive/ExecuteReady trio so the generic
// ForceWork harness can drive construction gathering.

// Build a GatherRawMaterials bound to a construction SITE entity using the FAITHFUL
// site ctor (seeds per-material slots from the site's BuildDescription + the amount
// already in the site StorageComponent) rather than the generic factory. buildEntityIBP
// is an InternalBasePtr* whose *value is the site entity pointer.
extern "C" void* GatherRawMaterials_ConstructForSite(void* buildEntityIBP) {
    void* mem = Object::detail::InternalMemAlloc(sizeof(GatherRawMaterials), 8);
    if (!mem) return nullptr;
    return static_cast<Object::Base*>(new (mem) GatherRawMaterials(buildEntityIBP));
}

// Drive Execute with the WORK-DRIVER ABI (self, citizenIBP). Execute's param3 is a
// scratch WpExt out-IBP (written internally) and param4 is unused, so 0/0 is safe.
extern "C" int GatherRawMaterials_ForceDrive(void* self, void* citizenIBP) {
    return reinterpret_cast<GatherRawMaterials*>(self)->GatherRawMaterials::Execute(citizenIBP, nullptr);   // B.3.8 bridge
}

// Execute-readiness gate for the force harness. GatherRawMaterials_ResolveFuncs() also
// requires the SERIALIZE-only fns (Work_Serialize / Serializer_* / SerializeInt /
// SerializeIntBulk / TreeSerialize) and the dtor-only parent vtable — none of which
// Execute or its pushed action chain touch. (The dispatch slot for SerializeIntBulk was
// recorded here as 0x1401528c0, which is CreateElement; it is 0x140151278.) So we
// resolve everything, then gate on only the Execute-critical subset (mirrors
// ForesterWork_ExecuteReady). Serialize never runs during standalone gameplay.
extern "C" bool GatherRawMaterials_DepsReady() {
    GatherRawMaterials_ResolveFuncs();   // populate all resolvable fns
    return s_fn_TreeRemoveByKey && s_fn_TreeRemoveNode &&
           s_fn_StateMachineRelease && s_fn_CanStartWork &&
           s_fn_FindComp_Position && s_fn_FindComp_Unk && s_fn_FindComp_WpExt &&
           s_fn_FindStorageForResource && s_fn_FindComp_Storage &&
           s_fn_StorageQueryCount && s_fn_GetRemainingCapacity && s_fn_LongTermPlan &&
           s_fn_CalcBestTargetPos && s_fn_CalcChildTargetPos && s_fn_ValidateWorkAssign &&
           s_fn_PathfindingFindPath && s_fn_RegisterWork && s_fn_MemAlloc && s_fn_MemFree &&
           s_fn_TreeNodeCleanup && fn_IsDerivedFrom && g_pGameWorldSingleton;
}
