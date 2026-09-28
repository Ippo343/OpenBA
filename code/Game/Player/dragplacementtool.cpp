// code/Game/Player/dragplacementtool.cpp
// Clean reimplementation of DragPlacementTool vtable methods from Application-x64.exe.
//
// DragPlacementTool (0x48 bytes) extends DragTool (0x38 bytes).
// Handles rectangular area placement (e.g., stockpile, marketplace).
//
// Vtable at 0x140161b90 (15 slots). The whole vtable is reimplemented in this file:
//   Slot0  Dtor    (0x14002d900) — scalar deleting destructor
//   Slot4  Startup (0x1400fc3e0) — find description components on entity
//   Slot13 CalcPath(0x1400fb5c0) — commits the dragged area on drag-release
//   Slot14 CalcCost(0x1400fbae0) — validate area and calculate cost
//   plus Draw and the remaining slots, all wired in the vtable at the foot of the file.
//
// This banner used to end with "Remaining slots (Draw, CalcPath) are too complex
// (terrain validation, entity creation, scene rendering) and are not yet
// reimplemented". That has been FALSE since Draw and CalcPath landed further down this
// same file; the comment simply outlived the port that replaced it. Corrected
// 2026-09-06 — this tool is the ONLY route to a player-placed crop field
// (CropField.rsc declares `createdrag` -> CreateDragDescription -> DragPlacementTool),
// so a stale "not reimplemented" here reads as "crop fields cannot be placed", which
// is not true.
//
// Decompiled from Application-x64.exe.

#include "Player/DragPlacementTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "Component/CreateDragDescription.h"
#include "Component/MapDescription.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// DragPlacementTool::DeletingDtor (vtable +0x00) — 0x14002d900
// ============================================================
// ENDGAME B.3.8 TOOL: the hand-rolled DragPlacementTool_Dtor restamped the
// leaf, DragTool and Tool tables read by address — the last two UNREGISTERED,
// so it stored a NULL vptr before ~Internal (B38 D9 #3). The compiler's dtor
// chain restamps the real tables, then runs Object::Internal::~Internal.
Object::Base* DragPlacementTool::DeletingDtor(unsigned flags) {
    this->~DragPlacementTool();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Helper: search entity component list for a specific type ID
// ============================================================
// Returns the first component matching targetTypeId, or nullptr.
static void* FindComponentByTypeId(uintptr_t entity, uint32_t targetTypeId) {
    if (!entity || !fn_IsDerivedFrom) return nullptr;

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // virtual slot 2 (+0x10): Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == targetTypeId) {
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                return comp;
            }
            break;
        }
    }
    return nullptr;
}

// ============================================================
// DragPlacementTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Searches entity component list for two description types:
//   1. CreateDragDescription (cell 0x1401c7430; the field name/global "CreatePlacementDescription"
//      lies — D5, Table 1/3) -> self->placementDesc (+0x38)
//   2. MapDescription (cell 0x1401c7a18; "AreaDescription" also lies) -> self->areaDesc (+0x40)
//
// Original: 0x1400fc3e0
void DragPlacementTool::Startup() {
    uintptr_t entity = (uintptr_t)descriptionPtr;

    // Find CreateDragDescription
    void* foundPlacement = FindComponentByTypeId(entity, CreateDragDescription::TypeId());

    // Original re-reads entity pointer after first search
    entity = (uintptr_t)descriptionPtr;
    placementDesc = foundPlacement;

    // Find MapDescription
    void* foundArea = FindComponentByTypeId(entity, MapDescription::TypeId());
    areaDesc = foundArea;
}

// ============================================================
// Resolved helper function pointers (set once)
// ============================================================

// CharacterComponent_impl_f5d0: terrain validation for a single cell.
// Checks terrain mask, height constraints, overlap flags.
// Params: (mapPtr, col, row, terrainMask, 0.0f, heightTol, 0.0f,
//          allowOverlap, hasExisting, entityTemplate)
// Original: 0x14009f5d0 (CharacterComponent_impl_f5d0; was a dropped-page typo
// 0x1400ff5d0 which is mid-function inside MainMenuDialog_helper_eec0 — Level-1
// caller-bug fix, confirmed by the CALL at 0x1400fbcac in DragPlacementTool_Slot14)
typedef bool (*fn_TerrainValidate_t)(void* mapPtr, uint32_t col, uint32_t row,
    uint32_t terrainMask, float param5, float heightTol, float param7,
    char allowOverlap, bool hasExisting, int64_t entityTemplate);
