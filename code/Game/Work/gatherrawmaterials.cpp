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
#include "Work/exe_bodies.h"
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
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"          // B3R: bodies called by name
#include "Component/exe_bodies.h"       // B3R: bodies called by name
#include "Dialog/dialog_exe_helpers.h"  // B3R: Dialog_FindComponent_d6c0
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "Component/BuildDescription.h"   // ENDGAME B.3.8 CB3: BuildDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/InteractDescription.h"   // ENDGAME B.3.9 D5: InteractDescription::TypeId()


// ============================================================
// Sub-functions, called by name (B3R; Work/Action/Component/Dialog headers)
// ============================================================
//   0x1400296b0 simple-BST serialize (CitizenAI_impl_96b0)  WorkBST_BST_Serialize_296b0
//               (0x140025630, used earlier, is MID-FUNCTION inside TradeUI_impl_55a0;
//                GatherRawMaterials_Shutdown@0x1400ec62a CALLs 0x1400296b0)
//   0x140025520 tree remove by key                          WorkBST_BSTFindRemove_25520
//   0x14001a6e0 tree remove node (CitizenAI_impl_a6e0)      WorkBST_BSTRemove_a6e0
//               (0x140024f30, used earlier, is MID-FUNCTION inside Toolbar_vslot14;
//                GatherRawMaterials_Update@0x1400ebd24 CALLs 0x14001a6e0)
//   0x1400c5400 state machine release                       WorkBase_TaskListCleanup_c5400
//   0x1400f7100 Work::CanStartWork                          WorkBase_ValidateTask_f7100
//   0x140002ed0 Entity_FindComponent_Position               WorkBase_FindComponent_2ed0
//   0x14000d6c0 FindComponent (BuildComponent)              Dialog_FindComponent_d6c0
//   0x140002e10 FindComponent_WorkplaceExt                  AttendanceComponent_GetJobHostComponent
//   0x14008d590 FindStorageForResource                      Pf_PickBestDropTarget_8d590
//   0x140006270 Entity_FindComponent_Storage                WorkBase_FindComponent_6270
//   0x14012cf10 StorageComponent_QueryCount                 CitizenComp_StorageComponent_QueryCount
//   0x140116540 GetRemainingCapacity                        WorkLeaf_FreeStorageCapacity_116540
//   0x140115e60 CitizenAI_State_LongTermPlan                CitizenAI_State_LongTermPlan
//   0x140122be0 CalcBestTargetPos                           WorkImpl_WalkablePos_122be0
//   0x140122b00 CalcChildTargetPos                          WorkImpl_WalkablePosRecursive_122b00
//   0x1400f6ef0 BuildStructure_impl_6ef0 validate           WorkImpl_BuildStructureStep_6ef0
//   0x1400bf790 Pathfinding_FindPath                        WorkAct_GatherRawMatCtor_bf790
//   0x140021a30 RegisterWork (CitizenAI_impl_1a30)          WorkBST_RegisterWork_1a30
//   0x140014c30 tree node cleanup                           WorkBase_TreeNodeFree_14c30
//   MemAlloc / MemFree                                      System::MemoryAllocator::MemAlloc / MemFree
// B3R_RULES R4: the old code resolved MemAlloc/MemFree by export name and fell back to
// import slots 0x1401527e8 / 0x1401527f0 under the WRONG names (they are the
// Particle::_rng data import and Vector3::Lerp). The export was always found, so the
// fallback never ran; it is deleted, never registered.
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: GatherRawMaterials_Ctor
// disassembles to `0x1400ec8bd: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// (0x1401612e0, used earlier, is an INTERIOR address of FireWork's vtable.)

