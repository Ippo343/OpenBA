// code/Game/Player/fencedtool.cpp
// Clean reimplementation of FencedTool vtable methods from Application-x64.exe.
//
// FencedTool (0x70 bytes) extends DragTool (0x38 bytes).
// Handles fenced area placement (pasture, crop fields).
//
// Vtable at 0x1401605d0 (15 slots). Four are reimplemented here:
//   Slot0  Dtor      (0x140026930) — scalar deleting destructor
//   Slot5  Shutdown2  (0x1400dccc0) — cleanup scene object and mesh
//   Slot6  OnKey      (0x1400dcbe0) — wraps DragTool::OnKey + rotation handling
//   Slot9  Shutdown   (0x1400dcd10) — remove scene object + call base shutdown
//
// Remaining slots (Startup, Serialize, Draw, CalcPath, CalcCost) are too complex
// (many unresolved helpers: MeshInstance, Transform, entity creation, fence segment
// calculation) and are not reimplemented here.
//
// Decompiled from Application-x64.exe.

#include "Player/fencedtool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

// ============================================================
// Vtable addresses (for vtable chain reset in Dtor)
// ============================================================
static void* s_vtable_FencedTool = nullptr;
static void* s_vtable_DragTool   = nullptr;
static void* s_vtable_Tool       = nullptr;

static void EnsureVtables() {
    if (!s_vtable_FencedTool) {
        s_vtable_FencedTool = (void*)ResolveAddr(Module::Exe, FencedToolAddr::Vtable);
        s_vtable_DragTool   = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool       = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// Object::Internal::~Internal — resolved via dispatch slot at 0x140152770
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// Graphics::Scene::RemoveObject — resolved locally via exe IAT dispatch slot
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
static fn_SceneRemoveObject_t s_fn_SceneRemoveObject = nullptr;

static bool s_sceneResolved = false;

static void EnsureSceneFuncs() {
    if (s_sceneResolved) return;
    // Resolve via IAT at [0x140151360] — same pattern as storagedisplaycomponent.cpp
    uintptr_t iatSlot = ResolveAddr(Module::Exe, 0x140151360);
    if (iatSlot) s_fn_SceneRemoveObject = *reinterpret_cast<fn_SceneRemoveObject_t*>(iatSlot);
    s_sceneResolved = true;
}

// ============================================================
// FencedTool::Dtor (vtable +0x00, Slot0)
// ============================================================
// Resets vtable chain: FencedTool -> DragTool -> Tool,
// calls Object::Internal::~Internal, conditionally frees memory.
// Original: 0x140026930
void* FencedTool_Dtor(FencedTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance
    self->vtable = s_vtable_FencedTool;
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
// FencedTool::Shutdown2 (vtable +0x28, Slot5)
// ============================================================
// Removes the fence preview from the scene and destroys the mesh instance.
// Original: 0x1400dccc0
void FencedTool_Shutdown2(FencedTool* self) {
    EnsureSceneFuncs();

    // Remove scene object if added
    if ((int)self->sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, self->sceneObjectId);
        }
    }

    // Destroy mesh instance via vtable dtor if allocated
    void* mesh = self->meshInstance;
    if (mesh) {
        // Call scalar deleting destructor: vtable[0](mesh, 1)
        typedef void* (*fn_MeshDtor_t)(void* self, uint32_t flags);
        void** vtable = *(void***)mesh;
        fn_MeshDtor_t dtor = (fn_MeshDtor_t)vtable[0];
        dtor(mesh, 1);  // flags=1: free memory
    }
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline void* GetKeyMap() {
    return ***(void****)g_pKeyMapObjectExref;
}

// ============================================================
// FencedTool::OnKey (vtable +0x30, Slot6)
// ============================================================
// Wraps DragTool::OnKey with rotation handling for fence orientation.
//
// After the base OnKey call:
//   - KeyMap::Pressed(0, 0x13) = rotate counter-clockwise: decrement rotation,
//     wrap from 0 to 3
//   - KeyMap::Pressed(0, 0x14) = rotate clockwise: increment rotation,
//     wrap from 4 to 0
//
// Rotation value stored at rotationSetting(+0x50)->+0x10.
// Always returns 1 (event consumed).
// Original: 0x1400dcbe0
uint64_t FencedTool_OnKey(FencedTool* self, char key) {
    // Call base DragTool::OnKey
    if (fn_DragTool_OnKey) {
        fn_DragTool_OnKey(self, key);
    }

    void* keyMap = GetKeyMap();

    // Check rotate counter-clockwise (key 0x13)
    bool rotLeft = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 0x13) : false;
    if (rotLeft) {
        // Access rotationSetting IBP at self+0x50
        void* rotSettingIBP = (void*)((uintptr_t)self + 0x50);
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(rotSettingIBP);
        }
        uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
        *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) - 1;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(rotSettingIBP);
        }
        rotSetting = *(uintptr_t*)rotSettingIBP;
        if (*(int*)(rotSetting + 0x10) < 0) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            rotSetting = *(uintptr_t*)rotSettingIBP;
            *(int*)(rotSetting + 0x10) = 3;
        }
    } else {
        // Check rotate clockwise (key 0x14)
        bool rotRight = fn_KeyMap_Pressed ? fn_KeyMap_Pressed(keyMap, 0, 0x14) : false;
        if (rotRight) {
            void* rotSettingIBP = (void*)((uintptr_t)self + 0x50);
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
            *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) + 1;

            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(rotSettingIBP);
            }
            rotSetting = *(uintptr_t*)rotSettingIBP;
            if (*(int*)(rotSetting + 0x10) > 3) {
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr(rotSettingIBP);
                }
                rotSetting = *(uintptr_t*)rotSettingIBP;
                *(int*)(rotSetting + 0x10) = 0;
            }
        }
    }

    return 1;
}

