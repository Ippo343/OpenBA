// code/Game/Player/fencedtool_calcpath.cpp
// FencedTool::CalcPath (vtable +0x68, Slot13) — the drag-commit that creates a
// fenced area (pasture / crop field): the master entity, the animal-shelter
// sub-entity, and the perimeter fence pieces (corners, straights, gate),
// all linked through the master's MultiObjectComponent.
//
// Ports (all verified against disassembly, not just decompile):
//   0x1400dae00  FencedTool_Slot13 (CalcPath)          — this file
//   0x1400dbc80  FencedTool_Slot13_sub_bc80            — spawn ONE fence piece
//   0x1400db8c0  FencedTool_Slot13_sub_b8c0            — walk one edge, spawn run
//   0x1401290d0  FencedTool_helper_90d0                — seat pick owner on shelter
//
// Section template array (CreateFencedDescription +0x28, ExternalPtr[6]), order
// fixed by Pasture.rsc `_sections`:
//   [0] AnimalShelter  [1] FenceEnd  [2] FenceCorner  [3] FenceStraight
//   [4] FenceStraightLong (2 cells)  [5] FenceGate (2 cells)
//
// Flow (retail 0x1400dae00):
//   1. vtable[14] (CalcCost) validates the drag rect; non-zero => abort (ret 0).
//   2. Edit-mode guard: if the placement dialog obj (tool+0x10) has +0x80 set,
//      the rect must equal the stored +0x98/+0x9c cells (re-fence of an
//      existing area) or abort.
//   3. 0x1400dbf90 places the shelter rect on the rotation-selected edge and
//      returns its world-centre pos + the yaw (0/-90/180/90 as float degrees).
//   4. Master entity: CtorFromDescription(tool+0x20), MapComponent gets the
//      full rect + yaw; existing entities of template desc+0x70 (Clear.rsc
//      grass) inside the rect are removed via 0x1401018d0.
//   5. Shelter entity: CtorFromDescription(sections[0]), MapComponent gets the
//      shelter rect, ModelComponent transform = FromYawPitchOffset(yaw, 0, pos)
//      (the pos is LOAD-BEARING — see the bridge-at-origin lesson), picking
//      user-data re-owned to the MASTER entity (helper_90d0), appended to the
//      master's MultiObject list, registered.
//   6. MultiObject+0x58 = perimeter cell count = (w+h)*2-4.
//   7. 4 corners (sub_bc80 idx 2, yaw 90/0/-90/180), skipped where the shelter
//      covers the corner; 4 edge runs (sub_b8c0).
//   8. Register the master entity. Return 1.

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Geometry.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "Player/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Entity/componententity_funcs.h"
#include "system/MemoryAllocator.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "FencedTool.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)

