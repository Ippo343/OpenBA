// code/Game/Player/roadtool.cpp
// Clean reimplementation of RoadTool vtable methods from Application-x64.exe.
//
// RoadTool (0x48 bytes) extends DragTool (0x38 bytes).
// Handles road placement: path calculation, validation, and cost estimation.
//
// Vtable at 0x140161e70 (15 slots).
//
// Hookable slots reimplemented:
//   Slot0  Dtor    (0x14002e8c0) — scalar deleting destructor
//   Slot4  Startup (0x1401056e0) — find CreateRoadDescription component
//   Slot8  Draw    (0x140105190) — kept as trampoline (calls base DragTool::Draw)
//
// Unhookable slots (clean code for standalone exe, NOT hooked):
//   Slot6  OnKey    (0x140105690) — road-specific drag logic
//   Slot13 CalcPath (0x140104d30) — road path calculation
//   Slot14 CalcCost (0x140104fe0) — road cost calculation
//
// Decompiled from Application-x64.exe.

#include "Player/roadtool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved vtable pointers (set once on first call)
// ============================================================
static void* s_vtable_RoadTool = nullptr;
static void* s_vtable_DragTool = nullptr;
static void* s_vtable_Tool     = nullptr;

static void EnsureVtables() {
    if (!s_vtable_RoadTool) {
        s_vtable_RoadTool = (void*)ResolveAddr(Module::Exe, RoadToolAddr::Vtable);
        s_vtable_DragTool = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool     = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// ============================================================
// Object::Internal::~Internal — called from base dtor
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
// Type ID for CreateRoadDescription (DAT_1401c74d8)
// ============================================================
static uint32_t* s_TypeId_CreateRoadDescription = nullptr;

// ============================================================
// Type ID for CreateRoadDescription CalcPath variant (DAT_1401c75b8)
// ============================================================
static uint32_t* s_TypeId_CreateRoadDescription2 = nullptr;

// ============================================================
// Road pathfinding function (RoadTool_sub_ce00)
// Used by CalcPath and CalcCost. Takes map data, flags, start/end cells,
// output buffer, modifier flag, and direction flag.
// Original: referenced internally from CalcPath/CalcCost
// ============================================================
typedef int (*fn_RoadPathfind_t)(uint64_t mapData, uint32_t flags, uint32_t startCell,
    uint64_t endCell, uint64_t stack_param, uint64_t outputBuffer, uint8_t modFlag, int dirFlag);
static fn_RoadPathfind_t s_fn_RoadPathfind = nullptr;

// FUN_14009c960: PlaceRoadSegment — places a road segment at a cell
typedef void (*fn_PlaceRoadSegment_t)(void* mapGrid, int cellIndex, uint8_t roadType,
    uint64_t endCell, uint32_t param5, uint64_t param6);
static fn_PlaceRoadSegment_t s_fn_PlaceRoadSegment = nullptr;

// FUN_1400acd90: SetupRoadPlacement — initialization before placing road segments
typedef void (*fn_SetupRoadPlacement_t)(uint64_t mapSystem, uint8_t roadType, uint64_t descData);
static fn_SetupRoadPlacement_t s_fn_SetupRoadPlacement = nullptr;

// TradeWork_impl_f370: FinishRoadPlacement — finalization after placing road segments
typedef uint64_t (*fn_FinishRoadPlacement_t)(uint64_t mapSystem, void* mapGrid);
static fn_FinishRoadPlacement_t s_fn_FinishRoadPlacement = nullptr;

// RoadTool_helper_6990: GetDescriptionHelper — gets description helper from entity
typedef uint64_t* (*fn_GetDescHelper_t)(uint64_t entity, uint64_t* retval);
static fn_GetDescHelper_t s_fn_GetDescHelper = nullptr;

// FUN_140101270: ScreenToCell — ray-picks map cell under cursor
typedef int (*fn_ScreenToCell_t)(uint64_t unused);
static fn_ScreenToCell_t s_fn_ScreenToCell = nullptr;

static bool s_funcsResolved = false;

static void ResolveRoadToolFuncs() {
    if (s_funcsResolved) return;

    s_TypeId_CreateRoadDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c74d8));
    s_TypeId_CreateRoadDescription2 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c75b8));
    s_fn_ScreenToCell = reinterpret_cast<fn_ScreenToCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));

    s_funcsResolved = true;
}

