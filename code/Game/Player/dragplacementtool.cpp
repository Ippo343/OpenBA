// code/Game/Player/dragplacementtool.cpp
// Clean reimplementation of DragPlacementTool vtable methods from Application-x64.exe.
//
// DragPlacementTool (0x48 bytes) extends DragTool (0x38 bytes).
// Handles rectangular area placement (e.g., stockpile, marketplace).
//
// Vtable at 0x140161b90 (15 slots). Three are reimplemented here:
//   Slot0  Dtor    (0x14002d900) — scalar deleting destructor
//   Slot4  Startup (0x1400fc3e0) — find description components on entity
//   Slot14 CalcCost(0x1400fbae0) — validate area and calculate cost
//
// Remaining slots (Draw, CalcPath) are too complex (terrain validation, entity
// creation, scene rendering) and stay as trampolines until helper functions
// are typed and resolved.
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
    if (g_TypeId_AreaDescription) {
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
// Original: 0x1400ff5d0
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
        ResolveAddr(Module::Exe, 0x1400ff5d0));
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
