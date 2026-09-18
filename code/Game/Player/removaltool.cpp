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

#include "Player/removaltool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// RemovalTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_RemovalTool = nullptr;
static void* s_vtable_DragTool    = nullptr;
static void* s_vtable_Tool        = nullptr;

static void EnsureVtables() {
    if (!s_vtable_RemovalTool) {
        s_vtable_RemovalTool = (void*)ResolveAddr(Module::Exe, RemovalToolAddr::Vtable);
        s_vtable_DragTool    = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool        = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// ============================================================
// Object::Internal::~Internal — called from base dtor
// Resolved via the function pointer slot at 0x140152770
// ============================================================
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// RemovalTool::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: RemovalTool -> DragTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002e7f0
void* RemovalTool_Dtor(RemovalTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_RemovalTool;
    self->vtable = s_vtable_DragTool;
    self->vtable = s_vtable_Tool;

    // Call Object::Internal::~Internal
    if (s_fn_Internal_Dtor) {
        s_fn_Internal_Dtor(self);
    }

    // Conditionally free memory
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// RemovalTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Searches the entity's component list for a component matching the
// RemovalDescription type ID (DAT_1401c74a0). Stores the found
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
void RemovalTool_Startup(RemovalTool* self) {
    uintptr_t entity = (uintptr_t)self->descriptionPtr;
    void* result = nullptr;

    if (!entity || !g_TypeId_RemovalDescription || !fn_IsDerivedFrom) {
        self->removalDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *g_TypeId_RemovalDescription;

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // Call vtable slot +0x10: GetTypeId() -> uint32_t
        typedef uint32_t (*fn_GetTypeId_t)(void* self);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom before casting
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                result = comp;
            }
            break;
        }
    }

    self->removalDesc = result;
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
uint64_t RemovalTool_CalcCost(RemovalTool* self, int startCell, int endCell,
                              uint32_t* outValid, int* outCostB, int* outCostA, uint32_t* outLength) {
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
    uintptr_t descEntity = *(uintptr_t*)((uintptr_t)self + 0x10);
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
void RemovalTool_Draw(RemovalTool* self, void* context, char param3) {
    ResolveDrawFuncs();

    uintptr_t descEntity = *(uintptr_t*)((uintptr_t)self + 0x10);
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

        if (*(char*)((uintptr_t)self + 0x28) == '\0') {
            // Not dragging — pick cell under cursor
            int cell = -1;
            if (s_fn_ScreenToCell) {
                cell = s_fn_ScreenToCell(0);
            }
            startCell = cell;
            endCell   = cell;
        } else {
            startCell = *(int*)((uintptr_t)self + 0x2c);
            endCell   = *(int*)((uintptr_t)self + 0x30);
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

        uintptr_t removalDesc = (uintptr_t)self->removalDesc;

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
                    canRemove = s_fn_RemovalSub48a0((uintptr_t)self, &entity, c, r);
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
                            canRemove = s_fn_RemovalSub4770((uintptr_t)self, &entityCopy1);
                        }
                    }

                    if (!canRemove) {
                        // Check building removal sub_4670
                        void* entityCopy2 = entity;
                        if (s_fn_RemovalSub4670) {
                            canRemove = (bool)s_fn_RemovalSub4670((uintptr_t)self, &entityCopy2);
                        }
                    }

                    if (!canRemove) {
                        // Check removeBuildings flag (+0x43) and parent entity destruction
                        if (removalDesc && *(char*)(removalDesc + 0x43) != '\0' && entity != nullptr) {
                            void* entityForCheck = entity;
                            if (fn_InternalBasePtr_CheckPtr)
                                fn_InternalBasePtr_CheckPtr(&entityForCheck);

                            // Get parent entity
                            uintptr_t parentIBP = 0;
                            if (s_fn_GetParentEntity) {
                                s_fn_GetParentEntity((uintptr_t)entityForCheck, &parentIBP);
                            }
                            if (parentIBP != 0) {
                                if (fn_InternalBasePtr_CheckPtr)
                                    fn_InternalBasePtr_CheckPtr(&parentIBP);
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
// Resolved as a direct exe function pointer call.
//
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
// Full reimplementation would require typing ~15 sub-functions and 4+ type IDs.
// Deferring to a future pass when those dependencies are resolved.
//
// Original: 0x1401035e0
typedef bool (*fn_RemovalToolCalcPath_Exe_t)(void* self, int startCell, int endCell);
static fn_RemovalToolCalcPath_Exe_t s_fn_RemovalToolCalcPath_Exe = nullptr;

bool RemovalTool_CalcPath(RemovalTool* self, int startCell, int endCell) {
    if (!s_fn_RemovalToolCalcPath_Exe) {
        s_fn_RemovalToolCalcPath_Exe = reinterpret_cast<fn_RemovalToolCalcPath_Exe_t>(
            ResolveAddr(Module::Exe, 0x1401035e0));
    }
    if (s_fn_RemovalToolCalcPath_Exe) {
        return s_fn_RemovalToolCalcPath_Exe(self, startCell, endCell);
    }
    return false;
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// RemovalTool: size 0x40, parent DragTool, 15-slot vtable @ 0x140161de0.
// ============================================================
#include "tool_base_standalone.h"
#include "Player/dragtool.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RemovalTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RemovalTool");
    return c;
}
// +0x58 (Slot11) GetToolSettingTypeId — retail RemovalTool slot 11 (0x140004950) is
// `*ret = 0; return ret;` (RemovalTool has NO tool-setting). SetEditMode
// (Toolbar_ShowGroup_Impl / FUN_140102b60) DEREFERENCES the returned pointer, so a
// Tool_Ret0 stub (returns 0 -> *0) crashes — same trap PlacementTool_GetSettingType
// documents. Return a valid pointer to a 0 so SetEditMode skips ToolSetting creation.
static uint32_t* RemovalTool_GetSettingType(void*, uint32_t* ret) {
    if (ret) *ret = 0;
    return ret;
}
extern "C" int DragTool_CancelDrag_Impl(void* self);   // 0x1400fc640 (dragtool.cpp)
static int RemovalTool_GetCursor(void*) { return 5; }  // retail 0x1400fc9d0
static void* g_RemovalTool_Vtable[15] = {
    (void*)&RemovalTool_Dtor,      // +0x00 Dtor
    (void*)&Tool_Slot1,            // +0x08
    (void*)&RemovalTool_GetTypeId, // +0x10 GetTypeId
    (void*)&Tool_Persist,          // +0x18 Serialize
    (void*)&RemovalTool_Startup,   // +0x20 Startup
    (void*)&Tool_Ret0,             // +0x28
    (void*)&DragTool_OnKey,        // +0x30 OnKey (base)
    (void*)&Tool_Ret0,             // +0x38
    (void*)&RemovalTool_Draw,      // +0x40 Draw
    // +0x48 Slot9 — retail 0x1400fc640, the SHARED DragTool cancel-drag/"can-deactivate?".
    // This was Tool_Ret0, i.e. "always OK to tear me down". EntityUI's right-click path
    // (EntityUI_ToolTick_SA) asks this slot first: non-zero means the tool cancelled its
    // own drag and STAYS; zero means teardown proceeds. Slot 5 (+0x28) is genuinely empty
    // on every DragTool subclass (retail 0x140104bc0 = `ret 0`), so slot 9 is the ONLY
    // thing that closes the CreateBar cost dialog. With Tool_Ret0 here, a right-click
    // mid-drag deleted the tool while its dialog was still open — the dialog outlived its
    // owner and nothing could dismiss it. Same slot Road/Bridge/DragPlacement already use.
    (void*)&DragTool_CancelDrag_Impl, // +0x48 Slot9 cancel drag (retail 0x1400fc640)
    (void*)&Tool_Ret0,             // +0x50
    (void*)&RemovalTool_GetSettingType, // +0x58 Slot11 GetToolSettingTypeId (*ret=0)
    // +0x60 (Slot12) cursor id. Retail is 0x1400fc9d0 — the shared DragTool body
    // that returns 5, the same one Road/Bridge/Fenced/DragPlacement all use.
    // This was Tool_Ret0, i.e. cursor 0, so the removal tool was the only drag
    // tool in the game showing the wrong cursor.
    (void*)&RemovalTool_GetCursor, // +0x60 cursor id (retail 0x1400fc9d0 = 5)
    (void*)&RemovalTool_CalcPath,  // +0x68 CalcPath
    (void*)&RemovalTool_CalcCost,  // +0x70 CalcCost
};
extern "C" void RegisterRemovalToolVtable() {
    RegisterExeAddr(0x140161de0, (uintptr_t)g_RemovalTool_Vtable);
}
extern "C" void* RemovalTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x40, g_RemovalTool_Vtable, 1);}
extern "C" void* RemovalTool_Factory_SA(void) {
    return Tool_Alloc(0x40, g_RemovalTool_Vtable, 1);
}
REGISTER_TYPE(RemovalTool, "DragTool", &RemovalTool_Factory_SA, &RemovalTool_CreateInPlace_SA);
