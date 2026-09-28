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

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include "Player/FencedTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "Player/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"
#include "object/TypeInfo.h"
#include "Player/RotationSetting.h"   // ENDGAME B.3.9 CB: 0x1401c63c8 cell class
#include "Component/CreateFencedDescription.h"
#include "Component/MapDescription.h"
#include "Component/ModelDescription.h"
#include "Component/CreatePlacedDescription.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)


// ============================================================
// FencedTool::Shutdown2 (vtable +0x28, Slot5)
// ============================================================
// Removes the fence preview from the scene and destroys the mesh instance.
// Original: 0x1400dccc0
void FencedTool::Shutdown() {
    // Remove scene object if added
    if ((int)sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x60);
        if (scene) {
            reinterpret_cast<Graphics::Scene*>(scene)
                ->RemoveObject(sceneObjectId);   // B.3.5 bridge
        }
    }

    // Destroy mesh instance via vtable dtor if allocated
    void* mesh = meshInstance;
    if (mesh) {
        delete reinterpret_cast<Graphics::MeshInstance*>(mesh);   // B.3.x bridge (slot 0, free)
    }
}

// ============================================================
// Helper: get KeyMap from external reference
// ============================================================
static inline void* GetKeyMap() {
    return ***(void****)g_pInputConfigSingletonExref;
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
// DragTool::OnKey is our own port (dragtool.cpp) — call it by symbol, exactly
// like BridgeTool/RoadTool wire it into their vtables. The old call went
// through fn_DragTool_OnKey = ResolveAddr(0x1400fc6b0), which was never
// registered, so the base drag NEVER ran: mouse-down couldn't set dragState
// and the preview stayed a single cursor-cell box (found 2026-07-30 manual test).
#include "Player/dragtool.h"

char FencedTool::OnKey(char key) {
    // Call base DragTool::OnKey
    DragTool::OnKey(key);

    void* keyMap = GetKeyMap();

    // Check rotate counter-clockwise (key 0x13)
    bool rotLeft = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x13);  // B.3.7 bridge
    if (rotLeft) {
        // Access rotationSetting IBP at self+0x50
        void* rotSettingIBP = (void*)&rotationSetting;
        uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
        if (!rotSetting) return 0;   // SA guard: no RotationSetting wired
        *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) - 1;

        rotSetting = *(uintptr_t*)rotSettingIBP;
        if (*(int*)(rotSetting + 0x10) < 0) {
            rotSetting = *(uintptr_t*)rotSettingIBP;
            *(int*)(rotSetting + 0x10) = 3;
        }
    } else {
        // Check rotate clockwise (key 0x14)
        bool rotRight = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x14);  // B.3.7 bridge
        if (rotRight) {
            void* rotSettingIBP = (void*)&rotationSetting;
            uintptr_t rotSetting = *(uintptr_t*)rotSettingIBP;
            if (!rotSetting) return 0;   // SA guard: no RotationSetting wired
            *(int*)(rotSetting + 0x10) = *(int*)(rotSetting + 0x10) + 1;

            rotSetting = *(uintptr_t*)rotSettingIBP;
            if (*(int*)(rotSetting + 0x10) > 3) {
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
// Removes scene object if added, resets ID to -1, then TAIL-JUMPS to the shared
// DragTool cancel-drag (0x1400fc640) — so its return value IS that function's:
// non-zero = "I cancelled my drag, keep me alive", zero = "OK to tear me down".
// EntityUI_ToolTick_SA reads that byte to decide whether a right-click destroys the
// tool. This was declared `void` and only forwarded through the fn_DragTool_Shutdown
// pointer, which resolves NULL in standalone (0x1400fc640 was never registered) — so
// mid-drag the fence drag was never cancelled, the CreateBar dialog was never closed,
// and the caller read a garbage EAX for the keep-alive decision.
// Original: 0x1400dcd10
char FencedTool::OnCancel() {
    // Remove scene object if added
    if ((int)sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x60);
        if (scene) {
            reinterpret_cast<Graphics::Scene*>(scene)
                ->RemoveObject(sceneObjectId);   // B.3.5 bridge
        }
        sceneObjectId = (uint32_t)-1;
    }

    // Tail-call the shared DragTool cancel-drag (retail `JMP 0x1400fc640`) and return
    // its result: the qualified (direct) call to DragTool::OnCancel, the body at
    // 0x1400fc640.
    return DragTool::OnCancel();
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
void FencedTool::Update(void* /*param2*/, char param3) {
    // Initialize local offset vector to {0, 0, 0}
    float offset[3] = {0.0f, 0.0f, 0.0f};

    if (param3 != 0 && dragState != 0) {
        // Placement parameters — outputs from FencedPlacementParams
        uint32_t outParam2[2] = {0, 0};
        uint32_t outParam3 = 0;
        uint32_t outParam4 = 0;
        uint32_t outParam5[2] = {0, 0};
        uint32_t outYaw[8];  // Scalar data for yaw/orientation
        memset(outYaw, 0, sizeof(outYaw));

        // 0x1400dbf90 returns uint64_t; the door read AL as the bool.
        bool valid = static_cast<uint8_t>(PlayerEdit_FencedAnchorCalc_dbf90(
            reinterpret_cast<uintptr_t>(this), outParam2, &outParam3, &outParam4, outParam5,
            offset, outYaw)) != 0;

        if (valid) {
            // Add mesh to scene if not already added
            if ((int)sceneObjectId < 0) {
                void* scene = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x60);
                if (scene && meshInstance) {
                    sceneObjectId = (int32_t)reinterpret_cast<Graphics::Scene*>(scene)
                        ->AddObject(reinterpret_cast<Graphics::Geometry*>(meshInstance));   // B.3.5 bridge
                }
            }

            // Build transform from yaw with zero pitch + zero offset.
            // Real signature: FromYawPitchOffset(retval, &yaw, &pitch, &offset)
            uint8_t localTransform[96];
            memset(localTransform, 0, sizeof(localTransform));
            float zeroPitch = 0.f;

            // R9 = `offset`, the world position FencedPlacementParams just wrote --
            // NOT a zero vector. Retail 0x1400dcb55 loads R9 from [rsp+0x50], which is
            // the very same buffer (r11-0x38) it passed as arg 6 to the params call at
            // 0x1400dcb25. Passing a separate zeroed vector here pinned the fence/gate
            // ghost mesh at world (0,0,0) instead of under the cursor -- the map origin
            // is water, so the preview appeared to be standing in a random lake.
            // PlacementTool's equivalent (0x1400fdff3) already carried a comment saying
            // exactly this; the fenced path was the one that had it wrong.
            *reinterpret_cast<Math::Transform*>(localTransform) =
                Math::Transform::FromYawPitchOffset(
                    *reinterpret_cast<const Math::Scalar*>(outYaw), zeroPitch,
                    *reinterpret_cast<const Math::Vector3*>(offset));

            {
                static int n = 0;
                if (n < 12) { ++n;
                    fprintf(stderr, "[FencedGhost] preview transform: offset=(%.2f, %.2f, %.2f)\n",
                            offset[0], offset[1], offset[2]);
                    fflush(stderr);
                }
            }

            // Set transform on mesh
            if (meshInstance) {
                reinterpret_cast<Graphics::Geometry*>(meshInstance)
                    ->SetTransform(*reinterpret_cast<const Math::Transform*>(localTransform));   // B.3.5 bridge
            }

            // Update mesh hierarchy
            if (meshInstance) {
                reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
                    ->UpdateHierarchy();   // B.3.5 bridge
            }

            return;
        }
    }

    // Remove mesh from scene if present
    if ((int32_t)sceneObjectId >= 0) {
        void* scene = *(void**)((uintptr_t)(GameUpdateLoop::_singleton) + 0x60);
        if (scene) {
            reinterpret_cast<Graphics::Scene*>(scene)
                ->RemoveObject((uint32_t)sceneObjectId);   // B.3.5 bridge
        }
        sceneObjectId = -1;
    }
}

// ============================================================
// CalcCost helpers, called by name:
//   0x14009f3e0 Reimpl_MapGetTerrainPos (Game/exe_bodies.h) -- terrain height query
//   0x1400dbf90 PlayerEdit_FencedAnchorCalc_dbf90 (Player/exe_bodies.h) --
//               anchor/rotation adjustment
// ============================================================

static constexpr float HALF_CELL = 0.5f;
static constexpr float HEIGHT_TOLERANCE = 100.0f;

// ============================================================
// FencedTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Validates a rectangular fence region between startCell and endCell.
// Returns: 0=valid, 1=too small, 2=too large, 3=blocked
// Original: 0x1400db490
int FencedTool::CalcCost(int startCell, int endCell,
                         uint32_t* outWidth, int* outHeight, int* outDepth,
                         int* outFenceSegments) {
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;

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
    uintptr_t desc = (uintptr_t)fencedDesc;
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
                    uintptr_t entityTypeRef = *(uintptr_t*)(*(uintptr_t*)cellPtr + 0x28);
                    if (entityTypeRef != allowedEntityType) { allValid = false; break; }
                }
            } else {
                if (cellEntity != 0) { allValid = false; break; }
            }

            // Check occupied flag
            if (*(uint8_t*)(cellPtr + 0x28) != 0) { allValid = false; break; }

            // Height validation
            {
                float halfOffset = (float)(mapWidth >> 1) - HALF_CELL;
                float inPos[4] = {0};
                *reinterpret_cast<Math::Vector3*>(inPos) = Math::Vector3(
                    (float)(cellIdx % mapWidth) - halfOffset,
                    (float)(cellIdx / mapWidth) - halfOffset, 0.0f);
                float outPos[4] = {0};
                uint32_t extra[2] = {0};
                Reimpl_MapGetTerrainPos(mapGrid, outPos, inPos, '\0', extra);

                if (!heightInitialized) { baseHeight = outPos[2]; heightInitialized = true; }
                Math::Scalar heightDiff(outPos[2] - baseHeight);
                if (HEIGHT_TOLERANCE < heightDiff.Abs().v) { allValid = false; break; }
            }
        }
    }

    if (!allValid) return 3;  // Blocked

    // Anchor/rotation adjustment
    uint32_t anchorMinCol = minCol, anchorMinRow = minRow;
    uint32_t anchorMaxCol = effectiveMaxCol, anchorMaxRow = effectiveMaxRow;
    float anchorPos[3] = {0};

    uint64_t anchorResult = PlayerEdit_FencedAnchorCalc_dbf90(
        reinterpret_cast<uintptr_t>(this), &anchorMinCol, &anchorMinRow, &anchorMaxCol, &anchorMaxRow,
        anchorPos, &anchorMinCol);
    if ((char)anchorResult == '\0') return 3;

    // Check anchor constraints if in edit mode
    uintptr_t toolState = (uintptr_t)dialogPtr;
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
            TileRecord* newData = (TileRecord*)System::MemoryAllocator::MemAlloc(
                (uint64_t)newCap * sizeof(TileRecord), 4);
            if (data && newData) {
                memcpy(newData, data, (uint64_t)count * sizeof(TileRecord));
            }
            if (data) System::MemoryAllocator::MemFree(data);
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
        if (data) System::MemoryAllocator::MemFree(data);
        data = nullptr;
        count = 0;
        capacity = 0;
    }
};

