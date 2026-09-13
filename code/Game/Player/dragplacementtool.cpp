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

#include "Player/dragplacementtool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Vtable addresses (for vtable chain reset in Dtor)
// ============================================================
static void* s_vtable_DragPlacementTool = nullptr;
static void* s_vtable_DragTool          = nullptr;
static void* s_vtable_Tool              = nullptr;

static void EnsureVtables() {
    if (!s_vtable_DragPlacementTool) {
        s_vtable_DragPlacementTool = (void*)ResolveAddr(Module::Exe, DragPlacementToolAddr::Vtable);
        s_vtable_DragTool          = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool              = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// Object::Internal::~Internal — resolved via dispatch slot at 0x140152770
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// DragPlacementTool::Dtor (vtable +0x00, Slot0)
// ============================================================
// Resets vtable chain: DragPlacementTool -> DragTool -> Tool,
// calls Object::Internal::~Internal, conditionally frees memory.
// Original: 0x14002d900
void* DragPlacementTool_Dtor(DragPlacementTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_DragPlacementTool;
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

        // vtable slot +0x10: GetTypeId() -> uint32_t
        typedef uint32_t (*fn_GetTypeId_t)(void* self);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
        uint32_t typeId = getTypeId(comp);

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
//   1. CreatePlacementDescription (DAT_1401c7430) -> self->placementDesc (+0x38)
//   2. AreaDescription (DAT_1401c7a18)            -> self->areaDesc (+0x40)
//
// Original: 0x1400fc3e0
void DragPlacementTool_Startup(DragPlacementTool* self) {
    uintptr_t entity = (uintptr_t)self->descriptionPtr;

    // Find CreatePlacementDescription
    void* foundPlacement = nullptr;
    if (g_TypeId_CreatePlacementDescription) {
        foundPlacement = FindComponentByTypeId(entity, *g_TypeId_CreatePlacementDescription);
    }

    // Original re-reads entity pointer after first search
    entity = (uintptr_t)self->descriptionPtr;
    self->placementDesc = foundPlacement;

    // Find AreaDescription
    void* foundArea = nullptr;
    // Value-check, not just pointer — same registered-but-zero trap as PlacementTool.
    if (g_TypeId_AreaDescription && *g_TypeId_AreaDescription) {
        foundArea = FindComponentByTypeId(entity, *g_TypeId_AreaDescription);
    }
    self->areaDesc = foundArea;
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
uint64_t DragPlacementTool_CalcCost(DragPlacementTool* self, int startCell, int endCell,
                                     uint32_t* outValid, int* outCostA, int* outCostB,
                                     uint32_t* outLength) {
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
    uintptr_t desc = (uintptr_t)self->placementDesc;

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
        uintptr_t dlg = *(uintptr_t*)((uintptr_t)self + 0x10);
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
struct TileArray {
    TileRecord* data;
    int32_t count;
    int32_t capacity;

    TileArray() : data(nullptr), count(0), capacity(0) {}

    void push(const TileRecord& rec) {
        int newCount = count + 1;
        if (newCount > capacity) {
            int newCap = (count == 0) ? newCount : newCount * 3;
            TileRecord* newData = (TileRecord*)fn_MemAlloc(
                g_pMemAllocSingleton, (uint64_t)newCap * sizeof(TileRecord), 4);
            if (newData && data && count > 0) {
                memcpy(newData, data, (size_t)count * sizeof(TileRecord));
            }
            if (data) {
                fn_MemFreeStatic(data);
            }
            data = newData;
            capacity = newCap;
        }
        if (data) {
            data[count] = rec;
        }
        count = newCount;
    }

    void free() {
        if (data) {
            fn_MemFreeStatic(data);
            data = nullptr;
        }
        count = 0;
        capacity = 0;
    }
};

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
void DragPlacementTool_Draw_Impl(DragPlacementTool* self, void* context, char param3) {
    EnsureHelpers();
    EnsureDrawFuncs();

    if (!fn_TileRender) return;

    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // --- Section 1: Existing placement overlay (yellow tiles) ---
    uintptr_t dialogCtrl = *(uintptr_t*)(selfAddr + 0x10);
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
                existArr.push(rec);
            }
        }

        // Render existing placement tiles
        // tileArray2 = existArr, tileArray1 = empty
        TileArray empty;
        // CharacterComponent_impl_08c0 signature: (unused, context, &arr1, &arr2)
        // arr1 = 0x14-byte tile records, arr2 = 0x20-byte road-style records
        // For placement tools, arr2 is always empty.
        fn_TileRender(nullptr, context, (void*)&existArr, (void*)&empty);
        existArr.free();
    }

    // --- Section 2: Current drag area overlay ---
    if (param3 == 0) {
        int startCell, endCell;

        // Get start/end cells based on drag state
        if (self->dragState == 0) {
            // Not dragging: use ScreenToCell for both
            int cell = -1;
            if (s_fn_ScreenToCell) {
                cell = s_fn_ScreenToCell(0);
            }
            startCell = cell;
            endCell = cell;
        } else {
            // Dragging: start from stored field, end from stored field
            startCell = self->startCell;
            endCell = self->endCell;
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
        uintptr_t desc = (uintptr_t)self->placementDesc;
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
                        dragArr.push(rec);

                        colIdx++;
                    } while ((int)colIdx <= (int)maxCol);
                }
                rowIdx++;
            } while ((int)rowIdx <= (int)maxRow);
        }

        // Render drag area tiles
        TileArray empty;
        fn_TileRender(nullptr, context, (void*)&dragArr, (void*)&empty);
        dragArr.free();
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
bool DragPlacementTool_CalcPath(DragPlacementTool* self, int startCell, int endCell) {
    EnsureCalcPathFuncs();

    // Call CalcCost via vtable+0x70 to validate
    // CalcCost takes 7 params: (self, start, end, &outValid, &outCostA, &outCostB, &outLength)
    // We use local variables for the output params
    typedef uint64_t (*fn_CalcCost_vt_t)(void* self, int start, int end,
        uint32_t* outValid, int* outCostA, int* outCostB, uint32_t* outLength);
    fn_CalcCost_vt_t calcCostFn = *(fn_CalcCost_vt_t*)(*(uintptr_t*)self + 0x70);

    uint32_t outValid = 0;
    int outCostA = 0;   // width
    int outCostB = 0;   // height
    uint32_t outLength = 0;

    uint64_t calcResult = calcCostFn(self, startCell, endCell,
        &outValid, &outCostA, &outCostB, &outLength);


    if ((int)calcResult != 0) {
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
    uintptr_t dlg = (uintptr_t)self->dialogPtr;
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
    uintptr_t placementDesc = (uintptr_t)self->placementDesc;
    int64_t entityTemplate = *(int64_t*)(placementDesc + 0x38);

    if (s_fn_ClearPathEntities) {
        s_fn_ClearPathEntities(self, minCol, minRow, maxCol, maxRow, entityTemplate);
    }

    // Allocate and clone a ComponentEntity from description
    void* newEntityMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (newEntityMem && s_fn_ComponentEntity_CopyCtor) {
        newEntity = s_fn_ComponentEntity_CopyCtor(newEntityMem, (void*)self->descriptionPtr);
    }

    // Wrap in InternalBasePtr
    void* entityIBP = nullptr;
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(&entityIBP, newEntity);
    }

    // Find BuildComponent on the entity
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr(&entityIBP);
    }

    void* buildCompIBP = nullptr;
    fn_FindComponent_Build((void*)entityIBP, &buildCompIBP);

    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr(&buildCompIBP);
    }

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
    int categoryId = *(int*)((uintptr_t)self->descriptionPtr + 0x20);

    if (fn_EntityList_RegisterEntity) {
        fn_EntityList_RegisterEntity(entityList, &entityIBP, categoryId);
    }

    return true;
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// DragPlacementTool: size 0x48, parent DragTool, 15-slot vtable @ 0x140161b90.
// ============================================================
#include "tool_base_standalone.h"
#include "Player/dragtool.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DragPlacementTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("DragPlacementTool");
    return c;
}
// Retail vtable 0x140161b90 slots 9/11/12 are literally the same three functions
// BridgeTool/RoadTool/FencedTool use (read straight out of .rdata):
//   +0x48 slot9  = 0x1400fc640  DragTool cancel-drag / allow-teardown
//   +0x58 slot11 = 0x140004950  GetToolSettingTypeId — `*ret = 0; return ret;`
//   +0x60 slot12 = 0x1400fc9d0  cursor id — `return 5`
// Slot 11 must be real before this tool can be reached at all: SetEditMode
// DEREFERENCES its return (entityui.cpp), so the Tool_Ret0 stub (nullptr) was a
// guaranteed null-deref on activation — the same trap BridgeTool hit.
extern "C" int DragTool_CancelDrag_Impl(void* self);
static uint32_t* DragPlacementTool_GetSettingType(void*, uint32_t* ret) {
    *ret = 0;
    return ret;
}
static int DragPlacementTool_Slot12(void*) { return 5; }

