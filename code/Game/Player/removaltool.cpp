// code/Game/Player/removaltool.cpp
// Clean reimplementation of RemovalTool vtable methods from Application-x64.exe.
//
// RemovalTool (0x40 bytes) extends DragTool (0x38 bytes).
// Handles building/item removal: selects area, marks entities for removal.
//
// Vtable at 0x140161de0 (15 slots). Three are reimplemented here:
//   Slot0  Dtor     (0x14002e7f0) — scalar deleting destructor
//   Slot4  Startup  (0x140104bd0) — find RemovalDescription component on entity
//   Slot14 CalcCost (0x140103ef0) — calculate removal area dimensions and validate
//
// Decompiled from Application-x64.exe.

#include "Player/RemovalTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "Component/CreateRemovalDescription.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// RemovalTool::DeletingDtor (vtable +0x00) — 0x14002e7f0
// ============================================================
// ENDGAME B.3.8 TOOL: the hand-rolled RemovalTool_Dtor restamped the Removal,
// DragTool and Tool tables read by address — the last two UNREGISTERED, so it
// stored a NULL vptr before ~Internal (B38 D9 #3). The compiler's dtor chain
// restamps the real tables, then runs Object::Internal::~Internal.
Object::Base* RemovalTool::DeletingDtor(unsigned flags) {
    this->~RemovalTool();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// RemovalTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Searches the entity's component list for a component matching the
// CreateRemovalDescription type ID (cell 0x1401c74a0; the global name
// "RemovalDescription" lies — D5, Table 1/3). Stores the found
// component pointer (or nullptr) in self->removalDesc (+0x38).
//
// The entity at self->descriptionPtr (+0x20) is a ComponentEntity with:
//   +0x28: void** componentArray  — array of component pointers
//   +0x30: int    componentCount  — number of components
//
// Each component has a vtable with GetTypeId at +0x10 (returns uint32_t).
// After finding a match by type ID, fn_IsDerivedFrom confirms the cast.
//
// Original: 0x140104bd0
void RemovalTool::Startup() {
    uintptr_t entity = (uintptr_t)descriptionPtr;
    void* result = nullptr;

    if (!entity || !fn_IsDerivedFrom) {
        removalDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = CreateRemovalDescription::TypeId();

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // Virtual slot 2 (+0x10): Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom before casting
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                result = comp;
            }
            break;
        }
    }

    removalDesc = result;
}

// ============================================================
// RemovalTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Calculates removal area dimensions between startCell and endCell.
//
// Steps:
//   1. Convert cell indices to grid (row, col) using map grid size
//   2. Compute min/max row/col for the bounding rectangle
//   3. Set output params: width = (maxCol-minCol+1), height = (maxRow-minRow+1)
//   4. If the description entity (+0x10) has constraints (+0x80 flag), verify
//      that the selection matches the constrained area exactly
//
// The field at self+0x10 (dialogPtr in DragTool) points to the description entity
// in RemovalTool's context, which has constraint fields:
//   +0x80: char hasConstraints — nonzero if removal area is constrained
//   +0x98: int constrainStartCell — constrained start cell index
//   +0x9c: int constrainEndCell — constrained end cell index
//
// Return value semantics:
//   0 = success (removal can proceed)
//   3 = failure (selection doesn't match constrained area)
//
// Original: 0x140103ef0
int RemovalTool::CalcCost(int startCell, int endCell, uint32_t* outValid,
                          int* outCostB, int* outCostA, int* outLength) {
    // Get map grid size from GameWorld
    int gridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

    // Convert cell indices to (row, col) grid coordinates
    int startRow = startCell / gridSize;
    int startCol = startCell % gridSize;
    int endRow   = endCell / gridSize;
    int endCol   = endCell % gridSize;

    // Compute min/max to get bounding rectangle
    int minCol = startCol, maxCol = endCol;
    if (endCol < startCol) {
        minCol = endCol;
        maxCol = startCol;
    }

    int minRow = startRow, maxRow = endRow;
    if (endRow < startRow) {
        minRow = endRow;
        maxRow = startRow;
    }

    // Set output params: dimensions of removal area
    *outLength = 1;
    *outCostB = (maxCol - minCol) + 1;   // width in columns
    *outCostA = (maxRow - minRow) + 1;   // height in rows
    *outValid = 0;

    // Check constraints from description entity at +0x10
    uintptr_t descEntity = (uintptr_t)dialogPtr;
    if (*(char*)(descEntity + 0x80) != '\0') {
        // Constrained removal — verify selection matches the constrained area exactly
        int cGridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

        int constrainStartCell = *(int*)(descEntity + 0x98);
        int constrainEndCell   = *(int*)(descEntity + 0x9c);

        int cStartCol = constrainStartCell % cGridSize;
        int cStartRow = constrainStartCell / cGridSize;
        int cEndCol   = constrainEndCell % cGridSize;
        int cEndRow   = constrainEndCell / cGridSize;

        if (minCol != cStartCol || minRow != cStartRow ||
            cEndCol != maxCol   || cEndRow != maxRow) {
            return 3;
        }
    }

    return 0;
}

