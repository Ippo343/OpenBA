// bridgetool_draw.cpp — BridgeTool::Draw (vtable +0x40, slot 8) — 0x1400fadc0.
//
// This is the RED/GREEN validity overlay the player sees while dragging a bridge.
// Until now BridgeTool's slot 8 was `Tool_Ret0`, so a bridge drag rendered nothing:
// CalcCost happily reported `valid=1 len=N` every frame and the placement worked, but
// there was no on-map preview at all. The headless harness could not see this — it
// drives CalcCost/CalcPath directly and never renders (notes/BRIDGE_COMPLETE_BRIEF.md,
// "What the harness could not tell us").
//
// Ghidra mislabels 0x1400fadc0 as `BridgeTool_OnRemove`; the body is unmistakably a
// draw: it builds a tile array and hands it to the shared tile renderer 0x1401008c0
// (`CharacterComponent_impl_08c0` = fn_TileRender), exactly as RoadTool::Draw
// (0x140105190, ported in roadtool.cpp) does.
//
// Shape of the overlay — two kinds of tile, both pushed into ONE array:
//   * body cells   — every cell of the bounding rect between the drag start and the
//                    snapped end. Green when the path validates AND the cell's terrain
//                    is in (maskBody | maskBodyExtra) AND the cell is unoccupied.
//   * clearance    — only while actually dragging, a ring of `neighborRadius` cells to
//     ring cells    each side of the span (perpendicular to the run), mirroring the
//                    clearance test ValidatePath performs.
//
// Colors are literal in the binary: 0xffff0000 red, 0xff00ff00 green.
//
// Map-grid and CreateBridgeDescription layouts are documented in
// bridgetool_validate.cpp; the small accessors below are the same ones (kept file-local
// to match how roadtool.cpp carries its own copies).
//
// Field used here that ValidatePath does NOT read:
//   +0x28 maskBodyExtra — OR'd with maskBody for the preview's "is this cell part of
//                         the span?" test only. ValidatePath uses maskBody alone.
//
// Argument slots verified against the disassembly rather than the decompiler (this
// project has twice been burned by trusting decompiled argument order):
//   0x1400fadfc  CALL 0x140101270   RCX = self                  -> ScreenToCell
//   0x1400fae0b  CALL 0x1400fb710   RCX = self, EDX = [self+0x30] -> SnapEndCell
//   0x1400fae83  CALL 0x1400fb2d0   RCX = self, EDX = start, R8D = end, R9D = 0
//   0x1400fb27b  CALL 0x1401008c0   RDX = context, R8 = &tiles, R9 = &empty
//                                   (RCX is never loaded — a leftover register; the
//                                    road port passes nullptr for it too)

#include "BridgeTool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

namespace {

// ---- map-grid accessors (see bridgetool_validate.cpp for the layout) --------
inline uint32_t* MapGrid() {
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return nullptr;
    return *reinterpret_cast<uint32_t**>(reinterpret_cast<uintptr_t>(gameWorld) + 0xf8);
}
inline int64_t TerrainBase(uint32_t* mapGrid) {
    return *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);
}
inline uintptr_t CellAt(int64_t terrainBase, int cellIdx) {
    return static_cast<uintptr_t>(terrainBase) + static_cast<int64_t>(cellIdx) * 0x30;
}
inline uint32_t TerrainBit(uintptr_t cell) {
    return 1u << (*reinterpret_cast<uint8_t*>(cell + 0x29) & 0x1f);
}
// Pending/queued-cell BST at mapGrid+0xe8. Node: key=+0x00, left=+0x10, right=+0x18.
bool PendingContains(uint32_t* mapGrid, int cellIdx) {
    int* node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(mapGrid) + 0xe8);
    while (node != nullptr) {
        if (cellIdx < *node) {
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x10);
        } else {
            if (cellIdx <= *node) return true;
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
        }
    }
    return false;
}

// ---- tile overlay array (same layout roadtool.cpp uses) --------------------
struct TileRenderEntry {
    uint32_t col;
    uint32_t row;
    uint32_t type;    // 1 = cell's terrain is in the bridge body mask, else 0
    uint32_t color;   // ARGB
    uint32_t extra;   // always 0
};
static_assert(sizeof(TileRenderEntry) == 0x14, "TileRenderEntry size mismatch");

struct TileArray {
    TileRenderEntry* data;
    int count;
    int capacity;
};

