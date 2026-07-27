// bridgetool_calcpath.cpp — BridgeTool::CalcPath (0x1400f9cd0), the bridge COMMIT.
//
// Dispatched from vtable slot 13 when a bridge drag is released. Before this file the
// slot delegated to ResolveAddr(Module::Exe, 0x1400f9cd0) — a leftover from the removed
// proxy-DLL mode. In the standalone exe there is no original exe to delegate to, so it
// resolved to 0, logged "[BridgeTool] ERROR: CalcPath function pointer is NULL", and no
// bridge was ever built.
//
// Shape (823 decompiled lines, condensed):
//   1. Snap the end cell, then gate on ValidatePath (bridgetool_validate.cpp).
//   2. Build a doubly-linked list of segment descriptors covering the span. Two size-0
//      end nodes (the bank pieces, drawn as 1 cell each) bracket a middle filled
//      CENTRE-OUTWARD with the largest fitting power-of-2 segment (8/4/2/1).
//      Node = MemAlloc(0x18, 8): +0x00 size(int) · +0x08 prev · +0x10 next.
//      In Ghidra the nodes decompile as float* and the sizes as denormals —
//      1.4013e-45 = 1, 2.8026e-45 = 2, 5.60519e-45 = 4, 1.12104e-44 = 8.
//   3. Clone a master ComponentEntity from the tool's description; its MapComponent
//      covers the whole span (plus the description's clearance radius) and its
//      MultiObjectComponent collects the segment entities.
//   4. Per segment: clone from CreateBridgeDescription's segmentTemplates (+0x38[0..4],
//      indexed 0=end piece, 1/2/3/4 = 1/2/4/8-cell), stamp cell coords + rotation onto its
//      MapComponent, build the yaw transform onto its ModelComponent, append it to the
//      master's MultiObjectComponent, and register it with the world entity list.
//   5. Stamp the total cell count on the MultiObjectComponent, register the master, free
//      the list.
//
// Component field layout is shared with the working DragPlacementTool commit
// (dragplacementtool.cpp): +0x28 col · +0x2a row · +0x2c width · +0x2e height (all int16)
// · +0x30 rotation (int32, degrees, normalised into [0,360) by adding 0x168).
// ComponentEntity component array is at entity+0x30 with the count at entity+0x38 —
// NOT the +0x28/+0x30 pair used for *description* entities in BridgeTool::Startup.
//
// KNOWN OMISSIONS vs retail, both deliberate and non-fatal:
//   · The f3e0 terrain-height/world-position math. Retail computes a per-segment world
//     position by averaging two corner samples, but then passes a ZERO offset to
//     FromYawPitchOffset, so the value never reaches the transform. Placement is driven
//     entirely by the MapComponent cell coords, exactly as DragPlacementTool does.
//   · Picking::Scene::SetMeshUserData — unbound in standalone. Only affects click-picking
//     a finished bridge; the geometry still renders and registers.

#include "BridgeTool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "address_registry.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" int      BridgeTool_SnapEndCell_Impl(void* self, int endCell);
extern "C" uint32_t BridgeTool_ValidatePath_Impl(void* self, int startCell, int endCell,
                                                 int* outLength);

namespace {

// ---- segment list -------------------------------------------------------------
struct SegNode {
    int32_t  size;      // +0x00  0 = bank end piece (drawn 1 cell wide)
    int32_t  _pad;      // +0x04
    SegNode* prev;      // +0x08
    SegNode* next;      // +0x10
};
static_assert(sizeof(SegNode) == 0x18, "SegNode must match MemAlloc(0x18,8)");

SegNode* SegAlloc(int size) {
    if (!fn_MemAlloc) return nullptr;
    SegNode* n = reinterpret_cast<SegNode*>(fn_MemAlloc(g_pMemAllocSingleton, 0x18, 8));
    if (!n) return nullptr;
    n->size = size; n->_pad = 0; n->prev = nullptr; n->next = nullptr;
    return n;
}

// Largest power of two (capped at 8) that fits in n — retail's nested <8/<4/<2 ladder.
inline int LargestFit(int n) {
    if (n >= 8) return 8;
    if (n >= 4) return 4;
    if (n >= 2) return 2;
    return (n > 0) ? 1 : 0;
}

void SegInsertAfter(SegNode* at, SegNode* n) {
    n->prev = at; n->next = at->next;
    if (at->next) at->next->prev = n;
    at->next = n;
}
void SegInsertBefore(SegNode* at, SegNode* n) {
    n->next = at; n->prev = at->prev;
    if (at->prev) at->prev->next = n;
    at->prev = n;
}

// ---- component lookup ---------------------------------------------------------
// ComponentEntity: +0x30 = component IBP array, +0x38 = count. Each component's
// vtable slot +0x10 is GetTypeId.
void* FindComponentOn(void* entityIBPSlot, uint32_t typeId) {
    if (!typeId) return nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBPSlot);
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(entityIBPSlot);
    if (!entity) return nullptr;

