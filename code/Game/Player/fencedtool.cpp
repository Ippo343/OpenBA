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
// calculation) and stay as trampolines.
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

            // Build transform from yaw with zero offset
            uint8_t localTransform[96];
            memset(localTransform, 0, sizeof(localTransform));
            uint32_t zeroVec = 0;

            if (fn_Transform_FromYawPitchOffset) {
                fn_Transform_FromYawPitchOffset(localTransform, outYaw, &zeroVec);
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

// Math::Scalar::Abs
typedef float* (*fn_ScalarAbs_t)(float* val);
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
            ResolveExport("?Abs@Scalar@Math@@SA?AV12@V12@@Z"));
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
                float* absResult = s_fn_ScalarAbs(&heightDiff);
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
