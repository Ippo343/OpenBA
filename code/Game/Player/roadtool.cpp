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
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "Action/exe_bodies.h"   // RoadTool_sub_ce00 (0x14009ce00), called by name
#include "Player/exe_bodies.h"
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"
#include "collection/Collection.h"
#include "Component/CreateRoadDescription.h"
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)
#include <cstdint>
#include <cstdio>

// 0x1401c74d8 is UNREGISTERED (Table 1): ResolveAddr always answers 0, so
// CreateRoadDescription::TypeId() is the only thing that has ever reached the
// derived check below (mechanically equivalent to the old raw-read +
// unconditional fallback). Read once ResolveRoadToolFuncs runs (after
// Engine::Startup), not at static-init time.
static uint32_t s_TypeId_CreateRoadDescription_v = 0;
static const uint32_t* s_TypeId_CreateRoadDescription = nullptr;

// ============================================================
// Road pathfinding function (RoadTool_sub_ce00, 0x14009ce00, code/Game/Action/
// keystone_a_zone.cpp), called by name (B3R Leftovers). Used by CalcPath and CalcCost.
// Takes map data, flags, start/end cells, an unread stack argument, the output buffer,
// modifier flag and direction flag; returns the path length (retail's EAX: the tail
// call 0x1400a6e30's int). The door typedef passed the direction flag as `int`; the
// body takes a byte and retail reads a byte (MOVZX at 0x14009cef1), so the calls pass
// the same low byte.
// ============================================================

// (Four statics for 0x14009c960 / 0x1400acd90 / 0x1400af370 / 0x140026990 were never
// resolved nor called -- dead readers, gone. 0x140101270 ScreenToCell is called by
// name: PlayerRay_ScreenToCell_101270.)

static bool s_funcsResolved = false;

