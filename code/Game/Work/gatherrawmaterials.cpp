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
#include "object/Internal.h"
#include "object/TypeInfo.h"
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
#include "Component/InteractComponent.h"
#include "Component/BuildComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/StorageComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Component/ComponentDescription.h"
#include "Component/Description.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"
#include "Game/Map.h"
#include "Game/ProfessionList.h"
#include "Action/PathSearch.h"   // RawArray (PickBestDropTarget)


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
//   0x1400f7100 Work::CanStartWork                          Work::ValidateTask
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
// Helper: the RawMaterialDescription among a ComponentDescription's sub-descriptions
// (+0x28 data / +0x30 count) -- the inlined scan retail repeats (no CheckPtr).
// ============================================================
static RawMaterialDescription* FindRawMaterialInList(ComponentDescription* entity) {
    const uint32_t typeId = RawMaterialDescription::TypeId();
    int count = entity->_subDescCount;
    for (int i = 0; i < count; i++) {
        Description* obj = entity->_subDescs[i];
        if (obj->Type() == typeId) {
            Description* comp = entity->_subDescs[i];
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
                return static_cast<RawMaterialDescription*>(comp);
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Serialize (0x1400ec560)
// Work::Serialize + 2 int arrays + tree serialization
// ============================================================
void GatherRawMaterials::Persist(IO::Stream& stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create IO::Serializer with version 0
    IO::Serializer serializer(stream, 0);

    // Check if loading (stream+8 == 0 means loading)
    bool isLoading = (static_cast<int>(serializer.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210

    // --- Serialize first int array count (+0x50) ---
    int count50 = field_50;
    serializer.Serialize(count50, nullptr);

    if (isLoading) {
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
        // GatherRawMaterials_Shutdown calls it at 1400ec5be). I32NoInit — a grown tail
        // is left as the allocation found it, like retail. Safe: load-only, and the bulk
        // int read below writes all count50 elements from the stream.
        GameColl::AsArray<GameColl::I32NoInit>(&field_48).SetSize(count50);
        field_50 = count50;
    }

    // Bulk-serialize the first int array
    serializer.Serialize(field_50, field_48, nullptr);

    // --- Serialize second int array count (+0x60) ---
    int count60 = field_60;
    serializer.Serialize(count60, nullptr);

    if (isLoading) {
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
        // GatherRawMaterials_Shutdown calls it at 1400ec606). I32NoInit — a grown tail
        // is left as the allocation found it, like retail. Safe: load-only, and the bulk
        // int read below writes all count60 elements from the stream.
        GameColl::AsArray<GameColl::I32NoInit>(&field_58).SetSize(count60);
        field_60 = count60;
    }

    // Bulk-serialize the second int array
    serializer.Serialize(field_60, field_58, nullptr);

    // --- Serialize tree at +0x68 (the helper takes the container as uintptr_t*: LEFT) ---
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(&field_68), &serializer);   // LEFT: SIG WorkBST_BST_Serialize_296b0
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
int GatherRawMaterials::Execute(CitizenComponent** entityIBP, System::Time* context) {
    // retail's param3 (R8) is a by-value scratch slot whose address the body takes:
    // it becomes the out slot of the WorkPlaceComponent find below, and that value
    // is what 0x1400f6ef0 receives as its fifth argument.
    void* param3 = context;

    // --- Find the InteractComponent on the work entity (self+0x10) (0x140002ed0) ---
    InteractComponent* posComp = field_10->FindComponent<InteractComponent>();

    // --- Find the BuildComponent on the work entity (0x14000d6c0) ---
    BuildComponent* descComp = field_10->FindComponent<BuildComponent>();

    // Its BuildDescription (+0x18)
    Description* descBase = descComp->_description;
    bool isDerived = Object::IsDerivedFromNullSafe(descBase, BuildDescription::TypeId());
    BuildDescription* gatherDesc = isDerived ? static_cast<BuildDescription*>(descBase) : nullptr;

    // --- Check the workplace enable flag on the site's WorkPlaceComponent (0x140002e10,
    // written into param3's slot) ---
    WorkPlaceComponent* workPlace = descComp->_entity->FindComponent<WorkPlaceComponent>();
    param3 = workPlace;
    if (workPlace->isAccepting == 0) {
        return 1;  // Workplace disabled
    }

    // --- Get game world data ---
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    Map* map = gameWorld->m_game;
    CitizenBehavior* worldEntityList = gameWorld->m_citizenList;

    // --- Get citizen position for cell calculation: CharacterComponent (+0x278) ->
    // ModelComponent (+0x68) -> the transform's translation ---
    const Math::Vector3& citizenPos = (*entityIBP)->character->modelComponent->transform.translation;

    int mapSize = map->size;
    int halfMap = mapSize >> 1;
    int cellX = (int)(citizenPos.x + (float)halfMap);
    int cellY = (int)(citizenPos.y + (float)halfMap);
    if (cellX < 0) cellX = 0;
    if (cellX >= mapSize) cellX = mapSize - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= mapSize) cellY = mapSize - 1;
    uint32_t citizenCell = (uint32_t)(cellY * mapSize + cellX);

    // --- Iterate material slots ---
    int numSlots = field_50;
    if (numSlots < 1) {
        return 1;
    }

    for (int slotIdx = 0; slotIdx < numSlots; slotIdx++) {
        // The BuildDescription cost entry for this slot
        const BuildCost* cost = &gatherDesc->_costs[slotIdx];

        // Calculate target amount: material quantity * the BuildComponent's cost multiplier
        int targetAmount = cost->quantity * descComp->costMultiplier;


        // Check: buildCount[slotIdx] + field_48[slotIdx] < targetAmount
        int buildCount = field_58[slotIdx];
        int assignCount = field_48[slotIdx];
        if (buildCount + assignCount >= targetAmount) {
            // This slot is already fulfilled
            goto next_slot;
        }

        {
            // --- Find the RawMaterialDescription of the cost's material ---
            RawMaterialDescription* rawMatComp = FindRawMaterialInList(cost->material);

            // --- Search for a storage entity with this resource ---
            ComponentEntity* foundEntity = nullptr;
            float searchPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            ComponentEntity* excludeIBP = nullptr;  // Original: local null-initialized IBP (NOT &self->field_10)

            PickBestDropTarget(reinterpret_cast<const RawArray<ComponentEntity*>*>(worldEntityList),   // LEFT: CitizenBehavior's sixteen lists are raw triples (ENT, H5)
                &foundEntity, citizenCell,
                cost->material, 1, '\0', searchPos,
                &excludeIBP);   // 0x14008d590

            if (foundEntity == nullptr) {
                // No storage found for this slot
                goto next_slot;
            }

            {
                // --- Calculate how much to gather ---
                int remaining = targetAmount - buildCount - assignCount;

                // Check citizen's carrying capacity (its entity's StorageComponent)
                StorageComponent* citizenStorage = (*entityIBP)->_entity->FindComponent<StorageComponent>();

                int capacity = citizenStorage->capacity;
                // (0x14012cf10 takes the storage as an integer: LEFT.)
                int currentStored = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(citizenStorage));   // LEFT: SIG CitizenComp_StorageComponent_QueryCount


                int carryWeight = rawMatComp->_weight;
                if (capacity - currentStored < carryWeight * remaining) {
                    // Not enough carry capacity -- recalculate (0x140116540 takes the
                    // citizen as an integer: LEFT)
                    int remCapacity = WorkLeaf_FreeStorageCapacity_116540(reinterpret_cast<intptr_t>(*entityIBP));   // LEFT: SIG WorkLeaf_FreeStorageCapacity_116540
                    remaining = remCapacity / carryWeight;
                }

                if (remaining == 0) {
                    // Can't carry anything -- replan
                    (*entityIBP)->LongTermPlan();
                    return 2;
                }

                // --- Find storage on the found entity and match material slot ---
                StorageComponent* storageComp = foundEntity->FindComponent<StorageComponent>();
                int storageSlotCount = storageComp->slotCount;
                int availableAmount = 0;

                // Search storage slots for matching material type
                // R8D=slotIdx starts at 0, increments on miss, used on match
                if (storageSlotCount > 0) {
                    const StorageComponent::Item* slotArray = storageComp->storageSlots;
                    for (int si = 0; si < storageSlotCount; si++) {
                        if (slotArray[si]._resource == cost->material) {
                            if (si >= 0) {  // R8D >= 0 (always true for unsigned, but matches original TEST R8D,R8D / JS)
                                availableAmount = slotArray[si]._count - slotArray[si]._reserve;
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

                Description* posDescBase = posComp->_description;
                int bestDist = 0x7fffffff;

                // DAT_1401c7938 = InteractDescription (ENDGAME B.3.9 D5: by cell -- the
                // registered cell's own class, not the legacy healing fallback which used
                // to read MapDescription::TypeId()). (0x140122be0 / 0x140122b00 take the
                // component and the node array as integers: LEFT.)
                bool isDerivedPos = Object::IsDerivedFromNullSafe(posDescBase, InteractDescription::TypeId());
                InteractDescription* posDesc = isDerivedPos ? static_cast<InteractDescription*>(posDescBase) : nullptr;
                uint8_t found = posDesc ? WorkImpl_WalkablePos_122be0(reinterpret_cast<uintptr_t>(posComp),   // LEFT: SIG WorkImpl_WalkablePos_122be0
                    reinterpret_cast<uintptr_t>(&posDesc->_buildNodes), searchPos, targetPos, &bestDist) : 0;   // LEFT: SIG WorkImpl_WalkablePos_122be0

                if (posComp->useLocationCount > 0) {
                    int childCount = 0;
                    do {
                        uint8_t childFound = WorkImpl_WalkablePosRecursive_122b00(
                            reinterpret_cast<uintptr_t>(posComp->useLocations[childCount]), 1,   // LEFT: SIG WorkImpl_WalkablePosRecursive_122b00
                            searchPos, targetPos, &bestDist);
                        childCount++;
                        found |= childFound;
                    } while (childCount < posComp->useLocationCount);
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
                int mapSz = map->size;
                int tgtCellX = (int)(targetPos[0] + (float)(mapSz >> 1));
                int tgtCellY = (int)(targetPos[1] + (float)(mapSz >> 1));
                if (tgtCellX < 0) tgtCellX = 0;
                if (tgtCellX >= mapSz) tgtCellX = mapSz - 1;
                if (tgtCellY < 0) tgtCellY = 0;
                if (tgtCellY >= mapSz) tgtCellY = mapSz - 1;
                int targetCell = tgtCellY * mapSz + tgtCellX;

                // --- Validate work assignment (0x1400f6ef0 takes the work as an integer; the
                // Time slot is the reused param3 scratch that holds the WorkPlaceComponent) ---
                CitizenComponent* citizenRef = *entityIBP;
                uint64_t validated = WorkImpl_BuildStructureStep_6ef0(reinterpret_cast<uintptr_t>(this), &citizenRef,   // LEFT: SIG WorkImpl_BuildStructureStep_6ef0 (param3 is not a System::Time)
                    targetCell, citizenCell, param3);

                if ((char)validated == '\0') {
                    return 0;
                }

                // --- Find path and assign work ---
                // Original uses a 3-qword stack buffer; [0]=worldEntityList
                // (0x1400bf790 takes the locations and the material as integers: LEFT.)
                void* pathBuf[3] = { worldEntityList, nullptr, nullptr };
                ComponentEntity* destEntity = foundEntity;
                ComponentEntity* srcEntity = field_10;  // Reread from self
                CitizenComponent* citizenPtr = *entityIBP;

                WorkAct_GatherRawMatCtor_bf790(
                    pathBuf,            // stack buffer (original: local_58[3])
                    &citizenPtr,        // citizenPtr
                    field_20,           // pathType (priority/workplaceId)
                    reinterpret_cast<uint64_t*>(&srcEntity),        // the work entity  // LEFT: SIG WorkAct_GatherRawMatCtor_bf790
                    targetPos,          // targetPos
                    reinterpret_cast<void**>(&destEntity),        // destEntityPtr  // LEFT: SIG WorkAct_GatherRawMatCtor_bf790
                    searchPos,          // srcPos
                    reinterpret_cast<int64_t>(cost->material),  // materialType  // LEFT: SIG WorkAct_GatherRawMatCtor_bf790
                    remaining,          // amount
                    (remaining << 8) | slotIdx,  // packed slot info
                    0, '\0', 0, 0);

                // --- Finalize: set citizen state and register work ---
                int priority = field_20;
                CitizenComponent* citizenFinal = *entityIBP;

                // Call vtable slot +0x50 (GetTarget)
                Profession* workEntity = nullptr;
                GetTarget(&workEntity);

                citizenFinal->statusFlags |= 4;
                citizenFinal->field_0x30 = priority;
                citizenFinal->field_0xB0 = 0;

                if (workEntity == citizenFinal->profession && priority >= 0) {
                    citizenFinal->happinessFlags |= 0x20000;
                }

                // (the tree helper takes the container as uintptr_t*: LEFT)
                WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(&field_68), &workEntity, entityIBP);   // LEFT: SIG WorkBST_RegisterWork_1a30

                // Increment build count for this slot
                field_58[slotIdx] += remaining;


                return 2;
            }
        }

    next_slot:
        ;
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
int GatherRawMaterials::OnComplete(CitizenComponent** param2, ComponentEntity** /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);

    // Remove entity from tree at +0x68 (the helper takes the container as uintptr_t*: LEFT)
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&field_68), param2);   // LEFT: SIG WorkBST_BSTFindRemove_25520

    // Extract slot index (low byte) and amount (high 24 bits, shifted right by 8)
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] (build counts) by amount
    field_58[slotIdx] -= amount;

    // Increment field_48[slotIdx] (assigned counts) by amount
    field_48[slotIdx] += amount;


    // Original returns uVar3 & 0xffffffffffffff00: retail's bool false in AL over the
    // array pointer left in RAX.
    return reinterpret_cast<uintptr_t>(field_48) & 0xFFFFFF00;  // CAST-OK: ptr_int
}

// ============================================================
// OnCancel (0x1400ebdb0) -- vtable slot 7
//
// Cancel handler. Removes from tree, decrements build count only
// (does NOT increment assigned count).
// ============================================================
int GatherRawMaterials::OnCancel(CitizenComponent** param2, ComponentEntity** /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);

    // Remove entity from tree at +0x68 (the helper takes the container as uintptr_t*: LEFT)
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&field_68), param2);   // LEFT: SIG WorkBST_BSTFindRemove_25520

    // Extract slot index and amount
    uint32_t slotIdx = param4 & 0xFF;
    int amount = (int)param4 >> 8;

    // Decrement field_58[slotIdx] only (cancel = don't credit the work)
    field_58[slotIdx] -= amount;
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
    while (true) {
        AssignmentNode* root = field_68;
        if (root == nullptr) return 0;

        // Find leftmost node
        AssignmentNode* node = root;
        while (node->left != nullptr) {
            node = node->left;
        }

        // Also try walking up from leftmost
        AssignmentNode* current = root;
        while (current != nullptr) {
            AssignmentNode* next = current->left;
            if (next == nullptr) break;
            current = next;
        }

        // node is now leftmost
        if (node == nullptr) return 0;

        // Release the citizen's action list: CharacterComponent (+0x278) -> +0x48
        CitizenComponent* citizenEntity = node->element;
        WorkBase_TaskListCleanup_c5400(&citizenEntity->character->actions);
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
    // Walk tree at +0x68 (field_68), collect invalid entries
    AssignmentNode* root = field_68;
    uint32_t invalidCount = 0;
    CitizenComponent** invalidArray = nullptr;
    uint32_t invalidCapacity = 0;

    if (root != nullptr) {
        // Find leftmost node
        AssignmentNode* node = root;
        while (node->left != nullptr) {
            node = node->left;
        }

        while (node != nullptr) {
            // Check if this task is still valid
            CitizenComponent* taskIBP[1];
            taskIBP[0] = node->element;

            char valid = ValidateTask(nullptr, taskIBP, nullptr);

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
                        CitizenComponent** newArray = static_cast<CitizenComponent**>(System::MemoryAllocator::MemAlloc((int64_t)(int)newCap << 3, 8));

                        // Copy old entries
                        int copyStart = (invalidArray == newArray) ? (int)invalidCount : 0;
                        for (int ci = copyStart; ci < (int)newCount; ci++) {
                            CitizenComponent** dst = &newArray[ci];
                            *dst = nullptr;
                            if (ci < (int)invalidCount) {
                                *dst = invalidArray[ci];
                            } else {
                                *dst = node->element;
                            }
                        }

                        if (invalidArray != newArray) {
                            System::MemoryAllocator::MemFree(invalidArray);
                        }
                        invalidArray = newArray;
                    } else {
                        // Space available, just add
                        invalidArray[invalidCount] = node->element;
                    }
                    invalidCount = newCount;
                }
            }

            // Get next node in tree (in-order traversal)
            AssignmentNode* right = node->right;
            if (right != nullptr) {
                // Go right, then leftmost
                node = right;
                while (node->left != nullptr) {
                    node = node->left;
                }
            } else {
                // Go up until we come from a left child
                AssignmentNode* parent = node->parent;
                while (parent != nullptr) {
                    if (parent->left == node) {
                        // node is left child of parent -- parent is next
                        break;
                    }
                    // node is right child, keep going up
                    node = parent;
                    parent = node->parent;
                }
                node = parent;
            }
        }
    }

    // Remove invalid entries from tree (keys compare unsigned)
    if ((int)invalidCount > 0) {
        for (uint32_t i = 0; i < invalidCount; i++) {
            // Find node in tree by key and remove
            CitizenComponent* searchKey = invalidArray[i];
            AssignmentNode* treeNode = field_68;

            while (treeNode != nullptr) {
                CitizenComponent* nodeKey = treeNode->element;
                if (searchKey < nodeKey) {
                    treeNode = treeNode->left;
                } else {
                    if (searchKey <= nodeKey) {
                        // Found -- remove this node (the helper takes the container as
                        // uintptr_t*: LEFT)
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&field_68), treeNode);   // LEFT: SIG WorkBST_BSTRemove_a6e0
                        break;
                    }
                    treeNode = treeNode->right;
                }
            }
        }

        // If any were removed, call vtable slot +0x40 (Cleanup) and reset build counts
        if ((int)invalidCount > 0) {
            Cleanup();   // slot 8 (vtable +0x40), a virtual dispatch in retail

            // Reset all build counts to 0
            int numSlots = field_60;
            for (int i = 0; i < numSlots; i++) {
                field_58[i] = 0;
            }
        }
    }

    System::MemoryAllocator::MemFree(invalidArray);
    return (invalidCount == 0) ? 1 : 0;
}