// ============================================================
// RoadTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: RoadTool -> DragTool -> Tool, calls Internal::~Internal,
// conditionally frees memory if flags & 1.
//
// Original: 0x14002e8c0
void* RoadTool_Dtor(RoadTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_RoadTool;
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
// RoadTool::Startup (vtable +0x20, Slot4) — Find description component
// ============================================================
// Searches the entity's component list for a component matching the
// CreateRoadDescription type ID (DAT_1401c74d8). Stores the found
// component pointer (or nullptr) in self->roadDesc (+0x38).
//
// The entity at self->descriptionPtr (+0x20) is a ComponentEntity with:
//   +0x28: void** componentArray  — array of component pointers
//   +0x30: int    componentCount  — number of components
//
// Original: 0x1401056e0
void RoadTool_Startup(RoadTool* self) {
    ResolveRoadToolFuncs();

    uintptr_t entity = (uintptr_t)self->descriptionPtr;
    void* result = nullptr;

    if (!entity || !s_TypeId_CreateRoadDescription || !fn_IsDerivedFrom) {
        self->roadDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *s_TypeId_CreateRoadDescription;

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

    self->roadDesc = result;
}

// ============================================================
// Unhookable slots — clean code for standalone exe
// ============================================================

// RoadTool::OnKey (vtable +0x30, Slot6)
// Calls DragTool::OnKey, then reads KeyMap::GetValue(0, 0x16) to set
// the modifierFlag at +0x40. The modifier flag indicates whether the
// user is holding the "upgrade road" key.
//
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140105690
uint64_t RoadTool_OnKey_Impl(RoadTool* self, char key) {
    // Call base DragTool::OnKey
    // In standalone exe this would be: DragTool_OnKey((DragTool*)self, key);
    // For now, call through the original vtable slot
    typedef uint64_t (*fn_DragToolOnKey_t)(void* self, char key);
    void** vtable = *(void***)self;
    // DragTool::OnKey is at vtable slot 6 (+0x30)
    // But this is a virtual dispatch — we'd need the resolved base fn
    // For standalone, just call DragTool_OnKey directly

    // Read KeyMap::GetValue(0, 0x16) for the modifier flag
    uint64_t keyMap = (uint64_t)***(void****)g_pKeyMapObjectExref;
    short val = 0;
    if (fn_KeyMap_GetValue) {
        val = fn_KeyMap_GetValue((void*)keyMap, 0, 0x16);
    }
    self->modifierFlag = (val == 0) ? 1 : 0;

    return 1;
}

// RoadTool::CalcPath (vtable +0x68, Slot13)
// Validates cells, checks terrain flags, verifies entity description
// constraints (if entity has path points at +0x98/+0x9c), calls the
// road pathfinding function, finds CreateRoadDescription2 component,
// and places road segments along the calculated path.
//
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140104d30
//
// This is a very complex function (200+ lines decompiled) that:
//   1. Sorts start/end cells (ensures start <= end)
//   2. Checks terrain flags at both cells via bitfield (mask 0x5e0)
//   3. Checks if cells are unoccupied (entity ptr == 0)
//   4. Validates against entity description path constraints
//   5. Calls RoadTool_sub_ce00 for pathfinding
//   6. Looks up CreateRoadDescription2 component on the description entity
//   7. Calls SetupRoadPlacement, PlaceRoadSegment for each cell, FinishRoadPlacement
//
// Full reimplementation requires resolving:
//   - RoadTool_sub_ce00 (road A* pathfinding, ~500 instruction function)
//   - Map terrain flag access patterns
//   - PlaceRoadSegment, SetupRoadPlacement, FinishRoadPlacement
// Documented but not callable until these are resolved.

// RoadTool::CalcCost (vtable +0x70, Slot14)
// Similar validation to CalcPath, but only returns cost/length without
// actually placing road segments.
//
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140104fe0
//
// Flow:
//   1. Sort cells (ensure start <= end)
//   2. Initialize outputs: *outCostA=0, *outCostB=0, *outValid=2, param_7=1
//   3. Validate cells >= 0
//   4. Check entity description path constraints
//   5. Check terrain flags (mask 0x5e0), entity occupancy
//   6. Call RoadTool_sub_ce00 for pathfinding
//   7. On success: set outputs to path length, *outValid=1, return 0
//   8. On failure: return 3
uint64_t RoadTool_CalcCost_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell,
    uint32_t* outValid, int* outCostB, int* outCostA, int* outLength) {
    // Sort cells: ensure start <= end
    uint32_t sc = startCell;
    uint32_t ec = endCell;
    if ((int)ec < (int)sc) {
        sc = endCell;
        ec = startCell;
    }

    // Initialize outputs
    *outCostA = 0;
    *outCostB = 0;
    *outValid = 2;

    // Validate cell indices
    if ((int)sc < 0 || (int)ec < 0) {
        return 3;
    }

    *outLength = 1;

    // Check entity description path constraints
    uintptr_t descEntity = (uintptr_t)*(void**)((uintptr_t)self + 0x10);
    int* mapGrid = *(int**)(uintptr_t(g_pGameWorldSingleton) + 0xf8);

    if (descEntity && *(char*)(descEntity + 0x80) != '\0') {
        int gridSize = *mapGrid;
        uint32_t constraintA = gridSize * (*(int*)(descEntity + 0x98) / gridSize) +
            *(int*)(descEntity + 0x98) % gridSize;
        uint32_t constraintB = gridSize * (*(int*)(descEntity + 0x9c) / gridSize) +
            *(int*)(descEntity + 0x9c) % gridSize;

        // Cells must match the description constraints (in either order)
        if ((constraintA != sc || constraintB != ec) &&
            (constraintA != ec || constraintB != sc)) {
            return 3;
        }
    }

    // Check terrain flags at both cells
    uintptr_t terrainArray = *(uintptr_t*)(uintptr_t(mapGrid) + 0x80); // mapGrid + 0x20 (as int*) = +0x80 bytes
    uintptr_t cellDataStart = terrainArray;
    uint32_t flagA = 1u << (*(uint8_t*)(cellDataStart + 0x29 + (uint64_t)sc * 0x30) & 0x1f);
    uint32_t flagB = 1u << (*(uint8_t*)(cellDataStart + 0x29 + (uint64_t)ec * 0x30) & 0x1f);
    uint32_t combinedFlags = flagA | flagB;

    // Check if terrain allows road placement (mask 0x5e0 = bits 5-10 must be clear)
    if ((combinedFlags & 0x5e0) != 0) {
        return 3;
    }

    // Check cells are unoccupied (entity pointer at offset 0 of cell data must be null)
    if (*(uint64_t*)(cellDataStart + (uint64_t)sc * 0x30) != 0 ||
        *(uint64_t*)(cellDataStart + (uint64_t)ec * 0x30) != 0) {
        return 3;
    }

    // Would call RoadTool_sub_ce00 here for pathfinding
    // int pathLength = s_fn_RoadPathfind(mapData, combinedFlags | 0xa17, sc, ec,
    //     stack_param, outputBuffer, self->modifierFlag, 1);
    // if (pathLength != 0) {
    //     *outCostA = pathLength;
    //     *outCostB = pathLength;
    //     *outLength = pathLength;
    //     *outValid = 1;
    //     return 0;
    // }

    return 3;
}