// ============================================================
// FencedTool::Shutdown (vtable +0x48, Slot9)
// ============================================================
// Removes scene object if added, resets ID to -1, calls base DragTool::Shutdown.
// Original: 0x1400dcd10
void FencedTool_Shutdown(FencedTool* self) {
    EnsureSceneFuncs();

    // Remove scene object if added
    if ((int)self->sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, self->sceneObjectId);
        }
        self->sceneObjectId = (uint32_t)-1;
    }

    // Call base DragTool::Shutdown (0x1400fc640)
    if (fn_DragTool_Shutdown) {
        fn_DragTool_Shutdown(self);
    }
}

// ============================================================
// FencedTool::Serialize (vtable +0x38, Slot7) — Fence preview mesh management
// ============================================================
// Despite being named "Serialize", this manages the fence preview mesh during
// drag placement. Calculates fence placement params, adds/removes mesh from
// the scene, and updates its transform.
//
// When active (param3 != 0 and dragging):
//   1. Calculate fence placement via fn_FencedPlacementParams
//   2. If valid: add mesh to scene (if not already), set transform, update hierarchy
//   3. If invalid: remove mesh from scene
// When inactive: remove mesh from scene if present.
// Original: 0x1400dcad0
void FencedTool_Serialize_Impl(FencedTool* self, void* /*param2*/, char param3) {
    EnsureSceneFuncs();

    // Initialize local offset vector to {0, 0, 0}
    float offset[3] = {0.0f, 0.0f, 0.0f};

    if (param3 != 0 && self->dragState != 0) {
        // Placement parameters — outputs from FencedPlacementParams
        uint32_t outParam2[2] = {0, 0};
        uint32_t outParam3 = 0;
        uint32_t outParam4 = 0;
        uint32_t outParam5[2] = {0, 0};
        uint32_t outYaw[8];  // Scalar data for yaw/orientation
        memset(outYaw, 0, sizeof(outYaw));

        if (!fn_FencedPlacementParams) goto remove;

        bool valid = fn_FencedPlacementParams(
            self, outParam2, &outParam3, &outParam4, outParam5, offset, outYaw);

        if (valid) {
            // Add mesh to scene if not already added
            if ((int)self->sceneObjectId < 0) {
                void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
                if (fn_SceneAddObject && scene && self->meshInstance) {
                    self->sceneObjectId = (int32_t)fn_SceneAddObject(scene, self->meshInstance);
                }
            }

            // Build transform from yaw with zero pitch + zero offset.
            // Real signature: FromYawPitchOffset(retval, &yaw, &pitch, &offset)
            uint8_t localTransform[96];
            memset(localTransform, 0, sizeof(localTransform));
            float zeroPitch = 0.f;
            float zeroOffset[3] = {0.f, 0.f, 0.f};

            if (fn_Transform_FromYawPitchOffset) {
                fn_Transform_FromYawPitchOffset(localTransform, outYaw, &zeroPitch, zeroOffset);
            }

            // Set transform on mesh
            if (fn_Geometry_SetTransform && self->meshInstance) {
                fn_Geometry_SetTransform(self->meshInstance, localTransform);
            }

            // Update mesh hierarchy
            if (fn_MeshInstance_UpdateHierarchy && self->meshInstance) {
                fn_MeshInstance_UpdateHierarchy(self->meshInstance);
            }

            return;
        }
    }

remove:
    // Remove mesh from scene if present
    if ((int32_t)self->sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, (uint32_t)self->sceneObjectId);
        }
        self->sceneObjectId = -1;
    }
}

// ============================================================
// CalcCost helper function pointers
// ============================================================

// CharacterComponent_impl_f3e0: Terrain height query
typedef void (*fn_TerrainHeightQuery_t)(void* mapGrid, float* outPos, float* inPos,
                                         char param4, void* outExtra);
static fn_TerrainHeightQuery_t s_fn_TerrainHeightQuery = nullptr;

// FUN_1400dbf90: Anchor/rotation adjustment
typedef uint64_t (*fn_FencedTool_AnchorCalc_t)(void* self, uint32_t* p2, uint32_t* p3,
                                                 uint32_t* p4, uint32_t* p5,
                                                 float* p6, uint32_t* p7);
static fn_FencedTool_AnchorCalc_t s_fn_FencedTool_AnchorCalc = nullptr;

// Math::Scalar::Abs — `?Abs@Scalar@Math@@QEBA?AV12@XZ` returns Scalar BY VALUE:
// member ABI is (RCX=this, RDX=&retSlot) -> returns RDX. The old one-arg typedef
// left the callee writing its result through a garbage RDX (the classic hidden
// ret-slot clobber; it faulted WRITE-on-own-code the first time the height
// branch ran, CalcCost_Impl+0x570 2026-07-30).
typedef float* (*fn_ScalarAbs_t)(const float* self, float* retSlot);
static fn_ScalarAbs_t s_fn_ScalarAbs = nullptr;

