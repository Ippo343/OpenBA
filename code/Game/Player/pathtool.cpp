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

#include "Player/pathtool_funcs.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)

// ============================================================
// Local function pointers for PathTool-specific operations
// ============================================================

// FUN_1401005a0: GetMouseRay — computes ray origin and direction from mouse position.
// Args: (keyMap, outDir, outOrigin)
typedef void (*fn_GetMouseRay_t)(uint64_t keyMap, void* outDir, void* outOrigin);
static fn_GetMouseRay_t s_fn_GetMouseRay = nullptr;

// FUN_1400aa530 (QuarryComponent_impl_a530): RaycastTerrain — intersects ray with terrain.
// Args: (mapSystem, rayOrigin, rayDir, outParam, outPos, outDistance) -> bool
typedef bool (*fn_RaycastTerrain_t)(uint64_t mapSystem, float* rayOrigin, void* rayDir,
    float* outParam, float* outPos, float* outDistance);
static fn_RaycastTerrain_t s_fn_RaycastTerrain = nullptr;

// FUN_1400a0280: GetEntityAtCell — looks up entity at map cell index.
// Args: (mapData, retIBP, cellIndex) -> InternalBasePtr*
typedef void* (*fn_GetEntityAtCell_t)(uint64_t mapData, void* retIBP, int cellIndex);
static fn_GetEntityAtCell_t s_fn_GetEntityAtCell = nullptr;

// PathTool_helper_d710: SetSelectedEntity — sets the selected entity with highlighting.
// Args: (self, entityIBP)
typedef void (*fn_PathToolSetSelected_t)(uint64_t self, void* entityIBP);
static fn_PathToolSetSelected_t s_fn_PathToolSetSelected = nullptr;

// NOTE: the former s_fn_SubmitDebugDraw helper was removed — it resolved a
// dropped-digit-typo address (0x1400c08c0, mid-function in FUN_1400c0690) and was
// never called (PathTool::Draw uses fn_TileRender for the debug overlay). Dead code.


static bool s_funcsResolved = false;