    const int count = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr   = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return nullptr;

    for (int i = 0; i < count; i++) {
        void* slot = reinterpret_cast<void*>(arr + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t id = ((GetTypeId_t)(*reinterpret_cast<uintptr_t*>(
                          *reinterpret_cast<uintptr_t*>(comp) + 0x10)))(comp);
        if (id == typeId) {
            if (fn_IsDerivedFrom && !fn_IsDerivedFrom(comp, typeId)) return nullptr;
            return comp;
        }
    }
    return nullptr;
}

uint32_t Tid(const char* name) { return ::Object::ResolveTypeIdByNameLazy(name); }

// Stamp cell rect + rotation onto a Map/Build-style component.
void StampCells(void* comp, int col, int row, int w, int h, int rotation) {
    if (!comp) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(comp);
    *reinterpret_cast<int16_t*>(c + 0x28) = static_cast<int16_t>(col);
    *reinterpret_cast<int16_t*>(c + 0x2a) = static_cast<int16_t>(row);
    *reinterpret_cast<int16_t*>(c + 0x2c) = static_cast<int16_t>(w);
    *reinterpret_cast<int16_t*>(c + 0x2e) = static_cast<int16_t>(h);
    while (rotation < 0) rotation += 0x168;      // retail normalises by +360 until >= 0
    *reinterpret_cast<int32_t*>(c + 0x30) = rotation;
}

// Build the yaw-only transform and apply it to a ModelComponent.
// Transform occupies model+0x148 .. +0x168 (0x20 bytes); +0x168 caches the yaw.
void ApplyYaw(void* model, float yawDeg) {
    if (!model || !fn_Transform_FromYawPitchOffset) return;
    uint8_t xform[96];
    std::memset(xform, 0, sizeof(xform));
    float yaw = yawDeg, pitch = 0.f, offset[3] = {0.f, 0.f, 0.f};
    fn_Transform_FromYawPitchOffset(xform, &yaw, &pitch, offset);

    uintptr_t m = reinterpret_cast<uintptr_t>(model);
    std::memcpy(reinterpret_cast<void*>(m + 0x148), xform, 0x20);
    *reinterpret_cast<float*>(m + 0x168) = yawDeg;
    if (fn_Geometry_SetTransform)
        fn_Geometry_SetTransform(reinterpret_cast<void*>(m + 0x28),
                                 reinterpret_cast<void*>(m + 0x148));
}

// Clone a ComponentEntity (0x48 bytes) from a template/description entity.
void* CloneEntity(void* templateEntity) {
    if (!templateEntity || !fn_MemAlloc) return nullptr;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!mem) return nullptr;
    typedef void* (*CopyCtor_t)(void*, void*);
    static CopyCtor_t s_copy = nullptr;
    if (!s_copy) s_copy = reinterpret_cast<CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    if (!s_copy) return nullptr;
    return s_copy(mem, templateEntity);
}

}  // namespace

