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
//   Slot8  Draw    (0x140105190) — road path preview rendering
//
// Unhookable slots (clean code for standalone exe, NOT hooked):
//   Slot6  OnKey    (0x140105690) — road-specific drag logic
//   Slot13 CalcPath (0x140104d30) — road path calculation
//   Slot14 CalcCost (0x140104fe0) — road cost calculation
//
// Decompiled from Application-x64.exe.

#include "Player/roadtool_funcs.h"
#include "Player/dragtool.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "collection/Collection.h"
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)
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
// Type ID for DecalDescription (DAT_1401c75b8) — the road's decal/texture
// description, looked up by CalcPath (0x140104d30) to get the surface it paints.
// NOT a "CreateRoadDescription variant": the RegisterType DATA writer at
// 0x14014ee35 (DecalDescription_Register) names this global DecalDescription.
// ============================================================
static uint32_t* s_TypeId_DecalDescription = nullptr;

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
    s_TypeId_DecalDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c75b8));
    s_fn_ScreenToCell = reinterpret_cast<fn_ScreenToCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));
    // Road A* (0x14009ce00). Shared by Draw and CalcCost; resolved here so CalcCost —
    // which runs before ResolveRoadDrawFuncs is even declared — can reach it too.
    if (!s_fn_RoadPathfind) {
        s_fn_RoadPathfind = reinterpret_cast<fn_RoadPathfind_t>(
            ResolveAddr(Module::Exe, 0x14009ce00));
    }

    // Both globals above are raw .rdata constants, so ResolveAddr returns NULL in the
    // standalone exe ([[reference_standalone_rdata_null_fallback]]). RoadTool::Startup
    // then matched no component and left roadDesc (+0x38) null, which is what made the
    // road tool activate but do nothing. Fall back to the live factory tree by name —
    // the proven ToolbarStandalone_Init / placementtool pattern. Both names verified
    // against their RegisterType DATA writers (14014ec90, 14014ee10).
    static uint32_t s_tid_CreateRoad = 0;
    static uint32_t s_tid_Decal      = 0;
    if (!s_TypeId_CreateRoadDescription) {
        s_tid_CreateRoad = ::Object::ResolveTypeIdByNameLazy("CreateRoadDescription");
        if (s_tid_CreateRoad) s_TypeId_CreateRoadDescription = &s_tid_CreateRoad;
    }
    if (!s_TypeId_DecalDescription) {
        s_tid_Decal = ::Object::ResolveTypeIdByNameLazy("DecalDescription");
        if (s_tid_Decal) s_TypeId_DecalDescription = &s_tid_Decal;
    }

    // Only latch once both type-ids are in hand. ResolveTypeIdByNameLazy needs the
    // factory tree, which is not populated until after Engine::Startup; Startup can run
    // before that, and latching early would leave the ids null for the whole session.
    s_funcsResolved = (s_TypeId_CreateRoadDescription != nullptr);
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
    // Retail (0x140105690) opens with `CALL 0x1400fc6b0` = DragTool::OnKey — a direct
    // rel32 call to the base, NOT a virtual dispatch (dispatching through vtable[6]
    // would re-enter RoadTool_OnKey_Impl and recurse forever). This call IS the drag
    // state machine: it starts/updates the drag, records startCell/endCell at +0x2c/
    // +0x30, drives the per-frame cost preview via slot 14 (CalcCost) and commits via
    // slot 13 (CalcPath) on release. Omitting it left the road tool inert — it would
    // activate but never respond to the mouse.
    DragTool_OnKey(reinterpret_cast<DragTool*>(self), key);

    // Read KeyMap::GetValue(0, 0x16) for the modifier flag
    uint64_t keyMap = (uint64_t)***(void****)g_pInputConfigSingletonExref;
    short val = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0x16);  // B.3.7 bridge
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
    ResolveRoadToolFuncs();

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
    // g_pGameWorldSingleton is the singleton SLOT (void**), so the world pointer needs a
    // deref first — this read was using the slot's own address as the world base, so
    // mapGrid came back null and the terrainArray read below faulted on 0x80. RoadTool_Draw
    // (below) already had this right.
    int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);

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

    // Run the road A* (0x14009ce00, ported in Action/keystone_a_zone.cpp and registered
    // by reg_keystone_a_zone.cpp). Retail passes mask `combinedFlags | 0xa17` and the
    // modifier byte at +0x40, writing the cell path into a stack buffer. This call was
    // left commented out, so CalcCost always fell through to `return 3` (invalid) — the
    // drag preview could never report a valid road or a cost.
    int pathBuf[2048];   // retail reserves 8184 bytes for the same buffer
    int pathLength = 0;
    if (s_fn_RoadPathfind) {
        pathLength = s_fn_RoadPathfind((uintptr_t)mapGrid, combinedFlags | 0xa17, sc, ec,
                                       0, (uintptr_t)pathBuf, self->modifierFlag, 1);
    }
    if (pathLength != 0) {
        *outCostA  = pathLength;
        *outCostB  = pathLength;
        *outLength = pathLength;
        *outValid  = 1;
        return 0;
    }

    return 3;
}