// 1:1 with the inlined grow in retail: capacity goes to count*3 once past the first
// element, allocation is MemAlloc(cap * 0x14, 4).
void PushTileEntry(TileArray* arr, const TileRenderEntry* entry) {
    const int newCount = arr->count + 1;
    if (newCount > arr->capacity) {
        const int newCap = (arr->count == 0) ? newCount : newCount * 3;
        TileRenderEntry* newBuf = reinterpret_cast<TileRenderEntry*>(
            fn_MemAlloc(g_pMemAllocSingleton,
                        static_cast<uint64_t>(newCap) * sizeof(TileRenderEntry), 4));
        if (!newBuf) return;
        for (int i = 0; i < arr->count; i++) newBuf[i] = arr->data[i];
        newBuf[arr->count] = *entry;
        if (arr->data != newBuf) fn_MemFreeStatic(arr->data);
        arr->data = newBuf;
        arr->count = newCount;
        arr->capacity = newCap;
    } else {
        arr->data[arr->count] = *entry;
        arr->count = newCount;
    }
}

constexpr uint32_t kRed   = 0xffff0000u;
constexpr uint32_t kGreen = 0xff00ff00u;

// FUN_140101270 — ScreenToCell (ray-picks the map cell under the cursor).
typedef int (*fn_ScreenToCell_t)(void* self);
fn_ScreenToCell_t s_fn_ScreenToCell = nullptr;
bool s_resolved = false;

void EnsureFuncs() {
    if (s_resolved) return;
    s_fn_ScreenToCell = reinterpret_cast<fn_ScreenToCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));
    s_resolved = true;
}

}  // namespace

