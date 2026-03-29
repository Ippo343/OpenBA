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
#include <cstdint>
#include <cstdio>

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