static void ResolvePathToolFuncs() {
    if (s_funcsResolved) return;

    s_fn_GetMouseRay = reinterpret_cast<fn_GetMouseRay_t>(
        ResolveAddr(Module::Exe, 0x1401005a0));
    s_fn_RaycastTerrain = reinterpret_cast<fn_RaycastTerrain_t>(
        ResolveAddr(Module::Exe, 0x1400aa530));
    s_fn_GetEntityAtCell = reinterpret_cast<fn_GetEntityAtCell_t>(
        ResolveAddr(Module::Exe, 0x1400a0280));
    s_fn_PathToolSetSelected = reinterpret_cast<fn_PathToolSetSelected_t>(
        ResolveAddr(Module::Exe, 0x1400dd710));


    s_funcsResolved = true;
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline uint64_t GetKeyMap() {
    return (uint64_t)***(void****)g_pInputConfigSingletonExref;
}

// ============================================================
// PathTool::DeletingDtor (vtable +0x00) — 0x140026b70
// ============================================================
// ENDGAME B.3.8 TOOL: the hand-rolled PathTool_Dtor restamped the Path and
// Tool tables read by address — Tool's UNREGISTERED, so it stored a NULL vptr
// before ~Internal (B38 D9 #3). The compiler's dtor chain restamps the real
// tables, then runs Object::Internal::~Internal. ~PathTool (0x1400de950) is
// NOT empty in retail — see below.
Object::Base* PathTool::DeletingDtor(unsigned flags) {
    this->~PathTool();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
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
uint64_t PathTool_OnKey(PathTool* self, char key) {
    ResolvePathToolFuncs();

    if (key == '\0') {
        return 1;
    }

    // Check if primary action button is pressed
    uint64_t keyMap = GetKeyMap();

    // KeyMap::Pressed(keyMap, 0, 5) — retail calls it through exe slot 0x140151758.
    bool pressed = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
    if (!pressed) return 1;

    // Get map data pointer
    uint64_t mapData = *(uint64_t*)(((uintptr_t)*g_pGameWorldSingleton) + 0xf8);

    // Compute mouse ray
    float rayOrigin[3] = {0};  // 12 bytes (vec3)
    float rayDir[3] = {0};     // 12 bytes (vec3)
    float distance[2] = {0};   // raycast distance param
    if (s_fn_GetMouseRay) {
        s_fn_GetMouseRay(keyMap, rayDir, rayOrigin);
    }

    // Raycast terrain
    distance[0] = 400.0f;
    uint64_t entityIBP = 0;
    int* mapGrid = *(int**)(((uintptr_t)*g_pGameWorldSingleton) + 0xf8);
    float outPos[4] = {0};

    // local_res18 is used for terrain height param: 400.0f
    float terrainParam[2] = {0};
    terrainParam[0] = 400.0f;  // 0x43c80000 as float

    uint64_t mapSystem = *(uint64_t*)(((uintptr_t)*g_pGameWorldSingleton) + 0x110);
    if (s_fn_RaycastTerrain && mapSystem) {
        bool hit = s_fn_RaycastTerrain(mapSystem, (float*)rayDir, rayOrigin,
            terrainParam, outPos, distance);
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
            if (cellIndex > 0 && s_fn_GetEntityAtCell) {
                uint64_t retIBP = 0;
                s_fn_GetEntityAtCell(mapData, &retIBP, cellIndex);
                entityIBP = retIBP;
            }
        }
    }

    // Try picking scene intersection as well
    float hitNormal[3] = {0};
    float hitPos[3] = {0};
    void* pickingScene = *(void**)(((uintptr_t)*g_pGameWorldSingleton) + 0x70);
    if (pickingScene) {
        // Picking::Scene::Intersect / GetUserData (import slots 0x140152428 /
        // 0x140152890), arguments in the door's positional order.
        uint32_t hitIdx = static_cast<uint32_t>(reinterpret_cast<const Picking::Scene*>(pickingScene)->Intersect(   // B.3.7 bridge
            *reinterpret_cast<const Vector3*>(rayDir), *reinterpret_cast<const Vector3*>(rayOrigin),
            *reinterpret_cast<const Math::Scalar*>(distance),
            *reinterpret_cast<Vector3*>(hitNormal), *reinterpret_cast<Vector3*>(hitPos)));
        if ((int)hitIdx >= 0) {
            void* userData = reinterpret_cast<const Picking::Scene*>(pickingScene)->GetUserData(hitIdx);   // B.3.7 bridge
            if (fn_InternalBasePtr_Ctor) {
                uint64_t tmpIBP = 0;
                new (&tmpIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(userData));   // B.3.3 bridge
                entityIBP = tmpIBP;
            }
        }
    }

    // If we found an entity and it differs from current selection, update
    if (entityIBP != 0) {
        if ((uint64_t)self->selectedEntity != entityIBP) {
            uint64_t tmpIBP = entityIBP;
            if (s_fn_PathToolSetSelected) {
                s_fn_PathToolSetSelected((uint64_t)self, &tmpIBP);
            }
        }
    }

    return 1;
}

// ============================================================
// Dynamic array helpers for Draw (0x14-byte tile records, 0x20-byte line records)
// ============================================================
// TileRecord: { int col, int row, int type, uint32_t color, int unused }
// LineRecord: { int col1, int row1, int type, uint32_t color, int unused,
//               int col2, int row2, float lineWidth }

struct PathTileRecord {
    int col;
    int row;
    int type;         // 4 = point marker
    uint32_t color;   // 0xfffccc4c
    int unused;       // 0
};
static_assert(sizeof(PathTileRecord) == 0x14, "PathTileRecord size mismatch");

struct PathLineRecord {
    int col1;
    int row1;
    int type;         // 5 = line segment
    uint32_t color;   // 0xfffccc4c
    int unused;       // 0
    int col2;
    int row2;
    float lineWidth;  // 0.15f (0x3e19999a)
};
static_assert(sizeof(PathLineRecord) == 0x20, "PathLineRecord size mismatch");

// Minimal growable array for tile records
struct PathTileArray {
    PathTileRecord* data;
    int count;
    int capacity;

    PathTileArray() : data(nullptr), count(0), capacity(0) {}

    void push(const PathTileRecord& rec) {
        int newCount = count + 1;
        if (newCount > capacity) {
            int newCap = (count == 0) ? newCount * 3 : newCount * 3;
            PathTileRecord* newData = (PathTileRecord*)fn_MemAlloc(
                g_pMemAllocSingleton, (uint64_t)newCap * sizeof(PathTileRecord), 4);
            if (data && newData) {
                memcpy(newData, data, (uint64_t)count * sizeof(PathTileRecord));
            }
            if (data) fn_MemFreeStatic(data);
            data = newData;
            capacity = newCap;
        }
        if (data) data[count] = rec;
        count = newCount;
    }

    void toEngineFormat(void* out) {
        uintptr_t* p = (uintptr_t*)out;
        p[0] = (uintptr_t)data;
        int32_t* ints = (int32_t*)&p[1];
        ints[0] = count;
        ints[1] = capacity;
    }

    void free() {
        if (data) fn_MemFreeStatic(data);
        data = nullptr;
        count = 0;
        capacity = 0;
    }
};

// Minimal growable array for line records
struct PathLineArray {
    PathLineRecord* data;
    int count;
    int capacity;

    PathLineArray() : data(nullptr), count(0), capacity(0) {}

    void push(const PathLineRecord& rec) {
        int newCount = count + 1;
        if (newCount > capacity) {
            int newCap = (count == 0) ? newCount * 3 : newCount * 3;
            PathLineRecord* newData = (PathLineRecord*)fn_MemAlloc(
                g_pMemAllocSingleton, (uint64_t)newCap * sizeof(PathLineRecord), 4);
            if (data && newData) {
                memcpy(newData, data, (uint64_t)count * sizeof(PathLineRecord));
            }
            if (data) fn_MemFreeStatic(data);
            data = newData;
            capacity = newCap;
        }
        if (data) data[count] = rec;
        count = newCount;
    }

    void toEngineFormat(void* out) {
        uintptr_t* p = (uintptr_t*)out;
        p[0] = (uintptr_t)data;
        int32_t* ints = (int32_t*)&p[1];
        ints[0] = count;
        ints[1] = capacity;
    }

    void free() {
        if (data) fn_MemFreeStatic(data);
        data = nullptr;
        count = 0;
        capacity = 0;
    }
};

// ============================================================
// PathTool::Draw (vtable +0x40, Slot8) — Render path preview overlay
// ============================================================
// Iterates two data arrays stored in the PathTool:
//   1. Point markers at +0x40 (int array) / +0x48 (count): single-cell highlights
//   2. Line segments at +0x50 (int pair array) / +0x58 (count): cell-to-cell lines
//
// For each point marker, converts flat cell index to (col, row) and builds a
// 0x14-byte TileRecord with type=4, color=0xfffccc4c.
//
// For each line segment, reads two cell indices, converts both to (col, row),
// and builds a 0x20-byte LineRecord with type=5, color=0xfffccc4c, lineWidth=0.15f.
//
// Both arrays are then passed to fn_TileRender which handles the actual GPU
// vertex submission (material setup, quad generation, etc.).
//
// Original: 0x1400dd3b0
void PathTool_Draw(PathTool* self, void* context) {
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int mapWidth = *mapGrid;

    // ---- Build tile array from point markers (+0x40, +0x48) ----
    PathTileArray tiles;
    int* pointArray = (int*)self->_field40;
    int pointCount  = self->_field48;

    for (int i = 0; i < pointCount; i++) {
        int cellIdx = pointArray[i];
        PathTileRecord rec;
        rec.col   = cellIdx % mapWidth;
        rec.row   = cellIdx / mapWidth;
        rec.type  = 4;
        rec.color = 0xfffccc4cu;
        rec.unused = 0;
        tiles.push(rec);
    }

    // ---- Build line array from line segments (+0x50, +0x58) ----
    PathLineArray lines;
    int* segArray  = (int*)self->_field50;
    int segCount   = self->_field58;

    for (int i = 0; i < segCount; i++) {
        int cellA = segArray[i * 2];
        int cellB = segArray[i * 2 + 1];
        PathLineRecord rec;
        rec.col1      = cellA % mapWidth;
        rec.row1      = cellA / mapWidth;
        rec.type      = 5;
        rec.color     = 0xfffccc4cu;
        rec.unused    = 0;
        rec.col2      = cellB % mapWidth;
        rec.row2      = cellB / mapWidth;
        rec.lineWidth = 0.15f;  // 0x3e19999a
        lines.push(rec);
    }

    // ---- Render via fn_TileRender ----
    uint8_t tileArrayBuf[16] = {0};
    uint8_t lineArrayBuf[16] = {0};
    tiles.toEngineFormat(tileArrayBuf);
    lines.toEngineFormat(lineArrayBuf);

    if (fn_TileRender) {
        fn_TileRender(nullptr, context, tileArrayBuf, lineArrayBuf);
    }

    lines.free();
    tiles.free();
}

// ============================================================
// PathTool::Serialize (vtable +0x50, Slot10) — Entity comparison
// ============================================================
// Calls CheckPtr on self->selectedEntity (+0x28), then compares *param2
// to the selected entity value. Returns true if they match.
//
// Original: 0x1400de8d0
bool PathTool_Serialize(PathTool* self, uint64_t* param2) {
    ResolvePathToolFuncs();

    // Call CheckPtr on self->selectedEntity to resolve any lazy pointer

    // Compare the stream value to the selected entity
    return *param2 == self->selectedEntity;
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

// Defined in pathtool_setselected.cpp (exe 0x1400dd250).
extern "C" void PathTool_ClearHighlight_d250(int64_t self);

// [5] 0x1400de930 is a bare JMP to 0x1400dd250 (clear the selection
// highlight). SetEditMode calls this slot on the OUTGOING tool before it is
// deleted; without it the last selected building stayed lit forever.
void PathTool::Shutdown() { PathTool_ClearHighlight_d250(reinterpret_cast<int64_t>(this)); }
char PathTool::OnKey(char active) {                                            // [6] 0x1400de6b0
    return static_cast<char>(PathTool_OnKey(this, active));
}
void PathTool::Draw(void* context, char) { PathTool_Draw(this, context); }    // [8] 0x1400dd3b0
bool PathTool::IsSelected(uint64_t* ref) { return PathTool_Serialize(this, ref); }  // [10] 0x1400de8d0

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

DEFINE_EXE_VTABLE(PathTool, "_ZTV8PathTool")
DEFINE_EXE_TYPE(PathTool, "Tool")