// Math::Vector3::Vector3(out, x, y, z)
typedef void* (*fn_Vector3_Ctor_local_t)(void* out, float x, float y, float z);
static fn_Vector3_Ctor_local_t s_fn_Vector3_Ctor_local = nullptr;

static void EnsureCalcCostHelpers() {
    if (!s_fn_TerrainHeightQuery) {
        s_fn_TerrainHeightQuery = reinterpret_cast<fn_TerrainHeightQuery_t>(
            ResolveAddr(Module::Exe, 0x14009f3e0));
        s_fn_FencedTool_AnchorCalc = reinterpret_cast<fn_FencedTool_AnchorCalc_t>(
            ResolveAddr(Module::Exe, 0x1400dbf90));
        s_fn_ScalarAbs = reinterpret_cast<fn_ScalarAbs_t>(
            ResolveExport("?Abs@Scalar@Math@@QEBA?AV12@XZ"));
        if (!s_fn_ScalarAbs)
            s_fn_ScalarAbs = reinterpret_cast<fn_ScalarAbs_t>(
                ResolveAddr(Module::Runtime, 0x18008f530));
        s_fn_Vector3_Ctor_local = reinterpret_cast<fn_Vector3_Ctor_local_t>(
            ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
        if (!s_fn_Vector3_Ctor_local)
            s_fn_Vector3_Ctor_local = reinterpret_cast<fn_Vector3_Ctor_local_t>(
                ResolveAddr(Module::Runtime, 0x18008f880));
    }
}

static constexpr float HALF_CELL = 0.5f;
static constexpr float HEIGHT_TOLERANCE = 100.0f;

// ============================================================
// FencedTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Validates a rectangular fence region between startCell and endCell.
// Returns: 0=valid, 1=too small, 2=too large, 3=blocked
// Original: 0x1400db490
int FencedTool_CalcCost_Impl(FencedTool* self, int startCell, int endCell,
                              uint32_t* outWidth, int* outHeight, int* outDepth,
                              int* outFenceSegments) {
    EnsureCalcCostHelpers();

    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    int* mapGrid = *(int**)(gameWorld + 0xf8);
    int mapWidth = *mapGrid;

    // Convert flat cell indices to (col, row) via divmod
    int startCol = startCell % mapWidth;
    int startRow = startCell / mapWidth;
    int endCol   = endCell % mapWidth;
    int endRow   = endCell / mapWidth;

    // Sort to get min/max coordinates
    uint32_t minCol = (uint32_t)startCol, maxCol = (uint32_t)endCol;
    if (endCol < startCol) { maxCol = (uint32_t)startCol; minCol = (uint32_t)endCol; }
    uint32_t effectiveMaxCol = maxCol;

    uint32_t minRow = (uint32_t)startRow, maxRow = (uint32_t)endRow;
    if (endRow < startRow) { maxRow = (uint32_t)startRow; minRow = (uint32_t)endRow; }
    uint32_t effectiveMaxRow = maxRow;

    int width  = (int)(maxCol - minCol) + 1;
    int height = (int)(maxRow - minRow) + 1;

    *outHeight = width;
    *outDepth  = height;
    *outWidth  = 0;
    *outFenceSegments = (height + width) * 2 - 4;

    // Read size constraints from description
    uintptr_t desc = *(uintptr_t*)(selfAddr + 0x38);
    int minW = *(int*)(desc + 0x60);
    int minH = *(int*)(desc + 0x64);
    if (width < minW || height < minH) return 1;  // Too small

    int maxW = *(int*)(desc + 0x58);
    int maxH = *(int*)(desc + 0x5c);
    if (maxW < width || maxH < height) return 2;  // Too large

    // Read terrain type mask and allowed entity type
    uint32_t terrainMask = *(uint32_t*)(desc + 0x20);
    uintptr_t allowedEntityType = *(uintptr_t*)(desc + 0x70);

    // Cell data base pointer
    uintptr_t cellDataBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);

    float baseHeight = 0.0f;
    bool heightInitialized = false;
    bool allValid = true;

    int endRow2 = (int)minRow + height;
    int endCol2 = (int)minCol + width;

    for (int row = (int)minRow; row < endRow2 && allValid; row++) {
        for (int col = (int)minCol; col < endCol2 && allValid; col++) {
            if (col < 0 || mapWidth <= col || row < 0 || mapWidth <= row) {
                allValid = false; break;
            }

            int cellIdx = mapWidth * row + col;
            uintptr_t cellPtr = (uintptr_t)cellIdx * 0x30 + cellDataBase;

            // Check terrain type mask
            uint8_t terrainByte = *(uint8_t*)(cellPtr + 0x29);
            if ((~terrainMask & (1u << (terrainByte & 0x1f))) != 0) {
                allValid = false; break;
            }

            // Check entity occupancy
            uintptr_t cellEntity = *(uintptr_t*)cellPtr;
            if (allowedEntityType != 0) {
                if (cellEntity != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)cellPtr);
                    uintptr_t entityTypeRef = *(uintptr_t*)(*(uintptr_t*)cellPtr + 0x28);
                    if (entityTypeRef != allowedEntityType) { allValid = false; break; }
                }
            } else {
                if (cellEntity != 0) { allValid = false; break; }
            }

            // Check occupied flag
            if (*(uint8_t*)(cellPtr + 0x28) != 0) { allValid = false; break; }

            // Height validation
            if (s_fn_TerrainHeightQuery && s_fn_Vector3_Ctor_local && s_fn_ScalarAbs) {
                float halfOffset = (float)(mapWidth >> 1) - HALF_CELL;
                float inPos[4];
                s_fn_Vector3_Ctor_local(inPos, (float)(cellIdx % mapWidth) - halfOffset,
                                  (float)(cellIdx / mapWidth) - halfOffset, 0.0f);
                float outPos[4] = {0};
                uint32_t extra[2] = {0};
                s_fn_TerrainHeightQuery(mapGrid, outPos, inPos, '\0', extra);

                if (!heightInitialized) { baseHeight = outPos[2]; heightInitialized = true; }
                float heightDiff = outPos[2] - baseHeight;
                float absSlot = 0.0f;
                float* absResult = s_fn_ScalarAbs(&heightDiff, &absSlot);
                if (absResult && HEIGHT_TOLERANCE < *absResult) { allValid = false; break; }
            }
        }
    }

    if (!allValid) return 3;  // Blocked

    // Anchor/rotation adjustment
    if (!s_fn_FencedTool_AnchorCalc) return 3;
    uint32_t anchorMinCol = minCol, anchorMinRow = minRow;
    uint32_t anchorMaxCol = effectiveMaxCol, anchorMaxRow = effectiveMaxRow;
    float anchorPos[3] = {0};

    uint64_t anchorResult = s_fn_FencedTool_AnchorCalc(
        self, &anchorMinCol, &anchorMinRow, &anchorMaxCol, &anchorMaxRow,
        anchorPos, &anchorMinCol);
    if ((char)anchorResult == '\0') return 3;

    // Check anchor constraints if in edit mode
    uintptr_t toolState = *(uintptr_t*)(selfAddr + 0x10);
    if (*(char*)(toolState + 0x80) != '\0') {
        int mw = *(*(int**)(gameWorld + 0xf8));
        int storedStart = *(int*)(toolState + 0x98);
        int storedEnd   = *(int*)(toolState + 0x9c);
        if (minCol != (uint32_t)(storedStart % mw)) return 3;
        if (minRow != (uint32_t)(storedStart / mw)) return 3;
        if (effectiveMaxCol != (uint32_t)(storedEnd % mw)) return 3;
        if (effectiveMaxRow != (uint32_t)(storedEnd / mw)) return 3;
    }

    return 0;  // Valid
}