static fn_TerrainValidate_t s_fn_TerrainValidate = nullptr;

// DAT_140172208: float constant for height tolerance
static float* s_pHeightTolerance = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;
    s_fn_TerrainValidate = reinterpret_cast<fn_TerrainValidate_t>(
        ResolveAddr(Module::Exe, 0x14009f5d0));
    s_pHeightTolerance = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172208));
    s_helpersResolved = true;
}

// ============================================================
// DragPlacementTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Calculates placement cost for a rectangular area.
//
// Converts start/end cells to grid coordinates, normalizes to min/max corners,
// checks description size constraints, then validates each cell's terrain.
// Finally checks for existing placement overlap.
//
// Return: 0=success, 1=too small, 2=too big, 3=invalid terrain/overlap
// Original: 0x1400fbae0
int DragPlacementTool::CalcCost(int startCell, int endCell, uint32_t* outValid,
                                int* outCostA, int* outCostB, int* outLength) {
    EnsureHelpers();

    // Get map grid size pointer
    int* mapPtr = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
    int gridSize = *mapPtr;

    // Convert cell indices to (col, row) grid coordinates
    int startRow = startCell / gridSize;
    int startCol = startCell % gridSize;
    int endRow = endCell / gridSize;
    int endCol = endCell % gridSize;

    // Normalize to min/max corners
    int minCol = startCol;
    int maxCol = endCol;
    if (endCol < startCol) {
        minCol = endCol;
        maxCol = startCol;
    }

    int minRow = startRow;
    int maxRow = endRow;
    if (endRow < startRow) {
        minRow = endRow;
        maxRow = startRow;
    }

    int width = (maxCol - minCol) + 1;
    int height = (maxRow - minRow) + 1;

    // Set output parameters
    *outCostA = width;
    *outCostB = height;
    *outValid = 0;
    *outLength = 1;

    float heightTol = s_pHeightTolerance ? *s_pHeightTolerance : 0.5f;

    // Read placement description fields
    uintptr_t desc = (uintptr_t)placementDesc;

    // Check min size (desc +0x30 = minWidth, +0x34 = minHeight)
    if (width < *(int*)(desc + 0x30) || height < *(int*)(desc + 0x34)) {
        return 1;
    }

    // Check max size (desc +0x28 = maxWidth, +0x2C = maxHeight)
    if (*(int*)(desc + 0x28) < width || *(int*)(desc + 0x2C) < height) {
        return 2;
    }

    // Validate terrain for each cell in the rectangle
    uint32_t terrainMask = *(uint32_t*)(desc + 0x20);
    char allowOverlap = *(char*)(desc + 0x24);
    int64_t entityTemplate = *(int64_t*)(desc + 0x38);

    bool allValid = true;
    for (int row = minRow; row <= maxRow && allValid; row++) {
        for (int col = minCol; col <= maxCol && allValid; col++) {
            // hasExisting = false (no occupancy grid in CalcCost, only in Draw)
            if (s_fn_TerrainValidate) {
                bool cellValid = s_fn_TerrainValidate(
                    mapPtr, (uint32_t)col, (uint32_t)row, terrainMask,
                    0.0f, heightTol, 0.0f,
                    allowOverlap, false, entityTemplate);
                if (!cellValid) {
                    allValid = false;
                }
            }
        }
    }

    if (allValid) {
        // Check for existing placement overlap via self->dialogPtr (+0x10)
        // The IBP at +0x10 points to an object with:
        //   +0x80: bool hasExistingPlacement
        //   +0x98: int existingStartCell
        //   +0x9c: int existingEndCell
        uintptr_t dlg = (uintptr_t)dialogPtr;
        if (dlg && *(char*)(dlg + 0x80) != '\0') {
            int existGridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
            int existStartCol = *(int*)(dlg + 0x98) % existGridSize;
            int existStartRow = *(int*)(dlg + 0x98) / existGridSize;
            int existEndCol = *(int*)(dlg + 0x9c) % existGridSize;
            int existEndRow = *(int*)(dlg + 0x9c) / existGridSize;
            if (minCol != existStartCol || minRow != existStartRow ||
                existEndCol != maxCol || existEndRow != maxRow) {
                return 3;
            }
        }
        return 0;
    }

    return 3;
}

