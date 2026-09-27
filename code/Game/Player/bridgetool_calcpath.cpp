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
// The f3e0 terrain-height/world-position math IS ported (CellRectCentre below). It was
// once dismissed as dead — "retail computes it and then passes a ZERO offset" — which was
// a misread of FromYawPitchOffset's argument order: the zero is the PITCH, the position is
// the offset. Dropping it parked every bridge entity at the world origin and silently made
// the bridge unbuildable; see the note on ApplyYaw.
// (Picking used to be listed here as an omission. It is DONE now: the per-segment
//  PickingComponent is re-owned to the master entity and its mesh is registered, so a
//  click anywhere on the span selects the bridge — see SeatSegmentPickOwner below.)

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Geometry.h"
#include "BridgeTool.h"
#include "Component/coll_bridge.h"
#include "Component/MapComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MultiObjectComponent.h"
#include "Component/PickingComponent.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "address_registry.h"
#include "Object/factory_registration.h"
#include "Component/pickingcomponent_funcs.h"   // PickingComponent_RegisterMeshForEntity
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)

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
// virtual slot 2 (+0x10) is Type().
void* FindComponentOn(void* entityIBPSlot, uint32_t typeId) {
    if (!typeId) return nullptr;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(entityIBPSlot);
    if (!entity) return nullptr;

    const int count = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr   = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return nullptr;

    for (int i = 0; i < count; i++) {
        void* slot = reinterpret_cast<void*>(arr + static_cast<int64_t>(i) * 8);
        void* comp = *reinterpret_cast<void**>(slot);
        if (!comp) continue;
        uint32_t id = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (id == typeId) {
            if (fn_IsDerivedFrom && !fn_IsDerivedFrom(comp, typeId)) return nullptr;
            return comp;
        }
    }
    return nullptr;
}

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

// World-space centre of a cell rect: sample the terrain position of the two opposite
// corner cells and average them. Retail does exactly this twice — once for the master
// (0x1400fa218..0x1400fa363) and once per segment (0x1400fa6f1..0x1400fa7e4) — via
// FUN_14009f3e0 (fn_MapGetTerrainPos). Cell -> map-space is (col - c, row - c) with
// c = (gridSize >> 1) - 0.5, and the final z is forced to 0 (0x1400fa35d).
bool CellRectCentre(uint32_t* mapGrid, int col0, int row0, int col1, int row1, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!mapGrid || !fn_MapGetTerrainPos) return false;
    const int   W = static_cast<int>(mapGrid[0]);
    const float c = static_cast<float>(W >> 1) - 0.5f;
    float inA[3] = { static_cast<float>(col0) - c, static_cast<float>(row0) - c, 0.0f };
    float inB[3] = { static_cast<float>(col1) - c, static_cast<float>(row1) - c, 0.0f };
    float a[3] = {0.f, 0.f, 0.f}, b[3] = {0.f, 0.f, 0.f};
    fn_MapGetTerrainPos(mapGrid, a, inA, '\0', nullptr);
    fn_MapGetTerrainPos(mapGrid, b, inB, '\0', nullptr);
    out[0] = (a[0] + b[0]) * 0.5f;
    out[1] = (a[1] + b[1]) * 0.5f;
    out[2] = 0.0f;
    return true;
}

// Build the yaw+offset transform and apply it to a ModelComponent.
// Transform occupies model+0x148 .. +0x168 (0x20 bytes); +0x168 caches the yaw.
//
// ⚠ The offset is NOT decorative and was the reason a placed bridge was never built.
// Retail passes the cell-rect centre computed above as FromYawPitchOffset's 4th
// argument (0x1400fa57c `LEA R9,[RBP-0x38]` for the master, 0x1400faa5d
// `LEA R9,[RSP+0x50]` per segment); the ZERO argument one slot over is the PITCH
// (`LEA R8,[RSP+0x30]` / `[RBP-0x70]`, both explicitly stored 0). An earlier reading
// of this function had the two swapped and dropped the position, which left every
// bridge entity's transform at the world origin. The transform is what
// FUN_140122be0 (GatherRawMaterials' CalcBestTargetPos) walks to find a tile a
// builder can stand on, so with it at the origin the gather bailed
// "no target pos (found==0)" forever and no material was ever hauled.
void ApplyYaw(void* model, float yawDeg, const float offset[3]) {
    if (!model) return;
    Math::Vector3 off(offset ? offset[0] : 0.f,
                      offset ? offset[1] : 0.f,
                      offset ? offset[2] : 0.f);
    Math::Transform xform = Math::Transform::FromYawPitchOffset(yawDeg, 0.f, off);

    uintptr_t m = reinterpret_cast<uintptr_t>(model);
    std::memcpy(reinterpret_cast<void*>(m + 0x148), &xform, 0x1c);
    *reinterpret_cast<float*>(m + 0x168) = yawDeg;
    reinterpret_cast<Graphics::Geometry*>(m + 0x28)
        ->SetTransform(*reinterpret_cast<const Math::Transform*>(m + 0x148));   // B.3.5 bridge
}

