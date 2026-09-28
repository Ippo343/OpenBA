// code/Game/Player/placementtool.cpp
// Clean reimplementation of PlacementTool vtable methods from Application-x64.exe.
//
// PlacementTool (0x70 bytes) extends Tool (0x28 bytes) directly (NOT DragTool).
// Handles building placement: rotation, cost preview, mesh rendering.
//
// Vtable at 0x140161d20 (13 slots). Currently reimplemented here:
//   Slot0  Dtor       (0x14002dc90) — scalar deleting destructor
//   Slot5  Shutdown2  (0x1400fe4e0) — cleanup scene objects + mesh + dialog
//   Slot6  OnKey      (0x1400fe2a0) — rotation keys + placement input
//   Slot7  Serialize  (0x1400fdee0) — placement logic + cost UI update
//
// Decompiled from Application-x64.exe.

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include <new>
#include "Player/PlacementTool.h"
#include "Player/RotationSetting.h"
#include "Component/CreatePlacedDescription.h"
#include "Component/MapDescription.h"
#include "Component/ModelDescription.h"
#include "Component/RadiusDescription.h"
#include "PlacementTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "system/String.h"
#include "system/Value.h"   // ENDGAME B.3.4b System callers: System::Random
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "object/Internal.h"    // ENDGAME B.3.6 UI callers (K4)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "input/Input.h"        // ENDGAME B.3.7 callers (K3)
#include "Player/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"
#include "system/MemoryAllocator.h"
#include "object/TypeInfo.h"

// ============================================================
// Exe helpers, called by name:
//   0x1400b0d00 DecalComponent_TerrainDecalRemove (Component/exe_bodies.h) --
//               the highlight/overlay removal
//   0x1400fd960 PlacementToolPlace_FlipVariant_d960, 0x1400fda00
//   PlayerRay_PlacementPosCalc_fda00, 0x1400fd650 ToolTile_ValidateCreate_fd650,
//   0x14009d580 ToolTile_FootprintValidity_d580, 0x1400ffc60 UpdateToolUI_fc60,
//   0x140005160 DragTool_FindDescription_5160, 0x1400ffbf0 DragTool_PlayTickSound_ffbf0,
//   0x1401000f0 DragTool_PlayPlaceSound_1000f0, 0x140100140 DragTool_PlayErrorSound_100140,
//   0x1400ff9e0 ToolUI_OpenToolDialog_ff9e0, 0x140055430 Player_HighlightAdd_55430,
//   0x140101380 PlayerTileRender_BuildRecords_1380 (Player/exe_bodies.h)
//   0x14009f3e0 Reimpl_MapGetTerrainPos (Game/exe_bodies.h)
// Import slot 0x140152a90 is Object::InternalBasePtr's ctor (placement new).
// The bodies' return types are wider than the bool / char the old door typedefs
// read: each call reads the low byte (AL), as the door did.
// ============================================================

// DAT_140172248 (.rdata): the float the tile-validity / tile-render calls pass as
// their second float. The registered cell holds 0x447a0000 = 1000.0f (NOT the 0.5
// an older comment here claimed); that is the value these calls have read.
static inline float Rdata_140172248() {
    float f;
    memcpy(&f, &kRdata_140172248, sizeof f);
    return f;
}

// ============================================================
// Helper: get KeyMap object from external reference (triple deref)
// ============================================================
static inline void* GetKeyMap() {
    return Input::Configuration::_singleton->_keyMap;
}

// ============================================================
// Helper: get GameWorld singleton pointer
// ============================================================
static inline uintptr_t GetGameWorld() {
    return (uintptr_t)GameUpdateLoop::_singleton;
}

// ============================================================
// PlacementTool::Shutdown2 (vtable +0x28, Slot5)
// ============================================================
// Removes placement preview from the scene, destroys the mesh instance,
// and closes the associated dialog.
// Original: 0x1400fe4e0
void PlacementTool::Shutdown() {
    // Remove scene object if added (sceneObjectIdx >= 0)
    if ((int)sceneObjectIdx >= 0) {
        void* scene = *(void**)(GetGameWorld() + 0x60);
        if (scene) {
            reinterpret_cast<Graphics::Scene*>(scene)
                ->RemoveObject(sceneObjectIdx);   // B.3.5 bridge
        }
    }

    // Remove radius display highlight if active (radiusDisplayIdx >= 0)
    if (radiusDisplayIdx >= 0) {
        void* highlightMgr = *(void**)(GetGameWorld() + 0x110);
        if (highlightMgr) {
            DecalComponent_TerrainDecalRemove(highlightMgr, radiusDisplayIdx);
        }
    }

    // Destroy mesh instance via vtable dtor if allocated
    void* mesh = meshInstance;
    if (mesh) {
        delete reinterpret_cast<Graphics::MeshInstance*>(mesh);   // B.3.x bridge (slot 0, free)
    }

    // Close dialog if open: dialogPtr (+0x10) points to an InternalBasePtr
    uintptr_t* ibpPtr = (uintptr_t*)dialogPtr;
    uintptr_t ibpVal = *ibpPtr;
    if (ibpVal != 0) {
        // Close the dialog via UI::UserInterface
        void* uiMgr = *(void**)(GetGameWorld() + 0x98);
        if (uiMgr) {
            uintptr_t localIBP = ibpVal;
            reinterpret_cast<UI::UserInterface*>(uiMgr)->CloseDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&localIBP));
        }

        // Clear the InternalBasePtr and write back
        uintptr_t localIBP = ibpVal;
        new (&localIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *ibpPtr = localIBP;
    }
}

