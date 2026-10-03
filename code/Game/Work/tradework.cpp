// code/Game/Work/tradework.cpp
// Clean C++ reimplementation of TradeWork -- all methods.
// Source: Game/Work/TradeWork.cpp (from profile build debug strings)
//
// TradeWork (0x58 bytes) -- Trade post work action.
// Inherits: Object::Internal -> Work (0x48) -> TradeWork
// Vtable: 0x140161a10
//
// The assignment set at +0x48 (TradeWork::Node, 0x38 bytes): the record
// {citizen, resource, quantity}, then parent / left / right / balance / height
// (Work/TradeWork.h).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/TradeWork.h"
#include "Work/tradework_funcs.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/Map.h"
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Player/exe_bodies.h"      // B3R: bodies called by name
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 D5
#include "Component/StorageComponent.h"         // ENDGAME B.3.9 D5
#include "Component/InteractDescription.h"      // ENDGAME B.3.9 D5
#include "Component/InteractComponent.h"
#include "Component/ComponentDescription.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Component/TradeComponent.h"
#include "Component/WorkComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x14002c8a0  TradeWork::SerializeAssignments (tradework_serialize.cpp)
//   0x14002c710  TradeWork::InsertAssignment     (tradework_serialize.cpp)
//   0x14002c690  PlayerTrade_BSTFindRemove_2c690
//   0x140025740  PlayerTrade_BSTRemove_25740
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x1400f7100  Work::ValidateTask
//   0x140014b00  TradeWork_helper_14b00
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: TradeWork_Ctor
// disassembles to `0x1400f6dbd: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.


// ============================================================
// TradeWork::Execute -- Trade assignment (0x1400f5d40, Ghidra "TradeWork_Serialize")
// ============================================================
// Faithful port of the exe trade-assignment logic. Structure:
//   0. capacity gate (workers vs WorkComponent capacity)              -> return 1
//   1. if the citizen already carries trade goods -> LongTermPlan     -> return 2
//   2. PASS 1: for every resource type, compute net = requested(the TradeComponent's
//      stock tree) - inFlight(this set) - storageAvail(building storage). Push
//      net>0 into the "import" list (local_c8), net<0 (abs) into the "export" list
//      (local_f8). Each list is a 16-byte-entry {resource, int qty} dynamic array
//      grown x3 (faithful to the inlined push_back).
//   3. PASS 2 over the export list: find a source storage via FindStorage(0x14008d590),
//      build a fetch path (Pathfinding_FindPath), insert a NEGATIVE-qty record into
//      the set, -> return 2 on first assignment.
//   4. PASS 3 over the import list: same, gated by StorageComponent CanAccept(0x14012cd80),
//      POSITIVE-qty record -> return 2 on first assignment.
//   default -> return 1.
// Not exercised by the autostart harness (needs a live trade post + trader).

namespace {


// 16-byte list entry: {resource, int qty, int pad}. Matches the exe stack record.
struct TWEntry { ComponentDescription* resource; int32_t qty; int32_t pad; };

// The resource's RawMaterialDescription: the first sub-description whose Type()
// is RawMaterialDescription's, IsDerivedFrom-checked (the loop retail inlines).
// The tid / null arms are the tree's guards.
RawMaterialDescription* TW_FindRawMaterial(ComponentDescription* desc) {
    uint32_t tid = RawMaterialDescription::TypeId();
    if (!tid || !desc) return nullptr;
    int n = desc->_subDescCount;
    for (int i = 0; i < n; i++) {
        Description* c = desc->_subDescs[i];
        uint32_t t = c->Type();
        if (t == tid)
            return (Object::IsDerivedFromNullSafe(c, tid)) ? static_cast<RawMaterialDescription*>(c) : nullptr;
    }
    return nullptr;
}

// Clamp a world coordinate (mapSize/2 + f) into [0, mapSize-1].
inline int TW_ClampCell(float f, int mapSize) {
    int v = static_cast<int>(static_cast<float>(mapSize >> 1) + f);
    if (v < 0) v = 0;
    if (v >= mapSize) v = mapSize - 1;
    return v;
}

// An entity's StorageComponent; the null-entity arm is the tree's guard.
inline StorageComponent* TW_StorageOf(ComponentEntity* entity) {
    return entity ? entity->FindComponent<StorageComponent>() : nullptr;
}

}  // namespace