// ---- exe deps, called by name ------------------------------------------------
//   0x1400c3720 ComponentEntity_CtorFromDescription, 0x1400c48a0
//   EntityList_RegisterEntity_Impl (Entity/componententity_funcs.h)
//   0x140003110 ActHunt_DynArraySetCount_3110 (Action/exe_bodies.h) -- IBP append
//   0x1401018d0 DragPlacementTool_ClearPathEntities_1018d0, 0x1400dbf90
//   PlayerEdit_FencedAnchorCalc_dbf90 (Player/exe_bodies.h)
//   the finder leaves (FindOn below): 0x140004680 FloorComponent_FindResData (map),
//   0x140003f60 ActCNR_FindComponent_3f60 (model; "ClearPathQueue" misnomer),
//   0x140026a30 BuildComponent_FindStorage26a30 (picking -- the body's name says
//   storage; the address and this use say PickingComponent), 0x140004740
//   InteractComponent_FindWorkplaceComponent (MultiObject)
namespace {

typedef void (*FinderFn)(int64_t entity, void* outIBP);

void FindMap(int64_t e, void* out)     { FloorComponent_FindResData(reinterpret_cast<void*>(e), out); }   // 0x140004680
void FindModel(int64_t e, void* out)   { ActCNR_FindComponent_3f60(static_cast<uintptr_t>(e), out); }     // 0x140003f60
void FindPicking(int64_t e, void* out) { BuildComponent_FindStorage26a30(e, out); }                       // 0x140026a30

// Find a component on an entity through one of the exe finder leaves; returns
// the raw component pointer (finders write an IBP through the out param).
void* FindOn(FinderFn finder, void* entity, void* entityIBPSlot) {
    (void)entity;
    int64_t out = 0;
    finder(*reinterpret_cast<int64_t*>(entityIBPSlot), &out);
    return reinterpret_cast<void*>(out);
}

// Stamp cell rect + rotation onto a MapComponent (+0x28 shorts, +0x30 degrees
// normalised into [0,360) by repeated +360 exactly like retail).
void StampMapRect(void* mapComp, int x, int y, int w, int h, int rotDeg) {
    if (!mapComp) return;
    uintptr_t c = reinterpret_cast<uintptr_t>(mapComp);
    *reinterpret_cast<int16_t*>(c + 0x28) = static_cast<int16_t>(x);
    *reinterpret_cast<int16_t*>(c + 0x2a) = static_cast<int16_t>(y);
    *reinterpret_cast<int16_t*>(c + 0x2c) = static_cast<int16_t>(w);
    *reinterpret_cast<int16_t*>(c + 0x2e) = static_cast<int16_t>(h);
    while (rotDeg < 0) rotDeg += 0x168;
    *reinterpret_cast<int32_t*>(c + 0x30) = rotDeg;
}

// Model transform = FromYawPitchOffset(yawDeg, 0, offset) into model+0x148,
// yaw cached at +0x168 (UN-normalised float, matching retail), then
// Geometry::SetTransform. Same helper family as bridgetool_calcpath's ApplyYaw.
void ApplyModelYaw(void* model, float yawDeg, const float offset[3]) {
    if (!model) return;
    Math::Vector3 off(offset[0], offset[1], offset[2]);
    Math::Transform xform = Math::Transform::FromYawPitchOffset(yawDeg, 0.f, off);
    uintptr_t m = reinterpret_cast<uintptr_t>(model);
    std::memcpy(reinterpret_cast<void*>(m + 0x148), &xform, 0x1c);
    *reinterpret_cast<float*>(m + 0x168) = yawDeg;
    reinterpret_cast<Graphics::Geometry*>(m + 0x28)
        ->SetTransform(*reinterpret_cast<const Math::Transform*>(m + 0x148));   // B.3.5 bridge
}

// 0x1401290d0 — seat the shelter's pick owner: pickComp+0x30 = master entity;
// if the pick mesh is already registered (+0x28 >= 0), re-point its user data.
void SeatPickOwner_1290d0(void* pickComp, void** masterIBPSlot) {
    if (!pickComp) return;
    uintptr_t p = reinterpret_cast<uintptr_t>(pickComp);
    *reinterpret_cast<void**>(p + 0x30) = *masterIBPSlot;
    if (*reinterpret_cast<int32_t*>(p + 0x28) < 0) return;
    void* gw = GameUpdateLoop::_singleton;
    void* scene = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x70) : nullptr;
    if (scene)
        reinterpret_cast<Picking::Scene*>(scene)->SetMeshUserData(*reinterpret_cast<uint32_t*>(p + 0x28),   // B.3.7 bridge
                      *reinterpret_cast<void**>(p + 0x30), 0, 0, nullptr);
}

// Grid cell -> world pos via the 0x14009f3e0 terrain query (bridge CellRectCentre
// idiom): map-space = (col - c, row - c), c = (gridDim >> 1) - 0.5.
void CellWorldPos(int* mapGrid, int col, int row, float out[3]) {
    out[0] = out[1] = out[2] = 0.f;
    if (!mapGrid) return;
    const float c = static_cast<float>(*mapGrid >> 1) - 0.5f;
    float in[3] = { static_cast<float>(col) - c, static_cast<float>(row) - c, 0.f };
    Reimpl_MapGetTerrainPos(reinterpret_cast<uint32_t*>(mapGrid), out, in, '\0', nullptr);
}