// ============================================================
// PlacementTool::OnKey (vtable +0x30, Slot6)
// ============================================================
// Main input handler for building placement tool.
//
// If param2 == 0: close dialog and clear dialog IBP
// If param2 != 0 and dialog not open: open dialog via helper
// Then check KeyMap for rotation/flip/placement keys:
//   Key 0x13: rotate CCW (decrement rotation, wrap <0 to 3)
//   Key 0x14: rotate CW (increment rotation, wrap >3 to 0)
//   Key 0x15: flip/mirror via sub_d960
//   Key 0x05: placement confirmation (if param2 != 0)
// Always returns 1.
//
// Disassembly reference: 0x1400fe2a0
// IAT slots used:
//   [0x140151550] = UI::UserInterface::CloseDialog
//   [0x140152a90] = InternalBasePtr::Ctor
//   [0x140151758] = KeyMap::Pressed
//   [0x140152ab0] = InternalBasePtr::CheckPtr
// Direct calls:
//   0x1400ff9e0 = dialog open helper
//   0x1400fd960 = flip/mirror handler
//   0x1400fda00 = placement position calc
//   0x1400fd650 = placement validation + create
//   0x1401000f0 = placement success finalize
//   0x140100140 = placement failure handler
//
// Original: 0x1400fe2a0
char PlacementTool::OnKey(char param2) {

    // dialogPtr at +0x10 points to an InternalBasePtr value (8 bytes)
    uintptr_t* dialogIBP = (uintptr_t*)dialogPtr;

    // --- Dialog open/close logic ---
    if (param2 == 0) {
        // Close dialog if open (check value twice, matching original)
        if (*dialogIBP != 0 && *dialogIBP != 0) {
            uintptr_t localIBP = *dialogIBP;
            void* uiMgr = *(void**)(GetGameWorld() + 0x98);
            if (uiMgr) {
                reinterpret_cast<UI::UserInterface*>(uiMgr)->CloseDialog(  // B.3.6 bridge
                    reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&localIBP));
            }
            // Clear IBP: construct with null
            new (&localIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *dialogIBP = localIBP;
        }
    } else {
        // Open dialog if not already open
        if (*dialogIBP == 0) {
            uintptr_t localResult = 0;
            ToolUI_OpenToolDialog_ff9e0(dialogIBP, &localResult,
                                        reinterpret_cast<uint64_t>(descriptionPtr));
        }
    }

    // --- Key handling ---
    // Get KeyMap via triple deref of external reference
    void* keyMap = GetKeyMap();

    // Key 0x13: Rotate counter-clockwise
    bool rotCCW = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x13);  // B.3.7 bridge
    if (rotCCW) {
        // Access rotationIBP at self+0x48, CheckPtr, then decrement value at +0x10
        uintptr_t rotSetting = (uintptr_t)rotationIBP;
        int* pRot = (int*)(rotSetting + 0x10);
        *pRot = *pRot - 1;

        // Wrap: if < 0, set to 3
        rotSetting = (uintptr_t)rotationIBP;
        if (*(int*)(rotSetting + 0x10) < 0) {
            rotSetting = (uintptr_t)rotationIBP;
            *(int*)(rotSetting + 0x10) = 3;
        }
    } else {
        // Key 0x14: Rotate clockwise
        bool rotCW = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x14);  // B.3.7 bridge
        if (rotCW) {
            uintptr_t rotSetting = (uintptr_t)rotationIBP;
            int* pRot = (int*)(rotSetting + 0x10);
            *pRot = *pRot + 1;

            // Wrap: if > 3, set to 0
            rotSetting = (uintptr_t)rotationIBP;
            if (*(int*)(rotSetting + 0x10) > 3) {
                rotSetting = (uintptr_t)rotationIBP;
                *(int*)(rotSetting + 0x10) = 0;
            }
        } else {
            // Key 0x15: Flip/Mirror
            bool flip = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 0x15);  // B.3.7 bridge
            if (flip) {
                PlacementToolPlace_FlipVariant_d960(this);
            }
        }
    }

    // --- Placement confirmation (key 5, only when active) ---
    if (param2 != 0) {
        bool placeKey = reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 5);  // B.3.7 bridge
        if (placeKey) {
            // Calculate placement position
            int startCol = 0, startRow = 0, endCol = 0, endRow = 0;
            float pos[3] = {0.0f, 0.0f, 0.0f};
            float yaw[2] = {0.0f, 0.0f};

            {
                bool valid = PlayerRay_PlacementPosCalc_fda00(reinterpret_cast<uintptr_t>(this),
                                                              &startCol, &startRow,
                                                              &endCol, &endRow, pos, yaw) != 0;
                if (valid) {
                    // Try to place via helper_d650
                    {
                        bool placed = static_cast<uint8_t>(ToolTile_ValidateCreate_fd650(
                            reinterpret_cast<long long>(this), startCol, startRow,
                            endCol, endRow, pos, yaw[0])) != 0;
                        if (placed) {
                            // Placement success — finalize
                            DragTool_PlayPlaceSound_1000f0(reinterpret_cast<uint64_t>(this));
                            return 1;
                        }
                    }
                    // Placement failed — run failure handler
                    DragTool_PlayErrorSound_100140(reinterpret_cast<uint64_t>(this));
                }
            }
        }
    }

    return 1;
}

