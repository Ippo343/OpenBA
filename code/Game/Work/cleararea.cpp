// code/Game/Work/cleararea.cpp
// Clean reimplementation of ClearArea Work action.
//
// ClearArea (0x78 bytes) -- area clearing work action.
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// Ctor: 0x1400e68d0, Vtable: 0x140161110
//
// Fields:
//   +0x48: WorkEntitySetNode* -- set of entity -> citizen (entities to clear)
//   +0x50: int treeCount    -- number of entries in +0x48 tree
//   +0x58: WorkCellSetNode*   -- set of cell -> citizen (ground cells to clear)
//   +0x60: int cellCount    -- number of entries in +0x58 tree
//   +0x68: void* mapEntity  -- InternalBasePtr to map component entity
//   +0x70: bool clearTrees  -- whether to clear trees/resources
//   +0x71: bool clearGround -- whether to clear ground (v1+)
//   +0x72: bool clearBuildings -- whether to clear buildings (v1+)
//
// Decompiled from Application-x64.exe.

#include "Work/ClearArea.h"
#include "Work/cleararea_funcs.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Game/exe_bodies.h"        // B3R: 0x14009f3e0
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/RawMaterialComponent.h"   // ENDGAME B.3.9 D5
#include "Component/WorkComponent.h"          // ENDGAME B.3.9 D5
#include "Component/WorkPlaceComponent.h"
#include "Component/MapComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/ProfessionList.h"
#include "Game/Map.h"

// Diagnostic trace for the harvest-completion investigation. Gated by
// BANISHED_CLEAR_TRACE (shared with clearareacomponent_states.cpp); default off.

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x140029c10 WorkLeaf_BstSerializeIBPpair_29c10   0x140029de0 WorkCellBST_Serialize_9de0
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable
//   0x1400298a0 WorkLeaf_BstRemoveByIBP_298a0        0x14001ca60 WorkCellBST_Remove_ca60
//   0x14000e320 WorkBase_TreeNodeFree_e320           0x1400c5400 WorkBase_TaskListCleanup_c5400

// ============================================================
// ClearArea::Ctor (0x1400e68d0)
// ============================================================
// The Work base init and the two stamps are the compiler's (Work(), then this
// class's table).
ClearArea::ClearArea()
    : Work(),
      field_48(nullptr),  // tree root
      field_50(0),        // tree count
      field_58(nullptr),  // cell tree root
      field_60(0),        // cell count
      field_68(nullptr) { // map entity IBP
}

// ============================================================
// ClearArea::ClearArea(entity, descRef, flags) (0x1400e6960)
// ============================================================
// Retail: local = *param_2; Work_Ctor25(this, &local); the ClearArea stamp (the
// compiler's); +0x48 = 0, +0x50 = 0, +0x58 = 0, +0x60 = 0, +0x68 = param_3 (a
// raw InternalBasePtr copy), +0x70/+0x71/+0x72 = the three flag bytes.
ClearArea::ClearArea(void* pEntityIBP, uint64_t descRef, uint8_t clearTrees,
                     uint8_t clearGround, uint8_t clearBuildings)
    : Work(pEntityIBP),
      field_48(nullptr),
      field_50(0),
      field_58(nullptr),
      field_60(0),
      field_68(reinterpret_cast<ComponentDescription*>(descRef)),   // LEFT: SIG ClearArea ctor (descRef uint64_t; callers in clearareacomponent_states.cpp, COMP-2)
      field_70(clearTrees),
      field_71(clearGround),
      field_72(clearBuildings) {}

// ============================================================
// ClearArea::BaseDtor (0x1400e6810)
// ============================================================
// Destroys ClearArea-specific data: frees both BST trees,
// resets vtable to Work, then tail-calls Internal::~Internal.
// The stamps (ClearArea, then Work) and the `~Internal` tail call are the compiler's.
ClearArea::~ClearArea() {

    // Free cell tree (+0x58)
    WorkCellSetNode* cellRoot = field_58;
    if (cellRoot) {
        // Recursively free left subtree, then right subtree, then root
        WorkBase_TreeNodeFree_e320(&field_58, cellRoot->left);
        WorkBase_TreeNodeFree_e320(&field_58, cellRoot->right);
        System::MemoryAllocator::MemFree(cellRoot);
    }
    field_58 = nullptr;
    field_60 = 0;

    // Free entity tree (+0x48)
    WorkEntitySetNode* treeRoot = field_48;
    if (treeRoot) {
        WorkBase_TreeNodeFree_e320(&field_48, treeRoot->left);
        WorkBase_TreeNodeFree_e320(&field_48, treeRoot->right);
        System::MemoryAllocator::MemFree(treeRoot);
    }
    field_48 = nullptr;
    field_50 = 0;
}

