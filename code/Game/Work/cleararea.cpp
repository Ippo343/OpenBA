// code/Game/Work/cleararea.cpp
// Clean reimplementation of ClearArea Work action.
//
// ClearArea (0x78 bytes) -- area clearing work action.
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// Ctor: 0x1400e68d0, Vtable: 0x140161110
//
// Fields:
//   +0x48: void* treeRoot   -- BST of InternalBasePtrs (entities to clear)
//   +0x50: int treeCount    -- number of entries in +0x48 tree
//   +0x58: void* cellRoot   -- BST of cell indices (ground cells to clear)
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
      field_68(reinterpret_cast<void*>(descRef)),
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
    void* cellRoot = field_58;
    if (cellRoot) {
        // Recursively free left subtree, then right subtree, then root
        WorkBase_TreeNodeFree_e320(&field_58, *(void**)((uintptr_t)cellRoot + 0x18));
        WorkBase_TreeNodeFree_e320(&field_58, *(void**)((uintptr_t)cellRoot + 0x20));
        System::MemoryAllocator::MemFree(cellRoot);
    }
    field_58 = nullptr;
    field_60 = 0;

    // Free entity tree (+0x48)
    void* treeRoot = field_48;
    if (treeRoot) {
        WorkBase_TreeNodeFree_e320(&field_48, *(void**)((uintptr_t)treeRoot + 0x18));
        WorkBase_TreeNodeFree_e320(&field_48, *(void**)((uintptr_t)treeRoot + 0x20));
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
void ClearArea::Persist(void* stream) {

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create Serializer with version=1
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Serialize entity BST at +0x48
    WorkLeaf_BstSerializeIBPpair_29c10(&field_48, serializerBuf);

    // Serialize map entity InternalBasePtr at +0x68
    Reimpl_ExternalPtr_Serialize_StringTable(&field_68, serializerBuf);

    // Serialize clearTrees bool at +0x70
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<bool*>(&field_70), nullptr);   // B.3.4 bridge

    // Version >= 1: serialize additional fields
    int version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        // Serialize cell BST at +0x58
        WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>(&field_58), serializerBuf);

        // Serialize clearGround bool at +0x71
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<bool*>(&field_71), nullptr);   // B.3.4 bridge

        // Serialize clearBuildings bool at +0x72
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<bool*>(&field_72), nullptr);   // B.3.4 bridge
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// ClearArea::OnComplete (0x1400e5e30)
// ============================================================
// Called when a work task completes. Inserts completed entry into
// the entity tree (+0x48) if entry is non-null, then searches
// the cell tree (+0x58) for the given cellIndex and removes it.
//
// Params: self, citizen (unused), treeEntry (IBP*), cellIndex
int ClearArea::OnComplete(void* /*citizen*/, void* treeEntry, int cellIndex) {

    // If treeEntry is non-null, insert into entity BST at +0x48
    uintptr_t entryPtr = treeEntry ? *(uintptr_t*)treeEntry : 0;
    if (entryPtr != 0) {
        WorkLeaf_BstRemoveByIBP_298a0(&field_48, treeEntry);
    }

    // Search cell BST (+0x58) for cellIndex and remove if found
    void* node = field_58;
    void* root = node;
    if (node == nullptr) return 0;

    // BST search for cellIndex
    while (node != nullptr) {
        int nodeVal = *(int*)node;
        if (cellIndex < nodeVal) {
            node = *(void**)((uintptr_t)node + 0x18);  // left child
        } else if (cellIndex > nodeVal) {
            node = *(void**)((uintptr_t)node + 0x20);  // right child
        } else {
            // Found — now search again from root and remove
            if (root == nullptr) return 0;
            void* searchNode = root;
            while (searchNode != nullptr) {
                int searchVal = *(int*)searchNode;
                if (cellIndex < searchVal) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                } else if (cellIndex > searchVal) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x20);
                } else {
                    // Remove this node from cell tree
                    WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(&field_58), searchNode);
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
int ClearArea::OnCancel(void* citizen, void* treeEntry, int arg3) {
    uintptr_t treeCopy = *(uintptr_t*)treeEntry;
    uintptr_t citizenCopy = *(uintptr_t*)citizen;
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
        void* node = field_48;
        void* prev = node;
        while (node != nullptr) {
            prev = node;
            node = *(void**)((uintptr_t)node + 0x18);  // traverse left to find min
        }

        // prev is the leftmost node
        // Get entity from node (+0x08 = InternalBasePtr)
        uintptr_t entity = *(uintptr_t*)((uintptr_t)prev + 8);

        // Get state machine component: entity->field_278
        uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

        // Remove pending action from state machine action list (+0x48)
        WorkBase_TaskListCleanup_c5400((void*)(smComp + 0x48));

        // Re-read count (RemovePendingAction may have modified it indirectly)
        // The tree node gets removed by the action system
    }

    // Clean up cell tree (+0x58)
    while (field_60 > 0) {
        // Find leftmost node in cell BST
        void* node = field_58;
        void* prev = node;
        while (node != nullptr) {
            prev = node;
            node = *(void**)((uintptr_t)node + 0x18);
        }

        // Get entity from node (+0x08)
        uintptr_t entity = *(uintptr_t*)((uintptr_t)prev + 8);

        // Get state machine component and remove pending action
        uintptr_t smComp = *(uintptr_t*)(entity + 0x278);
        WorkBase_TaskListCleanup_c5400((void*)(smComp + 0x48));
    }
    return 0;
}