// ============================================================
// Additional function pointers for Draw
// ============================================================

// FUN_140101270: ScreenToCell — ray-picks map cell under cursor.
// Returns cell index (row*gridSize + col), or -1 if no hit.
typedef int (*fn_ScreenToCell_t)(uint64_t unused);
static fn_ScreenToCell_t s_fn_ScreenToCell = nullptr;

static bool s_drawFuncsResolved = false;

static void EnsureDrawFuncs() {
    if (s_drawFuncsResolved) return;
    s_fn_ScreenToCell = (fn_ScreenToCell_t)ResolveAddr(Module::Exe, 0x140101270);
    if (!s_pHeightTolerance) {
        s_pHeightTolerance = (float*)ResolveAddr(Module::Exe, 0x140172208);
    }
    if (!s_fn_TerrainValidate) {
        s_fn_TerrainValidate = (fn_TerrainValidate_t)ResolveAddr(Module::Exe, 0x14009f5d0);
    }
    s_drawFuncsResolved = true;
}

// ============================================================
// Tile record: 0x14 bytes, used by CharacterComponent_impl_08c0
// ============================================================
struct TileRecord {
    int32_t  col;    // +0x00
    int32_t  row;    // +0x04
    uint32_t flags;  // +0x08 (0=normal, 2=has existing)
    uint32_t color;  // +0x0C (ARGB: 0xff00ff00=green, 0xffff0000=red, 0xffffff00=yellow)
    int32_t  extra;  // +0x10
};
static_assert(sizeof(TileRecord) == 0x14, "TileRecord size mismatch");

// ============================================================
// Dynamic tile array: mirrors engine's grow pattern
// ============================================================
// ENDGAME B.3.2 step 2: this WAS a struct with a hand-written `push` (a copy of
// rtcoll::SetSizeCore specialised to one append) and a `free` (a copy of
// Collection::Array<T>::~Array). It is now the real template, trait-derived row:
// stride 0x14, align 4, byteCopy, init == nullptr, destroy == nullptr. Compared arm
// for arm before the swap: push always had newCount == count + 1, so its
// `(count == 0) ? n : n*3` rule is the reference rule; MemAlloc(cap * 0x14, align 4);
// prefix memcpy'd only into a freshly allocated block (== "only when the block
// moved"); the old block freed on that same path; the new slot takes the fill;
// `count = newCount` stored after the allocation here vs before it in SetSizeCore,
// which nothing observes. `free` was MemFree + zero data/count/capacity == ~Array.
// Out-of-memory-only difference: push skipped the store when the block came back
// null; SetSizeCore skips the null element address per iteration. Same outcome.
using TileArray = Collection::Array<TileRecord>;