// ============================================================
// Draw helper: dynamic array for 0x14-byte tile records
// ============================================================
// TileRecord: { int col, int row, int flags, uint32_t color, int unused }
// This is the layout expected by fn_TileRender (CharacterComponent_impl_08c0).
// The array header is: { void* data, int count, int capacity } (24 bytes).

struct TileRecord {
    int col;
    int row;
    int flags;
    uint32_t color;
    int unused;
};
static_assert(sizeof(TileRecord) == 0x14, "TileRecord size mismatch");

// A minimal growable array matching the engine's pattern.
// The engine uses {ptr, count|capacity packed into int64} but we
// manage it explicitly for clarity.
struct TileArray {
    TileRecord* data;
    int count;
    int capacity;

    TileArray() : data(nullptr), count(0), capacity(0) {}

    void push(const TileRecord& rec) {
        int newCount = count + 1;
        if (newCount > capacity) {
            int newCap = (count == 0) ? newCount * 3 : newCount * 3;
            TileRecord* newData = (TileRecord*)fn_MemAlloc(
                g_pMemAllocSingleton, (uint64_t)newCap * sizeof(TileRecord), 4);
            if (data && newData) {
                memcpy(newData, data, (uint64_t)count * sizeof(TileRecord));
            }
            if (data) fn_MemFreeStatic(data);
            data = newData;
            capacity = newCap;
        }
        if (data) {
            data[count] = rec;
        }
        count = newCount;
    }