// ============================================================
// ClearArea::Serialize (0x1400e6740)
// ============================================================
// Pattern: Work_Serialize + Serializer(v1) + BST_Serialize(+0x48) +
//          IBP_Serialize(+0x68) + Bool(+0x70) + [v1: BST_Cell(+0x58) +
//          Bool(+0x71) + Bool(+0x72)]
void ClearArea::Persist(IO::Stream& stream) {

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create Serializer with version=1 (destroyed at the end, as retail)
    IO::Serializer serializer(stream, 1);

    // Serialize entity BST at +0x48
    WorkLeaf_BstSerializeIBPpair_29c10(&field_48, &serializer);

    // Serialize the description ExternalPtr at +0x68
    Reimpl_ExternalPtr_Serialize_StringTable(&field_68, &serializer);

    // Serialize clearTrees bool at +0x70 (the field is uint8_t storage)
    serializer.Serialize(*reinterpret_cast<bool*>(&field_70), nullptr);   // LEFT: H8 the ctor stores the caller's raw byte (0x1400e6960); a bool field would normalise it

    // Version >= 1: serialize additional fields
    int version = serializer.GetVersion();
    if (version > 0) {
        // Serialize cell BST at +0x58
        WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>(&field_58), &serializer);   // LEFT: SIG WorkCellBST_Serialize_9de0

        // Serialize clearGround bool at +0x71
        serializer.Serialize(*reinterpret_cast<bool*>(&field_71), nullptr);   // LEFT: H8 the ctor stores the caller's raw byte (0x1400e6960); a bool field would normalise it

        // Serialize clearBuildings bool at +0x72
        serializer.Serialize(*reinterpret_cast<bool*>(&field_72), nullptr);   // LEFT: H8 the ctor stores the caller's raw byte (0x1400e6960); a bool field would normalise it
    }
}

// ============================================================
// ClearArea::OnComplete (0x1400e5e30)
// ============================================================
// Called when a work task completes. Inserts completed entry into
// the entity tree (+0x48) if entry is non-null, then searches
// the cell tree (+0x58) for the given cellIndex and removes it.
//
// Params: self, citizen (unused), treeEntry (IBP*), cellIndex
int ClearArea::OnComplete(CitizenComponent** /*citizen*/, ComponentEntity** treeEntry, int cellIndex) {

    // If treeEntry is non-null, insert into entity BST at +0x48
    ComponentEntity* entryPtr = treeEntry ? *treeEntry : nullptr;
    if (entryPtr != nullptr) {
        WorkLeaf_BstRemoveByIBP_298a0(&field_48, treeEntry);
    }

    // Search cell BST (+0x58) for cellIndex and remove if found
    WorkCellSetNode* node = field_58;
    WorkCellSetNode* root = node;
    if (node == nullptr) return 0;

    // BST search for cellIndex
    while (node != nullptr) {
        int nodeVal = node->element.cell;
        if (cellIndex < nodeVal) {
            node = node->left;
        } else if (cellIndex > nodeVal) {
            node = node->right;
        } else {
            // Found — now search again from root and remove
            if (root == nullptr) return 0;
            WorkCellSetNode* searchNode = root;
            while (searchNode != nullptr) {
                int searchVal = searchNode->element.cell;
                if (cellIndex < searchVal) {
                    searchNode = searchNode->left;
                } else if (cellIndex > searchVal) {
                    searchNode = searchNode->right;
                } else {
                    // Remove this node from cell tree
                    WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(&field_58), searchNode);   // LEFT: SIG WorkCellBST_Remove_ca60
                    return 0;
                }
            }
            return 0;
        }
    }
    return 0;
}

