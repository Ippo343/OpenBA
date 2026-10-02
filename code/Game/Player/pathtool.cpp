// code/Game/Player/pathtool.cpp
// Clean reimplementation of PathTool vtable methods from Application-x64.exe.
//
// PathTool (0x60 bytes) extends Tool (0x28 bytes) directly (NOT DragTool).
// Handles path placement: multi-point entity selection with highlighting.
//
// Vtable at 0x140160680 (13 slots). Four are reimplemented here:
//   Slot0  Dtor      (0x140026b70) — scalar deleting destructor
//   Slot6  OnKey     (0x1400de6b0) — path creation input handler
//   Slot8  Draw      (0x1400dd3b0) — render path preview
//   Slot10 Serialize (0x1400de8d0) — save/load entity comparison
//
// Decompiled from Application-x64.exe.

#include "Player/PathTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "math/Math.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Player/exe_bodies.h"
#include "Game/Map.h"
#include "Game/WorldGeometry.h"
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)

// ============================================================
// PathTool helpers, called by name (Player/exe_bodies.h):
//   0x1401005a0 PlayerRay_GetMouseRay_1005a0 -- ray from the mouse position
//               (this call site passes (keyMap, dir, origin))
//   0x1400aa530 PlayerRay_TerrainMarch_aa530 -- intersect the ray with the terrain
//   0x1400a0280 PlayerEdit_EntityAtCell_a0280 -- entity at a map cell
//   0x1400dd710 PathTool_helper_d710 -- SetSelectedEntity (+ highlighting)
//   0x1401008c0 PlayerTileRender_DrawOverlay_08c0 -- the tile overlay submit
// NOTE: the former s_fn_SubmitDebugDraw helper was removed — it resolved a
// dropped-digit-typo address (0x1400c08c0, mid-function in FUN_1400c0690) and was
// never called. Dead code.
// ============================================================

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline uint64_t GetKeyMap() {
    return (uint64_t)Input::Configuration::_singleton->_keyMap;
}

// 0x1400de950 — ~PathTool. After the leaf restamp (the compiler's), three
// inlined Collection::Array<T>::~Array bodies, in this order:
//   MemFree([+0x50]); +0x50 = +0x58 = 0     (segment list, 8-byte pairs)
//   MemFree([+0x40]); +0x40 = +0x48 = 0     (point list, int32)
//   MemFree([+0x30]); +0x30 = +0x38 = 0     (route list, 0x200c records)
// through `[0x140152b68]` = System::MemoryAllocator::MemFree (the same static
// InternalMemFree reaches; MemFree(null) is a no-op in retail, which calls it
// unconditionally). Then Tool's restamp and `~Internal` (the compiler chain).
PathTool::~PathTool() {
    Object::detail::InternalMemFree(_field50);
    _field50 = nullptr;
    *reinterpret_cast<uint64_t*>(&_field58) = 0;   // size + capacity dwords
    Object::detail::InternalMemFree(_field40);
    _field40 = nullptr;
    *reinterpret_cast<uint64_t*>(&_field48) = 0;
    Object::detail::InternalMemFree(_field30);
    _field30 = nullptr;
    _field38 = nullptr;
}