// Faithful port of the inlined push_back for the import/export lists (16-byte entries,
// growth factor 3). Mirrors the exe: realloc only when capacity < count; on realloc copy
// the old entries then append; when in-place, just append; free the old buffer on realloc.
static void TW_PushEntry(TWEntry** arr, uint32_t* count, uint32_t* cap, ComponentDescription* resource,
                         int32_t qty) {
    uint32_t oldCount = *count;
    uint32_t newCount = oldCount + 1;
    TWEntry* oldArr = *arr;
    TWEntry* dst = oldArr;
    if (static_cast<int>(*cap) < static_cast<int>(newCount)) {
        uint32_t newCap = (oldCount != 0) ? newCount * 3 : newCount;
        *cap = newCap;
        dst = static_cast<TWEntry*>(
            System::MemoryAllocator::MemAlloc(static_cast<int64_t>(static_cast<int>(newCap)) << 4, 8));
    }
    uint32_t start = (oldArr == dst) ? oldCount : 0;
    if (dst) {
        for (uint32_t i = start; i < newCount; i++)
            dst[i] = (i < oldCount) ? oldArr[i] : TWEntry{resource, qty, 0};
    }
    if (oldArr != dst && oldArr) System::MemoryAllocator::MemFree(oldArr);
    *arr = dst;
    *count = newCount;
}

// The assignment tail both passes inline: read the work id (+0x20), call slot 10
// for the post's Profession, write the citizen's task-state fields and insert the
// record {citizen, resource, signed quantity}. Returns 2. Retail hands slot 10 the
// scratch the preceding Pathfinding_FindPath wrote into (&local_res8); the target is
// read straight back and the scratch is not read again.
int TradeWork::CommitAssignment(CitizenComponent* const* citizenRef, ComponentDescription* resource,
                                int quantity) {
    int taskId = field_20;                                        // plVar31[4] low dword

    CitizenComponent* citizen = *citizenRef;

    Profession* target = nullptr;
    GetTarget(&target);   // slot 10, by name (B.3.x)

    citizen->field_0x30 = taskId;
    citizen->statusFlags |= 4;
    citizen->field_0xB0 = 0;
    if (target == citizen->profession && taskId >= 0)
        citizen->happinessFlags |= 0x20000;

    Assignment rec;
    rec.citizen = citizen;          // +0x00 key
    rec.resource = resource;        // +0x08
    rec.quantity = quantity;        // +0x10
    rec._pad14 = 0;
    InsertAssignment(rec);
    return 2;
}