// ============================================================
// Tile entry for rendering (0x14 bytes = 5 uint32_t)
// ============================================================
struct TileRenderEntry {
    uint32_t col;
    uint32_t row;
    uint32_t type;     // 0 = flat tile, 1 = road tile
    uint32_t color;    // ARGB packed color
    uint32_t extra;    // 0
};
static_assert(sizeof(TileRenderEntry) == 0x14, "TileRenderEntry size mismatch");

// Dynamic array holding TileRenderEntry items
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
// Resolved exe-internal function pointer for Draw (ScreenToCell)
// ============================================================
// FUN_140101270: ScreenToCell — reuses s_fn_ScreenToCell from ResolveRoadToolFuncs

static bool s_roadDrawFuncsResolved = false;

static void ResolveRoadDrawFuncs() {
    if (s_roadDrawFuncsResolved) return;

    // Road pathfinding — reuse existing s_fn_RoadPathfind
    if (!s_fn_RoadPathfind) {
        s_fn_RoadPathfind = reinterpret_cast<fn_RoadPathfind_t>(
            ResolveAddr(Module::Exe, 0x14009ce00));
    }

    s_roadDrawFuncsResolved = true;
}

// ============================================================
// RoadTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Renders colored tile overlay for road placement preview.
// Two phases:
//   1. Constrained path from description entity (+0x80 flag)
//   2. Drag selection with pathfinding (strict then relaxed mask)
//
// Original: 0x140105190
void RoadTool_Draw(RoadTool* self, void* context, char param3) {
    ResolveRoadDrawFuncs();
    ResolveRoadToolFuncs();

    int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
    uintptr_t descEntity = *(uintptr_t*)((uintptr_t)self + 0x10);
    int pathBuf[2048]; // large stack buffer for pathfinding output

    // ---- Phase 1: Constrained path from description entity ----
    if (*(char*)(descEntity + 0x80) != '\0') {
        int gridSize = *mapGrid;
        int constrainStart = *(int*)(descEntity + 0x98);
        int constrainEnd   = *(int*)(descEntity + 0x9c);

        // Normalize to cell indices
        int startNorm = gridSize * (constrainStart / gridSize) + constrainStart % gridSize;
        int endNorm   = gridSize * (constrainEnd / gridSize) + constrainEnd % gridSize;

        int pathLen = 0;
        if (s_fn_RoadPathfind) {
            pathLen = s_fn_RoadPathfind((uintptr_t)mapGrid, 0xa17, (uint32_t)startNorm,
                (uint32_t)endNorm, 0, (uintptr_t)pathBuf, self->modifierFlag, 1);
        }

        // Build tile array from path results
        TileArray pathTiles;
        for (int i = 0; i < pathLen; i++) {
            TileRenderEntry entry;
            entry.col   = (uint32_t)(pathBuf[i] % *mapGrid);
            entry.row   = (uint32_t)(pathBuf[i] / *mapGrid);
            entry.type  = 0;
            entry.color = 0xffffff00; // yellow default
            entry.extra = 0;
            PushTileEntry(&pathTiles, &entry);
        }

        // Render: path tiles in R8, empty array in R9
        TileArray emptyTiles;
        if (fn_TileRender) {
            fn_TileRender(nullptr, context, &pathTiles, &emptyTiles);
        }

        // pathTiles / emptyTiles are released by ~Array at scope exit (no early
        // return inside this block).
    }

    // ---- Phase 2: Drag selection with pathfinding ----
    if (param3 == '\0') {
        uint32_t startCell, endCell;

        if (*(char*)((uintptr_t)self + 0x28) == '\0') {
            int cell = -1;
            if (s_fn_ScreenToCell) {
                cell = s_fn_ScreenToCell(0);
            }
            startCell = (uint32_t)cell;
            endCell   = (uint32_t)cell;
        } else {
            startCell = (uint32_t)*(int*)((uintptr_t)self + 0x2c);
            endCell   = (uint32_t)*(int*)((uintptr_t)self + 0x30);
        }

        if ((int)startCell < 0) return;

        // Sort cells: ensure start <= end
        uint32_t sc = startCell, ec = endCell;
        if ((int)ec >= 0 && (int)ec < (int)sc) {
            sc = endCell;
            ec = startCell;
        }

        if ((int)ec < 0) ec = sc;

        // Get terrain data
        uintptr_t terrainBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);

        // Check terrain flags at both cells
        uint32_t flagA = 1u << (*(uint8_t*)(terrainBase + 0x29 + (uint64_t)sc * 0x30) & 0x1f);
        uint32_t flagB = 1u << (*(uint8_t*)(terrainBase + 0x29 + (uint64_t)ec * 0x30) & 0x1f);
        uint32_t combinedFlags = flagA | flagB;

        // Both cells must be valid terrain (mask 0x5e0 clear) and unoccupied
        bool valid = ((combinedFlags & 0x5e0) == 0) &&
                     (*(uintptr_t*)(terrainBase + (uint64_t)sc * 0x30) == 0) &&
                     (*(uintptr_t*)(terrainBase + (uint64_t)ec * 0x30) == 0);

        int pathLen = 0;

        if (valid) {
            // Try strict pathfinding first (mask: combinedFlags | 0xa17)
            if (s_fn_RoadPathfind) {
                pathLen = s_fn_RoadPathfind((uintptr_t)mapGrid, combinedFlags | 0xa17,
                    sc, ec, 0, (uintptr_t)pathBuf, self->modifierFlag, 1);
            }
        }

        if (pathLen == 0) {
            // Fallback: relaxed pathfinding (mask: combinedFlags | 0xff7)
            if (s_fn_RoadPathfind) {
                pathLen = s_fn_RoadPathfind((uintptr_t)mapGrid, combinedFlags | 0xff7,
                    sc, ec, 0, (uintptr_t)pathBuf, self->modifierFlag, 0);
            }
        }

        if (pathLen > 0) {
            // Build tile array with per-tile color coding
            TileArray pathTiles;

            for (int i = 0; i < pathLen; i++) {
                int cellIdx = pathBuf[i];
                uintptr_t cellData = terrainBase + (uintptr_t)cellIdx * 0x30;
                uint8_t terrainByte = *(uint8_t*)(cellData + 0x29);
                uint32_t terrainFlag = 1u << (terrainByte & 0x1f);

                // Color: green if valid road terrain (0xa17), red if invalid or occupied
                uint32_t color;
                if ((terrainFlag & 0xa17) == 0 || *(uintptr_t*)cellData != 0) {
                    color = 0xffff0000; // red — invalid
                } else {
                    color = 0xff00ff00; // green — valid
                }

                TileRenderEntry entry;
                entry.col   = (uint32_t)(cellIdx % *mapGrid);
                entry.row   = (uint32_t)(cellIdx / *mapGrid);
                entry.type  = 1; // road tile
                entry.color = color;
                entry.extra = 0;
                PushTileEntry(&pathTiles, &entry);
            }

            // Render: path tiles in R8, empty array in R9
            TileArray emptyTiles;
            if (fn_TileRender) {
                fn_TileRender(nullptr, context, &pathTiles, &emptyTiles);
            }

            // pathTiles / emptyTiles are released by ~Array at scope exit (the
            // enclosing block's only early return, `startCell < 0`, precedes them).
        }
    }
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// RoadTool: size 0x48, parent DragTool, 15-slot vtable @ 0x140161e70.
// NOTE: slot13 CalcPath now lives in roadtool_calcpath.cpp; slot14 CalcCost is here.
// ============================================================
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RoadTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RoadTool");
    return c;
}
// +0x58 (Slot11) GetToolSettingTypeId — retail RoadTool slot 11 is 0x140004950, the
// SAME shared `*ret = 0; return ret;` thunk RemovalTool and PathTool use (RoadTool has
// no tool-setting; only PlacementTool's RotationSetting does). SetEditMode
// (FUN_140102b60 / Toolbar_ShowGroup_Impl) DEREFERENCES the returned pointer to decide
// whether to build a ToolSetting, so the previous Tool_Ret0 stub (returns 0 -> *0)
// would fault the instant a road button click created the tool.
static uint32_t* RoadTool_GetSettingType(void*, uint32_t* ret) {
    if (ret) *ret = 0;
    return ret;
}
// +0x60 (Slot12) cursor id — retail 0x1400fc9d0 returns 5 (shared with FencedTool).
// EntityUI_ToolTick_SA feeds this to SetCursor, so Tool_Ret0 gave the road tool the
// wrong (id 0) cursor.
static int RoadTool_Slot12(void*) { return 5; }
// +0x48 (Slot9) cancel/"can-deactivate?" — retail 0x1400fc640, the shared DragTool-family
// right-click handler (cancels an active drag + closes the tool dialog, returns 1 to keep
// the tool; returns 0 when idle so EntityUI proceeds with teardown). See dragtool.cpp.
extern "C" int DragTool_CancelDrag_Impl(void* self);
static void* g_RoadTool_Vtable[15] = {
    (void*)&RoadTool_Dtor,        // +0x00 Dtor
    (void*)&Tool_Slot1,           // +0x08
    (void*)&RoadTool_GetTypeId,   // +0x10 GetTypeId
    (void*)&Tool_Persist,         // +0x18 Serialize
    (void*)&RoadTool_Startup,     // +0x20 Startup
    (void*)&Tool_Ret0,            // +0x28
    (void*)&RoadTool_OnKey_Impl,  // +0x30 OnKey
    (void*)&Tool_Ret0,            // +0x38
    (void*)&RoadTool_Draw,        // +0x40 Draw
    (void*)&DragTool_CancelDrag_Impl, // +0x48 Slot9 cancel drag / allow-teardown (0x1400fc640)
    (void*)&Tool_Ret0,            // +0x50
    (void*)&RoadTool_GetSettingType, // +0x58 Slot11 GetToolSettingTypeId (*ret=0)
    (void*)&RoadTool_Slot12,      // +0x60 cursor id (retail returns 5)
    (void*)&RoadTool_CalcPath_Impl, // +0x68 Slot13 CalcPath (roadtool_calcpath.cpp)
    (void*)&RoadTool_CalcCost_Impl, // +0x70 CalcCost
};
extern "C" void RegisterRoadToolVtable() {
    RegisterExeAddr(0x140161e70, (uintptr_t)g_RoadTool_Vtable);
}
extern "C" void* RoadTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x48, g_RoadTool_Vtable, 1);}
extern "C" void* RoadTool_Factory_SA(void) {
    return Tool_Alloc(0x48, g_RoadTool_Vtable, 1);
}
REGISTER_TYPE(RoadTool, "DragTool", &RoadTool_Factory_SA, &RoadTool_CreateInPlace_SA);