// ============================================================
// Tile entry for rendering (0x14 bytes = 5 uint32_t)
// ============================================================
struct TileRenderEntry {
    uint32_t col;
    uint32_t row;
    uint32_t type;     // 0 = flat tile, 1 = road tile, 3 = removable
    uint32_t color;    // ARGB packed color
    uint32_t extra;    // 0
};
static_assert(sizeof(TileRenderEntry) == 0x14, "TileRenderEntry size mismatch");

// Dynamic array holding TileRenderEntry items (matches Ghidra pattern)
// ENDGAME B.3.2 step 2: now the real `Collection::Array<TileRenderEntry>` (trait-
// derived row: stride 0x14, align 4, byteCopy, init == nullptr, destroy == nullptr).
// The hand-written PushTileEntry grow below was compared arm for arm with
// rtcoll::SetSizeCore: `newCount == count` early return == the reference's
// exact-equal return (only reachable at INT_MAX for a +1 append); one append only,
// so `(count == 0) ? n : n*3` is the reference capacity rule; MemAlloc(cap * 0x14,
// align 4); prefix copied into the fresh block (== only when the block moved); the
// fill stored at [count]; old block freed when it moved; in-place path stores at
// [count]. Out-of-memory-only difference: this body dereferenced a null block,
// SetSizeCore skips the null element address.
using TileArray = Collection::Array<TileRenderEntry>;

// ============================================================
// Helper: push a TileRenderEntry into a dynamic array
// ============================================================
static void PushTileEntry(TileArray* arr, const TileRenderEntry* entry) {
    arr->SetSize(arr->_size + 1, *entry);
}

// ============================================================
// Resolved exe-internal function pointers for Draw
// ============================================================

// RemovalTool_sub_48a0: checks if entity at tile is removable by this tool
// Returns true if removable.
typedef bool (*fn_RemovalSub48a0_t)(uintptr_t self, void* entityIBP, uint32_t col, uint32_t row);
static fn_RemovalSub48a0_t s_fn_RemovalSub48a0 = nullptr;

// RemovalTool_sub_4770: checks if entity can be removed (road check)
typedef bool (*fn_RemovalSub4770_t)(uintptr_t self, void* entityIBP);
static fn_RemovalSub4770_t s_fn_RemovalSub4770 = nullptr;

// RemovalTool_sub_4670: checks if entity can be removed (building check)
typedef bool (*fn_RemovalSub4670_t)(uintptr_t self, void* entityIBP);
static fn_RemovalSub4670_t s_fn_RemovalSub4670 = nullptr;

// FUN_140003050: gets parent/owner entity from an entity (InternalBasePtr)
typedef void* (*fn_GetParentEntity_t)(uintptr_t entity, void* retIBP);
static fn_GetParentEntity_t s_fn_GetParentEntity = nullptr;

// FUN_14012ead0: checks if entity is marked for destruction
typedef bool (*fn_IsMarkedForDestruction_t)(uintptr_t entity);
static fn_IsMarkedForDestruction_t s_fn_IsMarkedForDestruction = nullptr;

