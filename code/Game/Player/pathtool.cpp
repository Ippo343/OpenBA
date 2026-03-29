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
// PathTool::Draw (vtable +0x40, Slot8) — Render path preview overlay
// ============================================================
// Builds render entries from path points and line segments, then submits
// to the debug draw system. This is a complex function (181 lines decompiled)
// that manages dynamic arrays of render entries.
//
// Since it involves complex dynamic array management with MemAlloc/MemFree and
// per-cell render entry construction, and calls the internal SubmitDebugDraw
// helper which handles the actual rendering, we delegate to the original
// via trampoline for now.
//
// TODO: Once the dynamic array helpers and render entry format are fully typed,
// this can be reimplemented in clean C++.
//
// Original: 0x1400dd3b0
// NOTE: This function is kept as trampoline passthrough in the reimpl wrapper.

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
