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
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// Resolved vtable pointers (set once on first call)
// ============================================================
static void* s_vtable_PathTool = nullptr;
static void* s_vtable_Tool     = nullptr;

static void EnsureVtables() {
    if (!s_vtable_PathTool) {
        s_vtable_PathTool = (void*)ResolveAddr(Module::Exe, PathToolAddr::Vtable);
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

// Picking::Scene::Intersect — ray vs picking scene intersection.
// Dispatch slot at 0x140152428.
typedef uint32_t (*fn_PickingIntersect_t)(void* scene, void* rayOrigin, void* rayDir,
    void* distance, void* outNormal, void* outPos);
static fn_PickingIntersect_t s_fn_PickingIntersect = nullptr;

// Picking::Scene::GetUserData — get user data for picking result.
// Dispatch slot at 0x140152890.
typedef void* (*fn_PickingGetUserData_t)(void* scene, uint32_t hitIndex);
static fn_PickingGetUserData_t s_fn_PickingGetUserData = nullptr;

// PathTool_helper_d710: SetSelectedEntity — sets the selected entity with highlighting.
// Args: (self, entityIBP)
typedef void (*fn_PathToolSetSelected_t)(uint64_t self, void* entityIBP);
static fn_PathToolSetSelected_t s_fn_PathToolSetSelected = nullptr;

// CharacterComponent_impl_08c0: SubmitDebugDraw — submits render entries for debug overlay.
// Args: (unused, context, pointArray, lineArray)
typedef void (*fn_SubmitDebugDraw_t)(void* unused, void* context, void* pointArray, void* lineArray);
static fn_SubmitDebugDraw_t s_fn_SubmitDebugDraw = nullptr;

// fn_KeyMap_Pressed dispatch slot at 0x140151758
static void** s_dispatchSlot_KeyMapPressed = nullptr;

// fn_InternalBasePtr_CheckPtr dispatch slot at 0x140152ab0
typedef void (*fn_CheckPtr_t)(void* self);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

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
    s_fn_SubmitDebugDraw = reinterpret_cast<fn_SubmitDebugDraw_t>(
        ResolveAddr(Module::Exe, 0x1400c08c0));

    // Dispatch-table resolved function pointers
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152428));
        if (d) s_fn_PickingIntersect = reinterpret_cast<fn_PickingIntersect_t>(*d);
    }
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152890));
        if (d) s_fn_PickingGetUserData = reinterpret_cast<fn_PickingGetUserData_t>(*d);
    }
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ab0));
        if (d) s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(*d);
    }

    s_dispatchSlot_KeyMapPressed = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140151758));

    s_funcsResolved = true;
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline uint64_t GetKeyMap() {
    return (uint64_t)***(void****)g_pKeyMapObjectExref;
}

// ============================================================
// PathTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: PathTool -> Tool, calls Object::Internal::~Internal,
// conditionally frees memory if flags & 1.
//
// Original: 0x140026b70
void* PathTool_Dtor(PathTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    // PathTool has NO intermediate class (Tool is direct base, no DragTool)
    self->vtable = s_vtable_PathTool;
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

    // Call KeyMap::Pressed(keyMap, 0, 5) through dispatch slot
    typedef bool (*fn_Pressed_t)(uint64_t self, int slot, int keyId);
    fn_Pressed_t fnPressed = nullptr;
    if (s_dispatchSlot_KeyMapPressed) {
        fnPressed = reinterpret_cast<fn_Pressed_t>(*s_dispatchSlot_KeyMapPressed);
    }

    if (!fnPressed) return 1;
    bool pressed = fnPressed(keyMap, 0, 5);
    if (!pressed) return 1;

    // Get map data pointer
    uint64_t mapData = *(uint64_t*)(uintptr_t(g_pGameWorldSingleton) + 0xf8);

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
    int* mapGrid = *(int**)(uintptr_t(g_pGameWorldSingleton) + 0xf8);
    float outPos[4] = {0};

    // local_res18 is used for terrain height param: 400.0f
    float terrainParam[2] = {0};
    terrainParam[0] = 400.0f;  // 0x43c80000 as float

    uint64_t mapSystem = *(uint64_t*)(uintptr_t(g_pGameWorldSingleton) + 0x110);
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
    void* pickingScene = *(void**)(uintptr_t(g_pGameWorldSingleton) + 0x70);
    if (s_fn_PickingIntersect && pickingScene) {
        uint32_t hitIdx = s_fn_PickingIntersect(pickingScene, rayDir, rayOrigin,
            distance, hitNormal, hitPos);
        if ((int)hitIdx >= 0 && s_fn_PickingGetUserData) {
            void* userData = s_fn_PickingGetUserData(pickingScene, hitIdx);
            if (fn_InternalBasePtr_Ctor) {
                uint64_t tmpIBP = 0;
                fn_InternalBasePtr_Ctor(&tmpIBP, userData);
                entityIBP = tmpIBP;
            }
        }
    }

    // If we found an entity and it differs from current selection, update
    if (entityIBP != 0) {
        if (s_fn_CheckPtr) {
            s_fn_CheckPtr(&entityIBP);
        }
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
    if (s_fn_CheckPtr) {
        s_fn_CheckPtr(&self->selectedEntity);
    }

    // Compare the stream value to the selected entity
    return *param2 == self->selectedEntity;
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// PathTool: size 0x60, parent Tool, 13-slot vtable @ 0x140160680.
// slot12 returns 4 (tool type id); other base slots stubbed.
// ============================================================
#ifdef STANDALONE_EXE
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PathTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PathTool");
    return c;
}
static int PathTool_Slot12(void*) { return 4; }  // exe 0x1400de910 returns 4
static void* g_PathTool_Vtable[13] = {
    (void*)&PathTool_Dtor,       // +0x00 Dtor
    (void*)&Tool_Slot1,          // +0x08
    (void*)&PathTool_GetTypeId,  // +0x10 GetTypeId
    (void*)&Tool_Persist,        // +0x18 Serialize (base)
    (void*)&Tool_Ret0,           // +0x20 (exe RET nop)
    (void*)&Tool_Ret0,           // +0x28 (cleanup highlight)
    (void*)&PathTool_OnKey,      // +0x30 OnKey
    (void*)&Tool_Ret0,           // +0x38 (exe RET nop)
    (void*)&PathTool_Draw,       // +0x40 Draw
    (void*)&Tool_Ret0,           // +0x48 (returns false)
    (void*)&PathTool_Serialize,  // +0x50 Serialize
    (void*)&Tool_Ret0,           // +0x58
    (void*)&PathTool_Slot12,     // +0x60 (returns 4)
};
extern "C" void RegisterPathToolVtable() {
    RegisterExeAddr(0x140160680, (uintptr_t)g_PathTool_Vtable);
}
extern "C" void* PathTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x60, g_PathTool_Vtable, 0);}
extern "C" void* PathTool_Factory_SA(void) {
    return Tool_Alloc(0x60, g_PathTool_Vtable, 0);
}
REGISTER_TYPE(PathTool, "Tool", &PathTool_Factory_SA, &PathTool_CreateInPlace_SA);
#endif // STANDALONE_EXE