// ============================================================
// DragPlacementTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Renders colored tile overlays for a drag-based area placement tool.
//
// Two main sections:
// 1. If an existing placement is recorded at dialog+0x80, render its
//    footprint as yellow tiles (0xffffff00).
// 2. If param3==0, render the current drag rectangle:
//    - Get start/end cells from DragTool drag state or ScreenToCell
//    - Normalize to min/max corners
//    - Check area size against description constraints
//    - Validate each cell's terrain, color green/red accordingly
//    - Pass tile arrays to CharacterComponent_impl_08c0 for rendering
//
// Disassembly reference: 0x1400fbe40
// Key sub-calls:
//   0x1401008c0 = CharacterComponent_impl_08c0 (tile renderer)
//   0x14009f5d0 = CharacterComponent_impl_f5d0 (terrain validate)
//   0x140101270 = FUN_140101270 (ScreenToCell)
// Uses: g_pGameWorldSingleton+0xf8 (map grid size pointer)
//       placementDesc (+0x38) for terrain constraints
//       dialog controller (+0x10) for existing placement info
//
// Original: 0x1400fbe40
void DragPlacementTool::Draw(void* context, char param3) {
    EnsureHelpers();
    EnsureDrawFuncs();

    if (!fn_TileRender) return;

    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // --- Section 1: Existing placement overlay (yellow tiles) ---
    uintptr_t dialogCtrl = (uintptr_t)dialogPtr;
    if (*(char*)(dialogCtrl + 0x80) != 0) {
        int gridSize = **(int**)(gameWorld + 0xf8);

        int existStart = *(int*)(dialogCtrl + 0x98);
        int existEnd = *(int*)(dialogCtrl + 0x9c);

        int existStartCol = existStart % gridSize;
        int existStartRow = existStart / gridSize;
        int existEndCol = existEnd % gridSize;
        int existEndRow = existEnd / gridSize;

        // Build tile array for existing placement footprint
        TileArray existArr;
        for (int row = existStartRow; row <= existEndRow; row++) {
            for (int col = existStartCol; col <= existEndCol; col++) {
                TileRecord rec;
                rec.col   = col;
                rec.row   = row;
                rec.flags = 0;                // no special flags
                rec.color = 0xffffff00;       // yellow
                rec.extra = 0;
                existArr.SetSize(existArr._size + 1, rec);
            }
        }

        // Render existing placement tiles
        // tileArray2 = existArr, tileArray1 = empty
        TileArray empty;
        // CharacterComponent_impl_08c0 signature: (unused, context, &arr1, &arr2)
        // arr1 = 0x14-byte tile records, arr2 = 0x20-byte road-style records
        // For placement tools, arr2 is always empty.
        fn_TileRender(nullptr, context, (void*)&existArr, (void*)&empty);
        // existArr / empty are released by ~Array at scope exit (no early return in scope).
    }

    // --- Section 2: Current drag area overlay ---
    if (param3 == 0) {
        int startCell, endCell;

        // Get start/end cells based on drag state
        if (dragState == 0) {
            // Not dragging: use ScreenToCell for both
            int cell = -1;
            if (s_fn_ScreenToCell) {
                cell = s_fn_ScreenToCell(0);
            }
            startCell = cell;
            endCell = cell;
        } else {
            // Dragging: start from stored field, end from stored field
            startCell = this->startCell;
            endCell = this->endCell;
        }

        float heightTol = s_pHeightTolerance ? *s_pHeightTolerance : 0.5f;
        int gridSize = **(int**)(gameWorld + 0xf8);

        // Convert cell indices to (col, row)
        int startCol = startCell % gridSize;
        int startRow = startCell / gridSize;
        int64_t gridSz64 = (int64_t)**(int**)(gameWorld + 0xf8);
        int endCol = (int)((int64_t)endCell % gridSz64);
        int endRow = (int)((int64_t)endCell / gridSz64);

        // Normalize to min/max corners
        uint32_t minCol = (uint32_t)endCol;
        uint32_t maxCol = (uint32_t)startCol;
        if ((int)endCol > (int)startCol) {
            minCol = (uint32_t)startCol;
            maxCol = (uint32_t)endCol;
        }

        uint32_t minRow = (uint32_t)startRow;
        uint32_t maxRow = (uint32_t)endRow;
        if ((int)endRow < (int)startRow) {
            minRow = (uint32_t)endRow;
            maxRow = (uint32_t)startRow;
        }

        int width = (int)(maxCol - minCol) + 1;
        int height = (int)(maxRow - minRow) + 1;

        // Check area constraints from placement description
        uintptr_t desc = (uintptr_t)placementDesc;
        bool tooBig = false;
        if (desc) {
            if (*(int*)(desc + 0x28) < width || *(int*)(desc + 0x2C) < height) {
                tooBig = true;
            }
        }

        // Read description fields for terrain validation
        uint32_t terrainMask = desc ? *(uint32_t*)(desc + 0x20) : 0;
        char allowOverlap = desc ? *(char*)(desc + 0x24) : 0;
        int64_t entityTemplate = desc ? *(int64_t*)(desc + 0x38) : 0;
        int* mapPtr = *(int**)(gameWorld + 0xf8);

        // Build tile array for drag area
        TileArray dragArr;

        // The original code also reads an occupancy grid string from the map component.
        // It's at `*(self+0x30)+0x28` passed to sub_1380, accessed as local_98.
        // In DragPlacementTool::Draw, the occupancy check uses local_98 which is 0 (no grid).

        uint32_t rowIdx = minRow;
        if ((int)minRow <= (int)maxRow) {
            // The original code tracks a decrementing counter for occupancy grid indexing.
            // Since local_98 is always 0 here, the occupancy check always returns false.
            do {
                uint32_t colIdx = minCol;
                if ((int)minCol <= (int)maxCol) {
                    do {
                        // Occupancy check: in the original, checks local_98 string
                        // Since local_98 == 0 in DragPlacementTool::Draw, bVar31 is always false
                        bool hasExisting = false;

                        // Validate terrain
                        bool cellValid = false;
                        if (s_fn_TerrainValidate) {
                            cellValid = s_fn_TerrainValidate(
                                mapPtr, colIdx, rowIdx, terrainMask,
                                0.0f, heightTol, 0.0f,
                                allowOverlap, hasExisting, entityTemplate);
                        }

                        // Determine color
                        uint32_t color;
                        if (tooBig || !cellValid) {
                            color = 0xffff0000;  // red
                        } else {
                            color = 0xff00ff00;  // green
                        }

                        TileRecord rec;
                        rec.col   = (int32_t)colIdx;
                        rec.row   = (int32_t)rowIdx;
                        rec.flags = hasExisting ? 2u : 0u;
                        rec.color = color;
                        rec.extra = 0;
                        dragArr.SetSize(dragArr._size + 1, rec);

                        colIdx++;
                    } while ((int)colIdx <= (int)maxCol);
                }
                rowIdx++;
            } while ((int)rowIdx <= (int)maxRow);
        }

        // Render drag area tiles
        TileArray empty;
        fn_TileRender(nullptr, context, (void*)&dragArr, (void*)&empty);
        // dragArr / empty are released by ~Array at scope exit (no early return in scope).
    }
}