// FUN_140101270: ScreenToCell
typedef int (*fn_ScreenToCell_t)(uint64_t unused);
static fn_ScreenToCell_t s_fn_ScreenToCell = nullptr;

static bool s_drawFuncsResolved = false;

static void ResolveDrawFuncs() {
    if (s_drawFuncsResolved) return;

    s_fn_RemovalSub48a0 = reinterpret_cast<fn_RemovalSub48a0_t>(
        ResolveAddr(Module::Exe, 0x1401048a0));
    s_fn_RemovalSub4770 = reinterpret_cast<fn_RemovalSub4770_t>(
        ResolveAddr(Module::Exe, 0x140104770));
    s_fn_RemovalSub4670 = reinterpret_cast<fn_RemovalSub4670_t>(
        ResolveAddr(Module::Exe, 0x140104670));
    s_fn_GetParentEntity = reinterpret_cast<fn_GetParentEntity_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_IsMarkedForDestruction = reinterpret_cast<fn_IsMarkedForDestruction_t>(
        ResolveAddr(Module::Exe, 0x14012ead0));
    s_fn_ScreenToCell = reinterpret_cast<fn_ScreenToCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));


    s_drawFuncsResolved = true;
}

// ============================================================
// RemovalTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Renders colored tile overlay for removal preview.
// Two phases:
//   1. Constrained tiles from description entity (+0x80 flag)
//   2. Drag selection rectangle with per-tile removability check
//
// Original: 0x140104050
void RemovalTool::Draw(void* context, char param3) {
    ResolveDrawFuncs();

    uintptr_t descEntity = (uintptr_t)dialogPtr;
    int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

    // ---- Phase 1: Constrained entity tiles ----
    if (*(char*)(descEntity + 0x80) != '\0') {
        int gridSize = *mapGrid;
        int constrainStart = *(int*)(descEntity + 0x98);
        int constrainEnd   = *(int*)(descEntity + 0x9c);

        int startCol = constrainStart % gridSize;
        int startRow = constrainStart / gridSize;
        int endCol   = constrainEnd % gridSize;
        int endRow   = constrainEnd / gridSize;

        // Build entity tile array
        TileArray entityTiles;

        for (int row = startRow; row <= endRow; row++) {
            for (int col = startCol; col <= endCol; col++) {
                TileRenderEntry entry;
                entry.col   = (uint32_t)col;
                entry.row   = (uint32_t)row;
                entry.type  = 0;
                entry.color = 0xffffff00; // retail constrained-tile color (0x140104050)
                entry.extra = 0;
                PushTileEntry(&entityTiles, &entry);
            }
        }

        // Render: entity tiles in R8, empty array in R9
        TileArray emptyTiles;
        if (fn_TileRender) {
            fn_TileRender(nullptr, context, &entityTiles, &emptyTiles);
        }

        // entityTiles / emptyTiles are released by ~Array at scope exit (no early
        // return inside this block).
    }

    // ---- Phase 2: Drag selection rectangle ----
    if (param3 == '\0') {
        int startCell, endCell;

        if (dragState == '\0') {
            // Not dragging — pick cell under cursor
            int cell = -1;
            if (s_fn_ScreenToCell) {
                cell = s_fn_ScreenToCell(0);
            }
            startCell = cell;
            endCell   = cell;
        } else {
            startCell = this->startCell;
            endCell   = this->endCell;
        }

        if (startCell < 0) return;
        if (endCell < 0) endCell = startCell;

        int gridSize = *mapGrid;

        // Convert to (row, col) grid coordinates
        uint32_t startRow2 = (uint32_t)(startCell / gridSize);
        uint32_t startCol2 = (uint32_t)(startCell % gridSize);
        uint32_t endRow2   = (uint32_t)(endCell / gridSize);
        uint32_t endCol2   = (uint32_t)(endCell % gridSize);

        // Compute min/max for bounding rectangle
        uint32_t minCol = startCol2, maxCol = endCol2;
        if ((int)endCol2 < (int)startCol2) {
            minCol = endCol2;
            maxCol = startCol2;
        }
        uint32_t minRow = startRow2, maxRow = endRow2;
        if ((int)endRow2 < (int)startRow2) {
            minRow = endRow2;
            maxRow = startRow2;
        }

        // Build selection tile array with per-tile removability check
        TileArray selTiles;
        mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

        uintptr_t removalDesc = (uintptr_t)this->removalDesc;

        for (uint32_t r = minRow; r <= maxRow; r++) {
            for (uint32_t c = minCol; c <= maxCol; c++) {
                // Get entity at this tile from map grid
                int tileIndex = *mapGrid * (int)r + (int)c;
                uintptr_t tileData = *(uintptr_t*)((uintptr_t)mapGrid + 0x80); // terrain array base
                uintptr_t cellBase = tileData + (uintptr_t)tileIndex * 0x30;

                // Get entity occupying this tile
                uintptr_t entityPtr = *(uintptr_t*)cellBase;
                uintptr_t entityPtr2 = *(uintptr_t*)(cellBase + 8);
                void* entity;
                if (entityPtr == 0) {
                    if (entityPtr2 == 0) {
                        // No entity — construct null IBP
                        void* nullIBP = nullptr;
                        if (fn_InternalBasePtr_Ctor)
                            new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                        entity = nullIBP;
                    } else {
                        entity = (void*)entityPtr2;
                    }
                } else {
                    entity = (void*)entityPtr;
                }

                // Check removability
                bool canRemove = false;

                if (s_fn_RemovalSub48a0) {
                    canRemove = s_fn_RemovalSub48a0((uintptr_t)this, &entity, c, r);
                }

                if (!canRemove) {
                    // Additional checks from description flags
                    mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

                    // Check road removal flag (+0x40 on removalDesc)
                    if (removalDesc && *(char*)(removalDesc + 0x40) != '\0') {
                        int tileIdx2 = *mapGrid * (int)r + (int)c;
                        uintptr_t tileData2 = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
                        uintptr_t cellBase2 = tileData2 + (uintptr_t)tileIdx2 * 0x30;
                        uint32_t terrainFlag = 1u << (*(uint8_t*)(cellBase2 + 0x29) & 0x1f);

                        // Check if cell has road-type terrain (bits 0 or 1)
                        if ((terrainFlag & 2) != 0 || (terrainFlag & 1) != 0 ||
                            *(char*)(cellBase2 + 0x2a) != (char)0xff) {
                            canRemove = true;
                        }

                        if (!canRemove) {
                            // BST traversal at mapGrid + 0x3a (as int* offset = 0xe8 bytes)
                            int* bstNode = *(int**)((uintptr_t)mapGrid + 0xe8);
                            while (bstNode != nullptr) {
                                if (tileIdx2 < *bstNode) {
                                    bstNode = *(int**)((uintptr_t)bstNode + 0x10);
                                } else if (tileIdx2 > *bstNode) {
                                    bstNode = *(int**)((uintptr_t)bstNode + 0x18);
                                } else {
                                    canRemove = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!canRemove) {
                        // Check road removal sub_4770
                        void* entityCopy1 = entity;
                        if (s_fn_RemovalSub4770) {
                            canRemove = s_fn_RemovalSub4770((uintptr_t)this, &entityCopy1);
                        }
                    }

                    if (!canRemove) {
                        // Check building removal sub_4670
                        void* entityCopy2 = entity;
                        if (s_fn_RemovalSub4670) {
                            canRemove = (bool)s_fn_RemovalSub4670((uintptr_t)this, &entityCopy2);
                        }
                    }

                    if (!canRemove) {
                        // Check removeBuildings flag (+0x43) and parent entity destruction
                        if (removalDesc && *(char*)(removalDesc + 0x43) != '\0' && entity != nullptr) {
                            void* entityForCheck = entity;

                            // Get parent entity
                            uintptr_t parentIBP = 0;
                            if (s_fn_GetParentEntity) {
                                s_fn_GetParentEntity((uintptr_t)entityForCheck, &parentIBP);
                            }
                            if (parentIBP != 0) {
                                if (s_fn_IsMarkedForDestruction) {
                                    canRemove = s_fn_IsMarkedForDestruction(parentIBP);
                                }
                            }
                        }
                    }
                }


                TileRenderEntry entry;
                entry.col   = c;
                entry.row   = r;
                entry.type  = canRemove ? 3 : 0;
                // Retail (0x140104050): removable tile -> RED (0xffff0000), non-removable
                // -> GREEN (0x8000ff00). NOT the intuitive green=valid — matches the game
                // (red marks "will be cleared"). Our reimpl had these swapped.
                entry.color = canRemove ? 0xffff0000 : 0x8000ff00;
                entry.extra = 0;
                PushTileEntry(&selTiles, &entry);
            }
        }

        // Render: selection tiles in R8, empty array in R9
        TileArray emptyTiles2;
        if (fn_TileRender) {
            fn_TileRender(nullptr, context, &selTiles, &emptyTiles2);
        }

        // selTiles / emptyTiles2 are released by ~Array at scope exit (the block's
        // only early return, `startCell < 0`, precedes their declarations).
    }
}

// ============================================================
// RemovalTool::CalcPath (vtable +0x68, Slot13)
// ============================================================
// This function is extremely complex (~300 lines of decompiled output):
//   - Iterates a rectangular grid of cells
//   - For each cell, checks removability via RemovalTool_sub_48a0
//   - Creates ComponentEntity copies from template (0x48 bytes each)
//   - Finds MapComponent (DWORD_1401c79e0) and DestroyComponent (DWORD_1401c75f0)
//     on entities via typed FindComponent loops
//   - Handles road removal: checks terrain flags, BST traversal for road tracking,
//     calls RemovalTool_helper_c700 for road validation
//   - Handles building removal: checks sub_4770/sub_4670, finds parent entities,
//     checks destruction status via FUN_14012ead0
//   - Handles crop removal: checks DWORD_1401c8070 component, BST lookup in
//     GameWorld+0x118 crop tracker, calls CitizenAI_impl_9ad0
//   - Calls FUN_1400c48a0 (EntityList_RegisterEntity) to register removal markers
//   - Calls TradeWork_impl_f370 for road count updates
//
// Original: 0x1401035e0
// ENDGAME B.3.8 callers: PORTED as RemovalTool::CalcPath in
// keystone_a_removalcalcpath.cpp; the own-address read of 0x1401035e0 that used to
// reach it through the registry is now the member itself.

// ============================================================
// RemovalTool members (RemovalTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_RemovalTool_Vtable[15], its slot stubs, the in-place _SA
// factory and RegisterRemovalToolVtable are gone: the compiler builds the
// table, and nothing reads 0x140161de0 back by address (the old dtor was the
// only reader). Inherited from the bases, as in retail: [6] DragTool::OnKey,
// [9] DragTool::OnCancel (0x1400fc640 — the right-click cancel that closes the
// CreateBar cost dialog), [10] false, [11] no setting (0x140004950 `*ret = 0`;
// SetEditMode dereferences the slot), [12] cursor 5 (0x1400fc9d0).
// ============================================================
#include "Object/factory_registration.h"
#include <new>

// [4] Startup, [8] Draw and [14] CalcCost are defined above, [13] CalcPath in
// keystone_a_removalcalcpath.cpp.

// create 0x14002e7a0: MemAlloc(0x40, 8), then the ctor 0x140104cc0.
void* RemovalTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RemovalTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RemovalTool());
}
// createInPlace 0x14002e7e0: `if (storage) ctor(storage)`.
void* RemovalTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RemovalTool());
}

// Kept for standalone/tool_harness.cpp (outside this block's files): the
// reflected create, by its old name.
extern "C" void* RemovalTool_Factory_SA(void) { return RemovalTool::CreateNewObject(); }

DEFINE_EXE_TYPE(RemovalTool, "DragTool")