// ============================================================
// PlacementTool::Serialize (vtable +0x38, Slot7)
// ============================================================
// Manages the placement preview visualization. Despite the name "Serialize",
// this is called each frame to update the placement preview.
//
// When active (param3 != 0):
//   1. Call sub_da00 to hit-test current cursor position for placement
//   2. If valid: manage scene object, set transform, update bounding box,
//      check tile validity, update cost dialog
//   3. If invalid: remove scene object and highlight
// When inactive (param3 == 0):
//   Remove scene object and highlight
//
// Disassembly reference: 0x1400fdee0
// IAT slots used:
//   [0x140151360] = Scene::RemoveObject
//   [0x140151370] = Scene::AddObject
//   [0x140151288] = Transform::FromYawPitchOffset
//   [0x1401512e0] = Geometry::SetTransform
//   [0x140152888] = MeshInstance::UpdateHierarchy
//   [0x140151460] = Geometry::GetTransform
//   [0x140151428] = String::String (copy ctor)
//   [0x140152af0] = String::~String
// Direct calls:
//   0x1400fda00 = sub_da00 (placement position calc)
//   0x1400b0d00 = HighlightRemove
//   0x140055430 = HighlightAdd (radius display)
//   0x14009d580 = TileValidityCheck
//   0x140005160 = description lookup
//   0x1400ffc60 = operator_fc60 (dialog update)
//   0x1400ffbf0 = cell change notify (tick sound)
//
// Original: 0x1400fdee0
void PlacementTool::Update(void* /*param2*/, char param3) {
    uintptr_t gameWorld = GetGameWorld();

    float pos[3] = {0.0f, 0.0f, 0.0f};
    int startCol = 0, startRow = 0, endCol = 0, endRow = 0;
    float yaw[2] = {0.0f, 0.0f};

    bool valid = false;
    if (param3 != 0) {
        valid = PlayerRay_PlacementPosCalc_fda00(reinterpret_cast<uintptr_t>(this), &startCol,
                                                 &startRow, &endCol, &endRow, pos, yaw) != 0;
    }


    if (!valid || param3 == 0) {
        // --- Invalid / inactive: remove scene object and highlight ---
        if ((int)sceneObjectIdx >= 0) {
            void* scene = *(void**)(gameWorld + 0x60);
            if (scene) {
                reinterpret_cast<Graphics::Scene*>(scene)
                    ->RemoveObject(sceneObjectIdx);   // B.3.5 bridge
            }
            sceneObjectIdx = -1;
        }
        if (radiusDisplayIdx >= 0) {
            void* highlightMgr = *(void**)(gameWorld + 0x110);
            if (highlightMgr) {
                DecalComponent_TerrainDecalRemove(highlightMgr, radiusDisplayIdx);
            }
            radiusDisplayIdx = -1;
        }
        return;
    }

    // --- Valid placement: update preview ---
    int* mapSize = *(int**)(gameWorld + 0xf8);

    // Update radius display if center cell changed (only if radiusComponent exists)
    if (radiusComponent != nullptr) {
        // Calculate center cell from placement bounds
        int centerRow = ((endRow - startRow) >> 1) + startRow;
        int centerCol = ((endCol - startCol) >> 1) + startCol;
        int centerCell = *mapSize * centerRow + centerCol;

        if (centerCell != lastCenterCell) {
            // Remove old highlight
            if (radiusDisplayIdx >= 0) {
                void* highlightMgr = *(void**)(gameWorld + 0x110);
                if (highlightMgr) {
                    DecalComponent_TerrainDecalRemove(highlightMgr, radiusDisplayIdx);
                }
            }
            // Add new highlight
            radiusDisplayIdx = Player_HighlightAdd_55430(
                reinterpret_cast<int64_t>(radiusComponent), centerCell);
            lastCenterCell = centerCell;
        }
    }

    // Add scene object if not yet added
    if ((int)sceneObjectIdx < 0) {
        void* scene = *(void**)(gameWorld + 0x60);
        if (scene && meshInstance) {
            sceneObjectIdx = (int)reinterpret_cast<Graphics::Scene*>(scene)
                ->AddObject(reinterpret_cast<Graphics::Geometry*>(meshInstance));   // B.3.5 bridge
        }
    }

    // Set transform: FromYawPitchOffset(retbuf, &yaw, &pitch=0, &offset).
    // 🔑 Ghidra (0x1400fdff3): R9=offset = the cursor WORLD position from sub_da00
    // (pos[]), NOT zero — that is what makes the ghost mesh follow the cursor. R8=pitch
    // is zeroed ([RBP-0x70]).
    uint8_t localTransform[96];
    memset(localTransform, 0, sizeof(localTransform));
    float zeroPitch = 0.f;

    // yaw[0] already has the float yaw value from sub_da00.
    {
        float yawVal = yaw[0];
        *reinterpret_cast<Math::Transform*>(localTransform) =
            Math::Transform::FromYawPitchOffset(
                yawVal, zeroPitch, *reinterpret_cast<const Math::Vector3*>(pos));
    }

    // Where the ghost mesh is actually being put this frame. If the preview appears
    // somewhere the cursor is not, this line says whether `pos` (the cursor world
    // position from sub_da00) is wrong or whether something downstream moved the mesh.
    {
        static int n = 0;
        if (n < 24) { ++n;
            fprintf(stderr, "[Preview] ghost transform: pos=(%.2f, %.2f, %.2f) yaw=%.2f "
                            "cells=(%d,%d)..(%d,%d) mesh=%p\n",
                    pos[0], pos[1], pos[2], yaw[0], startCol, startRow, endCol, endRow,
                    meshInstance);
            fflush(stderr);
        }
    }

    if (meshInstance) {
        reinterpret_cast<Graphics::Geometry*>(meshInstance)
            ->SetTransform(*reinterpret_cast<const Math::Transform*>(localTransform));   // B.3.5 bridge
    }

    if (meshInstance) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->UpdateHierarchy();   // B.3.5 bridge
    }

    // Tile validity check via helper at 0x14009d580
    // Uses PlacementDescription component fields at +0x28
    uintptr_t descComp = (uintptr_t)descComponent;  // +0x28
    int width = (endCol - startCol) + 1;
    int height = (endRow - startRow) + 1;
    int yawInt = (int)yaw[0];  // CVTTSS2SI EDI, XMM6
    char tileValid = 1;

    if (descComp) {
        // Match the calling convention from disasm at 0x1400fe09e:
        // RCX=mapSize, EDX=startCol, R8D=startRow, R9D=width
        // stack: height, flags(+0x20), ptr(+0x30), ptr(+0x28), 0.0, DAT_140172248, 0.0,
        //        0, mapComp+0x28, yawInt, desc+0x50
        uintptr_t mapComp = (uintptr_t)mapComponent;  // +0x30
        tileValid = (char)(static_cast<uint8_t>(ToolTile_FootprintValidity_d580(
            mapSize,
            startCol,       // R14D (from disasm)
            startRow,       // ESI (from disasm)
            width,          // R9D
            height,         // [RSP+0x20]
            *(uint32_t*)(descComp + 0x20),    // [RSP+0x28]
            (long long*)(descComp + 0x30),    // [RSP+0x30] — LEA, not deref
            (long long*)(descComp + 0x28),    // [RSP+0x38] — LEA, not deref
            0.0f,                              // [RSP+0x40]
            Rdata_140172248(),                 // [RSP+0x48] DAT_140172248
            0.0f,                              // [RSP+0x50]
            0,                                 // [RSP+0x58]
            (long long*)(mapComp + 0x28),     // [RSP+0x60] — mapComponent+0x28
            yawInt,                            // [RSP+0x68]
            *(long long*)(descComp + 0x50))) != 0);   // [RSP+0x70]
    }

    // Check dialog constraint (if dialogPtr has constraint flag at +0x80)
    uintptr_t dialogCtrl = (uintptr_t)dialogPtr;
    if (*(char*)(dialogCtrl + 0x80) != 0) {
        int mapSz = **(int**)(gameWorld + 0xf8);
        // Original checks: startCol vs (lVar2+0x98)%mapSz, startRow vs (lVar2+0x98)/mapSz,
        //                   endCol vs (lVar2+0x9c)%mapSz, endRow vs (lVar2+0x9c)/mapSz,
        //                   yawInt vs (lVar2+0xa0)
        if (startCol != *(int*)(dialogCtrl + 0x98) % mapSz ||
            startRow != *(int*)(dialogCtrl + 0x98) / mapSz ||
            *(int*)(dialogCtrl + 0x9c) % mapSz != endCol ||
            *(int*)(dialogCtrl + 0x9c) / mapSz != endRow ||
            *(int*)(dialogCtrl + 0xa0) != yawInt) {
            tileValid = 0;
        }
    }

    // Get cost/status strings from description entity
    uintptr_t costResult = 0;
    DragTool_FindDescription_5160(reinterpret_cast<uintptr_t>(descriptionPtr), &costResult);

    // Build cost string: index into string array at (costResult+0x50)
    // Index is 0 (valid) or 1 (invalid), based on tileValid
    uint32_t strIndex = (tileValid == 0) ? 1 : 0;
    System::String costString;  // System::String (8-byte COW refcounted)

    if (costResult != 0 && (int)strIndex < *(int*)(costResult + 0x58)) {
        // Copy from string array: array at +0x50, each entry is 8 bytes.
        // Retail copy-constructs a temporary, copies that into costString and
        // then releases the temporary — the scope below does both.
        uintptr_t strArrayBase = *(uintptr_t*)(costResult + 0x50);
        System::String tempString(
            *reinterpret_cast<const System::String*>(strArrayBase + (uintptr_t)strIndex * 8));
        costString = tempString;
    } else {
        // Empty-string fallback: retail aliases the zeroed costResult local as an
        // empty String, copies it and releases it — both no-ops on a null String,
        // so costString simply stays empty.
        costResult = 0;
    }

    // Get bounding box from mesh instance (+0x34 to +0x48 = 6 floats)
    void* mesh = meshInstance;
    float bbox[6];
    if (mesh) {
        memcpy(bbox, (void*)((uintptr_t)mesh + 0x34), 24);
    } else {
        memset(bbox, 0, sizeof(bbox));
    }

    // Get the mesh transform (by value → hidden retbuf; RCX=mesh, RDX=&retbuf).
    uint8_t meshTransformBuf[96];
    memset(meshTransformBuf, 0, sizeof(meshTransformBuf));
    void* meshTransform = nullptr;
    if (mesh) {
        // Retail retslot door; the body moves to the call site as exactly ONE
        // construction into the SAME storage the door wrote.
        ::new (static_cast<void*>(meshTransformBuf)) Math::Transform(
            reinterpret_cast<const Graphics::Geometry*>(mesh)->GetTransform());   // B.3.5 bridge
        meshTransform = meshTransformBuf;
    }

    // Update dialog with placement info via operator_fc60
    // Original calling convention at 0x1400fe1f9:
    //   RCX = self->dialogPtr (+0x10)
    //   EDX = 0
    //   R8D = 0
    //   R9  = &costString (LEA, pointer to string)
    //   [RSP+0x20] = R13B (tileValid)
    //   [RSP+0x28] = 1 (char)
    //   [RSP+0x30] = 0 (char)
    //   [RSP+0x38] = 1 (int)
    //   [RSP+0x40] = -1 (int)
    //   [RSP+0x48] = -1 (int)
    //   [RSP+0x50] = &bbox
    //   [RSP+0x58] = transform*
    {
        UpdateToolUI_fc60(
            dialogPtr,        // RCX: dialogPtr at +0x10
            0, 0,                   // EDX, R8D
            &costString,            // R9: pointer to cost string
            tileValid,              // [RSP+0x20]
            1,                      // [RSP+0x28]
            0,                      // [RSP+0x30]
            1,                      // [RSP+0x38]
            -1,                     // [RSP+0x40]
            -1,                     // [RSP+0x48]
            bbox,                   // [RSP+0x50]
            meshTransform);         // [RSP+0x58]
    }

    // Check if cell position changed (triggers tick sound)
    if (lastStartCol != startCol || lastStartRow != startRow) {
        lastStartCol = startCol;
        lastStartRow = startRow;
        DragTool_PlayTickSound_ffbf0(reinterpret_cast<uintptr_t>(dialogPtr));
    }
}