// ============================================================
// GetTarget (0x1400ec510) -- vtable slot 10
//
// The ProfessionList's +0x30 profession from the game world singleton.
// Hidden return ptr: RCX=self, RDX=retbuf.
// ============================================================
Profession** GatherRawMaterials::GetTarget(Profession** retbuf) {
    *retbuf = GameUpdateLoop::_singleton->m_professionList->field_0x30;
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
    // Free BST nodes at +0x68
    AssignmentNode* bstRoot = field_68;
    if (bstRoot != nullptr) {
        // Clean up left subtree
        WorkBase_TreeNodeFree_14c30(&field_68, bstRoot->left);
        // Clean up right subtree
        WorkBase_TreeNodeFree_14c30(&field_68, bstRoot->right);
        // Free root node
        System::MemoryAllocator::MemFree(bstRoot);
    }

    // Zero out BST root and count
    field_68 = nullptr;
    field_70 = 0;

    // Free second int array at +0x58 (count and capacity: one qword store in retail)
    System::MemoryAllocator::MemFree(field_58);
    field_58 = nullptr;
    field_60 = 0;
    field_64 = 0;

    // Free first int array at +0x48 (count and capacity: one qword store in retail)
    System::MemoryAllocator::MemFree(field_48);
    field_48 = nullptr;
    field_50 = 0;
    field_54 = 0;

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
    return reinterpret_cast<GatherRawMaterials*>(self)->GatherRawMaterials::Execute((CitizenComponent**)citizenIBP, nullptr);   // B.3.8 bridge; LEFT: the force harness drives it through a void* function pointer
}

// Execute-readiness gate for the force harness. B3R: every helper Execute and its pushed
// action chain use is called by name, so only the game world is left to test.
extern "C" bool GatherRawMaterials_DepsReady() {
    return GameUpdateLoop::_singleton != nullptr;
}