int TradeWork::Execute(CitizenComponent** citizenRef, System::Time* /*context*/) {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;

    int ret = 1;
    TWEntry* arrA = nullptr; uint32_t cntA = 0, capA = 0;   // import list (net > 0)  -> local_c8/local_c0
    TWEntry* arrB = nullptr; uint32_t cntB = 0, capB = 0;   // export list (net < 0)  -> local_f8/local_f0
    uint64_t outBuf[8] = {0};                               // FindPath scratch (&local_res8)

    // ---- 0. capacity gate ----------------------------------------------------
    int workers = workerCount;                           // param_1[10]
    WorkComponent* workComp = field_18;                  // param_1[3]
    if (workComp->workState == 0)
        workComp->workState = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(workComp));   // LEFT: C signature (GAME-a / COMP-f)
    if (workComp->workState <= workers)
        return 1;

    // ---- find components -----------------------------------------------------
    TradeComponent*    tradeComp = field_10->FindComponent<TradeComponent>();      // 0x140015140 (local_58)
    StorageComponent*  bldgStor  = field_10->FindComponent<StorageComponent>();    // 0x140006270 (local_50 / local_78)
    InteractComponent* posComp   = field_10->FindComponent<InteractComponent>();   // 0x140002ed0 (local_e0)
    CitizenBehavior*   resList   = gw ? gw->m_citizenList : nullptr;               // pplVar30 / local_70

    // ---- 1. citizen already carrying trade goods? ---------------------------
    CitizenComponent* citizenEnt = *citizenRef;
    StorageComponent* carried = citizenEnt->_entity->FindComponent<StorageComponent>();   // local_res20
    if (CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(carried)) > 0) {   // LEFT: C signature (COMP-b)
        citizenEnt->LongTermPlan();   // 0x140115e60
        return 2;
    }

    // ---- 2. PASS 1: build import/export lists over all resource types --------
    int resCount = resList ? resList->field_0x118 : 0;                    // pplVar30[0x23]
    ComponentDescription** resArr = resList ? resList->field_0x110 : nullptr;   // pplVar30[0x22]
    for (int i = 0; i < resCount; i++) {
        ComponentDescription* resType = resArr[i];

        // (existence probe of the RawMaterialDescription; result discarded, as in exe)
        TW_FindRawMaterial(resType);

        // requested count from the TradeComponent's stock tree (+0x60), keyed on the
        // resource (retail compares the keys as unsigned 64-bit values)
        int requested = 0;
        if (tradeComp) {
            TradeResourceNode* node = tradeComp->stockRoot;
            while (node) {
                if (resType < node->resource)       node = node->left;
                else if (resType <= node->resource) { requested = node->quantity; break; }
                else                                node = node->right;
            }
        }

        // storage available in the building storage comp (its items).
        int storageAvail = 0;
        if (bldgStor && bldgStor->slotCount > 0) {
            StorageComponent::Item* items = bldgStor->storageSlots;
            int pn = bldgStor->slotCount;
            for (int k = 0; k < pn; k++) {
                if (items[k]._resource == resType) {
                    storageAvail = items[k]._count - items[k]._reserve;
                    break;
                }
            }
        }

        // in-flight quantity from this set, by in-order traversal.
        int inFlight = 0;
        Node* root = bstRoot;
        if (root) {
            Node* node = root;
            Node* prev;
            do { prev = node; node = node->left; } while (node);
            node = prev;
            while (node) {
                if (node->value.resource == resType)
                    inFlight += node->value.quantity;
                Node* right = node->right;
                if (right) {
                    node = right;
                    while (node->left) node = node->left;
                } else {
                    Node* parent = node->parent;
                    while (parent && parent->right == node) {
                        node = parent; parent = parent->parent;
                    }
                    node = parent;
                }
            }
        }

        int net = (requested - inFlight) - storageAvail;
        if (net > 0)      TW_PushEntry(&arrA, &cntA, &capA, resType, net);       // import
        else if (net < 0) TW_PushEntry(&arrB, &cntB, &capB, resType, -net);      // export
    }

    // ---- 3. PASS 2 over the export list (arrB) -------------------------------
    for (uint32_t bi = 0; bi < cntB; bi++) {
        ComponentDescription* resType = arrB[bi].resource;
        RawMaterialDescription* matDesc = TW_FindRawMaterial(resType);   // pvVar24
        int qty = arrB[bi].qty;

        // clamp qty to the citizen's carry-storage free space:
        CitizenComponent* citizen = *citizenRef;
        StorageComponent* carryStor = TW_StorageOf(citizen->_entity);   // pvVar18
        // The matDesc / weight arm is the tree's guard; retail divides unguarded.
        if (matDesc && matDesc->_weight != 0) {
            int freeSpace = carryStor->capacity;
            int cur = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(carryStor));   // LEFT: C signature
            int room = (freeSpace - cur) / matDesc->_weight;
            if (room < qty) qty = room;
        }

        // building storage comp -> its items, find resType:
        StorageComponent* bldgStorComp = TW_StorageOf(field_10);   // -> local_e8
        // The null arm is the tree's guard.
        if (!bldgStorComp || bldgStorComp->slotCount <= 0) continue;
        StorageComponent::Item* items = bldgStorComp->storageSlots;
        int pn = bldgStorComp->slotCount;
        int matchK = -1;
        for (int k = 0; k < pn; k++) {
            if (items[k]._resource == resType) { matchK = k; break; }
        }
        if (matchK < 0) continue;
        int avail = items[matchK]._count - items[matchK]._reserve;
        if (avail == 0) continue;
        if (avail < qty) qty = avail;

        // fetch source cell from the citizen's model (character +0x68, translation):
        CitizenComponent* c2 = *citizenRef;
        ModelComponent* nav = c2->character->modelComponent;
        int mapSize = gw ? gw->m_game->size : 0;
        int cellX = TW_ClampCell(nav->transform.translation.x, mapSize);
        int cellY = TW_ClampCell(nav->transform.translation.y, mapSize);
        int cell = mapSize * cellY + cellX;

        float searchPos[4] = {};                           // local_90 (out from FindStorage)
        uint64_t srcStorIBP[2] = {0};                      // local_d8 (out IBP from FindStorage)
        int64_t srcStorPrimary = 0;                        // local_b8 (primary result)
        // LEFT: 0x14008d590's C signature (ACT-b) takes the town manager, the
        // resource and the position scratch as integers.
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(resList), &srcStorPrimary, cell,
                                    reinterpret_cast<uint64_t>(resType), qty,
                                    1, reinterpret_cast<uint32_t*>(searchPos), srcStorIBP);
        if (srcStorPrimary == 0) { continue; }

        // destination position from the trade post's interact component:
        float destPos[4] = {};                             // local_a0
        uint32_t range = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(posComp), 0);   // LEFT: C signature
        System::Random* rng = gw ? gw->m_random : nullptr;
        uint32_t rnd = rng ? rng->Rand(static_cast<uint32_t>(range)) : 0;
        Pf_EntityResolvePos_2e20(posComp, destPos, 0, rnd, nullptr);

        // build the fetch path (branch 1): dest = source storage, src = trade-post building.
        uint64_t destStorage = static_cast<uint64_t>(srcStorPrimary);   // local_d8 = local_b8
        CitizenComponent* citizenVal = *citizenRef;                     // local_b0 = *param2
        void* bldgVal = field_10;                                       // local_res20 = *local_b0(old)
        uint32_t pathType = static_cast<uint32_t>(field_20);
        WorkAct_GatherRawMatCtor_bf790(outBuf, &citizenVal, pathType, &destStorage,
                          searchPos, &bldgVal, destPos,
                          reinterpret_cast<int64_t>(resType),   // LEFT: bf790 takes the resource as an integer
                          qty, 1, 0, 1, 0, 1);

        ret = CommitAssignment(citizenRef, resType, -qty);   // NEGATIVE qty (export)
        goto cleanup;
    }

    // ---- 4. PASS 3 over the import list (arrA) -------------------------------
    for (uint32_t ai2 = 0; ai2 < cntA; ai2++) {
        ComponentDescription* resType = arrA[ai2].resource;
        RawMaterialDescription* matDesc = TW_FindRawMaterial(resType);   // pvVar24
        int qty = arrA[ai2].qty;                                          // local_res20 low = pSVar33+8

        // fetch destination cell from the citizen's model (character +0x68, translation):
        CitizenComponent* c2 = *citizenRef;
        ModelComponent* nav = c2->character->modelComponent;
        int mapSize = gw ? gw->m_game->size : 0;
        int cellX = TW_ClampCell(nav->transform.translation.x, mapSize);
        int cellY = TW_ClampCell(nav->transform.translation.y, mapSize);
        int cell = mapSize * cellY + cellX;

        float srcPos[4] = {};                              // local_a0 (out from FindStorage)
        uint64_t srcIBP[2] = {0};                          // local_90 (out IBP from FindStorage)
        int64_t srcStorage = 0;                            // local_e8 (primary result)
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(resList), &srcStorage, cell,
                                    reinterpret_cast<uint64_t>(resType), 1,
                                    0, reinterpret_cast<uint32_t*>(srcPos), srcIBP);   // LEFT: as above
        if (srcStorage == 0) continue;

        // source storage component: the FindStorage primary entity's StorageComponent,
        // then its items.
        ComponentEntity* srcEntity = reinterpret_cast<ComponentEntity*>(srcStorage);   // LEFT: 0x14008d590 returns the entity as an integer
        StorageComponent* a8 = TW_StorageOf(srcEntity);
        int qtyMove = qty;
        if (a8 && a8->slotCount > 0) {
            StorageComponent::Item* items = a8->storageSlots;
            int pn = a8->slotCount;
            int matchK = -1;
            for (int k = 0; k < pn; k++) {
                if (items[k]._resource == resType) { matchK = k; break; }
            }
            if (matchK < 0) continue;
            int avail = items[matchK]._count - items[matchK]._reserve;
            if (avail == 0) continue;
            if (avail < qtyMove) qtyMove = avail;
        } else {
            continue;
        }

        // clamp by the destination (citizen building) storage free space for resType:
        CitizenComponent* citizen = *citizenRef;
        StorageComponent* b8 = TW_StorageOf(citizen->_entity);   // local_b8
        if (matDesc && matDesc->_weight != 0) {   // the tree's guard, as in pass 2
            int cap  = b8->capacity;
            int cur  = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(b8));   // LEFT: C signature
            int room = (cap - cur) / matDesc->_weight;
            if (room < qtyMove) qtyMove = room;
        }

        // gate: can the trade-post building storage accept this resource+qty?
        if (static_cast<char>(Pf_StorageCanAccept_cd80(reinterpret_cast<int64_t>(bldgStor),
                                                       reinterpret_cast<int64_t>(resType), qtyMove)) == 0)   // LEFT: C signature; the door read AL
            continue;

        // source position from the trade post's interact component (+ its parts):
        Description* desc = posComp->_description;
        InteractDescription* interactDesc = nullptr;
        if (InteractDescription::TypeId() && Object::IsDerivedFromNullSafe(desc, InteractDescription::TypeId()))
            interactDesc = static_cast<InteractDescription*>(desc);
        // The null arm is the tree's guard; retail reads +0x30 of the result unguarded.
        uint32_t range = interactDesc ? static_cast<uint32_t>(interactDesc->_useNodes.count) : 0;
        if (posComp->useLocationCount > 0) {
            InteractComponent** parts = posComp->useLocations;
            int cn = posComp->useLocationCount;
            for (int k = 0; k < cn; k++) {
                range += Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(parts[k]), 0);   // LEFT: C signature
            }
        }
        System::Random* rng = gw ? gw->m_random : nullptr;
        uint32_t rnd = rng ? rng->Rand(static_cast<uint32_t>(range)) : 0;
        float srcPos2[4] = {};                             // local_68
        Pf_EntityResolvePos_2e20(posComp, srcPos2, 0, rnd, nullptr);

        // build the fetch path (branch 2): dest = trade-post building, src = source storage.
        CitizenComponent* citizenVal = *citizenRef;                        // local_b0 = *param2
        uint64_t bldgVal = reinterpret_cast<uint64_t>(field_10);           // local_d8 = *local_b0(old); LEFT: bf790's srcLoc is an integer slot
        void* srcStorVal = srcEntity;                                      // local_90 = local_e8
        uint32_t pathType = static_cast<uint32_t>(field_20);
        WorkAct_GatherRawMatCtor_bf790(outBuf, &citizenVal, pathType, &bldgVal,
                          srcPos2, &srcStorVal, srcPos,
                          reinterpret_cast<int64_t>(resType),   // LEFT: as above
                          qtyMove, 0, 0, 0, 0, 1);

        ret = CommitAssignment(citizenRef, resType, qtyMove);  // POSITIVE qty (import)
        goto cleanup;
    }