// ============================================================
// Draw helpers: 0x140101270 PlayerRay_ScreenToCell_101270 (the cell under the
// cursor), 0x14009f3e0 Reimpl_MapGetTerrainPos (terrain height query),
// 0x1401008c0 PlayerTileRender_DrawOverlay_08c0 (the tile overlay submit).
// ============================================================

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
void FencedTool::Draw(void* context, char param3) {
    uintptr_t dialog = (uintptr_t)dialogPtr;
    int* mapGrid = *(int**)((uintptr_t)GameUpdateLoop::_singleton + 0xf8);
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

        PlayerTileRender_DrawOverlay_08c0(0, context, reinterpret_cast<int64_t*>(tileArrayBuf),
                                          reinterpret_cast<int64_t*>(lineArrayBuf));

        editTiles.free();
    }

    // ---- Phase 2: Preview (only when not param3 and actively dragging) ----
    if (param3 != '\0') return;

    int startCell, endCell;
    if (dragState != 0) {
        startCell = this->startCell;
        endCell   = this->endCell;
    } else {
        // Get cell under cursor
        startCell = PlayerRay_ScreenToCell_101270((uintptr_t)***(void****)g_pInputConfigSingletonExref);
        endCell = startCell;
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
    uintptr_t desc = (uintptr_t)fencedDesc;
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
                    {
                        float halfOffset = (float)(mapWidth >> 1) - HALF_CELL_OFF;
                        float inPos[4] = {0};
                        *reinterpret_cast<Math::Vector3*>(inPos) = Math::Vector3(
                            (float)(cellIdx % mapWidth) - halfOffset,
                            (float)(cellIdx / mapWidth) - halfOffset, 0.0f);
                        float outPos[4] = {0};
                        uint32_t extra[2] = {0};
                        Reimpl_MapGetTerrainPos(mapGrid, outPos, inPos, '\0', extra);

                        Math::Scalar heightDiff(outPos[2] - 0.0f);
                        if (heightDiff.Abs().v <= HEIGHT_TOL) {
                            valid = true;
                        }
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

    PlayerTileRender_DrawOverlay_08c0(0, context, reinterpret_cast<int64_t*>(tileArrayBuf),
                                      reinterpret_cast<int64_t*>(lineArrayBuf));

    previewTiles.free();
}

// ============================================================
// Local function pointers for Startup
// ============================================================


// ============================================================
// Helper: search entity component list for a specific type ID
// ============================================================
// Returns the first component matching targetTypeId, or nullptr.
// Same pattern as DragPlacementTool's FindComponentByTypeId but local to this TU.
static void* FencedFindComponent(uintptr_t entity, uint32_t targetTypeId) {
    if (!entity) return nullptr;

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // virtual slot 2 (+0x10): Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == targetTypeId) {
            if (Object::IsDerivedFromNullSafe(comp, targetTypeId)) {
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
//   1. CreateFencedDescription (cell 0x1401c4100) on self->entity -> self->fencedDesc (+0x38)
//   2. MapDescription (cell 0x1401c7a18; global "AreaDescription" lies — D5) on
//      self->entity -> self->areaDesc (+0x40)
//   3. ModelDescription (cell 0x1401c7a88; global "FenceDescription" lies) on
//      fencedDesc->entity -> self->fenceDesc (+0x48)
//   4. CreatePlacedDescription (cell 0x1401c7468; global "SizeDescription" lies) on
//      fencedDesc->entity -> self->_field5C, _field60
//
// Also resolves RotationSetting IBP from toolSetting and allocates MeshInstance.
// Original: 0x1400dcd50
void FencedTool::Startup() {

    // 1. Find CreateFencedDescription on self->entity (+0x20)
    uintptr_t entity = (uintptr_t)descriptionPtr;
    void* fencedDesc = FencedFindComponent(entity, CreateFencedDescription::TypeId());
    this->fencedDesc = fencedDesc;
    // SA guard (retail would fault): without the CreateFencedDescription every
    // later step derefs null+0x28. Reachable when the 0x1401c4100 type-id
    // storage is unseeded (reg_pipeline_typeids.cpp) or the template lacks a
    // createfenced block. Trace loudly — a silent bail here cost a crash-debug.
    if (!fencedDesc) {
        fprintf(stderr, "[FencedTool] Startup: no CreateFencedDescription on entity=%p "
                "(tid=0x%x) — tool left un-wired\n", (void*)entity,
                CreateFencedDescription::TypeId());
        return;
    }

    // 2. Find MapDescription on self->entity (+0x20)
    entity = (uintptr_t)descriptionPtr;
    void* areaDesc = FencedFindComponent(entity, MapDescription::TypeId());
    this->areaDesc = areaDesc;

    // 3. Find ModelDescription on fencedDesc->entity (+0x28)
    uintptr_t fencedDescEntity = *(uintptr_t*)((uintptr_t)this->fencedDesc + 0x28);
    void* fenceDesc = FencedFindComponent(fencedDescEntity, ModelDescription::TypeId());
    this->fenceDesc = fenceDesc;

    // 4. Resolve RotationSetting from toolSetting IBP (+0x18)
    //    CheckPtr on the IBP first, then IsDerivedFrom, then construct new IBP at +0x50
    void* toolSettingVal = toolSetting;
    void* rotSetting = nullptr;
    if (toolSettingVal) {
        bool isDerived = Object::IsDerivedFromNullSafe(toolSettingVal, RotationSetting::TypeId());   // cell 0x1401c63c8
        if (isDerived) {
            rotSetting = toolSettingVal;
        }
    }
    // Construct InternalBasePtr at +0x50 with the resolved RotationSetting
    {
        void* localIBP = nullptr;
        new (&localIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(rotSetting));   // B.3.3 bridge
        rotationSetting = localIBP;
    }

    // 5. Find CreatePlacedDescription on fencedDesc->entity -> read +0x44 and +0x48
    uintptr_t fencedDescEntity2 = *(uintptr_t*)((uintptr_t)this->fencedDesc + 0x28);
    void* sizeDesc1 = FencedFindComponent(fencedDescEntity2, CreatePlacedDescription::TypeId());
    // Read field +0x44 from CreatePlacedDescription -> self->_field5C
    _field5C = *(uint32_t*)((uintptr_t)sizeDesc1 + 0x44);

    // Find CreatePlacedDescription again for +0x48 (original code does two separate lookups)
    uintptr_t fencedDescEntity3 = *(uintptr_t*)((uintptr_t)this->fencedDesc + 0x28);
    void* sizeDesc2 = FencedFindComponent(fencedDescEntity3, CreatePlacedDescription::TypeId());
    // Read field +0x48 from CreatePlacedDescription -> self->_field60
    _field60 = *(uint32_t*)((uintptr_t)sizeDesc2 + 0x48);

    // 6. Allocate MeshInstance (0x120 bytes, alignment 8)
    void* meshMem = System::MemoryAllocator::MemAlloc(0x120, 8);
    void* meshInst = nullptr;
    if (meshMem) {
        // Retail: `??0MeshInstance` then the exe's copy 0x14015e240 of the class's
        // own table (not a subclass) = `new MeshInstance()`.
        ::new (meshMem) Graphics::MeshInstance();   // B.3.9 EXA: the real class (exe table 0x14015e240 is its own)
        meshInst = meshMem;
    }
    meshInstance = meshInst;

    // 7. Set mesh from FenceDescription's model list (last entry)
    //    fenceDesc->+0x20 is a model array: +0x08 = data ptr, +0x10 = count
    uintptr_t modelArray = *(uintptr_t*)((uintptr_t)this->fenceDesc + 0x20);
    int modelCount = *(int*)(modelArray + 0x10);
    void** modelData = *(void***)(modelArray + 0x08);
    void* lastMesh = modelData[modelCount - 1];  // last entry

    if (meshInst) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInst)
            ->SetMesh(Graphics::ExternalPtrHandle{lastMesh});   // B.3.5 bridge
    }

    // 8. UpdateHierarchy — tail call in original (JMP to dispatch slot)
    if (meshInstance) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->UpdateHierarchy();   // B.3.5 bridge
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
// ENDGAME B.3.8 callers: the body is FencedTool::CalcPath (fencedtool_calcpath.cpp).

// ============================================================
// FencedTool members (FencedTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_FencedTool_Vtable[15], its slot stubs, the _SA factories
// and RegisterFencedToolVtable are gone: the compiler builds the table, and
// nothing reads 0x1401605d0 back by address (the old dtor was the only
// reader). Inherited from the bases, as in retail: [10] false, [12] cursor 5.
// ============================================================
#include "Object/factory_registration.h"
#include "Player/RotationSetting.h"
#include <new>

// [4] Startup, [5] Shutdown, [6] OnKey, [7] Update, [8] Draw, [9] OnCancel and
// [14] CalcCost are defined above; [13] CalcPath is in fencedtool_calcpath.cpp.

// [11] 0x140026920 `MOV EAX,[TypeInfo_RotationSetting]; MOV [RDX],EAX; MOV
// RAX,RDX; RET`: FencedTool HAS a tool setting — the rotation the shelter edge
// follows; SetEditMode builds it from this id and Startup wires it to +0x50.
// The old stub resolved the id through a function-local lazy cache; the class
// static reads the registration record written once at boot (B6-5).
Object::Type* FencedTool::GetSettingType(Object::Type* ret) const {
    *ret = Object::Type(RotationSetting::TypeId());
    return ret;
}

// create 0x1400268d0: MemAlloc(0x70, 8), then the ctor 0x1400dd070. The ctor
// sets sceneObjectId = -1 ("preview not in the scene"): the tree's zero-fill
// once turned that into a live id 0 (the start-town barn's mesh) and
// Scene::RemoveObject(0) spun forever.
void* FencedTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FencedTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FencedTool());
}
// createInPlace 0x140026910: `if (storage) ctor(storage)`.
void* FencedTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FencedTool());
}

DEFINE_EXE_TYPE(FencedTool, "DragTool")