// ============================================================
// PlacementTool::Draw (vtable +0x40, Slot8)
// ============================================================
// Renders the placement overlay tiles on the terrain.
//
// When param3 == 0 (active placement preview):
//   1. Call sub_da00 to get placement bounds (startCol/startRow/endCol/endRow)
//   2. Read rotation from IBP at +0x48 (CheckPtr, then read +0x10)
//   3. Read description component fields (terrainMask, ptrs, yaw angle, entity template)
//   4. Delegate to sub_1380 which validates each cell and renders colored overlay tiles
//
// Always checks dialog controller (+0x10) for existing placement marker at +0x80:
//   If hasExistingPlacement, renders that existing placement in yellow via sub_1380.
//
// Disassembly reference: 0x1400fdcc0
// Key sub-calls:
//   0x1400fda00 = sub_da00 (placement position calc, already resolved)
//   0x140101380 = sub_1380 (tile render + validate helper)
// Uses:
//   fn_InternalBasePtr_CheckPtr_slot for rotation IBP at +0x48
//   DAT_140172248 = half-cell float constant
//   descComponent (+0x28), mapComponent (+0x30)
//
// Original: 0x1400fdcc0
void PlacementTool::Draw(void* context, char param3) {
    float halfCell = Rdata_140172248();
    uintptr_t gameWorld = GetGameWorld();

    // --- Section 1: Active placement preview (param3 == 0) ---
    if (param3 == 0) {
        int startCol = 0, startRow = 0;
        uint32_t endCol = 0;
        int endRow = 0;
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float yaw = 0.0f;

        bool valid = PlayerRay_PlacementPosCalc_fda00(reinterpret_cast<uintptr_t>(this),
                                                      &startCol, &startRow,
                                                      (int*)&endCol, &endRow, pos, &yaw) != 0;

        if (valid) {
            // Read descComponent before CheckPtr (used for yawAngle and entityTemplate)
            uintptr_t descComp1 = (uintptr_t)descComponent;  // +0x28

            // CheckPtr on rotation IBP at +0x48

            // Re-read descComponent after CheckPtr (used for terrainMask, ptrs)
            uintptr_t descComp2 = (uintptr_t)descComponent;  // +0x28
            uintptr_t mapComp = (uintptr_t)mapComponent;     // +0x30

            // Read rotation value: *(*(self+0x48) + 0x10) * 0x5a
            uintptr_t rotIBP = (uintptr_t)rotationIBP;
            int yawInt = *(int*)(rotIBP + 0x10) * 0x5a;  // rotation * 90 degrees

            // yawAngle from first descComponent read (+0x40)
            int yawAngle = *(int*)(descComp1 + 0x40);

            PlayerTileRender_BuildRecords_1380(
                (uint64_t)(mapComp + 0x28),     // param1: mapComponent+0x28
                context,                         // param2: render context
                startCol,                        // param3: startCol
                startRow,                        // param4: startRow
                endCol,                          // param5: endCol
                endRow,                          // param6: endRow
                *(uint32_t*)(descComp2 + 0x20), // param7: terrainMask
                (int64_t*)(descComp2 + 0x30),   // param8: desc+0x30 (LEA)
                (int64_t*)(descComp2 + 0x28),   // param9: desc+0x28 (LEA)
                0.0f,                            // param10
                halfCell,                        // param11: DAT_140172248
                0.0f,                            // param12
                0,                               // param13: forceRed = false
                0,                               // param14: allowOverlap = false
                (int64_t*)(mapComp + 0x28),     // param15: mapComp+0x28 (same as param1)
                yawInt,                          // param16: rotation*90
                yawAngle,                        // param17: desc+0x40
                0,                               // param18: isExisting = false
                *(int64_t*)(descComp1 + 0x50)   // param19: entityTemplate from desc
            );
        }
    }

    // --- Section 2: Existing placement marker (if hasExistingPlacement) ---
    uintptr_t dialogCtrl = (uintptr_t)dialogPtr;
    if (*(char*)(dialogCtrl + 0x80) != 0) {
        uintptr_t descComp = (uintptr_t)descComponent;  // +0x28
        int mapSz1 = **(int**)(gameWorld + 0xf8);
        int mapSz2 = **(int**)(gameWorld + 0xf8);  // re-read (matches original)

        int existStart = *(int*)(dialogCtrl + 0x98);
        int existEnd = *(int*)(dialogCtrl + 0x9c);
        uint32_t existYaw = *(uint32_t*)(dialogCtrl + 0xa0);

        PlayerTileRender_BuildRecords_1380(
            (uint64_t)existYaw,                 // param1: existYaw as uint->void*
            context,                             // param2: render context
            existStart % mapSz2,                // param3: startCol
            existStart / mapSz2,                // param4: startRow
            (uint32_t)(existEnd % mapSz1),      // param5: endCol
            existEnd / mapSz1,                  // param6: endRow
            *(uint32_t*)(descComp + 0x20),      // param7: terrainMask
            (int64_t*)(descComp + 0x30),        // param8: desc+0x30
            (int64_t*)(descComp + 0x28),        // param9: desc+0x28
            0.0f,                                // param10
            halfCell,                            // param11
            0.0f,                                // param12
            0,                                   // param13: forceRed = false
            0,                                   // param14: allowOverlap = false
            (int64_t*)((uintptr_t)mapComponent + 0x28), // param15: mapComp+0x28
            (int)existYaw,                       // param16: yawInt
            *(int*)(descComp + 0x40),           // param17: yaw angle from desc
            1,                                   // param18: isExisting = true
            *(int64_t*)(descComp + 0x50)        // param19: entityTemplate
        );
    }
}