// ============================================================
// PathTool::OnKey (vtable +0x30, Slot6) — Path creation input handler
// ============================================================
// On key-down with Pressed(0,5): picks map cell under cursor via terrain
// raycast and picking scene intersection, then selects/highlights the entity.
//
// Flow:
//   1. Check if primary action button is pressed (KeyMap::Pressed(0,5))
//   2. Compute mouse ray (GetMouseRay)
//   3. Raycast terrain to get world position -> convert to cell index
//   4. Get entity at cell via GetEntityAtCell
//   5. Also try Picking::Scene::Intersect for 3D object picking
//   6. If found entity differs from current selection, call SetSelectedEntity
//   7. Always returns 1
//
// Original: 0x1400de6b0
char PathTool::OnKey(char key) {
    if (key == '\0') {
        return 1;
    }

    // Check if primary action button is pressed
    uint64_t keyMap = GetKeyMap();

    // KeyMap::Pressed(keyMap, 0, 5) — retail calls it through exe slot 0x140151758.
    bool pressed = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
    if (!pressed) return 1;

    // Get map data pointer
    uint64_t mapData = *(uint64_t*)(((uintptr_t)GameUpdateLoop::_singleton) + 0xf8);

    // Compute mouse ray
    float rayOrigin[3] = {0};  // 12 bytes (vec3)
    float rayDir[3] = {0};     // 12 bytes (vec3)
    float distance[2] = {0};   // raycast distance param
    PlayerRay_GetMouseRay_1005a0(reinterpret_cast<void*>(keyMap), rayDir, rayOrigin);

    // Raycast terrain
    distance[0] = 400.0f;
    uint64_t entityIBP = 0;
    int* mapGrid = *(int**)(((uintptr_t)GameUpdateLoop::_singleton) + 0xf8);
    float outPos[4] = {0};

    // local_res18 is used for terrain height param: 400.0f
    float terrainParam[2] = {0};
    terrainParam[0] = 400.0f;  // 0x43c80000 as float

    WorldGeometry* mapSystem = GameUpdateLoop::_singleton->m_worldGeometry;
    if (mapSystem) {
        bool hit = PlayerRay_TerrainMarch_aa530(mapSystem, (float*)rayDir, rayOrigin,
            terrainParam, outPos, distance) != 0;
        if (hit) {
            int gridSize = *mapGrid;
            int cellX = (int)(outPos[0] + (float)(gridSize >> 1));
            int cellY = (int)(outPos[1] + (float)(gridSize >> 1));
            // Clamp to grid bounds
            if (cellX < 0) cellX = 0;
            if (cellX >= gridSize) cellX = gridSize - 1;
            if (cellY < 0) cellY = 0;
            if (cellY >= gridSize) cellY = gridSize - 1;
            int cellIndex = gridSize * cellY + cellX;
            if (cellIndex > 0) {
                uint64_t retIBP = 0;
                PlayerEdit_EntityAtCell_a0280(mapData, &retIBP, cellIndex);
                entityIBP = retIBP;
            }
        }
    }

    // Try picking scene intersection as well
    float hitNormal[3] = {0};
    float hitPos[3] = {0};
    void* pickingScene = *(void**)(((uintptr_t)GameUpdateLoop::_singleton) + 0x70);
    if (pickingScene) {
        // Picking::Scene::Intersect / GetUserData (import slots 0x140152428 /
        // 0x140152890), arguments in the door's positional order.
        uint32_t hitIdx = static_cast<uint32_t>(reinterpret_cast<const Picking::Scene*>(pickingScene)->Intersect(   // B.3.7 bridge
            *reinterpret_cast<const Vector3*>(rayDir), *reinterpret_cast<const Vector3*>(rayOrigin),
            *reinterpret_cast<const Math::Scalar*>(distance),
            *reinterpret_cast<Vector3*>(hitNormal), *reinterpret_cast<Vector3*>(hitPos)));
        if ((int)hitIdx >= 0) {
            void* userData = reinterpret_cast<const Picking::Scene*>(pickingScene)->GetUserData(hitIdx);   // B.3.7 bridge
            {
                uint64_t tmpIBP = 0;
                new (&tmpIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(userData));   // B.3.3 bridge
                entityIBP = tmpIBP;
            }
        }
    }

    // If we found an entity and it differs from current selection, update
    if (entityIBP != 0) {
        if ((uint64_t)selectedEntity != entityIBP) {
            uint64_t tmpIBP = entityIBP;
            PathTool_helper_d710(reinterpret_cast<int64_t>(this), &tmpIBP);
        }
    }

    return 1;
}

// ============================================================
// Draw records: ToolTileRecord (0x14) and ToolSegmentRecord (0x20), Player/Tool.h,
// in real Collection::Arrays (stride 0x14 / 0x20, align 4, byteCopy). B4 PLAYER:
// the two hand-written {data, count, capacity} arrays and their toEngineFormat
// packs are gone. Their grow went to n*3 even from empty (capacity 3 for the first
// record); SetSizeCore's reference rule gives capacity 1 there. Allocation size
// only; no record differs.
// ============================================================