// ============================================================
// BridgeTool::Draw (vtable +0x40, Slot8) — 0x1400fadc0
// ============================================================
extern "C" void BridgeTool_Draw_Impl(void* selfPtr, void* context, char skip) {
    if (skip != '\0') return;

    EnsureFuncs();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfPtr);

    // ---- 1. Resolve the span being previewed -------------------------------
    int startCell, endCell;
    if (*reinterpret_cast<char*>(self + 0x28) == '\0') {
        // Not dragging: preview the single cell under the cursor.
        startCell = s_fn_ScreenToCell ? s_fn_ScreenToCell(selfPtr) : -1;
        endCell   = startCell;
    } else {
        startCell = *reinterpret_cast<int*>(self + 0x2c);
        endCell   = fn_BridgeTool_SnapEndCell
                  ? fn_BridgeTool_SnapEndCell(selfPtr, *reinterpret_cast<int*>(self + 0x30))
                  : -1;
    }
    if (startCell < 0) return;
    if (endCell < 0) endCell = startCell;

    uint32_t* mapGrid = MapGrid();
    if (!mapGrid) return;
    const int width = static_cast<int>(*mapGrid);
    if (width <= 0) return;
    const int64_t terrainBase = TerrainBase(mapGrid);
    if (!terrainBase) return;

    const uintptr_t desc = *reinterpret_cast<uintptr_t*>(self + 0x38);
    if (!desc) return;

    const uint32_t maskBody      = *reinterpret_cast<uint32_t*>(desc + 0x20);
    const uint32_t maskApproach  = *reinterpret_cast<uint32_t*>(desc + 0x24);
    const uint32_t maskBodyExtra = *reinterpret_cast<uint32_t*>(desc + 0x28);
    const uint32_t maskNeighbor  = *reinterpret_cast<uint32_t*>(desc + 0x30);
    const int      radius        = *reinterpret_cast<int*>(desc + 0x34);
    const uint32_t maskSpan      = maskBody | maskBodyExtra;

    // ---- 2. Bounding rect of the span --------------------------------------
    const int startRow = startCell / width, startCol = startCell % width;
    const int endRow   = endCell   / width, endCol   = endCell   % width;

    const int minCol = (endCol < startCol) ? endCol : startCol;
    const int maxCol = (endCol < startCol) ? startCol : endCol;
    const int minRow = (endRow < startRow) ? endRow : startRow;
    const int maxRow = (endRow < startRow) ? startRow : endRow;

    // Does the whole span validate? Drives the base color of every body cell.
    const bool pathValid = fn_BridgeTool_ValidatePath
        ? (fn_BridgeTool_ValidatePath(selfPtr, startCell, endCell, nullptr) != 0)
        : false;

    const bool isDrag = (startCell != endCell);

    // Map cell count — retail indexes the terrain array with no bounds test at all.
    // A span within `radius` of the map edge would read past the end of the array on
    // every frame of the drag, so the two `idx` range checks below are OURS, not
    // retail's. ValidatePath's port already guards the same way (`if (idx < 0)`).
    const int cellCount = width * width;

    TileArray tiles = {nullptr, 0, 0};

    // ---- 3. Body cells + clearance ring ------------------------------------
    for (int row = minRow; row <= maxRow; row++) {
        for (int col = minCol; col <= maxCol; col++) {
            const int bodyIdx = width * row + col;
            if (bodyIdx < 0 || bodyIdx >= cellCount) continue;

            const uintptr_t cell = CellAt(terrainBase, bodyIdx);
            const uint32_t  bit  = TerrainBit(cell);

            // type: is this cell's terrain part of the bridge body (i.e. water)?
            const uint32_t type = (maskBody & bit) != 0 ? 1u : 0u;

            // `color` deliberately persists across the clearance-ring iterations
            // below — retail reuses one register for both, so a red body cell forces
            // its whole ring red. Faithful, if surprising.
            uint32_t color;
            if (!pathValid || (maskSpan & bit) == 0 ||
                *reinterpret_cast<int64_t*>(cell) != 0) {
                color = kRed;
            } else {
                color = kGreen;
            }
            // Hovering a single cell: any approach (bank) terrain reads green, so the
            // player can see where a bridge is allowed to start.
            if (!isDrag && (maskApproach & bit) != 0) color = kGreen;

            TileRenderEntry entry = {static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                                     type, color, 0};
            PushTileEntry(&tiles, &entry);

            if (!isDrag) continue;

            // Clearance ring: `radius` cells to each side, perpendicular to the run.
            for (int off = -radius; off <= radius; off++) {
                if (off == 0) continue;

                int ringCol, ringRow;
                if (minCol == maxCol) {          // vertical span -> ring spreads in X
                    ringCol = col + off;
                    ringRow = row;
                } else {                          // horizontal span -> ring spreads in Y
                    ringCol = col;
                    ringRow = row + off;
                }
                if (ringCol < 0 || ringCol >= width || ringRow < 0) continue;
                const int idx = width * ringRow + ringCol;
                if (idx < 0 || idx >= cellCount) continue;

                const uintptr_t rCell = CellAt(terrainBase, idx);
                const uint32_t  rBit  = TerrainBit(rCell);

                const bool free = (*reinterpret_cast<int64_t*>(rCell) == 0)
                               && ((rBit & 2) == 0)
                               && ((rBit & 1) == 0)
                               && (*reinterpret_cast<int8_t*>(rCell + 0x2a) == -1);
                if (!free || PendingContains(mapGrid, idx)) color = kRed;

                if (color != kRed)
                    color = ((maskNeighbor & rBit) != 0) ? kGreen : kRed;

                TileRenderEntry rEntry = {static_cast<uint32_t>(ringCol),
                                          static_cast<uint32_t>(ringRow),
                                          type, color, 0};
                PushTileEntry(&tiles, &rEntry);
            }
        }
    }

    // ---- 4. Submit -----------------------------------------------------------
    // Retail passes an (uninitialised) RCX and a second, always-empty tile array in R9.
    TileArray empty = {nullptr, 0, 0};
    if (fn_TileRender) fn_TileRender(nullptr, context, &tiles, &empty);

    // Report the first hover frame and the first real DRAG frame separately — a hover
    // only ever emits one tile, so "it renders" says nothing about the span loop.
    {
        static bool s_loggedHover = false, s_loggedDrag = false;
        bool* slot = isDrag ? &s_loggedDrag : &s_loggedHover;
        if (!*slot && tiles.count > 0) {
            *slot = true;
            int green = 0;
            for (int i = 0; i < tiles.count; i++)
                if (tiles.data[i].color == kGreen) green++;
            fprintf(stderr, "[Bridge] Draw(%s): start=%d end=%d rect=(%d,%d)..(%d,%d) "
                    "valid=%d radius=%d tiles=%d green=%d red=%d render=%p\n",
                    isDrag ? "drag" : "hover", startCell, endCell, minCol, minRow,
                    maxCol, maxRow, (int)pathValid, radius, tiles.count, green,
                    tiles.count - green, (void*)fn_TileRender);
            fflush(stderr);
        }
    }

    fn_MemFreeStatic(empty.data);
    fn_MemFreeStatic(tiles.data);
}