// ============================================================
// ClearArea::OnCancel (0x1400e5e00)
// ============================================================
// Retail (disassembly): copy *R8 and *RDX to stack slots, then dispatch
// vtable +0x30 = slot 6 OnComplete(&citizenCopy, &treeCopy, R9 unchanged) and
// return its result. Slot 6 is OnComplete (3 arguments), not Cleanup (slot 8):
// the old 2-argument typedef left OnComplete's cellIndex in whatever R9 held.
int ClearArea::OnCancel(CitizenComponent** citizen, ComponentEntity** treeEntry, int arg3) {
    ComponentEntity* treeCopy = *treeEntry;
    CitizenComponent* citizenCopy = *citizen;
    return OnComplete(&citizenCopy, &treeCopy, arg3);
}

// ============================================================
// ClearArea::Cleanup (0x1400e6650)
// ============================================================
// Iterates entity tree (+0x48): while count > 0, finds the
// rightmost (last) tree node, gets its entity's state machine
// component (+0x278), and removes the pending action (+0x48).
// Then does the same for cell tree (+0x58).
int ClearArea::Cleanup() {

    // Clean up entity tree (+0x48)
    while (field_50 > 0) {
        // Find rightmost (last) node in BST
        WorkEntitySetNode* node = field_48;
        WorkEntitySetNode* prev = node;
        while (node != nullptr) {
            prev = node;
            node = node->left;  // traverse left to find min
        }

        // prev is the leftmost node; its value (+0x08) is the citizen's
        // InternalPtr<CitizenComponent>
        CitizenComponent* citizen = prev->element.citizen;

        // Remove the pending action from the citizen's CharacterComponent action list (+0x48)
        WorkBase_TaskListCleanup_c5400(&citizen->character->actions);

        // Re-read count (RemovePendingAction may have modified it indirectly)
        // The tree node gets removed by the action system
    }

    // Clean up cell tree (+0x58)
    while (field_60 > 0) {
        // Find leftmost node in cell BST
        WorkCellSetNode* node = field_58;
        WorkCellSetNode* prev = node;
        while (node != nullptr) {
            prev = node;
            node = node->left;
        }

        // The node's citizen (+0x08); remove its pending action
        CitizenComponent* citizen = prev->element.citizen;
        WorkBase_TaskListCleanup_c5400(&citizen->character->actions);
    }
    return 0;
}

// ============================================================
// ClearArea::GetTarget (0x1400e6720)
// ============================================================
// Returns the profession list's +0x30 profession (GameUpdateLoop::m_professionList).
// Uses hidden return pointer ABI (member function returning non-trivial).
Profession** ClearArea::GetTarget(Profession** retbuf) {
    *retbuf = GameUpdateLoop::_singleton->m_professionList->field_0x30;
    return retbuf;
}

// ============================================================
// ClearArea::GetPriority (0x140029860)
// ============================================================
// Returns true (1) if tree count > 0, false (0) otherwise.
int ClearArea::GetPriority() {
    return (field_50 > 0) ? 1 : 0;
}

// ============================================================
// Execute's sub-functions, called by name (B3R)
// ============================================================
//   0x14012e5c0 WorkComponent_CalcMaxWorkers   0x14009fc00 WorkLeaf_GetTilesInArea_9fc00
//   0x14009d7c0 WorkLeaf_SortTilesByDistance_9d7c0  0x1400052c0 WorkBase_FindComponent_52c0
//   0x14009ccf0 WorkGrid_FindCell_9ccf0        0x1400a00f0 WorkBase_ValidateCell_a00f0
//   0x1400f6ef0 WorkImpl_BuildStructureStep_6ef0  0x14009f3e0 Reimpl_MapGetTerrainPos
//   0x1400c1eb0 WorkAct_PerformWorkCtor_c1eb0  0x1400bffd0 WorkAct_PerformWorkCtor_bffd0
//   0x140029920 WorkLeaf_BstInsertByIBP_29920  0x140029a40 WorkCellBST_Insert_9a40
//   0x1400f8620 WorkGrid_TryAssignWorkplace_f8620
//   slot 0x140152b68 System::MemoryAllocator::MemFree; DAT_140172114 kRdata_140172114
// (0x140002e10 / 0x140004680 are fn_FindComponent_WorkPlace / _Build, engine_funcs.h.)
// RawMaterialComponent (DAT_1401c7c80) and WorkComponent (DAT_1401c8070) are read
// through their classes' TypeId().