// ============================================================
// Resolved helper function pointers for CalcPath
// ============================================================

// FUN_1401018d0: clears existing path entities and sets up grid range.
// Params: (self, minCol, minRow, maxCol, maxRow, entityTemplate)
typedef void (*fn_ClearPathEntities_t)(void* self, int minCol, int minRow,
    int maxCol, int maxRow, int64_t entityTemplate);
static fn_ClearPathEntities_t s_fn_ClearPathEntities = nullptr;

// ComponentEntity_CopyCtor (0x1400c3720): clone entity from description
typedef void* (*fn_ComponentEntity_CopyCtor_t)(void* self, void* desc);
static fn_ComponentEntity_CopyCtor_t s_fn_ComponentEntity_CopyCtor = nullptr;

static bool s_calcPathFuncsResolved = false;

static void EnsureCalcPathFuncs() {
    if (s_calcPathFuncsResolved) return;
    s_fn_ClearPathEntities = reinterpret_cast<fn_ClearPathEntities_t>(
        ResolveAddr(Module::Exe, 0x1401018d0));
    s_fn_ComponentEntity_CopyCtor = reinterpret_cast<fn_ComponentEntity_CopyCtor_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    s_calcPathFuncsResolved = true;
}

// ============================================================
// DragPlacementTool::CalcPath (vtable +0x68, Slot13)
// ============================================================
// Creates a preview placement entity for the drag area.
//
// Steps:
//   1. Call CalcCost (vtable+0x70) to validate the area — if non-zero, fail
//   2. Convert cell indices to grid coords, normalize min/max
//   3. If dialog has constraints (+0x80), verify selection matches exactly
//   4. Clear existing path entities via FUN_1401018d0
//   5. Create a ComponentEntity copy from the description entity
//   6. Find its BuildComponent and set placement coords + rotation
//   7. Register entity with the entity list
//
// Rotation: 0x5a (90 degrees) if height >= width, else 0.
//
// Original: 0x1400fb900
bool DragPlacementTool::CalcPath(int startCell, int endCell) {
    EnsureCalcPathFuncs();

    // Virtual CalcCost (slot 14, +0x70) to validate, into local out-values

    uint32_t outValid = 0;
    int outCostA = 0;   // width
    int outCostB = 0;   // height
    int outLength = 0;

    int calcResult = CalcCost(startCell, endCell, &outValid, &outCostA, &outCostB, &outLength);


    if (calcResult != 0) {
        return false;
    }

    // Get map grid size
    int gridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

    // Convert cell indices to grid coords
    int startRow = startCell / gridSize;
    int startCol = startCell % gridSize;
    int endRow = endCell / gridSize;
    int endCol = endCell % gridSize;

    // Normalize to min/max
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

    // Check constraints from dialog (+0x10)
    uintptr_t dlg = (uintptr_t)dialogPtr;
    if (*(char*)(dlg + 0x80) != '\0') {
        int cGridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
        int cStartCol = *(int*)(dlg + 0x98) % cGridSize;
        int cStartRow = *(int*)(dlg + 0x98) / cGridSize;
        int cEndCol = *(int*)(dlg + 0x9c) % cGridSize;
        int cEndRow = *(int*)(dlg + 0x9c) / cGridSize;

        if (minCol != cStartCol || minRow != cStartRow ||
            cEndCol != maxCol || cEndRow != maxRow) {
            // Faithful to retail (0x1400fb900): a restricted step demands the
            // EXACT rectangle, not one inside it.
            return false;
        }
    }

    // Clear existing path entities
    uintptr_t placementDesc = (uintptr_t)this->placementDesc;
    int64_t entityTemplate = *(int64_t*)(placementDesc + 0x38);

    if (s_fn_ClearPathEntities) {
        s_fn_ClearPathEntities(this, minCol, minRow, maxCol, maxRow, entityTemplate);
    }

    // Allocate and clone a ComponentEntity from description
    void* newEntityMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (newEntityMem && s_fn_ComponentEntity_CopyCtor) {
        newEntity = s_fn_ComponentEntity_CopyCtor(newEntityMem, (void*)descriptionPtr);
    }

    // Wrap in InternalBasePtr
    void* entityIBP = nullptr;
    if (fn_InternalBasePtr_Ctor) {
        new (&entityIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge
    }

    // Find BuildComponent on the entity

    void* buildCompIBP = nullptr;
    fn_FindComponent_Build((void*)entityIBP, &buildCompIBP);


    uintptr_t buildComp = *(uintptr_t*)&buildCompIBP;

    // Determine rotation: 0x5a (90 degrees) if height >= width, else 0
    int rotation = 0;
    if (outCostB >= outCostA) {
        rotation = 0x5a;  // 90 degrees
    }

    // Set placement coordinates on BuildComponent
    *(int16_t*)(buildComp + 0x28) = (int16_t)minCol;
    *(int16_t*)(buildComp + 0x2a) = (int16_t)minRow;
    *(int16_t*)(buildComp + 0x2c) = (int16_t)outCostA;  // width
    *(int16_t*)(buildComp + 0x2e) = (int16_t)outCostB;  // height
    *(int32_t*)(buildComp + 0x30) = rotation;

    // Register entity with entity list
    void* entityList = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xe8);
    int categoryId = *(int*)((uintptr_t)descriptionPtr + 0x20);

    if (fn_EntityList_RegisterEntity) {
        fn_EntityList_RegisterEntity(entityList, &entityIBP, categoryId);
    }

    return true;
}

// ============================================================
// DragPlacementTool members (DragPlacementTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_DragPlacementTool_Vtable[15], its slot stubs, the _SA
// factories and RegisterDragPlacementToolVtable are gone: the compiler builds
// the table, and nothing reads 0x140161b90 back by address (the old dtor was
// the only reader). Inherited from the bases, as in retail: [6]
// DragTool::OnKey (0x1400fc6b0), [9] DragTool::OnCancel (0x1400fc640), [10]
// false, [11] no setting (0x140004950 `*ret = 0`; SetEditMode dereferences
// the slot), [12] cursor 5 (0x1400fc9d0).
// ============================================================
#include "Object/factory_registration.h"
#include <new>

// [4] Startup, [8] Draw, [13] CalcPath and [14] CalcCost are defined above.

// create 0x14002d8b0: MemAlloc(0x48, 8), then the ctor 0x1400fc530.
void* DragPlacementTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DragPlacementTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DragPlacementTool());
}
// createInPlace 0x14002d8f0: `if (storage) ctor(storage)`.
void* DragPlacementTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DragPlacementTool());
}

DEFINE_EXE_TYPE(DragPlacementTool, "DragTool")