    // Returns the packed {ptr, count, capacity} layout the engine expects:
    // 8 bytes ptr + 4 bytes count + 4 bytes capacity
    void toEngineFormat(void* out) {
        uintptr_t* p = (uintptr_t*)out;
        p[0] = (uintptr_t)data;
        // Pack count and capacity into a single 64-bit value (count in low 32, cap in high 32)
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
// Local function pointers for Draw
// ============================================================

// FUN_140101270: GetCursorCell — computes the cell index under the cursor
typedef int (*fn_GetCursorCell_t)(uint64_t keyMap);
static fn_GetCursorCell_t s_fn_GetCursorCell = nullptr;

// FUN_14009f3e0: CharacterComponent_impl_f3e0 — terrain height query
// Already declared as s_fn_TerrainHeightQuery in CalcCost section above.

static bool s_drawFuncsResolved = false;

static void EnsureDrawHelpers() {
    if (s_drawFuncsResolved) return;
    s_fn_GetCursorCell = reinterpret_cast<fn_GetCursorCell_t>(
        ResolveAddr(Module::Exe, 0x140101270));
    // Ensure CalcCost helpers are resolved too (terrain height, Vector3, ScalarAbs)
    EnsureCalcCostHelpers();
    s_drawFuncsResolved = true;
}

// ============================================================
// FencedTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Two-phase rendering:
//   Phase 1 (edit mode): If self->dialog(+0x10)->editMode(+0x80), renders the
//     existing placed tiles from startCell/endCell stored in the dialog.
//   Phase 2 (preview): While dragging, renders the rectangular preview area with
//     per-tile terrain/height validation (green=valid, red=invalid).
//
// Both phases build arrays of 0x14-byte TileRecords and call fn_TileRender.
// Original: 0x1400dc440
void FencedTool_Draw_Impl(FencedTool* self, void* context, char param3) {
    EnsureDrawHelpers();

    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t dialog = *(uintptr_t*)(selfAddr + 0x10);
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int mapWidth = *mapGrid;

    // ---- Phase 1: Edit mode (existing placed area) ----
    if (*(char*)(dialog + 0x80) != '\0') {
        int storedStart = *(int*)(dialog + 0x98);
        int storedEnd   = *(int*)(dialog + 0x9c);

        int startCol = storedStart / mapWidth;
        int startRow = storedStart % mapWidth;
        int endCol   = storedEnd / mapWidth;
        int endRow   = storedEnd % mapWidth;

        TileArray editTiles;

        for (int r = startCol; r <= endCol; r++) {
            for (int c = startRow; c <= endRow; c++) {
                TileRecord rec;
                rec.col   = c;
                rec.row   = r;
                rec.flags = 0;
                rec.color = 0xffffff00;  // yellow-ish marker
                rec.unused = 0;
                editTiles.push(rec);
            }
        }

        // Build engine format arrays and render
        uint8_t tileArrayBuf[16] = {0};
        uint8_t lineArrayBuf[16] = {0};
        editTiles.toEngineFormat(tileArrayBuf);

        if (fn_TileRender) {
            fn_TileRender(nullptr, context, tileArrayBuf, lineArrayBuf);
        }

        editTiles.free();
    }

    // ---- Phase 2: Preview (only when not param3 and actively dragging) ----
    if (param3 != '\0') return;

    int startCell, endCell;
    if (self->dragState != 0) {
        startCell = self->startCell;
        endCell   = self->endCell;
    } else {
        // Get cell under cursor
        if (s_fn_GetCursorCell) {
            startCell = s_fn_GetCursorCell((uint64_t)***(void****)g_pKeyMapObjectExref);
            endCell = startCell;
        } else {
            return;
        }
    }

    const float HEIGHT_TOL = 100.0f;  // DAT_140172208
    const float HALF_CELL_OFF = 0.5f; // DAT_140172114

    // Convert to col/row
    int col1 = startCell % mapWidth;
    int row1 = startCell / mapWidth;
    int col2 = endCell % mapWidth;
    int row2 = endCell / mapWidth;

    // Sort min/max
    int minCol = col1, maxCol = col2;
    if (col2 < col1) { minCol = col2; maxCol = col1; }
    int minRow = row1, maxRow = row2;
    if (row2 < row1) { minRow = row2; maxRow = row1; }

    int width  = maxCol - minCol + 1;
    int height = maxRow - minRow + 1;

    // Check size constraints from description
    uintptr_t desc = *(uintptr_t*)(selfAddr + 0x38);
    bool tooLarge = false;
    if (width > *(int*)(desc + 0x58) || height > *(int*)(desc + 0x5c)) {
        tooLarge = true;
    }

    // Read terrain info for per-cell validation
    uintptr_t cellDataBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    uint32_t terrainMask = *(uint32_t*)(desc + 0x20);
    uintptr_t allowedEntityType = *(uintptr_t*)(desc + 0x70);

    // Build a placement validity bitmap (using System::String pattern from decompilation)
    // The original builds a string-like structure for the grid. We simplify to direct checks.

    TileArray previewTiles;

    int iterHeight = height;  // decrements as rows advance

    for (int r = minRow; r <= maxRow; r++) {
        for (int c = minCol; c <= maxCol; c++) {
            // Check if this cell was already placed (edit mode overlap flag)
            // The original uses a string-based bitmap. We check directly.
            bool alreadyPlaced = false;
            // (The bitmap logic from the original is complex; for preview purposes,
            //  we skip it — placed tiles are drawn in Phase 1.)

            bool valid = false;
            if (c >= 0 && c < mapWidth && r >= 0 && r < mapWidth) {
                int cellIdx = mapWidth * r + c;
                uintptr_t cellPtr = (uintptr_t)cellIdx * 0x30 + cellDataBase;

                // Terrain type check
                uint8_t terrainByte = *(uint8_t*)(cellPtr + 0x29);
                if ((~terrainMask & (1u << (terrainByte & 0x1f))) != 0) {
                    valid = false;
                } else {
                    // Entity occupancy check
                    uintptr_t cellEntity = *(uintptr_t*)cellPtr;
                    if (allowedEntityType != 0) {
                        if (cellEntity != 0) {
                            if (fn_InternalBasePtr_CheckPtr)
                                fn_InternalBasePtr_CheckPtr((void*)cellPtr);
                            uintptr_t entityTypeRef = *(uintptr_t*)(*(uintptr_t*)cellPtr + 0x28);
                            if (entityTypeRef != allowedEntityType) {
                                valid = false;
                                goto after_check;
                            }
                        }
                    } else {
                        if (cellEntity != 0) {
                            valid = false;
                            goto after_check;
                        }
                    }

                    // Occupied flag check
                    if (!alreadyPlaced && *(uint8_t*)(cellPtr + 0x28) != 0) {
                        valid = false;
                        goto after_check;
                    }

                    // Height validation
                    if (s_fn_TerrainHeightQuery && s_fn_Vector3_Ctor_local && s_fn_ScalarAbs) {
                        float halfOffset = (float)(mapWidth >> 1) - HALF_CELL_OFF;
                        float inPos[4];
                        s_fn_Vector3_Ctor_local(inPos,
                            (float)(cellIdx % mapWidth) - halfOffset,
                            (float)(cellIdx / mapWidth) - halfOffset, 0.0f);
                        float outPos[4] = {0};
                        uint32_t extra[2] = {0};
                        s_fn_TerrainHeightQuery(mapGrid, outPos, inPos, '\0', extra);

                        float heightDiff = outPos[2] - 0.0f;
                        float absSlot = 0.0f;
                        float* absResult = s_fn_ScalarAbs(&heightDiff, &absSlot);
                        if (absResult && *absResult <= HEIGHT_TOL) {
                            valid = true;
                        }
                    } else {
                        valid = true; // Can't validate height, assume ok
                    }
                }
            }
after_check:

            TileRecord rec;
            rec.col   = c;
            rec.row   = r;
            rec.flags = alreadyPlaced ? 2 : 0;
            rec.color = (tooLarge || !valid) ? 0xffff0000u : 0xff00ff00u;
            rec.unused = 0;
            previewTiles.push(rec);
        }
        iterHeight--;
    }

    // Render preview tiles
    uint8_t tileArrayBuf[16] = {0};
    uint8_t lineArrayBuf[16] = {0};
    previewTiles.toEngineFormat(tileArrayBuf);

    if (fn_TileRender) {
        fn_TileRender(nullptr, context, tileArrayBuf, lineArrayBuf);
    }

    previewTiles.free();
}

// ============================================================
// Local function pointers for Startup
// ============================================================

// Graphics::MeshInstance::MeshInstance(this) — dispatch slot 0x140151490
typedef void* (*fn_MeshInstance_Ctor_t)(void* self);
static fn_MeshInstance_Ctor_t s_fn_MeshInstance_Ctor = nullptr;

// Graphics::MeshInstance::SetMesh(this, mesh) — dispatch slot 0x140151498
typedef void (*fn_MeshInstance_SetMesh_t)(void* self, void* mesh);
static fn_MeshInstance_SetMesh_t s_fn_MeshInstance_SetMesh = nullptr;

// MeshInstance vtable override pointer — 0x14015e240
static void** s_MeshInstance_Vtable = nullptr;

// RotationSetting type ID — DAT_1401c63c8
static uint32_t* s_TypeId_RotationSetting = nullptr;

static bool s_startupFuncsResolved = false;

static void EnsureStartupHelpers() {
    if (s_startupFuncsResolved) return;

    // MeshInstance::MeshInstance — dispatch slot 0x140151490
    uintptr_t slot1 = ResolveAddr(Module::Exe, 0x140151490);
    if (slot1) s_fn_MeshInstance_Ctor = *(fn_MeshInstance_Ctor_t*)slot1;

    // MeshInstance::SetMesh — dispatch slot 0x140151498
    uintptr_t slot2 = ResolveAddr(Module::Exe, 0x140151498);
    if (slot2) s_fn_MeshInstance_SetMesh = *(fn_MeshInstance_SetMesh_t*)slot2;

    // MeshInstance vtable override at 0x14015e240
    s_MeshInstance_Vtable = (void**)ResolveAddr(Module::Exe, 0x14015e240);

    // RotationSetting type ID at DAT_1401c63c8
    s_TypeId_RotationSetting = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c63c8);

    s_startupFuncsResolved = true;
}

// ============================================================
// Helper: search entity component list for a specific type ID
// ============================================================
// Returns the first component matching targetTypeId, or nullptr.
// Same pattern as DragPlacementTool's FindComponentByTypeId but local to this TU.
static void* FencedFindComponent(uintptr_t entity, uint32_t targetTypeId) {
    if (!entity || !fn_IsDerivedFrom) return nullptr;

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // vtable slot +0x10: GetTypeId() -> uint32_t
        typedef uint32_t (*fn_GetTypeId_t)(void* self);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                return comp;
            }
            break;
        }
    }
    return nullptr;
}