// ============================================================
// PlacementTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Initializes the placement tool by finding description components from the
// entity at +0x20, resolving the RotationSetting IBP, allocating a MeshInstance,
// and selecting a random mesh variant.
//
// Component lookups (FindComponent loops using vtable GetTypeId at +0x10):
//   1. DAT_1401c7468 (SizeDescription)       -> self->descComponent   (+0x28)
//   2. DAT_1401c7a18 (AreaDescription)       -> self->mapComponent    (+0x30)
//   3. DAT_1401c7a88 (FenceDescription)      -> self->createComponent (+0x38)
//   4. DWORD_1401c4870 (RadiusComponent)     -> self->radiusComponent (+0x40)
//
// Then:
//   5. CheckPtr on IBP at +0x18, IsDerivedFrom RotationSetting, store IBP at +0x48
//   6. Allocate 0x120-byte MeshInstance, construct, set vtable to 0x14015e240
//   7. Random::Rand(meshCount) to pick variant
//   8. SetMesh from variant's mesh array (last entry)
//   9. UpdateHierarchy on mesh
//  10. Store variant index at +0x64
//
// IAT dispatch slots used:
//   [0x140152a98] = fn_IsDerivedFrom
//   [0x140152ab0] = fn_InternalBasePtr_CheckPtr
//   [0x140152a90] = fn_InternalBasePtr_Ctor
//   [0x140152b60] = System::MemoryAllocator::MemAlloc
//   [0x140151490] = Graphics::MeshInstance::MeshInstance (ctor)
//   [0x1401512e8] = System::Random::Rand
//   [0x140151498] = Graphics::MeshInstance::SetMesh
//   [0x140152888] = Graphics::MeshInstance::UpdateHierarchy
//   [0x1401c63c8] = TypeInfo_RotationSetting (type ID)
//
// Original: 0x1400fe580

static bool s_startupTypeIdsLogged = false;

static void LogStartupTypeIds() {
    if (s_startupTypeIdsLogged) return;

    // ENDGAME B.3.9 EXA: the MeshInstance ctor slot (0x140151490) and the exe
    // table (0x14015e240) reads went: the preview mesh is `::new` of the class.

    // ENDGAME B.3.5 batch K1: the `MeshInstance::SetMesh` cache (slot 0x140151498) that
    // sat beside it had NO caller left -- batch I2 converted its one call site and left
    // the resolve behind, so it was dead on arrival. Typedef, static and resolve removed.



    // The four *Description type-ids Startup's findComp uses (D5, ENDGAME B.3.9 CG3):
    // read BY CELL, never by the (wrong) engine_funcs global names.
    //   +0x28 descComponent   = cell 0x1401c7468 = CreatePlacedDescription (NOT "SizeDescription")
    //   +0x30 mapComponent    = cell 0x1401c7a18 = MapDescription          (NOT "AreaDescription")
    //   +0x38 createComponent = cell 0x1401c7a88 = ModelDescription        (NOT "FenceDescription");
    //         Startup derefs createComponent+0x28 (mesh count) UNGUARDED — MUST be present.
    //   +0x40 radiusComponent = cell 0x1401c4870 = RadiusDescription       (NOT "RadiusComponent")
    // `X::TypeId()` is always resolved (no ResolveAddr/.rdata fallback needed), so the
    // old healing block that patched the engine_funcs globals with local storage when
    // ResolveAddr came back NULL no longer has anything to heal.
    fprintf(stderr, "[Placement] type-ids: CreatePlaced=%u Map=%u Model=%u Radius=%u\n",
            CreatePlacedDescription::TypeId(), MapDescription::TypeId(),
            ModelDescription::TypeId(), RadiusDescription::TypeId());
    fflush(stderr);

    s_startupTypeIdsLogged = true;
}