// ============================================================
// Tile info entry — 16 bytes per tile returned by GetTilesInArea
// ============================================================
struct TileInfo {
    uint32_t tileIndex;          // +0x00
    uint32_t pad04;              // +0x04
    ComponentEntity* entityPtr;  // +0x08: InternalBasePtr to entity on this tile (null if empty)
};
static_assert(sizeof(TileInfo) == 0x10, "TileInfo size mismatch");

// ============================================================
// ClearArea::Execute implementation
// ============================================================
int ClearArea::Execute(CitizenComponent** citizenIBP, System::Time* time) {

    // field_50 = tree count done, field_60 = ground count done
    int treesCleared = field_50;
    int groundCleared = field_60;


    // --- Check if work entity is valid ---
    // CheckPtr on field_18 (the WorkComponent InternalPtr at +0x18)
    WorkComponent* workEntity = field_18;

    // Get total slot count for the entity
    if (workEntity->workState == 0) {
        int count = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(workEntity));   // LEFT: SIG WorkComponent_CalcMaxWorkers
        workEntity->workState = count;
    }

    // If treesCleared + groundCleared >= totalSlots, all work done
    int totalSlots = workEntity->workState;
    if (treesCleared + groundCleared >= totalSlots) {
        return 1;
    }

    // --- Check the work entity's WorkPlaceComponent is accepting ---
    // (fn_FindComponent_WorkPlace, 0x140002e10)
    WorkPlaceComponent* workPlace = nullptr;
    fn_FindComponent_WorkPlace(field_10, &workPlace);

    if (workPlace != nullptr) {
        // Re-find and check if component is active (byte at +0x50)
        WorkPlaceComponent* workPlace2 = nullptr;
        fn_FindComponent_WorkPlace(field_10, &workPlace2);
        if (workPlace2->isAccepting == 0) {
            return 1;  // component not active
        }
    }

    // --- Get tile list for the clear area ---
    int result = 0;

    // Tile array (dynamic): ptr + count
    struct TileArray {
        TileInfo* ptr;
        int count;
        int _pad;
    };
    TileArray tileArray;
    tileArray.ptr = nullptr;
    tileArray.count = 0;
    tileArray._pad = 0;

    // The world map; the grid callees take it as a leading int*
    Map* map = GameUpdateLoop::_singleton->m_game;
    int* worldGrid = (int*)map;   // LEFT: SIG WorkGrid_* / WorkLeaf_* (Map passed as int*)

    // Read the tile-query filter flag.
    //
    // Retail 0x1400e5ee0 reads `cVar2 = *(char *)((longlong)param_1 + 0x71)`
    // here -- clearGround, NOT clearTrees (+0x70). The flag is the last
    // argument to GetTilesInArea and decides whether EMPTY tiles (entityPtr
    // == 0) are included in the result. Phase 3 below derefs
    // `tileEntity + 0x38` unguarded, exactly as retail does, which is only
    // safe because a clearGround=false query returns occupied tiles only.
    // Passing +0x70 instead let empty tiles into the array and phase 3
    // faulted on READ of 0x38.
    char includeEmptyTiles = field_71;  // +0x71 = clearGround flag

    // Find MapComponent on the work entity (fn_FindComponent_Build: the cell is MapComponent)
    MapComponent* area = nullptr;
    fn_FindComponent_Build(field_10, &area);

    // Read area bounds (shorts at +0x28, +0x2a, +0x2c, +0x2e)
    int16_t x1 = area->tileX0;
    int16_t y1 = area->tileY0;
    int16_t x2 = area->tileX1;
    int16_t y2 = area->tileY1;

    // Get tiles in the area
    WorkLeaf_GetTilesInArea_9fc00(worldGrid, (int)x1, (int)y1, (int)x2, (int)y2,
                                 &tileArray, includeEmptyTiles);

    int tileCount = tileArray.count;
    if (tileCount == 0) {
        goto done_nothing;
    }

    // --- Compute reference tile index for distance sorting ---
    {
        // CheckPtr on the citizen slot; CharacterComponent (+0x278) -> ModelComponent (+0x68)
        CharacterComponent* charComp = (*citizenIBP)->character;

        // ModelComponent's transform translation (+0x148 + 0x10)
        const Math::Vector3& pos = charComp->modelComponent->transform.translation;

        float posX = pos.x;
        float posY = pos.y;
        // posZ (+0x160) is stored but unused for ref calc

        int gridSize = map->size;
        int halfGrid = gridSize >> 1;

        // Convert world position to tile coordinates
        int tileX = (int)(posX + (float)halfGrid);
        int tileY = (int)(posY + (float)halfGrid);

        // Clamp to grid bounds
        if (tileX < 0) tileX = 0;
        if (tileX >= gridSize) tileX = gridSize - 1;
        if (tileY < 0) tileY = 0;
        if (tileY >= gridSize) tileY = gridSize - 1;

        uint32_t refTileIdx = (uint32_t)(gridSize * tileY + tileX);

        // Sort tiles by distance from citizen
        WorkLeaf_SortTilesByDistance_9d7c0(worldGrid, &tileArray, refTileIdx);

        // ============================================================
        // Phase 1: Clear trees (field_70)
        // ============================================================
        if (field_70 != 0 && tileCount > 0) {
            TileInfo* tiles = tileArray.ptr;
            for (int i = 0; i < tileCount; i++) {
                TileInfo* tile = &tiles[i];
                ComponentEntity* treeEntity = tile->entityPtr;

                if (treeEntity == nullptr) {
                    continue;  // no entity on this tile
                }

                // Check if tree entity is already in our tracking BST at +0x48
                // (the tree nodes stay raw: LEFT)
                WorkEntitySetNode* bstNode = field_48;
                bool alreadyTracked = false;
                while (bstNode != nullptr) {
                    ComponentEntity* nodeKey = bstNode->element.entity;
                    if (treeEntity < nodeKey) {
                        bstNode = bstNode->left;
                    } else {
                        if (treeEntity <= nodeKey) {
                            alreadyTracked = true;
                            break;  // found match
                        }
                        bstNode = bstNode->right;
                    }
                }
                if (alreadyTracked) {
                    continue;
                }

                // Check filter: if field_68 is set, the tree's description (+0x28) must match
                if (field_68 != nullptr) {
                    if (treeEntity->_description != field_68) {
                        continue;  // doesn't match filter
                    }
                }

                // Find NaturalResourceComponent on tree entity
                NaturalResourceComponent* natRes = treeEntity->FindComponent<NaturalResourceComponent>();

                if (natRes == nullptr) {
                    continue;
                }

                // Find valid cell near this tile
                int cellIdx = WorkGrid_FindCell_9ccf0(worldGrid, tile->tileIndex, refTileIdx, 0xe07);
                if (cellIdx < 0) {
                    continue;
                }

                // Validate cell is passable
                int valid = static_cast<int>(WorkBase_ValidateCell_a00f0(worldGrid, 0xe1f, (int)(uint32_t)cellIdx, refTileIdx));   // the door read EAX
                if (!(valid & 0xFF)) {
                    continue;
                }

                // Try to set up the build structure work
                CitizenComponent* actionStackPtr = *citizenIBP;
                int buildResult = static_cast<int>(StepProgress(&actionStackPtr,
                                                          (uint32_t)cellIdx, refTileIdx, time));   // the door read EAX
                if (!(buildResult & 0xFF)) {
                    // BuildStructure_impl returned false → result = 0 (no work assigned)
                    result = 0;
                    goto cleanup;
                }

                // Check if natRes component has "removable" flag (bit 20 of +0x20)
                if (natRes != nullptr) {
                    uint32_t flags = natRes->_flags;
                    if ((flags >> 20) & 1) {
                        continue;  // already flagged, skip
                    }
                }

                // Compute world position from cell index
                int gridSz = map->size;
                float halfOffset = (float)(gridSz >> 1) - kRdata_140172114;
                float worldX = (float)(cellIdx % gridSz) - halfOffset;
                float worldY = (float)(cellIdx / gridSz) - halfOffset;

                Math::Vector3 posVec(worldX, worldY, 0.0f);

                // Get nearby walkable position
                float walkPos[4];
                Reimpl_MapGetTerrainPos(map, walkPos, &posVec.x, 0, nullptr);

                // Create PerformWork action for tree removal
                NaturalResourceComponent* targetIBP = natRes;
                CitizenComponent* citizenRef = *citizenIBP;

                // PerformWork_Action_Ctor3(citizenIBP, workId, targetIBP, worldPos, 0, 0)
                WorkAct_PerformWorkCtor_c1eb0(&citizenRef, field_20, &targetIBP, walkPos, 0, 0);

                // Post-work setup on citizen
                int workId = field_20;
                CitizenComponent* citizenObj = *citizenIBP;

                // Call vtable[10] (+0x50) on this — Work::GetTarget(this, &workIBP)
                Profession* workIBP = nullptr;
                Profession* workEntity2 = *GetTarget(&workIBP);

                // Set fields on citizen: +0x30 = workId, +0x54 |= 4, +0xb0 = 0
                citizenObj->field_0x30 = workId;
                citizenObj->statusFlags |= 4;
                citizenObj->field_0xB0 = 0;

                // If the work's profession matches the citizen's (+0x1f0) and workId >= 0, set flag
                if (workEntity2 == citizenObj->profession && workId >= 0) {
                    citizenObj->happinessFlags |= 0x20000;
                }

                // Insert tree entity into tracking BST
                WorkEntityAssignment insertPair;
                insertPair.entity = treeEntity;
                insertPair.citizen = *citizenIBP;
                WorkLeaf_BstInsertByIBP_29920(&field_48, &workIBP, &insertPair);

                result = 2;
                goto cleanup;
            }
            // Exhausted all tiles in phase 1, reload tileCount
            tileCount = tileArray.count;
        }

        // ============================================================
        // Phase 2: Clear ground tiles (field_71)
        // ============================================================
        if (field_71 != 0 && tileCount > 0) {
            TileInfo* tiles = tileArray.ptr;
            for (int i = 0; i < tileCount; i++) {
                TileInfo* tile = &tiles[i];
                uint32_t tileIdx = tile->tileIndex;

                // Only process tiles with no entity
                if (tile->entityPtr != nullptr) continue;

                // Check if tile is already in ground tracking BST at +0x58
                WorkCellSetNode* bstNode = field_58;
                bool alreadyTracked = false;
                while (bstNode != nullptr) {
                    int nodeKey = bstNode->element.cell;
                    if ((int)tileIdx < nodeKey) {
                        bstNode = bstNode->left;
                    } else if ((int)tileIdx <= nodeKey) {
                        alreadyTracked = true;
                        break;
                    } else {
                        bstNode = bstNode->right;
                    }
                }
                if (alreadyTracked) {
                    tileCount = tileArray.count;
                    continue;
                }

                // Validate cell passability
                int passable = static_cast<int>(WorkBase_ValidateCell_a00f0(worldGrid, 0xe1f, (int)tileIdx, refTileIdx));   // the door read EAX
                if (!(passable & 0xFF)) {
                    tileCount = tileArray.count;
                    continue;
                }

                // A null entity InternalBasePtr
                int64_t nullIBP = 0;

                // Zero-init clear info
                struct { int64_t a; int32_t b; } clearInfo;
                clearInfo.a = 0;
                clearInfo.b = 0;

                // Compute world position
                int gridSz = map->size;
                float halfOffset = (float)(gridSz >> 1) - kRdata_140172114;
                float worldX = (float)((int)tileIdx % gridSz) - halfOffset;
                float worldY = (float)((int)tileIdx / gridSz) - halfOffset;

                Math::Vector3 posVec(worldX, worldY, 0.0f);

                float walkPos[4];
                Reimpl_MapGetTerrainPos(map, walkPos, &posVec.x, 0, nullptr);

                // Create PerformWork action for ground clearing
                CitizenComponent* citizenRef = *citizenIBP;
                int workId = field_20;

                WorkAct_PerformWorkCtor_bffd0(&citizenRef, workId, reinterpret_cast<uint32_t*>(walkPos),   // LEFT: SIG WorkAct_PerformWorkCtor_bffd0
                                     static_cast<int>(tileIdx),   // the door passed (uint64_t)tileIdx; the body reads R9D
                                     2, 0, tileIdx, 0, 0,
                                     reinterpret_cast<float*>(&clearInfo), &nullIBP);   // LEFT: SIG WorkAct_PerformWorkCtor_bffd0

                // Post-work setup on citizen
                int workIdSaved = field_20;
                CitizenComponent* citizenObj = *citizenIBP;

                // Call vtable[10] (+0x50) — Work::GetTarget
                Profession* workIBP = nullptr;
                Profession* workEntity2 = *GetTarget(&workIBP);

                citizenObj->field_0x30 = workIdSaved;
                citizenObj->statusFlags |= 4;
                citizenObj->field_0xB0 = 0;

                if (workEntity2 == citizenObj->profession && workIdSaved >= 0) {
                    citizenObj->happinessFlags |= 0x20000;
                }

                // Insert tile into ground tracking BST
                WorkCellAssignment groundPair;
                groundPair.cell = static_cast<int32_t>(tileIdx);
                groundPair._pad04 = 0;
                groundPair.citizen = *citizenIBP;
                WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>(&field_58), &workIBP, &groundPair);   // LEFT: SIG WorkCellBST_Insert_9a40

                result = 0;
                goto cleanup;
            }
        }

        // ============================================================
        // Phase 3: Clear natural resources (field_72)
        // ============================================================
        if (field_72 != 0 && tileCount > 0) {
            TileInfo* tiles = tileArray.ptr;
            for (int i = 0; i < tileCount; i++) {
                TileInfo* tile = &tiles[i];

                // CheckPtr on entity for this tile; its RawMaterialComponent
                // (DAT_1401c7c80) -- the FindComponent walk, inlined by retail
                RawMaterialComponent* natResComp = tile->entityPtr->FindComponent<RawMaterialComponent>();

                if (natResComp == nullptr) continue;

                // Now find the WorkComponent on the same entity
                WorkComponent* storageLocComp = tile->entityPtr->FindComponent<WorkComponent>();

                // Get the posted work id from the WorkComponent
                int resourceTypeId = storageLocComp->workId;

                // GameUpdateLoop +0x118, the work scheduler
                WorkScheduler* gatherSys = GameUpdateLoop::_singleton->m_tutorial;

                // Create citizen IBP for gather work
                CitizenComponent* citizenRef = *citizenIBP;

                int gatherResult = static_cast<int>(WorkGrid_TryAssignWorkplace_f8620(reinterpret_cast<uintptr_t>(gatherSys), &citizenRef,   // LEFT: SIG WorkGrid_TryAssignWorkplace_f8620
                                                          resourceTypeId, 0));   // the door read EAX
                if (gatherResult & 0xFF) {
                    result = 2;
                    goto cleanup;
                }
            }
        }
    }