// ============================================================
// ClearArea::GetTarget (0x1400e6720)
// ============================================================
// Returns the map entity from gameWorld singleton:
//   gameWorld->field_b0->field_30
// Uses hidden return pointer ABI (member function returning non-trivial).
void* ClearArea::GetTarget(void* retbuf) {
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t mapHolder = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)retbuf = *(uintptr_t*)(mapHolder + 0x30);
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
    uint32_t tileIndex;     // +0x00
    uint32_t pad04;         // +0x04
    uint64_t entityPtr;     // +0x08: InternalBasePtr to entity on this tile (0 if empty)
};
static_assert(sizeof(TileInfo) == 0x10, "TileInfo size mismatch");

// ============================================================
// ClearArea::Execute implementation
// ============================================================
int ClearArea::Execute(void* actionStack, void* time) {

    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // field_50 = tree count done, field_60 = ground count done
    int treesCleared = field_50;
    int groundCleared = field_60;


    // --- Check if work entity is valid ---
    // CheckPtr on field_18 (work entity IBP at +0x18, which is param_1[3] in decompilation)
    void* workEntity = field_18;

    // Get total slot count for the entity
    if (*(int*)((uintptr_t)workEntity + 0x30) == 0) {
        int count = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(workEntity));
        *(int*)((uintptr_t)workEntity + 0x30) = count;
    }

    // If treesCleared + groundCleared >= totalSlots, all work done
    int totalSlots = *(int*)((uintptr_t)workEntity + 0x30);
    if (treesCleared + groundCleared >= totalSlots) {
        return 1;
    }

    // --- Check citizen entity has active build state ---
    void* citizenEntity = field_10;

    // One-shot: dump the owner entity (field_10) component type-ids so we know what
    // it is and whether the WorkPlaceComponent gate legitimately applies.

    // Find WorkplaceComponent (FUN_140002e10 = FindComponent_BuildState2)
    uint64_t buildStateIBP = 0;
    fn_FindComponent_WorkPlace(citizenEntity, &buildStateIBP);

    if (buildStateIBP != 0) {
        // Re-find and check if component is active (byte at +0x50)
        uint64_t buildStateIBP2 = 0;
        fn_FindComponent_WorkPlace(field_10, &buildStateIBP2);
        void* bsComp = reinterpret_cast<void*>(buildStateIBP2);
        if (*(char*)((uintptr_t)bsComp + 0x50) == 0) {
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

    // Get WorldGrid from game singleton
    void* gameWorld = GameUpdateLoop::_singleton;
    void* worldGrid = *(void**)((uintptr_t)gameWorld + 0xF8);

    // Get citizen entity and read the tile-query filter flag.
    //
    // Retail 0x1400e5ee0 reads `cVar2 = *(char *)((longlong)param_1 + 0x71)`
    // here -- clearGround, NOT clearTrees (+0x70). The flag is the last
    // argument to GetTilesInArea and decides whether EMPTY tiles (entityPtr
    // == 0) are included in the result. Phase 3 below derefs
    // `tileEntity + 0x38` unguarded, exactly as retail does, which is only
    // safe because a clearGround=false query returns occupied tiles only.
    // Passing +0x70 instead let empty tiles into the array and phase 3
    // faulted on READ of 0x38.
    uint64_t citizenIBP = reinterpret_cast<uint64_t>(field_10);
    char includeEmptyTiles = field_71;  // +0x71 = clearGround flag

    // Find BuildComponent on citizen entity
    uint64_t buildCompIBP = 0;
    fn_FindComponent_Build(reinterpret_cast<void*>(citizenIBP), &buildCompIBP);
    void* buildComp = reinterpret_cast<void*>(buildCompIBP);

    // Read area bounds from build component (short values at +0x28, +0x2a, +0x2c, +0x2e)
    int16_t x1 = *(int16_t*)((uintptr_t)buildComp + 0x28);
    int16_t y1 = *(int16_t*)((uintptr_t)buildComp + 0x2a);
    int16_t x2 = *(int16_t*)((uintptr_t)buildComp + 0x2c);
    int16_t y2 = *(int16_t*)((uintptr_t)buildComp + 0x2e);

    // Get tiles in the area
    WorkLeaf_GetTilesInArea_9fc00(reinterpret_cast<int*>(worldGrid), (int)x1, (int)y1, (int)x2, (int)y2,
                                 &tileArray, includeEmptyTiles);

    int tileCount = tileArray.count;
    if (tileCount == 0) {
        goto done_nothing;
    }

    // --- Compute reference tile index for distance sorting ---
    {
        // CheckPtr on actionStack
        void* citizen = *(void**)actionStack;

        // Get CharacterComponent (+0x278) -> PositionComponent (+0x68)
        void* charComp = *(void**)((uintptr_t)citizen + 0x278);

        void* posComp = *(void**)((uintptr_t)charComp + 0x68);

        // Read position (floats at +0x158, +0x15c, +0x160)
        float posX = *(float*)((uintptr_t)posComp + 0x158);
        float posY = *(float*)((uintptr_t)posComp + 0x15c);
        // posZ = *(float*)((uintptr_t)posComp + 0x160);  // stored but unused for ref calc

        int gridSize = *(int*)worldGrid;
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
        WorkLeaf_SortTilesByDistance_9d7c0(reinterpret_cast<int*>(worldGrid), &tileArray, refTileIdx);

        // ============================================================
        // Phase 1: Clear trees (field_70)
        // ============================================================
        if (field_70 != 0 && tileCount > 0) {
            TileInfo* tiles = tileArray.ptr;
            for (int i = 0; i < tileCount; i++) {
                TileInfo* tile = &tiles[i];
                uint64_t treeEntity = tile->entityPtr;

                if (treeEntity == 0) {
                    continue;  // no entity on this tile
                }

                // Check if tree entity is already in our tracking BST at +0x48
                void* bstNode = field_48;
                bool alreadyTracked = false;
                while (bstNode != nullptr) {
                    uint64_t nodeKey = *(uint64_t*)bstNode;
                    if (treeEntity < nodeKey) {
                        bstNode = *(void**)((uintptr_t)bstNode + 0x18);  // left child
                    } else {
                        if (treeEntity <= nodeKey) {
                            alreadyTracked = true;
                            break;  // found match
                        }
                        bstNode = *(void**)((uintptr_t)bstNode + 0x20);  // right child
                    }
                }
                if (alreadyTracked) {
                    continue;
                }

                // Check filter: if field_68 is set, tree's +0x28 must match
                if (field_68 != nullptr) {
                    void* treeEntityObj = reinterpret_cast<void*>(treeEntity);
                    int64_t treeType = *(int64_t*)((uintptr_t)treeEntityObj + 0x28);
                    if (treeType != reinterpret_cast<int64_t>(field_68)) {
                        continue;  // doesn't match filter
                    }
                }

                // Find NaturalResource component on tree entity
                uint64_t natResIBP = 0;
                WorkBase_FindComponent_52c0(reinterpret_cast<uintptr_t>(treeEntity), &natResIBP);

                if (natResIBP == 0) {
                    continue;
                }

                // Find valid cell near this tile
                int cellIdx = WorkGrid_FindCell_9ccf0(reinterpret_cast<int*>(worldGrid), tile->tileIndex, refTileIdx, 0xe07);
                if (cellIdx < 0) {
                    continue;
                }

                // Validate cell is passable
                int valid = static_cast<int>(WorkBase_ValidateCell_a00f0(reinterpret_cast<int*>(worldGrid), 0xe1f, (int)(uint32_t)cellIdx, refTileIdx));   // the door read EAX
                if (!(valid & 0xFF)) {
                    continue;
                }

                // Try to set up the build structure work
                void* actionStackPtr = *(void**)actionStack;
                int buildResult = static_cast<int>(WorkImpl_BuildStructureStep_6ef0(reinterpret_cast<uintptr_t>(this), &actionStackPtr,
                                                          (uint32_t)cellIdx, refTileIdx, time));   // the door read EAX
                if (!(buildResult & 0xFF)) {
                    // BuildStructure_impl returned false → result = 0 (no work assigned)
                    result = 0;
                    goto cleanup;
                }

                // Check if natRes component has "removable" flag (bit 20 of +0x20)
                if (natResIBP != 0) {
                    void* natResObj = reinterpret_cast<void*>(natResIBP);
                    uint32_t flags = *(uint32_t*)((uintptr_t)natResObj + 0x20);
                    if ((flags >> 20) & 1) {
                        continue;  // already flagged, skip
                    }
                }

                // Compute world position from cell index
                int gridSz = *(int*)worldGrid;
                float halfOffset = (float)(gridSz >> 1) - kRdata_140172114;
                float worldX = (float)(cellIdx % gridSz) - halfOffset;
                float worldY = (float)(cellIdx / gridSz) - halfOffset;

                Math::Vector3 posVec(worldX, worldY, 0.0f);

                // Get nearby walkable position
                float walkPos[4];
                Reimpl_MapGetTerrainPos(worldGrid, walkPos, &posVec.x, 0, nullptr);

                // Create PerformWork action for tree removal
                uint64_t targetIBP = natResIBP;
                uint64_t citizenRef = reinterpret_cast<uint64_t>(*(void**)actionStack);

                // PerformWork_Action_Ctor3(citizenIBP, workId, targetIBP, worldPos, 0, 0)
                WorkAct_PerformWorkCtor_c1eb0(&citizenRef, field_20, &targetIBP, walkPos, 0, 0);

                // Post-work setup on citizen
                int workId = field_20;
                void* citizenObj = *(void**)actionStack;

                // Call vtable[10] (+0x50) on this — Work::GetTarget(this, &workIBP)
                uint64_t workIBP = 0;
                void* workResult = GetTarget(&workIBP);
                uint64_t workEntity2 = *(uint64_t*)workResult;

                // Set fields on citizen: +0x30 = workId, +0x54 |= 4, +0xb0 = 0
                *(int*)((uintptr_t)citizenObj + 0x30) = workId;
                *(uint32_t*)((uintptr_t)citizenObj + 0x54) |= 4;
                *(uint32_t*)((uintptr_t)citizenObj + 0xb0) = 0;

                // If work entity matches citizen's +0x1f0 and workId >= 0, set flag
                if (workEntity2 == *(uint64_t*)((uintptr_t)citizenObj + 0x1f0) && workId >= 0) {
                    *(uint32_t*)((uintptr_t)citizenObj + 0x88) |= 0x20000;
                }

                // Insert tree entity into tracking BST
                struct { uint64_t key; uint64_t value; } insertPair;
                insertPair.key = treeEntity;
                insertPair.value = reinterpret_cast<uint64_t>(*(void**)actionStack);
                WorkLeaf_BstInsertByIBP_29920(reinterpret_cast<void*>(s + 0x48), &workIBP,
                                        reinterpret_cast<void*>(&insertPair));

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
                if (tile->entityPtr != 0) continue;

                // Check if tile is already in ground tracking BST at +0x58
                void* bstNode = field_58;
                bool alreadyTracked = false;
                while (bstNode != nullptr) {
                    int nodeKey = *(int*)bstNode;
                    if ((int)tileIdx < nodeKey) {
                        bstNode = *(void**)((uintptr_t)bstNode + 0x18);
                    } else if ((int)tileIdx <= nodeKey) {
                        alreadyTracked = true;
                        break;
                    } else {
                        bstNode = *(void**)((uintptr_t)bstNode + 0x20);
                    }
                }
                if (alreadyTracked) {
                    tileCount = tileArray.count;
                    continue;
                }

                // Validate cell passability
                int passable = static_cast<int>(WorkBase_ValidateCell_a00f0(reinterpret_cast<int*>(worldGrid), 0xe1f, (int)tileIdx, refTileIdx));   // the door read EAX
                if (!(passable & 0xFF)) {
                    tileCount = tileArray.count;
                    continue;
                }

                // Create InternalBasePtr for null entity
                uint64_t nullIBP = 0;
                new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

                // Zero-init clear info
                struct { int64_t a; int32_t b; } clearInfo;
                clearInfo.a = 0;
                clearInfo.b = 0;

                // Compute world position
                int gridSz = *(int*)worldGrid;
                float halfOffset = (float)(gridSz >> 1) - kRdata_140172114;
                float worldX = (float)((int)tileIdx % gridSz) - halfOffset;
                float worldY = (float)((int)tileIdx / gridSz) - halfOffset;

                Math::Vector3 posVec(worldX, worldY, 0.0f);

                float walkPos[4];
                Reimpl_MapGetTerrainPos(worldGrid, walkPos, &posVec.x, 0, nullptr);

                // Create PerformWork action for ground clearing
                void* actionStackPtrForWork = *(void**)actionStack;
                uint64_t citizenRef = reinterpret_cast<uint64_t>(actionStackPtrForWork);
                int workId = field_20;

                WorkAct_PerformWorkCtor_bffd0(&citizenRef, workId, reinterpret_cast<uint32_t*>(walkPos),
                                     static_cast<int>(tileIdx),   // the door passed (uint64_t)tileIdx; the body reads R9D
                                     2, 0, tileIdx, 0, 0,
                                     reinterpret_cast<float*>(&clearInfo), reinterpret_cast<int64_t*>(&nullIBP));

                // Post-work setup on citizen
                int workIdSaved = field_20;
                void* citizenObj = *(void**)actionStack;

                // Call vtable[10] (+0x50) — Work::GetTarget
                uint64_t workIBP = 0;
                void* workResult = GetTarget(&workIBP);
                uint64_t workEntity2 = *(uint64_t*)workResult;

                *(int*)((uintptr_t)citizenObj + 0x30) = workIdSaved;
                *(uint32_t*)((uintptr_t)citizenObj + 0x54) |= 4;
                *(uint32_t*)((uintptr_t)citizenObj + 0xb0) = 0;

                if (workEntity2 == *(uint64_t*)((uintptr_t)citizenObj + 0x1f0) && workIdSaved >= 0) {
                    *(uint32_t*)((uintptr_t)citizenObj + 0x88) |= 0x20000;
                }

                // Insert tile into ground tracking BST
                struct { uint32_t tileKey; uint32_t pad; uint64_t citizenRef; } groundPair;
                groundPair.tileKey = tileIdx;
                groundPair.pad = 0;
                groundPair.citizenRef = reinterpret_cast<uint64_t>(*(void**)actionStack);
                WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>(s + 0x58), &workIBP,
                                           reinterpret_cast<void*>(&groundPair));

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

                // CheckPtr on entity for this tile
                void* tileEntity = reinterpret_cast<void*>(tile->entityPtr);

                // Search entity's component array for NaturalResourceDescription type
                uintptr_t te = (uintptr_t)tileEntity;
                int compCount = *(int*)(te + 0x38);
                uintptr_t compArray = *(uintptr_t*)(te + 0x30);

                // Find RawMaterialComponent (DAT_1401c7c80; ENDGAME B.3.9 D5: by cell,
                // not the legacy local name "NaturalResourceDesc")
                void* natResComp = nullptr;
                for (int j = 0; j < compCount; j++) {
                    void** compIBP = reinterpret_cast<void**>(compArray + (uintptr_t)j * 8);
                    void* comp = *compIBP;

                    uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)

                    if (typeId == RawMaterialComponent::TypeId()) {
                        // Verify with IsDerivedFrom
                        void** compIBP2 = reinterpret_cast<void**>(
                            *(uintptr_t*)(te + 0x30) + (uintptr_t)j * 8);
                        void* comp2 = *compIBP2;
                        if (Object::IsDerivedFromNullSafe(comp2, RawMaterialComponent::TypeId())) {
                            natResComp = comp2;
                        }
                        break;  // goto LAB_1400e6532
                    }
                }

                // Create IBP for natRes component
                uint64_t natResIBP = 0;
                new (&natResIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(natResComp));   // B.3.3 bridge

                if (natResIBP == 0) continue;

                // Now find StorageLocationComponent on same entity
                tileEntity = reinterpret_cast<void*>(tile->entityPtr);
                te = (uintptr_t)tileEntity;
                compCount = *(int*)(te + 0x38);
                compArray = *(uintptr_t*)(te + 0x30);

                void* storageLocComp = nullptr;
                for (int j = 0; j < compCount; j++) {
                    void** compIBP = reinterpret_cast<void**>(compArray + (uintptr_t)j * 8);
                    void* comp = *compIBP;

                    uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)

                    if (typeId == WorkComponent::TypeId()) {
                        void** compIBP2 = reinterpret_cast<void**>(
                            *(uintptr_t*)(te + 0x30) + (uintptr_t)j * 8);
                        void* comp2 = *compIBP2;
                        if (Object::IsDerivedFromNullSafe(comp2, WorkComponent::TypeId())) {
                            storageLocComp = comp2;
                        }
                        break;
                    }
                }

                uint64_t storageIBP = 0;
                new (&storageIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(storageLocComp));   // B.3.3 bridge

                // Get resource type ID from storage location component
                int resourceTypeId = *(int*)(storageIBP + 0x28);

                // Get gather system from game singleton
                void* gameWorld2 = GameUpdateLoop::_singleton;
                void* gatherSys = *(void**)((uintptr_t)gameWorld2 + 0x118);

                // Create citizen IBP for gather work
                uint64_t citizenRef = reinterpret_cast<uint64_t>(*(void**)actionStack);

                int gatherResult = static_cast<int>(WorkGrid_TryAssignWorkplace_f8620(reinterpret_cast<uintptr_t>(gatherSys), &citizenRef,
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

    uintptr_t s = (uintptr_t)this;

    // Dynamic array for entities to remove from tree at +0x48
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCap = 0;

    // ---- Phase 1: Validate entity tree at +0x48 ----
    void* root48 = field_48;
    if (root48 != nullptr) {
        // Find leftmost node
        void* node = root48;
        while (*(void**)((uintptr_t)node + 0x18) != nullptr) {
            node = *(void**)((uintptr_t)node + 0x18);
        }

        // In-order traversal
        while (node != nullptr) {
            // Read entity IBP from node+0x08
            uintptr_t entityIBP = *(uintptr_t*)((uintptr_t)node + 0x08);

            // Validate this entry
            uint8_t valid = WorkBase_ValidateTask_f7100((void*)s, nullptr,
                                                  (void*)&entityIBP, nullptr);

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
                        uintptr_t* newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((uint64_t)newCap * 8, 8);

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
            void* right = *(void**)((uintptr_t)node + 0x20);
            if (right != nullptr) {
                node = right;
                while (*(void**)((uintptr_t)node + 0x18) != nullptr) {
                    node = *(void**)((uintptr_t)node + 0x18);
                }
            } else {
                void* parent = *(void**)((uintptr_t)node + 0x10);
                if (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) != node) {
                    while (parent != nullptr &&
                           *(void**)((uintptr_t)parent + 0x20) == node) {
                        node = parent;
                        parent = *(void**)((uintptr_t)node + 0x10);
                    }
                    node = parent;
                } else {
                    node = parent;
                }
            }
        }
    }

    // ---- Phase 2: Validate cell tree at +0x58 ----
    void* root58 = field_58;
    if (root58 != nullptr) {
        // Find leftmost node
        void* node = root58;
        while (*(void**)((uintptr_t)node + 0x18) != nullptr) {
            node = *(void**)((uintptr_t)node + 0x18);
        }

        // In-order traversal
        while (node != nullptr) {
            uintptr_t entityIBP = *(uintptr_t*)((uintptr_t)node + 0x08);

            uint8_t valid = WorkBase_ValidateTask_f7100((void*)s, nullptr,
                                                  (void*)&entityIBP, nullptr);
            if (valid == 0) {
                // Cell tree has invalid entry — early return 0
                System::MemoryAllocator::MemFree(nullptr);
                System::MemoryAllocator::MemFree(removeArr);
                return 0;
            }

            // Move to in-order successor
            void* right = *(void**)((uintptr_t)node + 0x20);
            if (right != nullptr) {
                node = right;
                while (*(void**)((uintptr_t)node + 0x18) != nullptr) {
                    node = *(void**)((uintptr_t)node + 0x18);
                }
            } else {
                void* parent = *(void**)((uintptr_t)node + 0x10);
                if (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) != node) {
                    while (parent != nullptr &&
                           *(void**)((uintptr_t)parent + 0x20) == node) {
                        node = parent;
                        parent = *(void**)((uintptr_t)node + 0x10);
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
            void* bstNode = field_48;
            while (bstNode != nullptr) {
                uintptr_t nodeKey = *(uintptr_t*)bstNode;
                if (removeArr[i] < nodeKey) {
                    bstNode = *(void**)((uintptr_t)bstNode + 0x18); // left
                } else {
                    if (removeArr[i] <= nodeKey) {
                        // Found — remove it
                        WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(&field_48), bstNode);
                        break;
                    }
                    bstNode = *(void**)((uintptr_t)bstNode + 0x20); // right
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
#include "address_registry.h"

DEFINE_EXE_TYPE(ClearArea, "Work")
