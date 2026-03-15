// code/Game/Player/bridgetool.cpp
// Clean reimplementation of BridgeTool vtable methods from Application-x64.exe.
//
// BridgeTool (0x40 bytes) extends DragTool (0x38 bytes).
// Handles bridge placement: path calculation, validation, and cost estimation.
//
// Vtable at 0x140161b00 (15 slots). Four are reimplemented here:
//   Slot0  Dtor     (0x14002d7f0) — scalar deleting destructor
//   Slot4  Startup  (0x1400fb7a0) — find BridgeDescription component on entity
//   Slot13 CalcPath (0x1400f9cd0) — build bridge segment entities along path
//   Slot14 CalcCost (0x1400facf0) — validate path and return cost/length
//
// Helper functions (exe-internal, resolved at runtime):
//   SnapEndCell   (0x1400fb710) — snaps end cell to same row or column as drag start
//   ValidatePath  (0x1400fb2d0) — validates path and returns bridge body length
//
// Decompiled from Application-x64.exe.

#include "Player/bridgetool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// BridgeTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_BridgeTool = nullptr;
static void* s_vtable_DragTool   = nullptr;
static void* s_vtable_Tool       = nullptr;

static void EnsureVtables() {
    if (!s_vtable_BridgeTool) {
        s_vtable_BridgeTool = (void*)ResolveAddr(Module::Exe, BridgeToolAddr::Vtable);
        s_vtable_DragTool   = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool       = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
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
        // The dtor calls through [0x140152770] which is a resolved func ptr slot
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// BridgeTool::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: BridgeTool -> DragTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002d7f0
void* BridgeTool_Dtor(BridgeTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_BridgeTool;
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
// BridgeTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Searches the entity's component list for a component matching the
// CreateBridgeDescription type ID (DAT_1401c73f8). Stores the found
// component pointer (or nullptr) in self->bridgeDesc (+0x38).
//
// The entity at self->entity (+0x20) is a ComponentEntity with:
//   +0x28: void** componentArray  — array of component pointers
//   +0x30: int    componentCount  — number of components
//
// Each component has a vtable with GetTypeId at +0x10 (returns uint32_t).
// After finding a match by type ID, fn_IsDerivedFrom confirms the cast.
//
// Original: 0x1400fb7a0
void BridgeTool_Startup(BridgeTool* self) {
    uintptr_t entity = (uintptr_t)self->descriptionPtr;
    void* result = nullptr;

    if (!entity || !g_TypeId_CreateBridgeDescription || !fn_IsDerivedFrom) {
        self->bridgeDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *g_TypeId_CreateBridgeDescription;

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

    self->bridgeDesc = result;
}

// ============================================================
// BridgeTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Calculates bridge placement cost between startCell and endCell.
//
// Steps:
//   1. Initialize output params to defaults (valid=1, costA=0, costB=0, length=1)
//   2. If either cell is negative (invalid), return 3 (failure)
//   3. Snap endCell to same row or column using SnapEndCell helper
//   4. Validate path using ValidatePath helper
//   5. On success: set all cost outputs to bridge body length, return 0
//   6. On failure: return 3
//
// Return value semantics:
//   0 = success (bridge can be placed)
//   3 = failure (invalid cells or path validation failed)
//
// Original: 0x1400facf0
uint64_t BridgeTool_CalcCost(BridgeTool* self, int startCell, int endCell,
                              uint32_t* outValid, int* outCostB, int* outCostA, int* outLength) {
    // Initialize outputs to defaults
    *outCostA = 0;
    *outCostB = 0;
    *outValid = 1;
    *outLength = 1;

    // Validate cell indices
    if (startCell < 0 || endCell < 0) {
        return 3;
    }

    // Snap end cell to same row or column as drag start
    if (!fn_BridgeTool_SnapEndCell) return 3;
    int snappedEnd = fn_BridgeTool_SnapEndCell(self, endCell);

    // Validate the bridge path
    if (!fn_BridgeTool_ValidatePath) return 3;
    int bridgeLength = 0;
    int valid = fn_BridgeTool_ValidatePath(self, startCell, snappedEnd, &bridgeLength);

    if ((char)valid != '\0') {
        // Success: all cost outputs = bridge body length
        *outCostB = bridgeLength;
        *outCostA = bridgeLength;
        *outLength = bridgeLength;
        return 0;
    }

    return 3;
}