void PlacementTool::Startup() {
    LogStartupTypeIds();

    uintptr_t entity = (uintptr_t)descriptionPtr;  // +0x20

    // --- Find 4 components via GetTypeId + IsDerivedFrom ---
    // Component list is at entity+0x28 (pointer array), count at entity+0x30.
    // The original code loops through components, calls vtable[+0x10] (GetTypeId),
    // compares, then confirms with IsDerivedFrom. On first type match that fails
    // IsDerivedFrom, it stops (returns nullptr), matching original break-on-match.

    auto findComp = [&](uint32_t targetTypeId) -> void* {
        if (!entity) return nullptr;
        int count = *(int*)(entity + 0x30);
        void** components = *(void***)(entity + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = components[i];
            if (!comp) continue;
            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
            if (typeId == targetTypeId) {
                if (Object::IsDerivedFromNullSafe(comp, targetTypeId)) {
                    return comp;
                }
                return nullptr;
            }
        }
        return nullptr;
    };

    // 1. CreatePlacedDescription (cell 0x1401c7468) -> descComponent (+0x28)
    descComponent = findComp(CreatePlacedDescription::TypeId());

    // 2. MapDescription (cell 0x1401c7a18) -> mapComponent (+0x30)
    mapComponent = findComp(MapDescription::TypeId());

    // 3. ModelDescription (cell 0x1401c7a88) -> createComponent (+0x38)
    createComponent = findComp(ModelDescription::TypeId());

    // 4. RadiusDescription (cell 0x1401c4870) -> radiusComponent (+0x40)
    radiusComponent = findComp(RadiusDescription::TypeId());

    // --- Resolve RotationSetting IBP ---
    // CheckPtr on IBP at +0x18

    // Read resolved pointer, check IsDerivedFrom RotationSetting
    void* ibpVal = toolSetting;
    void* rotSetting = nullptr;
    if (ibpVal) {
        bool isDerived = Object::IsDerivedFromNullSafe(ibpVal, RotationSetting::TypeId());   // cell 0x1401c63c8
        if (isDerived) {
            rotSetting = ibpVal;
        }
    }

    // Construct IBP with the resolved RotationSetting, store at +0x48
    uint64_t localIBP = 0;
    new (&localIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(rotSetting));   // B.3.3 bridge
    rotationIBP = localIBP;

    // --- Allocate and construct MeshInstance (0x120 bytes) ---
    void* meshMem = System::MemoryAllocator::MemAlloc(0x120, 8);

    void* meshInst = nullptr;
    if (meshMem) {
        // Retail: `??0MeshInstance` then the exe's copy 0x14015e240 of the class's
        // own table (not a subclass) = `new MeshInstance()`.
        ::new (meshMem) Graphics::MeshInstance();   // B.3.9 EXA: the real class (exe table 0x14015e240 is its own)
        meshInst = meshMem;
    }
    meshInstance = meshInst;

    // --- Pick random mesh variant ---
    // createComponent (+0x38) has mesh count at +0x28
    uintptr_t modelComp = (uintptr_t)createComponent;
    uint32_t meshCount = *(uint32_t*)(modelComp + 0x28);

    // Get game world's Random at +0xd8
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    void* rng = *(void**)(gameWorld + 0xd8);

    uint32_t variant = 0;
    if (rng) {
        variant = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(meshCount));   // B.3.4 bridge
    }

    // --- Set mesh from variant's mesh array ---
    // ModelComponent+0x20 points to a mesh array. Each entry is 0x18 bytes:
    //   +0x00: unknown
    //   +0x08: pointer to sub-array of mesh pointers
    //   +0x10: count of entries in sub-array
    // We select the last entry: subArray[count - 1]
    uintptr_t meshArray = *(uintptr_t*)(modelComp + 0x20);
    int64_t variantIdx = (int64_t)(int32_t)variant;
    uintptr_t entryBase = meshArray + variantIdx * 0x18;

    int subCount = *(int*)(entryBase + 0x10);
    uintptr_t subArray = *(uintptr_t*)(entryBase + 0x8);
    void* mesh = *(void**)(subArray + (int64_t)(subCount - 1) * 8);

    if (meshInstance) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->SetMesh(Graphics::ExternalPtrHandle{mesh});   // B.3.5 bridge
    }

    // --- Update hierarchy ---
    if (meshInstance) {
        reinterpret_cast<Graphics::MeshInstance*>(meshInstance)
            ->UpdateHierarchy();   // B.3.5 bridge
    }

    // Store variant index
    meshVariant = (int32_t)variant;

    // Diagnostic: dump the built preview mesh's state so we can see why it may not
    // render. Log the bounds (mesh+0x34, 6 floats) — if zero, the renderer
    // frustum-culls it → invisible.
    if (meshInstance) {
        uintptr_t m = (uintptr_t)meshInstance;
        const float* b = (const float*)(m + 0x34);
        fprintf(stderr, "[Placement] mesh built: mesh=%p vtable=%p meshPtr=%p "
                "count=%u variant=%u bounds=[%.2f %.2f %.2f .. %.2f %.2f %.2f]\n",
                meshInstance, *(void**)m,
                mesh, meshCount, variant, b[0], b[1], b[2], b[3], b[4], b[5]);
        fflush(stderr);
    }
}

// ============================================================
// PlacementTool members (PlacementTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_PlacementTool_Vtable[13], its slot stubs, the in-place _SA
// factory, the post-alloc -1 patch-up and RegisterPlacementToolVtable are
// gone: the compiler builds the table, the real ctor writes retail's -1
// sentinels (the zero-fill once made Serialize remove scene object 0), and
// nothing reads 0x140161d20 back by address (the old dtor was the only
// reader). Inherited from Tool, as in retail: [10] false.
// ============================================================
#include "Object/factory_registration.h"
#include "Player/RotationSetting.h"
#include <new>

// [4] Startup, [5] Shutdown, [6] OnKey, [7] Update and [8] Draw are defined above.

// [11] 0x140026920 (shared with FencedTool): `*ret = TypeInfo_RotationSetting`.
// SetEditMode builds the RotationSetting from this id on first activation and
// dereferences the returned slot. The class static replaces the old
// function-local lazy cache (B6-5).
Object::Type* PlacementTool::GetSettingType(Object::Type* ret) const {
    *ret = Object::Type(RotationSetting::TypeId());
    return ret;
}

// create 0x14002dc40: MemAlloc(0x70, 8), then the ctor 0x1400fe850.
void* PlacementTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PlacementTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PlacementTool());
}
// createInPlace 0x14002dc80: `if (storage) ctor(storage)`.
void* PlacementTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PlacementTool());
}

// Kept for standalone/tool_harness.cpp (outside this block's files): the
// reflected create, by its old name.
extern "C" void* PlacementTool_Factory_SA(void) { return PlacementTool::CreateNewObject(); }

DEFINE_EXE_TYPE(PlacementTool, "Tool")

// ============================================================
// Standalone tool-harness probe (NOT a faithful reimpl — scaffolding for
// standalone/tool_harness.cpp, Phase-1 of GUI_PLAN Keystone F).
// Logs descriptionPtr(+0x20)'s component array + the 4 *Description type-ids
// Startup searches for, confirms the mesh-bearing ModelDescription (+0x38
// target) is present, THEN calls the faithful Startup. If Model is absent it
// logs and bails rather than letting Startup's unguarded createComponent+0x28
// deref hard-crash — so a wrong descriptionPtr source is diagnosed, not fatal.
// ============================================================
extern "C" int PlacementTool_HarnessStartupProbe(void* toolSelf) {
    PlacementTool* self = (PlacementTool*)toolSelf;
    LogStartupTypeIds();  // logs the 4 type-ids

    uintptr_t entity = (uintptr_t)self->descriptionPtr;  // +0x20
    if (!entity) {
        fprintf(stderr, "[Placement] PROBE: descriptionPtr(+0x20) is NULL\n");
        fflush(stderr);
        return 0;
    }

    int count = *(int*)(entity + 0x30);
    void** comps = *(void***)(entity + 0x28);
    fprintf(stderr, "[Placement] PROBE: descEntity=%p compArray=%p count=%d\n",
            (void*)entity, (void*)comps, count);

    uint32_t tCreate = CreatePlacedDescription::TypeId();
    uint32_t tMap    = MapDescription::TypeId();
    uint32_t tModel  = ModelDescription::TypeId();
    uint32_t tRadius = RadiusDescription::TypeId();

    bool haveCreate = false, haveMap = false, haveModel = false, haveRadius = false;
    if (comps && count > 0 && count < 256) {
        for (int i = 0; i < count; i++) {
            void* c = comps[i];
            if (!c) continue;
            uint32_t id = reinterpret_cast<Object::Base*>(c)->Type();  // B.3.8 bridge
            if (id == tCreate) haveCreate = true;
            if (id == tMap)    haveMap = true;
            if (id == tModel)  haveModel = true;
            if (id == tRadius) haveRadius = true;
            fprintf(stderr, "[Placement] PROBE:   comp[%d]=%p typeId=%u\n", i, c, id);
        }
    }
    fprintf(stderr, "[Placement] PROBE: present Create=%d Map=%d Model=%d Radius=%d\n",
            haveCreate, haveMap, haveModel, haveRadius);
    fflush(stderr);

    if (!haveModel) {
        fprintf(stderr, "[Placement] PROBE: ModelDescription ABSENT — skipping Startup "
                        "(would crash at +0x38 deref). descriptionPtr is not the right "
                        "entity for this template.\n");
        fflush(stderr);
        return 0;
    }

    fprintf(stderr, "[Placement] PROBE: Model present — calling faithful Startup...\n");
    fflush(stderr);
    self->PlacementTool::Startup();
    fprintf(stderr, "[Placement] PROBE: Startup returned (meshInstance=%p variant=%d)\n",
            self->meshInstance, self->meshVariant);
    fflush(stderr);
    return 1;
}