// ============================================================
// FencedTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Initializes the fenced tool by finding description components on the entity,
// resolving RotationSetting, allocating a MeshInstance for fence preview, and
// setting its mesh from the FenceDescription's model.
//
// Component lookups:
//   1. CreateFencedDescription (DAT_1401c4100) on self->entity -> self->fencedDesc (+0x38)
//   2. AreaDescription (DAT_1401c7a18) on self->entity -> self->areaDesc (+0x40)
//   3. FenceDescription (DAT_1401c7a88) on fencedDesc->entity -> self->fenceDesc (+0x48)
//   4. SizeDescription (DAT_1401c7468) on fencedDesc->entity -> self->_field5C, _field60
//
// Also resolves RotationSetting IBP from toolSetting and allocates MeshInstance.
// Original: 0x1400dcd50
void FencedTool_Startup_Impl(FencedTool* self) {
    EnsureStartupHelpers();

    uintptr_t selfAddr = (uintptr_t)self;

    // 1. Find CreateFencedDescription on self->entity (+0x20)
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x20);
    void* fencedDesc = nullptr;
    if (g_TypeId_CreateFencedDescription) {
        fencedDesc = FencedFindComponent(entity, *g_TypeId_CreateFencedDescription);
    }
    self->fencedDesc = fencedDesc;
    // SA guard (retail would fault): without the CreateFencedDescription every
    // later step derefs null+0x28. Reachable when the 0x1401c4100 type-id
    // storage is unseeded (reg_pipeline_typeids.cpp) or the template lacks a
    // createfenced block. Trace loudly — a silent bail here cost a crash-debug.
    if (!fencedDesc) {
        fprintf(stderr, "[FencedTool] Startup: no CreateFencedDescription on entity=%p "
                "(tid ptr=%p) — tool left un-wired\n", (void*)entity,
                (void*)g_TypeId_CreateFencedDescription);
        return;
    }

    // 2. Find AreaDescription on self->entity (+0x20)
    entity = *(uintptr_t*)(selfAddr + 0x20);
    void* areaDesc = nullptr;
    if (g_TypeId_AreaDescription) {
        areaDesc = FencedFindComponent(entity, *g_TypeId_AreaDescription);
    }
    self->areaDesc = areaDesc;

    // 3. Find FenceDescription on fencedDesc->entity (+0x28)
    uintptr_t fencedDescEntity = *(uintptr_t*)((uintptr_t)self->fencedDesc + 0x28);
    void* fenceDesc = nullptr;
    if (g_TypeId_FenceDescription) {
        fenceDesc = FencedFindComponent(fencedDescEntity, *g_TypeId_FenceDescription);
    }
    self->fenceDesc = fenceDesc;

    // 4. Resolve RotationSetting from toolSetting IBP (+0x18)
    //    CheckPtr on the IBP first, then IsDerivedFrom, then construct new IBP at +0x50
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x18));
    }
    void* toolSettingVal = *(void**)(selfAddr + 0x18);
    void* rotSetting = nullptr;
    if (s_TypeId_RotationSetting && fn_IsDerivedFrom && toolSettingVal) {
        bool isDerived = fn_IsDerivedFrom(toolSettingVal, *s_TypeId_RotationSetting);
        if (isDerived) {
            rotSetting = toolSettingVal;
        }
    }
    // Construct InternalBasePtr at +0x50 with the resolved RotationSetting
    if (fn_InternalBasePtr_Ctor) {
        void* localIBP = nullptr;
        fn_InternalBasePtr_Ctor(&localIBP, rotSetting);
        *(void**)(selfAddr + 0x50) = localIBP;
    }

    // 5. Find SizeDescription on fencedDesc->entity -> read +0x44 and +0x48
    uintptr_t fencedDescEntity2 = *(uintptr_t*)((uintptr_t)self->fencedDesc + 0x28);
    void* sizeDesc1 = nullptr;
    if (g_TypeId_SizeDescription) {
        sizeDesc1 = FencedFindComponent(fencedDescEntity2, *g_TypeId_SizeDescription);
    }
    // Read field +0x44 from SizeDescription -> self->_field5C
    self->_field5C = *(uint32_t*)((uintptr_t)sizeDesc1 + 0x44);

    // Find SizeDescription again for +0x48 (original code does two separate lookups)
    uintptr_t fencedDescEntity3 = *(uintptr_t*)((uintptr_t)self->fencedDesc + 0x28);
    void* sizeDesc2 = nullptr;
    if (g_TypeId_SizeDescription) {
        sizeDesc2 = FencedFindComponent(fencedDescEntity3, *g_TypeId_SizeDescription);
    }
    // Read field +0x48 from SizeDescription -> self->_field60
    self->_field60 = *(uint32_t*)((uintptr_t)sizeDesc2 + 0x48);

    // 6. Allocate MeshInstance (0x120 bytes, alignment 8)
    void* meshMem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x120, 8) : nullptr;
    void* meshInst = nullptr;
    if (meshMem) {
        // Construct MeshInstance
        if (s_fn_MeshInstance_Ctor) {
            s_fn_MeshInstance_Ctor(meshMem);
        }
        // Override vtable to 0x14015e240
        if (s_MeshInstance_Vtable) {
            *(void**)meshMem = s_MeshInstance_Vtable;
        }
        meshInst = meshMem;
    }
    self->meshInstance = meshInst;

    // 7. Set mesh from FenceDescription's model list (last entry)
    //    fenceDesc->+0x20 is a model array: +0x08 = data ptr, +0x10 = count
    uintptr_t modelArray = *(uintptr_t*)((uintptr_t)self->fenceDesc + 0x20);
    int modelCount = *(int*)(modelArray + 0x10);
    void** modelData = *(void***)(modelArray + 0x08);
    void* lastMesh = modelData[modelCount - 1];  // last entry

    if (s_fn_MeshInstance_SetMesh && meshInst) {
        s_fn_MeshInstance_SetMesh(meshInst, lastMesh);
    }

    // 8. UpdateHierarchy — tail call in original (JMP to dispatch slot)
    if (fn_MeshInstance_UpdateHierarchy && self->meshInstance) {
        fn_MeshInstance_UpdateHierarchy(self->meshInstance);
    }
}