static void* g_DragPlacementTool_Vtable[15] = {
    (void*)&DragPlacementTool_Dtor,      // +0x00 Dtor
    (void*)&Tool_Slot1,                  // +0x08
    (void*)&DragPlacementTool_GetTypeId, // +0x10 GetTypeId
    (void*)&Tool_Persist,                // +0x18 Serialize
    (void*)&DragPlacementTool_Startup,   // +0x20 Startup
    (void*)&Tool_Ret0,                   // +0x28
    (void*)&DragTool_OnKey,              // +0x30 OnKey (base)
    (void*)&Tool_Ret0,                   // +0x38
    (void*)&DragPlacementTool_Draw_Impl, // +0x40 Draw
    (void*)&DragTool_CancelDrag_Impl,    // +0x48 Slot9 cancel drag (retail 0x1400fc640)
    (void*)&Tool_Ret0,                   // +0x50 retail 0x1400268a0 = shared `return 0`
    (void*)&DragPlacementTool_GetSettingType, // +0x58 Slot11 (retail 0x140004950, *ret=0)
    (void*)&DragPlacementTool_Slot12,    // +0x60 cursor id (retail 0x1400fc9d0 = 5)
    (void*)&DragPlacementTool_CalcPath,  // +0x68 CalcPath
    (void*)&DragPlacementTool_CalcCost,  // +0x70 CalcCost
};
extern "C" void RegisterDragPlacementToolVtable() {
    RegisterExeAddr(0x140161b90, (uintptr_t)g_DragPlacementTool_Vtable);
}
extern "C" void* DragPlacementTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x48, g_DragPlacementTool_Vtable, 1);}
extern "C" void* DragPlacementTool_Factory_SA(void) {
    return Tool_Alloc(0x48, g_DragPlacementTool_Vtable, 1);
}
REGISTER_TYPE(DragPlacementTool, "DragTool", &DragPlacementTool_Factory_SA, &DragPlacementTool_CreateInPlace_SA);