// ============================================================
// 0x1400dbc80 — spawn ONE fence piece.
//   sectionIdx indexes CreateFencedDescription+0x28 (see table in header).
//   (x,y,w,h) is the piece's cell rect; angleDeg the piece rotation (may be
//   negative — MapComponent stores it normalised, the model yaw keeps the raw
//   value). World pos = midpoint of the rect's two corner cell centres, z=0.
//   The new entity is appended to the master's MultiObject list + registered.
// ============================================================
void SpawnFencePiece_dbc80(FencedTool* self, int sectionIdx, int x, int y,
                           int w, int h, int angleDeg, void** multiObjIBPSlot) {
    void* gw = GameUpdateLoop::_singleton;
    if (!gw) return;
    int* mapGrid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gw) + 0xf8);

    float a[3], b[3], centre[3];
    CellWorldPos(mapGrid, x, y, a);
    CellWorldPos(mapGrid, x + w - 1, y + h - 1, b);
    centre[0] = (a[0] + b[0]) * 0.5f;
    centre[1] = (a[1] + b[1]) * 0.5f;
    centre[2] = 0.0f;                       // retail zeroes z (0x1400dbdfa)

    uintptr_t desc = reinterpret_cast<uintptr_t>(self->fencedDesc);
    void* sectionTmpl = *reinterpret_cast<void**>(desc + 0x28 + static_cast<int64_t>(sectionIdx) * 8);
    if (!sectionTmpl) return;

    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* ent = mem ? ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(mem), sectionTmpl) : nullptr;
    int64_t entIBP = 0;
    new (&entIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(ent));   // B.3.3 bridge
    if (!entIBP) return;

    void* mapComp = FindOn(FindMap, ent, &entIBP);
    StampMapRect(mapComp, x, y, w, h, angleDeg);

    void* model = FindOn(FindModel, ent, &entIBP);
    if (model) ApplyModelYaw(model, static_cast<float>(angleDeg), centre);

    // Report WHERE every fence piece is put. A "gate model ended up in a lake" report is
    // unactionable without this: it cannot distinguish a bad cell rect (the caller walked
    // the wrong edge) from a bad world position (CellWorldPos / the terrain lookup) from a
    // piece that is simply drawn at z=0, which is lake level. Section 5 is the Gate.
    {
        static int n = 0;
        if (n < 24) { ++n;
            fprintf(stderr, "[FencePiece] idx=%d%s cell=(%d,%d) %dx%d angle=%d "
                            "world=(%.2f, %.2f, %.2f) mapWidth=%d\n",
                    sectionIdx, (sectionIdx == 5) ? " GATE" : "", x, y, w, h, angleDeg,
                    centre[0], centre[1], centre[2], mapGrid ? *mapGrid : -1);
            fflush(stderr);
        }
    }

    // Append to the master's MultiObject list (arr at +0x48, count at +0x50).
    uintptr_t multi = *reinterpret_cast<uintptr_t*>(multiObjIBPSlot);
    if (multi) {
        int64_t val = entIBP;
        ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(multi + 0x48),
                                      *reinterpret_cast<int32_t*>(multi + 0x50) + 1, &val);
    }

    {
        int64_t val = entIBP;
        int prio = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(sectionTmpl) + 0x20);
        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xe8), &val, prio);
    }
}