static void ResolveRoadToolFuncs() {
    if (s_funcsResolved) return;

    s_TypeId_CreateRoadDescription_v = CreateRoadDescription::TypeId();
    s_TypeId_CreateRoadDescription = &s_TypeId_CreateRoadDescription_v;
    s_funcsResolved = true;
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
void RoadTool::Startup() {
    ResolveRoadToolFuncs();

    uintptr_t entity = (uintptr_t)descriptionPtr;
    void* result = nullptr;

    if (!entity || !s_TypeId_CreateRoadDescription) {
        roadDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *s_TypeId_CreateRoadDescription;

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // Virtual slot 2 (+0x10): Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom before casting
            if (Object::IsDerivedFromNullSafe(comp, targetTypeId)) {
                result = comp;
            }
            break;
        }
    }

    roadDesc = result;
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
char RoadTool::OnKey(char key) {
    // Retail (0x140105690) opens with `CALL 0x1400fc6b0` = DragTool::OnKey — a direct
    // rel32 call to the base, NOT a virtual dispatch (dispatching through vtable[6]
    // would re-enter RoadTool::OnKey and recurse forever). This call IS the drag
    // state machine: it starts/updates the drag, records startCell/endCell at +0x2c/
    // +0x30, drives the per-frame cost preview via slot 14 (CalcCost) and commits via
    // slot 13 (CalcPath) on release. Omitting it left the road tool inert — it would
    // activate but never respond to the mouse.
    DragTool::OnKey(key);

    // Read KeyMap::GetValue(0, 0x16) for the modifier flag
    uint64_t keyMap = (uint64_t)***(void****)g_pInputConfigSingletonExref;
    short val = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetValue(0, 0x16);  // B.3.7 bridge
    modifierFlag = (val == 0) ? 1 : 0;

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
int RoadTool::CalcCost(int startCell, int endCell, uint32_t* outValid,
                       int* outCostB, int* outCostA, int* outLength) {
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
    uintptr_t descEntity = (uintptr_t)dialogPtr;
    // g_pGameWorldSingleton is the singleton SLOT (void**), so the world pointer needs a
    // deref first — this read was using the slot's own address as the world base, so
    // mapGrid came back null and the terrainArray read below faulted on 0x80. RoadTool::Draw
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
    pathLength = RoadTool_sub_ce00((int64_t)(uintptr_t)mapGrid, combinedFlags | 0xa17, sc, ec,
                                   nullptr, (int64_t)(uintptr_t)pathBuf, modifierFlag, 1);
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

// Draw calls 0x140101270 PlayerRay_ScreenToCell_101270 and 0x1401008c0
// PlayerTileRender_DrawOverlay_08c0 by name, and the road A* RoadTool_sub_ce00 too.

// ============================================================
// RoadTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Renders colored tile overlay for road placement preview.
// Two phases:
//   1. Constrained path from description entity (+0x80 flag)
//   2. Drag selection with pathfinding (strict then relaxed mask)
//
// Original: 0x140105190
void RoadTool::Draw(void* context, char param3) {
    ResolveRoadToolFuncs();

    int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
    uintptr_t descEntity = (uintptr_t)dialogPtr;
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
        pathLen = RoadTool_sub_ce00((int64_t)(uintptr_t)mapGrid, 0xa17, (uint32_t)startNorm,
            (uint32_t)endNorm, nullptr, (int64_t)(uintptr_t)pathBuf, modifierFlag, 1);

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
        PlayerTileRender_DrawOverlay_08c0(0, context, reinterpret_cast<int64_t*>(&pathTiles),
                                          reinterpret_cast<int64_t*>(&emptyTiles));

        // pathTiles / emptyTiles are released by ~Array at scope exit (no early
        // return inside this block).
    }

    // ---- Phase 2: Drag selection with pathfinding ----
    if (param3 == '\0') {
        uint32_t startCell, endCell;

        if (dragState == '\0') {
            int cell = PlayerRay_ScreenToCell_101270(0);
            startCell = (uint32_t)cell;
            endCell   = (uint32_t)cell;
        } else {
            startCell = (uint32_t)this->startCell;
            endCell   = (uint32_t)this->endCell;
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
            pathLen = RoadTool_sub_ce00((int64_t)(uintptr_t)mapGrid, combinedFlags | 0xa17,
                sc, ec, nullptr, (int64_t)(uintptr_t)pathBuf, modifierFlag, 1);
        }

        if (pathLen == 0) {
            // Fallback: relaxed pathfinding (mask: combinedFlags | 0xff7)
            pathLen = RoadTool_sub_ce00((int64_t)(uintptr_t)mapGrid, combinedFlags | 0xff7,
                sc, ec, nullptr, (int64_t)(uintptr_t)pathBuf, modifierFlag, 0);
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
            PlayerTileRender_DrawOverlay_08c0(0, context, reinterpret_cast<int64_t*>(&pathTiles),
                                              reinterpret_cast<int64_t*>(&emptyTiles));

            // pathTiles / emptyTiles are released by ~Array at scope exit (the
            // enclosing block's only early return, `startCell < 0`, precedes them).
        }
    }
}

// ============================================================
// RoadTool members (RoadTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_RoadTool_Vtable[15], its slot stubs, the _SA factories and
// RegisterRoadToolVtable are gone: the compiler builds the table, and nothing
// reads 0x140161e70 back by address (the old dtor was the only reader).
// Inherited from the bases, as in retail: [9] DragTool::OnCancel (0x1400fc640),
// [10] false, [11] no setting (0x140004950 `*ret = 0`; SetEditMode
// dereferences the slot), [12] cursor 5 (0x1400fc9d0).
// ============================================================
#include <new>

// [4] Startup, [6] OnKey, [8] Draw and [14] CalcCost are defined above, [13]
// CalcPath in roadtool_calcpath.cpp.

// create 0x14002e870: MemAlloc(0x48, 8), then the ctor 0x1401057d0.
void* RoadTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RoadTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RoadTool());
}
// createInPlace 0x14002e8b0: `if (storage) ctor(storage)`.
void* RoadTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RoadTool());
}

DEFINE_EXE_TYPE(RoadTool, "DragTool")