done_nothing:
    result = 1;

cleanup:
    // Free the tile array
    if (tileArray.ptr) {
        System::MemoryAllocator::MemFree(tileArray.ptr);
    }
    tileArray.ptr = nullptr;
    tileArray.count = 0;
    return result;
}

// ============================================================
// ClearArea::Validate (0x1400e5b10) — slot 9
// ============================================================
// Validates both BSTs: entity tree at +0x48 and cell tree at +0x58.
// For each entry in the entity tree, calls FUN_1400f7100 to check
// if the citizen still has a matching CompleteTask action.
// Invalid entries are collected and removed from the entity tree.
// Then checks the cell tree at +0x58: if any entry is invalid,
// returns 0 immediately (early exit).
// Returns: 1 if all entries valid (or both BSTs empty), 0 otherwise.
//
// NOT HOOKED — bug class 3 (RSP capture), standalone exe only.
// ============================================================

// Sub-functions, called by name (B3R): 0x1400f7100 WorkBase_ValidateTask_f7100,
// 0x14001ca60 WorkCellBST_Remove_ca60.

int ClearArea::Validate() {

    // Dynamic array for entities to remove from tree at +0x48
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCap = 0;

    // ---- Phase 1: Validate entity tree at +0x48 ----
    WorkEntitySetNode* root48 = field_48;
    if (root48 != nullptr) {
        // Find leftmost node
        WorkEntitySetNode* node = root48;
        while (node->left != nullptr) {
            node = node->left;
        }

        // In-order traversal
        while (node != nullptr) {
            // The node's citizen (+0x08)
            CitizenComponent* citizen = node->element.citizen;
            // The tree collects the citizen; retail 0x1400e5c25..0x1400e5c34 collects the node's
            // key (the entity, +0x00): known_port_issues row 220.
            uintptr_t entityIBP = reinterpret_cast<uintptr_t>(citizen);   // LEFT: known_port_issues row 220 (the removal list holds the citizen)

            // Validate this entry
            uint8_t valid = ValidateTask(nullptr, &citizen, nullptr);

            if (valid == 0) {
                // Invalid — add to removal list
                uint32_t newCount = removeCount + 1;
                if (newCount != removeCount) {
                    if ((int)removeCap < (int)newCount) {
                        uint32_t newCap = newCount;
                        if (removeCount != 0) {
                            newCap = newCount * 3;
                        }
                        removeCap = newCap;
                        uintptr_t* newArr = static_cast<uintptr_t*>(System::MemoryAllocator::MemAlloc((uint64_t)newCap * 8, 8));

                        // Copy existing entries
                        uint32_t copyStart = 0;
                        if (removeArr == newArr) {
                            copyStart = removeCount;
                        }
                        for (uint32_t i = copyStart; i < newCount; i++) {
                            if (newArr != nullptr) {
                                uintptr_t src = 0;
                                if (i < removeCount && removeArr != nullptr) {
                                    src = removeArr[i];
                                } else {
                                    src = entityIBP;
                                }
                                newArr[i] = src;
                            }
                        }

                        if (removeArr != newArr) {
                            System::MemoryAllocator::MemFree(removeArr);
                        }
                        removeArr = newArr;
                    } else {
                        removeArr[removeCount] = entityIBP;
                    }
                    removeCount = newCount;
                }
            }

            // Move to in-order successor
            WorkEntitySetNode* right = node->right;
            if (right != nullptr) {
                node = right;
                while (node->left != nullptr) {
                    node = node->left;
                }
            } else {
                WorkEntitySetNode* parent = node->parent;
                if (parent != nullptr && parent->left != node) {
                    while (parent != nullptr &&
                           parent->right == node) {
                        node = parent;
                        parent = node->parent;
                    }
                    node = parent;
                } else {
                    node = parent;
                }
            }
        }
    }

    // ---- Phase 2: Validate cell tree at +0x58 ----
    WorkCellSetNode* root58 = field_58;
    if (root58 != nullptr) {
        // Find leftmost node
        WorkCellSetNode* node = root58;
        while (node->left != nullptr) {
            node = node->left;
        }

        // In-order traversal
        while (node != nullptr) {
            CitizenComponent* citizen = node->element.citizen;

            uint8_t valid = ValidateTask(nullptr, &citizen, nullptr);
            if (valid == 0) {
                // Cell tree has invalid entry — early return 0
                System::MemoryAllocator::MemFree(nullptr);
                System::MemoryAllocator::MemFree(removeArr);
                return 0;
            }

            // Move to in-order successor
            WorkCellSetNode* right = node->right;
            if (right != nullptr) {
                node = right;
                while (node->left != nullptr) {
                    node = node->left;
                }
            } else {
                WorkCellSetNode* parent = node->parent;
                if (parent != nullptr && parent->left != node) {
                    while (parent != nullptr &&
                           parent->right == node) {
                        node = parent;
                        parent = node->parent;
                    }
                    node = parent;
                } else {
                    node = parent;
                }
            }
        }
    }

    // ---- Phase 3: Remove invalid entries from entity tree ----
    if ((int)removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            WorkEntitySetNode* bstNode = field_48;
            while (bstNode != nullptr) {
                uintptr_t nodeKey = reinterpret_cast<uintptr_t>(bstNode->element.entity);   // LEFT: known_port_issues row 220 (compared with the collected citizens)
                if (removeArr[i] < nodeKey) {
                    bstNode = bstNode->left;
                } else {
                    if (removeArr[i] <= nodeKey) {
                        // Found — remove it
                        WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(&field_48), bstNode);   // LEFT: SIG WorkCellBST_Remove_ca60
                        break;
                    }
                    bstNode = bstNode->right;
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(nullptr);
    System::MemoryAllocator::MemFree(removeArr);
    return 1;
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_ClearArea_Vtable` (slot 1 answered null, slot 2 a
// lazily cached id), REGISTER_TYPE over ClearArea_Factory / ClearArea_CreateInPlace.

// create 0x140029820: MemAlloc(sizeof, 8), then the ctor.
void* ClearArea::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClearArea), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ClearArea());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* ClearArea::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ClearArea());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(ClearArea, "Work")