// ---- picking ------------------------------------------------------------------
// Retail tail of the per-segment block (0x1400faab3 .. 0x1400fab7b): scan the SEGMENT
// entity's components for the PickingComponent (type id DAT_1401c7c10), then
//
//     *(pickComp + 0x30) = masterEntity;                       // always
//     if (*(int*)(pickComp + 0x28) >= 0)                       // already has a mesh
//         Picking::Scene::SetMeshUserData(gw->pickScene, meshId,
//                                         *(pickComp + 0x30), 0, 0, nullptr);
//
// i.e. every deck piece is re-OWNED by the master bridge entity, so a click anywhere
// on the span selects the bridge as one object. +0x30 is exactly what
// PickingComponent::OnAdded later hands to Picking::Scene::AddMesh as the pick
// userData, so seating it here is the load-bearing half; the SetMeshUserData call only
// covers the case where the clone arrived with a mesh already registered.
//
// This is the fix behind the edittool.cpp:833 guard: a segment whose +0x30 was never
// seated registers (or leaves) garbage user data, and EditTool's pick deref faults on
// it. Slot 0x1401529a0 is registered in standalone/address_registry.cpp.
void SeatSegmentPickOwner(void* segIBPSlot, void* masterEntity, uint32_t tidPicking,
                          void* gameWorld) {
    void* pc = FindComponentOn(segIBPSlot, tidPicking);
    if (!pc) return;
    uintptr_t p = reinterpret_cast<uintptr_t>(pc);
    *reinterpret_cast<void**>(p + 0x30) = masterEntity;

    const int meshId = *reinterpret_cast<int32_t*>(p + 0x28);
    if (meshId < 0) return;              // no mesh yet — OnAdded/RegisterMesh reads +0x30

    void* scene = gameWorld ? *reinterpret_cast<void**>(
                                  reinterpret_cast<uintptr_t>(gameWorld) + 0x70) : nullptr;
    if (scene)
        reinterpret_cast<Picking::Scene*>(scene)->SetMeshUserData(static_cast<uint32_t>(meshId),   // B.3.7 bridge
                      *reinterpret_cast<void**>(p + 0x30), 0, 0, nullptr);
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

// Last-commit record, for the standalone bridge harness (BANISHED_TOOL_HARNESS=bridge).
// Same role as g_pConstructionDemoEntity: gives a headless test something to poll
// without having to re-derive the entities from the map grid. Diagnostic only —
// nothing in the game path reads these.
extern "C" {
void* g_BridgeLastMaster = nullptr;
void* g_BridgeLastSegments[32] = {};
int   g_BridgeLastSegmentCount = 0;
}

// ============================================================
// BridgeTool::CalcPath (vtable +0x68, Slot13) — 0x1400f9cd0
// ============================================================
bool BridgeTool::CalcPath(int startCell, int endCell) {
    if (startCell < 0 || endCell < 0) return 0;

    const int snapped = BridgeTool_SnapEndCell_Impl(this, endCell);
    int bodyLen = 0;
    if (!BridgeTool_ValidatePath_Impl(this, startCell, snapped, &bodyLen)) return 0;
    if (bodyLen <= 0) return 0;

    const uintptr_t desc = reinterpret_cast<uintptr_t>(bridgeDesc);
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
    void* master = CloneEntity(descriptionPtr);
    void* masterIBP = nullptr;
    if (fn_InternalBasePtr_Ctor) new (&masterIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(master));   // B.3.3 bridge
    if (!master) return 0;

    const uint32_t tidMap     = MapComponent::TypeId();
    const uint32_t tidModel   = ModelComponent::TypeId();
    const uint32_t tidMulti   = MultiObjectComponent::TypeId();
    const uint32_t tidPicking = PickingComponent::TypeId();

    {
        int mw = stepCol * bodyLen, mh = stepRow * bodyLen;
        const int crossW = radius * stepRow * 2 + 1;
        const int crossH = radius * stepCol * 2 + 1;
        if (mw < crossW) mw = crossW;
        if (mh < crossH) mh = crossH;
        const int mCol = bodyCol - radius * stepRow;
        const int mRow = bodyRow - radius * stepCol;
        StampCells(FindComponentOn(&masterIBP, tidMap), mCol, mRow, mw, mh, stepRow * 0x5a);
        float centre[3];
        CellRectCentre(mapGrid, mCol, mRow, mCol + mw - 1, mRow + mh - 1, centre);
        ApplyYaw(FindComponentOn(&masterIBP, tidModel),
                 static_cast<float>(stepRow) * 90.0f, centre);
    }
    void* multi = FindComponentOn(&masterIBP, tidMulti);

    // ---- 4. Per-segment entities ---------------------------------------------------
    void* entityList = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0xe8);
    int   cursorCol = bodyCol, cursorRow = bodyRow;
    int   placedCells = 0, placedSegs = 0;
    g_BridgeLastMaster = master;
    g_BridgeLastSegmentCount = 0;

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
            if (fn_InternalBasePtr_Ctor) new (&segIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(seg));   // B.3.3 bridge


            StampCells(FindComponentOn(&segIBP, tidMap), cursorCol, cursorRow, segW, segH, rot);
            float segCentre[3];
            CellRectCentre(mapGrid, cursorCol, cursorRow,
                           cursorCol + segW - 1, cursorRow + segH - 1, segCentre);
            ApplyYaw(FindComponentOn(&segIBP, tidModel), static_cast<float>(rot), segCentre);

            // Append to the master's MultiObjectComponent (array +0x48, count +0x50).
            if (multi) {
                uintptr_t m = reinterpret_cast<uintptr_t>(multi);
                const int newCount = *reinterpret_cast<int*>(m + 0x50) + 1;
                // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
                // align 8, new tail zeroed. Element is the 8-byte segment
                // InternalBasePtr written into the new slot just below.
                GameColl::ArrayAt<GameColl::Ptr8>(m, 0x48).SetSize(newCount);
                uintptr_t arr = *reinterpret_cast<uintptr_t*>(m + 0x48);
                if (arr) {
                    *reinterpret_cast<uint64_t*>(arr + static_cast<int64_t>(newCount - 1) * 8) =
                        reinterpret_cast<uint64_t>(segIBP);
                    *reinterpret_cast<int*>(m + 0x50) = newCount;
                }
            }

            // Re-own the segment's pick mesh to the master (retail 0x1400faab3..fab7b),
            // BEFORE the register — that is the window PickingComponent::OnAdded reads.
            SeatSegmentPickOwner(&segIBP, master, tidPicking, gameWorld);

            if (fn_EntityList_RegisterEntity && entityList && tpl) {
                const int categoryId = *reinterpret_cast<int*>(
                                           reinterpret_cast<uintptr_t>(tpl) + 0x20);
                fn_EntityList_RegisterEntity(entityList, &segIBP, categoryId);
            }

            // Standalone only: PickingComponent is an inert factory here, so the OnAdded
            // that RegisterEntity would have broadcast never runs and the segment never
            // enters Picking::Scene (bridges were unclickable). Drive the ported
            // registration explicitly, exactly where retail's OnAdded sits. It honours
            // the +0x30 we just seated, so the mesh's user data is the master entity.
            PickingComponent_RegisterMeshForEntity(seg);
            if (g_BridgeLastSegmentCount < 32)
                g_BridgeLastSegments[g_BridgeLastSegmentCount++] = seg;
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
                                   reinterpret_cast<uintptr_t>(descriptionPtr) + 0x20);
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