// ============================================================
// Standalone tool-harness CREATE driver (GUI_PLAN Keystone F, Phase 2).
// ============================================================
// Drives the faithful CREATE path (helper_d650 = ToolTile_ValidateCreate_fd650)
// for a HARNESS-CHOSEN centre cell, bypassing sub_da00's cursor pick
// (ScreenToCell/GetMouseRay/terrain-march) which has no valid cursor at the
// settle frame. Mirrors sub_da00's exact footprint-rectangle + world-centre math
// (tool_base_ray.cpp PlayerRay_PlacementPosCalc_fda00, lines ~375-439) with
// orientation fixed at 0 — so it never derefs rotationIBP (+0x48), and yaw = 0.
//
// helper_d650 reads self+0x10 (dialogPtr) → *(that)+0x80 as the "existing
// placement constraint" flag; the real path fills it from OpenToolDialog. Here we
// point it at a zeroed controller buffer so +0x80 == 0 and the constraint block is
// skipped (a fresh, unconstrained placement).
//
// Requires PlacementTool::Startup to have run first (descComponent/mapComponent/
// createComponent/meshInstance/meshVariant populated) — the probe above does that.
//
// The per-cell footprint validity (helper_d650 → d580 → f5d0) rejects any cell that
// is occupied (another entity), wrong terrain type, or too steep. Map centre is
// occupied by the start-town buildings/trees, so we SCAN outward in expanding
// Chebyshev rings and place at the first buildable cell. helper_d650 is fully
// side-effect-free when validity fails (it returns before MemAlloc/CopyCtor), so
// repeated attempts are safe.

// Try a single centre cell: compute the footprint rect + world-centre pos (orient 0,
// yaw 0), call the faithful helper_d650. Returns nonzero only when a building was
// actually created. On validity failure this is a harmless no-op.
// Compute the footprint rectangle + world-centre position for a chosen centre
// cell, mirroring sub_da00 exactly (orient 0 → no width/height swap, yaw 0). This
// is the cursor-free stand-in for sub_da00's screen pick, shared by the CREATE
// (helper_d650) and PREVIEW (ghost) harness probes. Returns false if the footprint
// falls off the map.
static bool HarnessCellPlacement(PlacementTool* self, int centerCell,
                                 int* outStartCol, int* outStartRow,
                                 int* outEndCol, int* outEndRow, float* pos) {
    uintptr_t descComp = (uintptr_t)self->descComponent;  // +0x28 CreatePlacedDescription
    if (!descComp) return false;

    // Footprint dims live in the CreatePlacedDescription (+0x44 width, +0x48 height).
    uint32_t dimX = *(uint32_t*)(descComp + 0x44);
    uint32_t dimY = *(uint32_t*)(descComp + 0x48);

    uintptr_t gameWorld = GetGameWorld();
    int* grid = *(int**)(gameWorld + 0xf8);
    int width = grid[0];
    int col = centerCell % width;
    int row = centerCell / width;

    // Centre the footprint over (col,row) — mirrors sub_da00 exactly.
    int startCol, endCol, startRow, endRow, dimYHalf;
    if ((dimX & 1) == 0) {
        startCol = (col - ((int)dimX >> 1)) + 1;
        endCol   = ((int)dimX >> 1) + col;
    } else {
        int h = (int)(dimX - 1) >> 1;
        startCol = col - h;
        endCol   = h + col;
    }
    if ((dimY & 1) == 0) {
        dimYHalf = (int)dimY >> 1;
        startRow = (row - dimYHalf) + 1;
    } else {
        dimYHalf = (int)(dimY - 1) >> 1;
        startRow = row - dimYHalf;
    }
    endRow = dimYHalf + row;

    // Footprint fully on map, startCol >= 1 (mirrors sub_da00's own gate).
    if (startCol < 0 || width <= startCol || startRow < 0 || width <= startRow ||
        startCol < 1 || width <= endCol || endRow < 0 || width <= endRow) {
        return false;
    }

    // World-centre position = average of the two diagonal corner cells' world
    // positions via GridToWorld (CharacterComponent_impl_f3e0 @ 0x14009f3e0). 0.5.
    const float halfCell = 0.5f;
    pos[0] = pos[1] = pos[2] = 0.0f;
    {
        int gw = grid[0];
        float centerOff = (float)(gw >> 1) - halfCell;
        int idxA = startRow * gw + startCol;
        float inA[3] = { (float)(idxA % gw) - centerOff, (float)(idxA / gw) - centerOff, 0.0f };
        float worldA[3] = {0.0f, 0.0f, 0.0f};
        Reimpl_MapGetTerrainPos(grid, worldA, inA, '\0', nullptr);

        int idxB = endRow * gw + endCol;
        float inB[3] = { (float)(idxB % gw) - centerOff, (float)(idxB / gw) - centerOff, 0.0f };
        float worldB[3] = {0.0f, 0.0f, 0.0f};
        Reimpl_MapGetTerrainPos(grid, worldB, inB, '\0', nullptr);

        pos[0] = (worldB[0] + worldA[0]) * halfCell;
        pos[1] = (worldB[1] + worldA[1]) * halfCell;
    }

    *outStartCol = startCol; *outStartRow = startRow;
    *outEndCol   = endCol;   *outEndRow   = endRow;
    return true;
}

static int HarnessTryPlaceAtCell(PlacementTool* self, int centerCell) {
    int startCol, startRow, endCol, endRow;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    if (!HarnessCellPlacement(self, centerCell, &startCol, &startRow, &endCol, &endRow, pos))
        return 0;

    uint64_t placed = ToolTile_ValidateCreate_fd650(reinterpret_cast<long long>(self), startCol,
                                                    startRow, endCol, endRow, pos, 0.0f /*yaw*/);
    return (int)(placed & 0xff);
}