// ============================================================
// BridgeTool::CalcPath (vtable +0x68, Slot13) — 0x1400f9cd0
// ============================================================
extern "C" uint64_t BridgeTool_CalcPath_Impl(void* selfPtr, int startCell, int endCell) {
    BridgeTool* self = reinterpret_cast<BridgeTool*>(selfPtr);
    if (!self || startCell < 0 || endCell < 0) return 0;

    const int snapped = BridgeTool_SnapEndCell_Impl(self, endCell);
    int bodyLen = 0;
    if (!BridgeTool_ValidatePath_Impl(self, startCell, snapped, &bodyLen)) return 0;
    if (bodyLen <= 0) return 0;

    const uintptr_t desc = reinterpret_cast<uintptr_t>(self->bridgeDesc);
    if (!desc) return 0;
    const uint32_t maskBody = *reinterpret_cast<uint32_t*>(desc + 0x20);
    const int      radius   = *reinterpret_cast<int*>(desc + 0x34);

    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return 0;
    uint32_t* mapGrid = *reinterpret_cast<uint32_t**>(
                            reinterpret_cast<uintptr_t>(gameWorld) + 0xf8);
    if (!mapGrid) return 0;
    const int width = static_cast<int>(mapGrid[0]);
    if (width <= 0) return 0;
    const int64_t terrainBase = *reinterpret_cast<int64_t*>(
                                    reinterpret_cast<uintptr_t>(mapGrid) + 0x80);
    if (!terrainBase) return 0;

    // ---- 1. Build the segment list ------------------------------------------------
    // Two size-0 end nodes, then the middle (bodyLen - 2 cells) filled centre-outward.
    SegNode* head = SegAlloc(0);
    SegNode* tail = SegAlloc(0);
    if (!head || !tail) return 0;
    head->next = tail; tail->prev = head;

    {
        int remaining = bodyLen - 2;
        if (remaining > 0) {
            const int first = LargestFit(remaining);
            SegNode* mid = SegAlloc(first);
            if (mid) SegInsertAfter(head, mid);

            // grow left from the centre
            int left = (remaining / 2) - (first / 2);
            SegNode* cursor = mid;
            while (left > 0 && cursor) {
                const int sz = LargestFit(left);
                if (sz <= 0) break;
                SegNode* n = SegAlloc(sz);
                if (!n) break;
                SegInsertBefore(cursor, n);
                cursor = n;
                left -= sz;
            }

            // grow right from the centre
            cursor = mid;
            for (int filled = (remaining / 2) + (first / 2); filled < remaining && cursor; ) {
                const int sz = LargestFit(remaining - filled);
                if (sz <= 0) break;
                SegNode* n = SegAlloc(sz);
                if (!n) break;
                SegInsertAfter(cursor, n);
                cursor = n;
                filled += sz;
            }
        }
    }

    // ---- 2. Span geometry + axis step ---------------------------------------------
    int startRow = startCell / width, startCol = startCell % width;
    int endRow   = snapped   / width, endCol   = snapped   % width;
    int minCol = startCol, maxCol = endCol;
    if (endCol < startCol) { minCol = endCol; maxCol = startCol; }
    int minRow = startRow, maxRow = endRow;
    if (endRow < startRow) { minRow = endRow; maxRow = startRow; }

    // First body cell in the span — where the deck actually starts.
    int bodyCol = minCol, bodyRow = minRow;
    {
        bool found = false;
        for (int row = minRow; row <= maxRow && !found; row++) {
            for (int col = minCol; col <= maxCol; col++) {
                const uintptr_t cell = static_cast<uintptr_t>(terrainBase)
                                     + static_cast<int64_t>(width * row + col) * 0x30;
                const uint32_t bit = 1u << (*reinterpret_cast<uint8_t*>(cell + 0x29) & 0x1f);
                if (maskBody & bit) { bodyCol = col; bodyRow = row; found = true; break; }
            }
        }
    }
    const int stepCol = (bodyCol != maxCol) ? 1 : 0;   // horizontal run
    const int stepRow = (bodyRow != maxRow) ? 1 : 0;   // vertical run

    // ---- 3. Master entity ----------------------------------------------------------
    void* master = CloneEntity(self->descriptionPtr);
    void* masterIBP = nullptr;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&masterIBP, master);
    if (!master) return 0;

    const uint32_t tidMap    = Tid("MapComponent");
    const uint32_t tidModel  = Tid("ModelComponent");
    const uint32_t tidMulti  = Tid("MultiObjectComponent");

    {
        int mw = stepCol * bodyLen, mh = stepRow * bodyLen;
        const int crossW = radius * stepRow * 2 + 1;
        const int crossH = radius * stepCol * 2 + 1;
        if (mw < crossW) mw = crossW;
        if (mh < crossH) mh = crossH;
        StampCells(FindComponentOn(&masterIBP, tidMap),
                   bodyCol - radius * stepRow, bodyRow - radius * stepCol,
                   mw, mh, stepRow * 0x5a);
        ApplyYaw(FindComponentOn(&masterIBP, tidModel), static_cast<float>(stepRow) * 90.0f);
    }
    void* multi = FindComponentOn(&masterIBP, tidMulti);

    // ---- 4. Per-segment entities ---------------------------------------------------
    void* entityList = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0xe8);
    int   cursorCol = bodyCol, cursorRow = bodyRow;
    int   placedCells = 0, placedSegs = 0;

    for (SegNode* n = head; n != nullptr; n = n->next) {
        const int rawSize = n->size;
        const int span    = (rawSize == 0) ? 1 : rawSize;   // end pieces occupy 1 cell

        int segW = span, segH = 1, rot = 0;
        if (stepCol == 0) { rot = 0x5a; segW = 1; segH = span; }   // vertical run
        // End pieces face outward.
        if (n == tail && stepCol == 0)      rot = 0x5a;
        else if (n == head && stepCol == 0) rot = -0x5a;
        else if (n == tail && stepRow == 0) rot = 0;
        else if (n == head && stepRow == 0) rot = 0xb4;

        // segmentTemplates[0..4] at desc+0x38: 0 = end piece, then 1/2/4/8 cells.
        int tplIdx = 0;
        if      (rawSize == 1) tplIdx = 1;
        else if (rawSize == 2) tplIdx = 2;
        else if (rawSize == 4) tplIdx = 3;
        else if (rawSize == 8) tplIdx = 4;

        void* tpl = *reinterpret_cast<void**>(desc + 0x38 + static_cast<int64_t>(tplIdx) * 8);
        void* seg = CloneEntity(tpl);
        if (seg) {
            void* segIBP = nullptr;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&segIBP, seg);

            StampCells(FindComponentOn(&segIBP, tidMap), cursorCol, cursorRow, segW, segH, rot);
            ApplyYaw(FindComponentOn(&segIBP, tidModel), static_cast<float>(rot));

            // Append to the master's MultiObjectComponent (array +0x48, count +0x50).
            if (multi && fn_ExeArrayResize) {
                uintptr_t m = reinterpret_cast<uintptr_t>(multi);
                const int newCount = *reinterpret_cast<int*>(m + 0x50) + 1;
                fn_ExeArrayResize(reinterpret_cast<void*>(m + 0x48), newCount);
                uintptr_t arr = *reinterpret_cast<uintptr_t*>(m + 0x48);
                if (arr) {
                    *reinterpret_cast<uint64_t*>(arr + static_cast<int64_t>(newCount - 1) * 8) =
                        reinterpret_cast<uint64_t>(segIBP);
                    *reinterpret_cast<int*>(m + 0x50) = newCount;
                }
            }

            if (fn_EntityList_RegisterEntity && entityList && tpl) {
                const int categoryId = *reinterpret_cast<int*>(
                                           reinterpret_cast<uintptr_t>(tpl) + 0x20);
                fn_EntityList_RegisterEntity(entityList, &segIBP, categoryId);
            }
            placedSegs++;
        }

        placedCells += span;
        cursorCol   += segW * stepCol;
        cursorRow   += segH * stepRow;
    }

    // ---- 5. Finish: stamp the cell total, register the master, free the list --------
    if (multi)
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(multi) + 0x58) =
            static_cast<uint32_t>(placedCells);

    if (fn_EntityList_RegisterEntity && entityList) {
        const int categoryId = *reinterpret_cast<int*>(
                                   reinterpret_cast<uintptr_t>(self->descriptionPtr) + 0x20);
        fn_EntityList_RegisterEntity(entityList, &masterIBP, categoryId);
    }

    for (SegNode* n = head; n != nullptr; ) {
        SegNode* next = n->next;
        if (fn_MemFree) fn_MemFree(g_pMemAllocSingleton, n);
        n = next;
    }

    fprintf(stderr, "[Bridge] CalcPath: start=%d end=%d snapped=%d len=%d -> %d segments, "
            "%d cells, master=%p multi=%p (stepCol=%d stepRow=%d radius=%d)\n",
            startCell, endCell, snapped, bodyLen, placedSegs, placedCells,
            master, multi, stepCol, stepRow, radius);
    fflush(stderr);

    return 1;
}