// ============================================================
// 0x1400db8c0 — walk one edge of the rect from (x0,y0) to (x1,y1), spawning
// the fence run. Literal transcription of the retail control flow:
//   - cells covered by the shelter rect are a GAP; the first fence cell after
//     a gap gets an End piece at the base angle, the last fence cell before
//     the gap gets an End rotated to face it (180 for horizontal, -90 for
//     vertical edges);
//   - a 1-cell Straight (idx 3) is used when fewer than two cells remain or
//     the 2-cell piece would collide with the shelter;
//   - otherwise 2-cell pieces: StraightLong (idx 4), with exactly ONE Gate
//     (idx 5) after the first long piece on edges the shelter does not touch
//     (the shelter itself is the entrance on its own edge).
// ============================================================
void EdgeRun_db8c0(FencedTool* self, int x0, int y0, int x1, int y1,
                   int shMinX, int shMinY, int shMaxX, int shMaxY,
                   int angleDeg, void** multiObjIBPSlot) {
    bool prevFence  = false;   // bVar2: previous cell was a fence cell
    bool afterGap   = false;   // bVar5: a shelter gap has been seen on this run
    bool prevLong   = false;   // bVar3: previous spawn was a long piece
    bool gateLatch  = false;   // bVar6
    bool gateLatch2 = false;   // bVar7
    const bool stepXOn = (x1 - x0) > 0;
    const int  spanY   = y1 - y0;
    const int  stepY   = (spanY > 0) ? 1 : 0;

    int wLong, hLong;
    if (angleDeg == 0) { wLong = 2; hLong = 1; }
    else               { wLong = 1; hLong = 2; }

    // bVar4: TRUE when the shelter touches none of this run's cells.
    bool noShelterOnRun = true;
    uintptr_t desc = reinterpret_cast<uintptr_t>(self->fencedDesc);
    if (*reinterpret_cast<uint8_t*>(desc + 0x68) != 0 && y0 <= y1) {
        for (int yy = y0; yy <= y1; ++yy)
            for (int xx = x0; xx <= x1; ++xx)
                if (shMinX <= xx && xx <= shMaxX && shMinY <= yy && yy <= shMaxY)
                    noShelterOnRun = false;
    }

    int y = y0;
    for (;;) {
        if (y1 < y) return;
        const int stepX = stepXOn ? 1 : 0;
        if (x0 <= x1) {
            int x     = x0;
            int lookX = x0 + stepX;
            do {
                const int lookY = y + stepY;
                const bool inShelter = (shMinX <= x && x <= shMaxX &&
                                        shMinY <= y && y <= shMaxY);
                if (!inShelter) {
                    int idx = -1, ang = angleDeg;
                    bool spawned1Cell = false;
                    if (afterGap && !prevFence) {
                        idx = 1; ang = angleDeg; spawned1Cell = true;      // End after the gap
                    } else if (prevFence &&
                               shMinX <= lookX && lookX <= shMaxX &&
                               shMinY <= lookY && lookY <= shMaxY) {
                        idx = 1;                                           // End before the gap
                        ang = (angleDeg == 0) ? 0xb4 : -0x5a;
                        spawned1Cell = true;
                    } else if ((x1 < x + 2 * stepX) || (y1 < y + 2 * stepY) ||
                               (shMinX <= lookX && lookX <= shMaxX &&
                                shMinY <= lookY && lookY <= shMaxY)) {
                        idx = 3; ang = angleDeg; spawned1Cell = true;      // 1-cell Straight
                    }
                    if (spawned1Cell) {
                        void* val = *multiObjIBPSlot;
                        SpawnFencePiece_dbc80(self, idx, x, y, 1, 1, ang, &val);
                        gateLatch = gateLatch2;
                    } else if (gateLatch || !prevLong || !noShelterOnRun) {
                        void* val = *multiObjIBPSlot;
                        SpawnFencePiece_dbc80(self, 4, x, y, wLong, hLong, angleDeg, &val); // StraightLong
                        x += stepX; lookX += stepX; y += stepY;
                        prevLong = true;
                        gateLatch = gateLatch2;
                    } else {
                        void* val = *multiObjIBPSlot;
                        SpawnFencePiece_dbc80(self, 5, x, y, wLong, hLong, angleDeg, &val); // Gate
                        x += stepX; lookX += stepX; y += stepY;
                        gateLatch = true;
                    }
                    prevFence = true;
                    gateLatch2 = gateLatch;
                } else {
                    prevFence = false;
                    afterGap = true;
                    prevLong = false;
                }
                x += 1;
                lookX += 1;
            } while (x <= x1);
        }
        y += 1;
    }
}

} // namespace