// ============================================================
// FencedTool::CalcPath (vtable +0x68, Slot13)
// ============================================================
// Creates fence entities along the drag rectangle perimeter.
//
// This function is extremely complex (~150 lines decompiled) and depends on
// ~15 unresolved helper functions:
//   - FUN_1400dbf90: anchor/rotation calculation (already resolved as s_fn_FencedTool_AnchorCalc)
//   - ComponentEntity_CopyCtor: entity deep-copy
//   - FUN_140004680: entity component accessor
//   - FUN_140004740: entity component accessor (different type)
//   - FUN_1401018d0: set cell range on entity
//   - Entity_ClearPathQueue: clear entity path queue
//   - Math::Transform::FromYawPitchOffset: build rotation transform
//   - Math::Transform::operator=: copy transform
//   - Graphics::Geometry::SetTransform: apply transform to geometry
//   - FencedTool_helper_90d0: link fence entity to main entity
//   - FUN_140003110: append entity to internal list
//   - FUN_1400c48a0: register entity with world entity list
//   - FencedTool_Slot13_sub_bc80: place corner fence posts (4 calls)
//   - FencedTool_Slot13_sub_b8c0: place side fence segments (4 calls)
//
// PORTED 2026-07-30: full reimplementation in fencedtool_calcpath.cpp (master
// entity + shelter + corner/edge/gate fence pieces via MultiObject). The old
// body here delegated to ResolveAddr(0x1400dae00) which was never registered,
// so committing a fenced drag was a silent no-op.
//
// Original: 0x1400dae00
extern "C" uint64_t FencedTool_CalcPath_Port(FencedTool* self, int64_t startCell,
                                             int64_t endCell);