// ============================================================
// PathTool::Draw (vtable +0x40, Slot8) — Render path preview overlay
// ============================================================
// Iterates two data arrays stored in the PathTool:
//   1. Point markers at +0x40 (int array) / +0x48 (count): single-cell highlights
//   2. Line segments at +0x50 (int pair array) / +0x58 (count): cell-to-cell lines
//
// For each point marker, converts flat cell index to (col, row) and builds a
// 0x14-byte tile record with variant=4, color=0xfffccc4c.
//
// For each line segment, reads two cell indices, converts both to (col, row),
// and builds a 0x20-byte segment record with variant=5, color=0xfffccc4c,
// width=0.15f.
//
// Both arrays are then passed to 0x1401008c0, which handles the actual GPU
// vertex submission (material setup, quad generation, etc.).
//
// Original: 0x1400dd3b0
void PathTool::Draw(Video::Context* context, char) {
    int mapWidth = GameUpdateLoop::_singleton->m_game->size;

    // ---- Build tile array from point markers (+0x40, +0x48) ----
    Collection::Array<ToolTileRecord> tiles;
    const int32_t* pointArray = _field40;
    int pointCount  = _field48;

    for (int i = 0; i < pointCount; i++) {
        int cellIdx = pointArray[i];
        ToolTileRecord rec;
        rec.col     = cellIdx % mapWidth;
        rec.row     = cellIdx / mapWidth;
        rec.variant = 4;
        rec.color   = 0xfffccc4cu;
        rec.angle   = 0;
        tiles.SetSize(tiles._size + 1, rec);
    }

    // ---- Build line array from segments (+0x50, +0x58) ----
    Collection::Array<ToolSegmentRecord> lines;
    const int32_t* segArray  = _field50;
    int segCount = _field58;

    for (int i = 0; i < segCount; i++) {
        int cellA = segArray[i * 2];
        int cellB = segArray[i * 2 + 1];
        ToolSegmentRecord rec;
        rec.col1    = cellA % mapWidth;
        rec.row1    = cellA / mapWidth;
        rec.variant = 5;
        rec.color   = 0xfffccc4cu;
        rec.angle   = 0;
        rec.col2    = cellB % mapWidth;
        rec.row2    = cellB / mapWidth;
        rec.width   = 0.15f;  // 0x3e19999a
        lines.SetSize(lines._size + 1, rec);
    }

    // ---- Render via 0x1401008c0 ----
    PlayerTileRender_DrawOverlay_08c0(nullptr, context, &tiles, &lines);
    // lines / tiles are released by ~Array at scope exit.
}

// ============================================================
// PathTool::Serialize (vtable +0x50, Slot10) — Entity comparison
// ============================================================
// Calls CheckPtr on self->selectedEntity (+0x28), then compares *param2
// to the selected entity value. Returns true if they match.
//
// Original: 0x1400de8d0
bool PathTool::IsSelected(uint64_t* param2) {
    // Call CheckPtr on self->selectedEntity to resolve any lazy pointer

    // Compare the stream value to the selected entity
    return *param2 == selectedEntity;
}

// ============================================================
// PathTool members (PathTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_PathTool_Vtable[13], its slot stubs, the _SA factories and
// RegisterPathToolVtable are gone: the compiler builds the table, and nothing
// reads 0x140160680 back by address (the old dtor was the only reader).
// Inherited from Tool, as in retail: [11] no setting (0x140004950 `*ret = 0`;
// SetEditMode dereferences the returned slot).
// ============================================================
#include "Object/factory_registration.h"
#include <new>

// [6] OnKey (0x1400de6b0), [8] Draw (0x1400dd3b0) and [10] IsSelected (0x1400de8d0)
// are defined above; [5] Shutdown (0x1400de930, a bare JMP to 0x1400dd250) is in
// pathtool_setselected.cpp next to the helper it jumps to.

// create 0x140026b10: MemAlloc(0x60, 8), then the ctor 0x1400de9e0.
void* PathTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PathTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PathTool());
}
// createInPlace 0x140026b50: `if (storage) ctor(storage)`.
void* PathTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PathTool());
}

DEFINE_EXE_TYPE(PathTool, "Tool")