// ============================================================
// 0x1400dae00 — FencedTool::CalcPath (Slot13). Returns 1 on commit, 0 on abort.
// ============================================================
bool FencedTool::CalcPath(int startCell, int endCell) {
    void* gw = GameUpdateLoop::_singleton;
    if (!gw || !fencedDesc) {
        return 0;
    }

    // 1. Validate through the virtual CalcCost (slot 14).
    uint32_t dummyW = 0;
    int rectW = 0, rectH = 0, segCount = 0;
    int rc = CalcCost(static_cast<int>(startCell), static_cast<int>(endCell),
                      &dummyW, &rectW, &rectH, &segCount);
    if (rc != 0) {
        return 0;
    }

    int* mapGrid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gw) + 0xf8);
    const int W = *mapGrid;
    int minCol = static_cast<int>(startCell) % W, minRow = static_cast<int>(startCell) / W;
    int maxCol = static_cast<int>(endCell) % W,   maxRow = static_cast<int>(endCell) / W;
    if (maxCol < minCol) { int t = minCol; minCol = maxCol; maxCol = t; }
    if (maxRow < minRow) { int t = minRow; minRow = maxRow; maxRow = t; }

    // 2. Edit-mode guard (re-fence must match the stored rect). Retail reads
    //    tool+0x10 unconditionally; null-guarded here (SA: the placement dialog
    //    object may be absent in harness flows — treated as "not editing").
    uint8_t* editObj = reinterpret_cast<uint8_t*>(dialogPtr);
    if (editObj && *(editObj + 0x80) != 0) {
        int sc = *reinterpret_cast<int32_t*>(editObj + 0x98);
        int ec = *reinterpret_cast<int32_t*>(editObj + 0x9c);
        if (minCol != sc % W || minRow != sc / W || maxCol != ec % W || maxRow != ec / W) {
            return 0;
        }
    }

    // 3. Shelter placement + yaw.
    uint32_t shMinX = 0, shMinY = 0, shMaxX = 0, shMaxY = 0;
    float shelterPos[3] = { 0.f, 0.f, 0.f };
    float yawF = 0.f;
    // The door typed the last argument float*; the body takes uint32_t* (same pointer).
    PlayerEdit_FencedAnchorCalc_dbf90(reinterpret_cast<uintptr_t>(this), &shMinX, &shMinY, &shMaxX,
                                      &shMaxY, shelterPos, reinterpret_cast<uint32_t*>(&yawF));
    const int yawI = static_cast<int>(yawF);

    // 4. Master entity from the tool's description template (tool+0x20).
    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* master = mem ? ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(mem), descriptionPtr) : nullptr;
    int64_t masterIBP = 0;
    new (&masterIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(master));   // B.3.3 bridge
    if (!masterIBP) return 0;

    void* masterMap = FindOn(FindMap, master, &masterIBP);
    StampMapRect(masterMap, minCol, minRow, rectW, rectH, yawI);

    int64_t multiIBP = 0;
    InteractComponent_FindWorkplaceComponent(static_cast<uintptr_t>(masterIBP), &multiIBP);

    {
        int64_t clearTmpl = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uintptr_t>(fencedDesc) + 0x70);
        DragPlacementTool_ClearPathEntities_1018d0(reinterpret_cast<void*>(masterIBP), minCol, minRow,
                                                   maxCol, maxRow, clearTmpl);
    }

    // 5. Shelter entity from sections[0].
    void* shelterTmpl = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(fencedDesc) + 0x28);
    void* smem = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* shelter = (smem && shelterTmpl) ? ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(smem), shelterTmpl) : nullptr;
    int64_t shelterIBP = 0;
    new (&shelterIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(shelter));   // B.3.3 bridge
    if (shelterIBP) {
        void* shMap = FindOn(FindMap, shelter, &shelterIBP);
        StampMapRect(shMap, static_cast<int>(shMinX), static_cast<int>(shMinY),
                     static_cast<int>(shMaxX - shMinX) + 1,
                     static_cast<int>(shMaxY - shMinY) + 1, yawI);

        void* shModel = FindOn(FindModel, shelter, &shelterIBP);
        if (shModel) ApplyModelYaw(shModel, yawF, shelterPos);

        void* shPick = FindOn(FindPicking, shelter, &shelterIBP);
        void* masterVal = reinterpret_cast<void*>(masterIBP);
        SeatPickOwner_1290d0(shPick, &masterVal);

        if (multiIBP) {
            int64_t val = shelterIBP;
            ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(multiIBP + 0x48),
                                          *reinterpret_cast<int32_t*>(multiIBP + 0x50) + 1, &val);
        }
        {
            int64_t val = shelterIBP;
            int prio = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(shelterTmpl) + 0x20);
            EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xe8),
                                           &val, prio);
        }
    }

    // 6. Perimeter cell count on the MultiObjectComponent.
    if (multiIBP)
        *reinterpret_cast<int32_t*>(multiIBP + 0x58) = (rectH + rectW) * 2 - 4;

    const int sMinX = static_cast<int>(shMinX), sMinY = static_cast<int>(shMinY);
    const int sMaxX = static_cast<int>(shMaxX), sMaxY = static_cast<int>(shMaxY);
    void* multiVal = reinterpret_cast<void*>(multiIBP);

    // 7. Corners (skip any the shelter covers) + 4 edge runs.
    if (minCol < sMinX || sMaxX < minCol || minRow < sMinY || sMaxY < minRow)
        SpawnFencePiece_dbc80(this, 2, minCol, minRow, 1, 1, 0x5a, &multiVal);
    if (minCol < sMinX || sMaxX < minCol || maxRow < sMinY || sMaxY < maxRow)
        SpawnFencePiece_dbc80(this, 2, minCol, maxRow, 1, 1, 0, &multiVal);
    if (maxCol < sMinX || sMaxX < maxCol || maxRow < sMinY || sMaxY < maxRow)
        SpawnFencePiece_dbc80(this, 2, maxCol, maxRow, 1, 1, -0x5a, &multiVal);
    if (maxCol < sMinX || sMaxX < maxCol || minRow < sMinY || sMaxY < minRow)
        SpawnFencePiece_dbc80(this, 2, maxCol, minRow, 1, 1, 0xb4, &multiVal);

    EdgeRun_db8c0(this, minCol + 1, minRow, maxCol - 1, minRow,
                  sMinX, sMinY, sMaxX, sMaxY, 0, &multiVal);
    EdgeRun_db8c0(this, minCol + 1, maxRow, maxCol - 1, maxRow,
                  sMinX, sMinY, sMaxX, sMaxY, 0, &multiVal);
    EdgeRun_db8c0(this, minCol, minRow + 1, minCol, maxRow - 1,
                  sMinX, sMinY, sMaxX, sMaxY, 0x5a, &multiVal);
    EdgeRun_db8c0(this, maxCol, minRow + 1, maxCol, maxRow - 1,
                  sMinX, sMinY, sMaxX, sMaxY, 0x5a, &multiVal);

    // 8. Register the master entity (priority from the tool's own template).
    {
        int64_t val = masterIBP;
        int prio = *reinterpret_cast<int32_t*>(
            reinterpret_cast<uintptr_t>(descriptionPtr) + 0x20);
        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xe8),
                                       &val, prio);
    }

    return 1;
}