uint64_t FencedTool_CalcPath_Impl(FencedTool* self, uint64_t startCell, uint64_t endCell) {
    return FencedTool_CalcPath_Port(self, static_cast<int64_t>(startCell),
                                    static_cast<int64_t>(endCell));
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// FencedTool: size 0x70, parent DragTool, 15-slot vtable @ 0x1401605d0.
// ============================================================
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FencedTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("FencedTool");
    return c;
}
// +0x58 (Slot11) GetToolSettingTypeId — retail 0x140026920 writes
// TypeInfo_RotationSetting through the out param (FencedTool HAS a tool
// setting: the rotation the shelter edge follows). SetEditMode DEREFERENCES
// the returned pointer (the BridgeTool slot-11 lesson — a Tool_Ret0 stub here
// faulted the instant the fenced node activated, EntityUI_SetEditMode+0x2e0).
// The .rdata type-id global is null in standalone, so resolve by name; the
// non-zero id also makes SetEditMode build the RotationSetting that
// FencedTool_Startup wires to +0x50 — dbf90's rotation source.
static uint32_t* FencedTool_GetSettingType(void*, uint32_t* ret) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RotationSetting");
    if (ret) *ret = c;
    return ret;
}
// +0x60 (Slot12) cursor id — retail 0x1400fc9d0 `return 5` (shared with Road/Bridge).
static int FencedTool_Slot12(void*) { return 5; }
static void* g_FencedTool_Vtable[15] = {
    (void*)&FencedTool_Dtor,          // +0x00 Dtor
    (void*)&Tool_Slot1,               // +0x08
    (void*)&FencedTool_GetTypeId,     // +0x10 GetTypeId
    (void*)&Tool_Persist,             // +0x18 Serialize (base)
    (void*)&FencedTool_Startup_Impl,  // +0x20 Startup
    (void*)&FencedTool_Shutdown2,     // +0x28 Shutdown2
    (void*)&FencedTool_OnKey,         // +0x30 OnKey
    (void*)&FencedTool_Serialize_Impl,// +0x38 Serialize (preview mesh)
    (void*)&FencedTool_Draw_Impl,     // +0x40 Draw
    (void*)&FencedTool_Shutdown,      // +0x48 Shutdown
    (void*)&Tool_Ret0,                // +0x50 retail 0x1400268a0 = shared `return 0`
    (void*)&FencedTool_GetSettingType,// +0x58 Slot11 GetToolSettingTypeId (RotationSetting)
    (void*)&FencedTool_Slot12,        // +0x60 cursor id (retail returns 5)
    (void*)&FencedTool_CalcPath_Impl, // +0x68 CalcPath
    (void*)&FencedTool_CalcCost_Impl, // +0x70 CalcCost
};
extern "C" void RegisterFencedToolVtable() {
    RegisterExeAddr(0x1401605d0, (uintptr_t)g_FencedTool_Vtable);
}
extern "C" void* FencedTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x70, g_FencedTool_Vtable, 1);}
extern "C" void* FencedTool_Factory_SA(void) {
    return Tool_Alloc(0x70, g_FencedTool_Vtable, 1);
}
REGISTER_TYPE(FencedTool, "DragTool", &FencedTool_Factory_SA, &FencedTool_CreateInPlace_SA);