// Gated Execute-decision trace (BANISHED_BUILD_TRACE), to pinpoint where a construction
// gather work bails before enqueuing a real town->site haul. Diagnostic only.
#include <cstdlib>
#include <cstring>
#include <new>

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
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
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
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(selfAddr + 0x68), serializerBuf);

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

    // --- Find PositionComponent on work entity (self+0x10) ---
    uintptr_t posCompIBP = 0;
    WorkBase_FindComponent_2ed0(selfQ[2], &posCompIBP);

    // --- Find description component on work entity ---
    uintptr_t descCompIBP = 0;
    Dialog_FindComponent_d6c0(reinterpret_cast<void*>(selfQ[2]), &descCompIBP);

    // Get GatherDescription from desc component +0x18
    void* descBase = *(void**)(descCompIBP + 0x18);
    bool isDerived = Object::IsDerivedFromNullSafe(descBase, BuildDescription::TypeId());
    void* gatherDesc = isDerived ? descBase : nullptr;

    // --- Check workplace enable flag via WpExt component ---
    uintptr_t descComp = descCompIBP;
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(*(uintptr_t*)(descComp + 0x10)), (void*)&param3);
    void* wpExtIBP = (void*)&param3;
    if (*(char*)(*(uintptr_t*)wpExtIBP + 0x50) == '\0') {
        return 1;  // Workplace disabled
    }

    // --- Get game world data ---
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
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

            Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(worldEntityList), reinterpret_cast<int64_t*>(&foundEntity), citizenCell,
                *(uint64_t*)(slotOffset + matDescBase), 1, '\0', reinterpret_cast<uint32_t*>(searchPos),
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
                WorkBase_FindComponent_6270(*(uintptr_t*)(citizenEnt + 0x10), &citizenStorageIBP);

                int capacity = *(int*)(citizenStorageIBP + 0x38);
                int currentStored = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>((void*)citizenStorageIBP));


                int carryWeight = *(int*)((uintptr_t)rawMatComp + 0x48);
                if (capacity - currentStored < carryWeight * remaining) {
                    // Not enough carry capacity -- recalculate
                    int remCapacity = WorkLeaf_FreeStorageCapacity_116540(*(uintptr_t*)entityIBP);
                    remaining = remCapacity / carryWeight;
                }

                if (remaining == 0) {
                    // Can't carry anything -- replan
                    CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*(void**)entityIBP));
                    return 2;
                }

                // --- Find storage on the found entity and match material slot ---
                uintptr_t storageCompIBP2 = 0;
                void* retIBP = WorkBase_FindComponent_6270((uintptr_t)foundEntity, &storageCompIBP2);

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
                bool isDerivedPos = Object::IsDerivedFromNullSafe(posDescBase, InteractDescription::TypeId());
                void* posDesc = isDerivedPos ? posDescBase : nullptr;
                uint8_t found = posDesc ? WorkImpl_WalkablePos_122be0(posCompIBP,
                    (uintptr_t)posDesc + 0x38, searchPos, targetPos, &bestDist) : 0;

                void* posComp2 = (void*)posCompIBP;
                if (*(int*)((uintptr_t)posComp2 + 0x30) > 0) {
                    uintptr_t childOffset = 0;
                    int childCount = 0;
                    do {
                        uintptr_t* childPtr = (uintptr_t*)(*(uintptr_t*)((uintptr_t)posComp2 + 0x28) + childOffset);
                        uint8_t childFound = WorkImpl_WalkablePosRecursive_122b00(*childPtr, 1,
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
                uint64_t validated = WorkImpl_BuildStructureStep_6ef0(selfAddr, &citizenRef,
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

                WorkAct_GatherRawMatCtor_bf790(
                    pathBuf,            // stack buffer (original: local_58[3])
                    &citizenPtr,        // citizenPtr
                    (int)selfQ[4],      // pathType (priority/workplaceId)
                    reinterpret_cast<uint64_t*>(&mapGridPtr),        // mapGridPtr
                    targetPos,          // targetPos
                    reinterpret_cast<void**>(&destEntity),        // destEntityPtr
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

                WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x68)), workEntIBP, entityIBP);

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
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x68)), param2);

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
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x68)), param2);

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
        WorkBase_TaskListCleanup_c5400((void*)(citizenAI + 0x48));
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

            char valid = WorkBase_ValidateTask_f7100((void*)this, nullptr, taskIBP, nullptr);

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
                        void* newArray = System::MemoryAllocator::MemAlloc((int64_t)(int)newCap << 3, 8);

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
                            System::MemoryAllocator::MemFree(invalidArray);
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
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(selfAddr + 0x68), (void*)treeNode);
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

    System::MemoryAllocator::MemFree(invalidArray);
    return (invalidCount == 0) ? 1 : 0;
}

// ============================================================
// GetTarget (0x1400ec510) -- vtable slot 10
//
// Returns target position from game world singleton.
// Hidden return ptr: RCX=self, RDX=retbuf.
// ============================================================
void* GatherRawMaterials::GetTarget(void* retbuf) {
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
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
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x68), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x68), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        System::MemoryAllocator::MemFree(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x68) = nullptr;
    *(uint32_t*)(selfAddr + 0x70) = 0;

    // Free second int array at +0x58
    System::MemoryAllocator::MemFree(*(void**)(selfAddr + 0x58));
    *(void**)(selfAddr + 0x58) = nullptr;
    *(uint64_t*)(selfAddr + 0x60) = 0;

    // Free first int array at +0x48
    System::MemoryAllocator::MemFree(*(void**)(selfAddr + 0x48));
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint64_t*)(selfAddr + 0x50) = 0;

}



// Slot11 (+0x58) — retail 0x14002a400 is `return 0 < *(int*)(self + 0x70)` (this class
// keeps its tree count at +0x70). Was pinned to 0 by Work_SlotRet0 (2026-09-05).
int GatherRawMaterials::GetPriority() {
    return field_70 > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
GatherRawMaterials::GatherRawMaterials()
    : Work(), field_48(nullptr), field_50(0), field_54(0), field_58(nullptr),
      field_60(0), field_64(0), field_68(nullptr), field_70(0) {
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

// Execute-readiness gate for the force harness. B3R: every helper Execute and its pushed
// action chain use is called by name, so only the game world is left to test.
extern "C" bool GatherRawMaterials_DepsReady() {
    return GameUpdateLoop::_singleton != nullptr;
}