extern "C" int PlacementTool_HarnessPlaceAtCell(void* toolSelf, int centerCell) {
    PlacementTool* self = (PlacementTool*)toolSelf;
    if (!self->descComponent) {
        fprintf(stderr, "[Placement] PLACE: descComponent(+0x28) NULL — run Startup first\n");
        fflush(stderr);
        return 0;
    }

    uintptr_t gameWorld = GetGameWorld();
    int* grid = *(int**)(gameWorld + 0xf8);
    int width = grid[0];
    int c0 = centerCell % width;
    int r0 = centerCell / width;
    uintptr_t dc = (uintptr_t)self->descComponent;
    fprintf(stderr, "[Placement] PLACE: scanning from (col=%d row=%d) foot=%ux%u for a buildable cell...\n",
            c0, r0, *(uint32_t*)(dc + 0x44), *(uint32_t*)(dc + 0x48));
    fflush(stderr);

    // Wire a zeroed dialog-controller so helper_d650's *(self+0x10)+0x80 reads 0
    // (no existing-placement constraint). Static so it outlives this call.
    static uint8_t s_zeroDialogCtrl[0x100] = {0};
    self->dialogPtr = (void*)&s_zeroDialogCtrl[0];

    // Expanding Chebyshev-ring scan around the centre; place at the first hit.
    const int kMaxRadius = 48;
    int attempts = 0;
    for (int rad = 0; rad <= kMaxRadius; rad++) {
        for (int dr = -rad; dr <= rad; dr++) {
            for (int dcx = -rad; dcx <= rad; dcx++) {
                // Only the perimeter of this ring (interior handled by smaller rad).
                if (rad != 0 && dr > -rad && dr < rad && dcx > -rad && dcx < rad) continue;
                int rr = r0 + dr, cc = c0 + dcx;
                if (rr < 1 || rr >= width - 1 || cc < 1 || cc >= width - 1) continue;
                attempts++;
                int cell = rr * width + cc;
                if (HarnessTryPlaceAtCell(self, cell)) {
                    fprintf(stderr, "[Placement] PLACE: SUCCESS at cell %d (col=%d row=%d), "
                            "ring=%d after %d attempts — building entity materialized\n",
                            cell, cc, rr, rad, attempts);
                    fflush(stderr);
                    DragTool_PlayPlaceSound_1000f0(reinterpret_cast<uint64_t>(self));  // finalize (place sound)
                    return 1;
                }
            }
        }
    }
    DragTool_PlayErrorSound_100140(reinterpret_cast<uint64_t>(self));  // fail (error sound)
    fprintf(stderr, "[Placement] PLACE: FAILED — no buildable cell within radius %d (%d attempts)\n",
            kMaxRadius, attempts);
    fflush(stderr);
    return 0;
}

// GUI_PLAN Keystone F, Phase 3B — cursor-free GHOST PREVIEW probe.
// Serialize's preview normally follows the OS cursor via sub_da00, which the
// headless harness has none of. This drives the exact same ghost pipeline
// (Scene::AddObject → Transform::FromYawPitchOffset → Geometry::SetTransform →
// MeshInstance::UpdateHierarchy) for a fixed centre cell, leaving the preview mesh
// standing in the live render scene so the fix is provable without a mouse. It
// logs the resolved fn pointers + the resulting transform translation so a
// degenerate (all-zero) transform — the old "invisible ghost" bug — is visible in
// the log. Does NOT create a permanent entity (that's HarnessPlaceAtCell).
extern "C" int PlacementTool_HarnessPreviewAtCell(void* toolSelf, int centerCell) {
    PlacementTool* self = (PlacementTool*)toolSelf;
    if (!self->descComponent || !self->meshInstance) {
        fprintf(stderr, "[Preview] descComponent(+0x28)=%p meshInstance(+0x68)=%p — run Startup first\n",
                (void*)self->descComponent, (void*)self->meshInstance);
        fflush(stderr);
        return 0;
    }


    int startCol, startRow, endCol, endRow;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    if (!HarnessCellPlacement(self, centerCell, &startCol, &startRow, &endCol, &endRow, pos)) {
        fprintf(stderr, "[Preview] cell %d off-map — no preview\n", centerCell);
        fflush(stderr);
        return 0;
    }
    fprintf(stderr, "[Preview] cell=%d rect=(%d,%d)-(%d,%d) pos=(%.2f,%.2f,%.2f)\n",
            centerCell, startCol, startRow, endCol, endRow, pos[0], pos[1], pos[2]);

    // 1) Submit the preview mesh to the live scene (gameWorld+0x60 — same scene the
    //    working ModelComponent render path uses).
    uintptr_t gameWorld = GetGameWorld();
    void* scene = *(void**)(gameWorld + 0x60);
    if ((int)self->sceneObjectIdx < 0 && scene && self->meshInstance) {
        self->sceneObjectIdx = (int)reinterpret_cast<Graphics::Scene*>(scene)
            ->AddObject(reinterpret_cast<Graphics::Geometry*>(self->meshInstance));   // B.3.5 bridge
    }

    // 2) Build the placement transform from yaw + cursor world offset (the fix:
    //    TransformFromYaw was null in standalone → zero transform → invisible).
    uint8_t localTransform[96];
    memset(localTransform, 0, sizeof(localTransform));
    float yawVal = 0.0f, zeroPitch = 0.0f;
    *reinterpret_cast<Math::Transform*>(localTransform) =
        Math::Transform::FromYawPitchOffset(
            yawVal, zeroPitch, *reinterpret_cast<const Math::Vector3*>(pos));

    // Log the first 12 floats so a degenerate (all-zero) transform is obvious and the
    // translation (which should carry pos) is visible.
    const float* xf = reinterpret_cast<const float*>(localTransform);
    fprintf(stderr, "[Preview] transform[0..11]= %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | "
            "%.2f %.2f %.2f %.2f\n",
            xf[0], xf[1], xf[2], xf[3], xf[4], xf[5], xf[6], xf[7], xf[8], xf[9], xf[10], xf[11]);

    if (self->meshInstance) {
        reinterpret_cast<Graphics::Geometry*>(self->meshInstance)
            ->SetTransform(*reinterpret_cast<const Math::Transform*>(localTransform));   // B.3.5 bridge
        reinterpret_cast<Graphics::MeshInstance*>(self->meshInstance)
            ->UpdateHierarchy();   // B.3.5 bridge
    }

    bool degenerate = (xf[0] == 0.0f && xf[1] == 0.0f && xf[2] == 0.0f &&
                       xf[4] == 0.0f && xf[5] == 0.0f && xf[6] == 0.0f);
    fprintf(stderr, "[Preview] sceneObjectIdx=%d transform=%s — ghost mesh %s in the live scene\n",
            (int)self->sceneObjectIdx, degenerate ? "DEGENERATE(all-zero!)" : "valid",
            (self->sceneObjectIdx >= 0 && !degenerate) ? "SUBMITTED" : "NOT submitted");
    fflush(stderr);
    return (self->sceneObjectIdx >= 0 && !degenerate) ? 1 : 0;
}