cleanup:
    System::MemoryAllocator::MemFree(arrB); System::MemoryAllocator::MemFree(arrA);
    return ret;
}


// ============================================================
// TradeWork::OnComplete [6] -- BSTFindRemove + return false (0x1400f5d20)
// ============================================================
// LEFT (both slots): 0x14002c690's C signature (PLAYER) takes the set as int64_t*.
int TradeWork::OnComplete(CitizenComponent** param2, ComponentEntity** /*arg2*/, int /*arg3*/) {
    PlayerTrade_BSTFindRemove_2c690(reinterpret_cast<int64_t*>(&bstRoot), param2);
    return false;
}


// ============================================================
// TradeWork::OnCancel [7] -- BSTFindRemove thunk (0x1400f5d10)
// ============================================================
int TradeWork::OnCancel(CitizenComponent** param2, ComponentEntity** /*arg2*/, int /*arg3*/) {
    PlayerTrade_BSTFindRemove_2c690(reinterpret_cast<int64_t*>(&bstRoot), param2);
    return 0;
}


// ============================================================
// TradeWork::Cleanup [8] (0x1400f6be0)
// While the set is not empty: clear the action stack of the leftmost citizen
// (its CompleteTask cancel removes it from the set).
// ============================================================
int TradeWork::Cleanup() {
    while (workerCount > 0) {
        // Find the leftmost node
        Node* node = bstRoot;
        Node* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = node->left;
        }

        // Cancel the work item
        CitizenComponent* citizen = leftmost->value.citizen;
        WorkBase_TaskListCleanup_c5400(&citizen->character->actions);
    }
    return 0;
}


// ============================================================
// TradeWork::Validate [9] (0x1400f5ae0)
// Walks the set in order, validates each citizen (Work::ValidateTask), then
// removes the invalid ones. Returns true if all valid, false otherwise.
// ============================================================
int TradeWork::Validate() {
    Node* root = bstRoot;
    CitizenComponent** removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find the leftmost node (minimum)
    Node* node = root;
    while (node->left != nullptr) {
        node = node->left;
    }

    // In-order traversal
    while (node != nullptr) {
        CitizenComponent* citizen = node->value.citizen;   // local_res8

        // Validate this entry (retail passes its stale RDX, which the body never reads)
        bool stillValid = ValidateTask(nullptr, &citizen, nullptr);

        if (!stillValid) {
            // Add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                CitizenComponent** newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = static_cast<CitizenComponent**>(System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 8));
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    CitizenComponent* val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = citizen;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    System::MemoryAllocator::MemFree(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        Node* rightChild = node->right;
        if (rightChild != nullptr) {
            node = rightChild;
            while (node->left != nullptr) {
                node = node->left;
            }
        } else {
            Node* parent = node->parent;
            if (parent != nullptr && parent->left != node) {
                while (parent != nullptr && parent->right == node) {
                    node = parent;
                    parent = parent->parent;
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from the set
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            Node* searchNode = bstRoot;
            while (searchNode != nullptr) {
                if (removeArr[i] < searchNode->value.citizen) {
                    searchNode = searchNode->left;
                } else {
                    if (removeArr[i] <= searchNode->value.citizen) {
                        // LEFT: 0x140025740's C signature (PLAYER) takes the set as int64_t*.
                        PlayerTrade_BSTRemove_25740(reinterpret_cast<int64_t*>(&bstRoot), searchNode);
                        break;
                    }
                    searchNode = searchNode->right;
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(removeArr);
    return removeCount == 0;
}


// ============================================================
// TradeWork::GetTarget [10] (0x1400f6c50)
// The post's WorkPlaceComponent's Profession, through the hidden return slot.
// ============================================================
Profession** TradeWork::GetTarget(Profession** outPtr) {
    // 0x140002e10 = FindComponent<WorkPlaceComponent>.
    WorkPlaceComponent* workplace = field_10->FindComponent<WorkPlaceComponent>();
    *outPtr = workplace->profession;
    return outPtr;
}


// ============================================================
// TradeWork::GetPriority [11] / HasWorkers (0x140029860)
// Returns (workerCount > 0). Shared with many work classes.
// ============================================================
int TradeWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return workerCount > 0 ? 1 : 0;
}


// ============================================================
// TradeWork::Persist [3] (0x1400f6cb0)
// Work::Persist + Serializer(v0) + the set.
// No extra int field (unlike TendArea).
// ============================================================
void TradeWork::Persist(void* stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // Persist's stream is void*
    SerializeAssignments(ser);
}   // ~Serializer


// ============================================================
// TradeWork dtor (called by slot 0 0x14002c660). The stamps and the `~Internal`
// call are the compiler's chain. Frees the set's nodes.
// ============================================================
TradeWork::~TradeWork() {
    Node* root = bstRoot;
    if (root != nullptr) {
        // LEFT: 0x140014b00's C signature (PLAYER) takes the set as an integer.
        TradeWork_helper_14b00(reinterpret_cast<int64_t>(&bstRoot), root->left);
        TradeWork_helper_14b00(reinterpret_cast<int64_t>(&bstRoot), root->right);
        System::MemoryAllocator::MemFree(root);
    }

    // Zero out the root and the count
    bstRoot = nullptr;
    workerCount = 0;
}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
TradeWork::TradeWork() : Work(), bstRoot(nullptr), workerCount(0), _pad54{} {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================

// create 0x14002c620: MemAlloc(sizeof, 8), then the ctor.
void* TradeWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TradeWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TradeWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* TradeWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TradeWork());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(TradeWork, "Work")
