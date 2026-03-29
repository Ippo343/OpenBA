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

#include "Player/placementtool_funcs.h"
#include "PlacementTool.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// Locally resolved function pointers (exe addresses)
// Resolved from IAT dispatch slots and direct addresses in the exe.
// IAT slot addresses verified against Ghidra disassembly of OnKey/Serialize.
// ============================================================

// Graphics::Scene::RemoveObject — IAT dispatch slot [0x140151360]
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
static fn_SceneRemoveObject_t s_fn_SceneRemoveObject = nullptr;

// Graphics::Scene::AddObject — IAT dispatch slot [0x140151370]
// Verified from Serialize disasm at 0x1400fdfd9: CALL [0x140151370]
typedef uint32_t (*fn_SceneAddObject_t)(void* scene, void* geometry);
static fn_SceneAddObject_t s_fn_SceneAddObject = nullptr;

// HighlightRemove — direct call at 0x1400b0d00
typedef void (*fn_HighlightRemove_t)(void* highlightMgr, int overlayId);
static fn_HighlightRemove_t s_fn_HighlightRemove = nullptr;

// UI::UserInterface::CloseDialog — IAT dispatch slot [0x140151550]
// Takes (uiMgr, &localIBP): RCX=uiMgr, RDX=pointer to IBP value
typedef void (*fn_CloseDialog_t)(void* uiMgr, void* dialogIBPPtr);
static fn_CloseDialog_t s_fn_CloseDialog = nullptr;

// InternalBasePtr::Ctor — IAT dispatch slot [0x140152a90]
typedef void* (*fn_IBP_Ctor_t)(void* self, void* internal);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

// InternalBasePtr::CheckPtr — IAT dispatch slot [0x140152ab0]
typedef void (*fn_IBP_CheckPtr_t)(void* self);
static fn_IBP_CheckPtr_t s_fn_IBP_CheckPtr = nullptr;

// KeyMap::Pressed — IAT dispatch slot [0x140151758]
typedef bool (*fn_KeyMapPressed_t)(void* keyMap, int slot, int keyId);
static fn_KeyMapPressed_t s_fn_KeyMapPressed = nullptr;

// Math::Transform::FromYawPitchOffset — IAT dispatch slot [0x140151288]
// Verified from Serialize disasm at 0x1400fe003: CALL [0x140151288]
typedef void (*fn_TransformFromYaw_t)(void* outTransform, void* yaw, void* offset);
static fn_TransformFromYaw_t s_fn_TransformFromYaw = nullptr;

// Graphics::Geometry::SetTransform — IAT dispatch slot [0x1401512e0]
// Verified from Serialize disasm at 0x1400fe011: CALL [0x1401512e0]
typedef void (*fn_GeomSetTransform_t)(void* geometry, void* transform);
static fn_GeomSetTransform_t s_fn_GeomSetTransform = nullptr;

// Graphics::MeshInstance::UpdateHierarchy — IAT dispatch slot [0x140152888]
// Verified from Serialize disasm at 0x1400fe01b: CALL [0x140152888]
typedef void (*fn_MeshUpdateHier_t)(void* mesh);
static fn_MeshUpdateHier_t s_fn_MeshUpdateHier = nullptr;

// Graphics::Geometry::GetTransform — IAT dispatch slot [0x140151460]
// Verified from Serialize disasm at 0x1400fe1b5: CALL [0x140151460]
typedef void* (*fn_GeomGetTransform_t)(void* geometry);
static fn_GeomGetTransform_t s_fn_GeomGetTransform = nullptr;

// System::String::String (copy ctor) — IAT dispatch slot [0x140151428]
// Verified from Serialize disasm at 0x1400fe12c and 0x1400fe155
typedef void* (*fn_StringCtor_t)(void* dst, void* src);
static fn_StringCtor_t s_fn_StringCtor = nullptr;

// System::String::~String — IAT dispatch slot [0x140152af0]
// Verified from Serialize disasm at 0x1400fe16c, 0x1400fe183, 0x1400fe207
typedef void (*fn_StringDtor_t)(void* str);
static fn_StringDtor_t s_fn_StringDtor = nullptr;

// PlacementTool_sub_d960 — mirror/flip handler at 0x1400fd960
typedef void (*fn_sub_d960_t)(void* self);
static fn_sub_d960_t s_fn_sub_d960 = nullptr;

// PlacementTool_sub_da00 — placement position calc at 0x1400fda00
typedef bool (*fn_sub_da00_t)(void* self, int* outStartCol, int* outStartRow,
    int* outEndCol, int* outEndRow, float* outPos, float* outYaw);
static fn_sub_da00_t s_fn_sub_da00 = nullptr;

// PlacementTool_helper_d650 — placement validation+create at 0x1400fd650
typedef bool (*fn_helper_d650_t)(void* self, int startCol, int startRow,
    int endCol, int endRow, float* pos, float yaw);
static fn_helper_d650_t s_fn_helper_d650 = nullptr;

// Tile validity check at 0x14009d580 (NOT 0x1400fd580!)
// Verified from Serialize disasm at 0x1400fe09e: CALL 0x14009d580
// Signature: (mapSize*, startCol, startRow, width, height, flags, ptr1, ptr2,
//   float, float, float, char, ptr3, yawInt, ptr4) -> bool
typedef bool (*fn_PlaceTileCheck_t)(void* mapSize, int startCol, int startRow,
    int width, int height, uint32_t flags, void* ptr1, void* ptr2,
    float f1, float f2, float f3, char c1, void* ptr3, int yawInt, void* ptr4);
static fn_PlaceTileCheck_t s_fn_TileValidityCheck = nullptr;

// operator_impl_fc60 �� dialog update with placement info at 0x1400ffc60
// Verified from Serialize disasm at 0x1400fe1fd: CALL 0x1400ffc60
// Params: (dialogPtr, 0, 0, costStringPtr, valid, 1, 0, 1, -1, -1, bbox*, transform*)
typedef void (*fn_operator_fc60_t)(void* dialogPtr, int a2, int a3, void* costStringPtr,
    char valid, char c2, char c3, int i1, int i2, int i3, void* bbox, void* transform);
static fn_operator_fc60_t s_fn_operator_fc60 = nullptr;

// FUN_140005160 — entity lookup helper
typedef void (*fn_FUN_140005160_t)(void* descPtr, void* outResult);
static fn_FUN_140005160_t s_fn_FUN_140005160 = nullptr;

// FUN_1400ffbf0 — tick sound / cell change notify
typedef void (*fn_FUN_1400ffbf0_t)(void* dialogPtr);
static fn_FUN_1400ffbf0_t s_fn_FUN_1400ffbf0 = nullptr;

// FUN_1401000f0 — placement success finalize
typedef void (*fn_FUN_1401000f0_t)(void* self);
static fn_FUN_1401000f0_t s_fn_FUN_1401000f0 = nullptr;

// FUN_140100140 — placement failure handler
typedef void (*fn_FUN_140100140_t)(void* self);
static fn_FUN_140100140_t s_fn_FUN_140100140 = nullptr;

// FUN_1400ff9e0 — dialog open helper
typedef void* (*fn_FUN_1400ff9e0_t)(void* dialogIBP, void* outResult, void* descPtr);
static fn_FUN_1400ff9e0_t s_fn_FUN_1400ff9e0 = nullptr;

// FUN_140055430 — radius highlight add (used when radiusComponent != nullptr)
typedef int (*fn_HighlightAdd_t)(void* radiusComponent, int cellIndex);
static fn_HighlightAdd_t s_fn_HighlightAdd = nullptr;

// PlacementTool_OnRemove_sub_1380 — tile render + validate helper at 0x140101380
// Builds tile record array for placement overlay, validates terrain, then renders.
// 19 params: (occupancyGrid, context, startCol, startRow, endCol, endRow,
//   terrainMask, ptr1, ptr2, f1, f2, f3, forceRed, allowOverlap,
//   occupancyGridPtr, yawInt, yawAngle, isExisting, entityTemplate)
typedef void (*fn_sub_1380_t)(void* occupancyGrid, void* context, int startCol, int startRow,
    uint32_t endCol, int endRow, uint32_t terrainMask, void* ptr1, void* ptr2,
    float f1, float f2, float f3, char forceRed, char allowOverlap,
    void* occupancyGridPtr, int yawInt, int yawAngle, char isExisting, int64_t entityTemplate);
static fn_sub_1380_t s_fn_sub_1380 = nullptr;

// DAT_140172248 — constant float (0.5)
static float* s_pFloat_halfCell = nullptr;

static bool s_funcsResolved = false;

static void EnsureFuncs() {
    if (s_funcsResolved) return;

    // IAT dispatch slots: read function pointer from .rdata indirect slot
    auto resolveIAT = [](uintptr_t ghidraAddr) -> void* {
        uintptr_t slot = ResolveAddr(Module::Exe, ghidraAddr);
        if (slot) return *reinterpret_cast<void**>(slot);
        return nullptr;
    };

    // IAT slots verified against Serialize/OnKey disassembly
    s_fn_SceneRemoveObject = (fn_SceneRemoveObject_t)resolveIAT(0x140151360);
    s_fn_SceneAddObject    = (fn_SceneAddObject_t)resolveIAT(0x140151370);
    s_fn_CloseDialog       = (fn_CloseDialog_t)resolveIAT(0x140151550);
    s_fn_IBP_Ctor          = (fn_IBP_Ctor_t)resolveIAT(0x140152a90);
    s_fn_IBP_CheckPtr      = (fn_IBP_CheckPtr_t)resolveIAT(0x140152ab0);
    s_fn_KeyMapPressed     = (fn_KeyMapPressed_t)resolveIAT(0x140151758);
    s_fn_TransformFromYaw  = (fn_TransformFromYaw_t)resolveIAT(0x140151288);
    s_fn_GeomSetTransform  = (fn_GeomSetTransform_t)resolveIAT(0x1401512e0);
    s_fn_MeshUpdateHier    = (fn_MeshUpdateHier_t)resolveIAT(0x140152888);
    s_fn_GeomGetTransform  = (fn_GeomGetTransform_t)resolveIAT(0x140151460);
    s_fn_StringCtor        = (fn_StringCtor_t)resolveIAT(0x140151428);
    s_fn_StringDtor        = (fn_StringDtor_t)resolveIAT(0x140152af0);

    // Direct exe function addresses
    s_fn_HighlightRemove   = (fn_HighlightRemove_t)ResolveAddr(Module::Exe, 0x1400b0d00);
    s_fn_sub_d960          = (fn_sub_d960_t)ResolveAddr(Module::Exe, 0x1400fd960);
    s_fn_sub_da00          = (fn_sub_da00_t)ResolveAddr(Module::Exe, 0x1400fda00);
    s_fn_helper_d650       = (fn_helper_d650_t)ResolveAddr(Module::Exe, 0x1400fd650);
    s_fn_TileValidityCheck = (fn_PlaceTileCheck_t)ResolveAddr(Module::Exe, 0x14009d580);
    s_fn_operator_fc60     = (fn_operator_fc60_t)ResolveAddr(Module::Exe, 0x1400ffc60);
    s_fn_FUN_140005160     = (fn_FUN_140005160_t)ResolveAddr(Module::Exe, 0x140005160);
    s_fn_FUN_1400ffbf0     = (fn_FUN_1400ffbf0_t)ResolveAddr(Module::Exe, 0x1400ffbf0);
    s_fn_FUN_1401000f0     = (fn_FUN_1401000f0_t)ResolveAddr(Module::Exe, 0x1401000f0);
    s_fn_FUN_140100140     = (fn_FUN_140100140_t)ResolveAddr(Module::Exe, 0x140100140);
    s_fn_FUN_1400ff9e0     = (fn_FUN_1400ff9e0_t)ResolveAddr(Module::Exe, 0x1400ff9e0);
    s_fn_HighlightAdd      = (fn_HighlightAdd_t)ResolveAddr(Module::Exe, 0x140055430);

    // Float constants in .rdata
    s_pFloat_halfCell = (float*)ResolveAddr(Module::Exe, 0x140172248);

    // PlacementTool_OnRemove_sub_1380: the big tile render+validate helper
    // Used by PlacementTool::Draw to render placement overlay tiles.
    // Takes 19 parameters including placement bounds, terrain info, rotation, etc.
    // Internally builds 0x14-byte tile records and calls CharacterComponent_impl_08c0.
    // Original: 0x140101380
    s_fn_sub_1380 = (fn_sub_1380_t)ResolveAddr(Module::Exe, 0x140101380);

    s_funcsResolved = true;
}

// ============================================================
// PlacementTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_PlacementTool = nullptr;
static void* s_vtable_Tool          = nullptr;

static void EnsureVtables() {
    if (!s_vtable_PlacementTool) {
        s_vtable_PlacementTool = (void*)ResolveAddr(Module::Exe, PlacementToolAddr::Vtable);
        s_vtable_Tool          = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
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

// ============================================================
// Helper: get KeyMap object from external reference (triple deref)
// ============================================================
static inline void* GetKeyMap() {
    return ***(void****)g_pKeyMapObjectExref;
}

// ============================================================
// Helper: get GameWorld singleton pointer
// ============================================================
static inline uintptr_t GetGameWorld() {
    return (uintptr_t)*g_pGameWorldSingleton;
}

// ============================================================
// PlacementTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: PlacementTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
// Original: 0x14002dc90
void* PlacementTool_Dtor_Impl(PlacementTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_PlacementTool;
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
// PlacementTool::Shutdown2 (vtable +0x28, Slot5)
// ============================================================
// Removes placement preview from the scene, destroys the mesh instance,
// and closes the associated dialog.
// Original: 0x1400fe4e0
void PlacementTool_Shutdown2_Impl(PlacementTool* self) {
    EnsureFuncs();

    // Remove scene object if added (sceneObjectIdx >= 0)
    if ((int)self->sceneObjectIdx >= 0) {
        void* scene = *(void**)(GetGameWorld() + 0x60);
        if (s_fn_SceneRemoveObject && scene) {
            s_fn_SceneRemoveObject(scene, self->sceneObjectIdx);
        }
    }

    // Remove radius display highlight if active (radiusDisplayIdx >= 0)
    if (self->radiusDisplayIdx >= 0) {
        void* highlightMgr = *(void**)(GetGameWorld() + 0x110);
        if (s_fn_HighlightRemove && highlightMgr) {
            s_fn_HighlightRemove(highlightMgr, self->radiusDisplayIdx);
        }
    }

    // Destroy mesh instance via vtable dtor if allocated
    void* mesh = self->meshInstance;
    if (mesh) {
        typedef void* (*fn_MeshDtor_t)(void* self, uint32_t flags);
        void** vtable = *(void***)mesh;
        fn_MeshDtor_t dtor = (fn_MeshDtor_t)vtable[0];
        dtor(mesh, 1);  // flags=1: free memory
    }

    // Close dialog if open: dialogPtr (+0x10) points to an InternalBasePtr
    uintptr_t* ibpPtr = (uintptr_t*)self->dialogPtr;
    uintptr_t ibpVal = *ibpPtr;
    if (ibpVal != 0) {
        // Close the dialog via UI::UserInterface
        void* uiMgr = *(void**)(GetGameWorld() + 0x98);
        if (s_fn_CloseDialog && uiMgr) {
            uintptr_t localIBP = ibpVal;
            s_fn_CloseDialog(uiMgr, &localIBP);
        }

        // Clear the InternalBasePtr and write back
        uintptr_t localIBP = ibpVal;
        if (s_fn_IBP_Ctor) {
            s_fn_IBP_Ctor(&localIBP, nullptr);
        }
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
uint64_t PlacementTool_OnKey_Impl(PlacementTool* self, char param2) {
    EnsureFuncs();

    uintptr_t selfAddr = (uintptr_t)self;

    // dialogPtr at +0x10 points to an InternalBasePtr value (8 bytes)
    uintptr_t* dialogIBP = (uintptr_t*)self->dialogPtr;

    // --- Dialog open/close logic ---
    if (param2 == 0) {
        // Close dialog if open (check value twice, matching original)
        if (*dialogIBP != 0 && *dialogIBP != 0) {
            uintptr_t localIBP = *dialogIBP;
            void* uiMgr = *(void**)(GetGameWorld() + 0x98);
            if (s_fn_CloseDialog && uiMgr) {
                s_fn_CloseDialog(uiMgr, &localIBP);
            }
            // Clear IBP: construct with null
            if (s_fn_IBP_Ctor) {
                s_fn_IBP_Ctor(&localIBP, nullptr);
            }
            *dialogIBP = localIBP;
        }
    } else {
        // Open dialog if not already open
        if (*dialogIBP == 0) {
            uintptr_t localResult = 0;
            if (s_fn_FUN_1400ff9e0) {
                s_fn_FUN_1400ff9e0(dialogIBP, &localResult, self->descriptionPtr);
            }
        }
    }

    // --- Key handling ---
    // Get KeyMap via triple deref of external reference
    void* keyMap = GetKeyMap();

    // Key 0x13: Rotate counter-clockwise
    bool rotCCW = s_fn_KeyMapPressed ? s_fn_KeyMapPressed(keyMap, 0, 0x13) : false;
    if (rotCCW) {
        // Access rotationIBP at self+0x48, CheckPtr, then decrement value at +0x10
        void* rotIBPAddr = (void*)(selfAddr + 0x48);
        if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
        uintptr_t rotSetting = *(uintptr_t*)(selfAddr + 0x48);
        int* pRot = (int*)(rotSetting + 0x10);
        *pRot = *pRot - 1;

        // Wrap: if < 0, set to 3
        if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
        rotSetting = *(uintptr_t*)(selfAddr + 0x48);
        if (*(int*)(rotSetting + 0x10) < 0) {
            if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
            rotSetting = *(uintptr_t*)(selfAddr + 0x48);
            *(int*)(rotSetting + 0x10) = 3;
        }
    } else {
        // Key 0x14: Rotate clockwise
        bool rotCW = s_fn_KeyMapPressed ? s_fn_KeyMapPressed(keyMap, 0, 0x14) : false;
        if (rotCW) {
            void* rotIBPAddr = (void*)(selfAddr + 0x48);
            if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
            uintptr_t rotSetting = *(uintptr_t*)(selfAddr + 0x48);
            int* pRot = (int*)(rotSetting + 0x10);
            *pRot = *pRot + 1;

            // Wrap: if > 3, set to 0
            if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
            rotSetting = *(uintptr_t*)(selfAddr + 0x48);
            if (*(int*)(rotSetting + 0x10) > 3) {
                if (s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr(rotIBPAddr);
                rotSetting = *(uintptr_t*)(selfAddr + 0x48);
                *(int*)(rotSetting + 0x10) = 0;
            }
        } else {
            // Key 0x15: Flip/Mirror
            bool flip = s_fn_KeyMapPressed ? s_fn_KeyMapPressed(keyMap, 0, 0x15) : false;
            if (flip) {
                if (s_fn_sub_d960) {
                    s_fn_sub_d960(self);
                }
            }
        }
    }

    // --- Placement confirmation (key 5, only when active) ---
    if (param2 != 0) {
        bool placeKey = s_fn_KeyMapPressed ? s_fn_KeyMapPressed(keyMap, 0, 5) : false;
        if (placeKey) {
            // Calculate placement position
            int startCol = 0, startRow = 0, endCol = 0, endRow = 0;
            float pos[3] = {0.0f, 0.0f, 0.0f};
            float yaw[2] = {0.0f, 0.0f};

            if (s_fn_sub_da00) {
                bool valid = s_fn_sub_da00(self, &startCol, &startRow,
                                            &endCol, &endRow, pos, yaw);
                if (valid) {
                    // Try to place via helper_d650
                    if (s_fn_helper_d650) {
                        bool placed = s_fn_helper_d650(self, startCol, startRow,
                                                        endCol, endRow, pos, yaw[0]);
                        if (placed) {
                            // Placement success — finalize
                            if (s_fn_FUN_1401000f0) {
                                s_fn_FUN_1401000f0(self);
                            }
                            return 1;
                        }
                    }
                    // Placement failed — run failure handler
                    if (s_fn_FUN_140100140) {
                        s_fn_FUN_140100140(self);
                    }
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
void PlacementTool_Serialize_Impl(PlacementTool* self, void* /*param2*/, char param3) {
    EnsureFuncs();

    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t gameWorld = GetGameWorld();

    float pos[3] = {0.0f, 0.0f, 0.0f};
    int startCol = 0, startRow = 0, endCol = 0, endRow = 0;
    float yaw[2] = {0.0f, 0.0f};

    bool valid = false;
    if (param3 != 0 && s_fn_sub_da00) {
        valid = s_fn_sub_da00(self, &startCol, &startRow, &endCol, &endRow, pos, yaw);
    }

    if (!valid || param3 == 0) {
        // --- Invalid / inactive: remove scene object and highlight ---
        if ((int)self->sceneObjectIdx >= 0) {
            void* scene = *(void**)(gameWorld + 0x60);
            if (s_fn_SceneRemoveObject && scene) {
                s_fn_SceneRemoveObject(scene, self->sceneObjectIdx);
            }
            self->sceneObjectIdx = -1;
        }
        if (self->radiusDisplayIdx >= 0) {
            void* highlightMgr = *(void**)(gameWorld + 0x110);
            if (s_fn_HighlightRemove && highlightMgr) {
                s_fn_HighlightRemove(highlightMgr, self->radiusDisplayIdx);
            }
            self->radiusDisplayIdx = -1;
        }
        return;
    }

    // --- Valid placement: update preview ---
    int* mapSize = *(int**)(gameWorld + 0xf8);

    // Update radius display if center cell changed (only if radiusComponent exists)
    if (self->radiusComponent != nullptr) {
        // Calculate center cell from placement bounds
        int centerRow = ((endRow - startRow) >> 1) + startRow;
        int centerCol = ((endCol - startCol) >> 1) + startCol;
        int centerCell = *mapSize * centerRow + centerCol;

        if (centerCell != self->lastCenterCell) {
            // Remove old highlight
            if (self->radiusDisplayIdx >= 0) {
                void* highlightMgr = *(void**)(gameWorld + 0x110);
                if (s_fn_HighlightRemove && highlightMgr) {
                    s_fn_HighlightRemove(highlightMgr, self->radiusDisplayIdx);
                }
            }
            // Add new highlight
            if (s_fn_HighlightAdd) {
                self->radiusDisplayIdx = s_fn_HighlightAdd(self->radiusComponent, centerCell);
            }
            self->lastCenterCell = centerCell;
        }
    }

    // Add scene object if not yet added
    if ((int)self->sceneObjectIdx < 0) {
        void* scene = *(void**)(gameWorld + 0x60);
        if (s_fn_SceneAddObject && scene && self->meshInstance) {
            self->sceneObjectIdx = (int)s_fn_SceneAddObject(scene, self->meshInstance);
        }
    }

    // Set transform: FromYawPitchOffset(outTransform, &yaw, &zeroOffset)
    // The yaw comes from sub_da00 output. Offset is zero vector.
    uint8_t localTransform[96];
    memset(localTransform, 0, sizeof(localTransform));
    int zeroVec = 0;

    // Original stores yaw into [RBP-0x78], passes that as XMM6/param
    // yaw[0] already has the float yaw value from sub_da00
    if (s_fn_TransformFromYaw) {
        // Match original: MOVSS [RBP-0x78], XMM6 then LEA RDX,[RBP-0x78]
        float yawVal = yaw[0];
        s_fn_TransformFromYaw(localTransform, &yawVal, &zeroVec);
    }

    if (s_fn_GeomSetTransform && self->meshInstance) {
        s_fn_GeomSetTransform(self->meshInstance, localTransform);
    }

    if (s_fn_MeshUpdateHier && self->meshInstance) {
        s_fn_MeshUpdateHier(self->meshInstance);
    }

    // Tile validity check via helper at 0x14009d580
    // Uses PlacementDescription component fields at +0x28
    uintptr_t descComp = (uintptr_t)self->descComponent;  // +0x28
    int width = (endCol - startCol) + 1;
    int height = (endRow - startRow) + 1;
    int yawInt = (int)yaw[0];  // CVTTSS2SI EDI, XMM6
    char tileValid = 1;

    if (s_fn_TileValidityCheck && descComp) {
        // Match the calling convention from disasm at 0x1400fe09e:
        // RCX=mapSize, EDX=startCol, R8D=startRow, R9D=width
        // stack: height, flags(+0x20), ptr(+0x30), ptr(+0x28), 0.0, DAT_140172248, 0.0,
        //        0, mapComp+0x28, yawInt, desc+0x50
        uintptr_t mapComp = (uintptr_t)self->mapComponent;  // +0x30
        tileValid = (char)s_fn_TileValidityCheck(
            mapSize,
            startCol,       // R14D (from disasm)
            startRow,       // ESI (from disasm)
            width,          // R9D
            height,         // [RSP+0x20]
            *(uint32_t*)(descComp + 0x20),    // [RSP+0x28]
            (void*)(descComp + 0x30),         // [RSP+0x30] — LEA, not deref
            (void*)(descComp + 0x28),         // [RSP+0x38] — LEA, not deref
            0.0f,                              // [RSP+0x40]
            s_pFloat_halfCell ? *s_pFloat_halfCell : 0.5f,  // [RSP+0x48]
            0.0f,                              // [RSP+0x50]
            0,                                 // [RSP+0x58]
            (void*)(mapComp + 0x28),          // [RSP+0x60] — mapComponent+0x28
            yawInt,                            // [RSP+0x68]
            *(void**)(descComp + 0x50));       // [RSP+0x70]
    }

    // Check dialog constraint (if dialogPtr has constraint flag at +0x80)
    uintptr_t dialogCtrl = *(uintptr_t*)(selfAddr + 0x10);
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
    if (s_fn_FUN_140005160) {
        s_fn_FUN_140005160(self->descriptionPtr, &costResult);
    }

    // Build cost string: index into string array at (costResult+0x50)
    // Index is 0 (valid) or 1 (invalid), based on tileValid
    uint32_t strIndex = (tileValid == 0) ? 1 : 0;
    uint64_t costString = 0;  // System::String (8-byte COW refcounted)
    uint64_t tempString = 0;
    int cleanupFlags = 0;  // track which strings need cleanup

    if (costResult != 0 && (int)strIndex < *(int*)(costResult + 0x58)) {
        // Copy from string array: array at +0x50, each entry is 8 bytes
        uintptr_t strArrayBase = *(uintptr_t*)(costResult + 0x50);
        void* srcStr = (void*)(strArrayBase + (uintptr_t)strIndex * 8);
        if (s_fn_StringCtor) {
            s_fn_StringCtor(&tempString, srcStr);
        }
        cleanupFlags = 1;  // need to cleanup tempString
    } else {
        // Empty string fallback
        costResult = 0;
        tempString = 0;  // null string
        cleanupFlags = 2;  // need to cleanup costResult local (set to 0)
    }

    // Copy into costString for the dialog update call
    void* strSrc = (cleanupFlags == 1) ? &tempString : &costResult;
    if (s_fn_StringCtor) {
        s_fn_StringCtor(&costString, strSrc);
    }

    // Cleanup intermediate strings (matching original cleanup order)
    if (cleanupFlags == 2) {
        // Cleanup the fallback costResult local
        if (s_fn_StringDtor) s_fn_StringDtor(&costResult);
    } else if (cleanupFlags == 1) {
        // Cleanup tempString
        if (s_fn_StringDtor) s_fn_StringDtor(&tempString);
    }

    // Get bounding box from mesh instance (+0x34 to +0x48 = 6 floats)
    void* mesh = self->meshInstance;
    float bbox[6];
    if (mesh) {
        memcpy(bbox, (void*)((uintptr_t)mesh + 0x34), 24);
    } else {
        memset(bbox, 0, sizeof(bbox));
    }

    // Get transform pointer from mesh
    void* meshTransform = nullptr;
    if (s_fn_GeomGetTransform && mesh) {
        meshTransform = s_fn_GeomGetTransform(mesh);
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
    if (s_fn_operator_fc60) {
        s_fn_operator_fc60(
            self->dialogPtr,        // RCX: dialogPtr at +0x10
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

    // Cleanup costString
    if (s_fn_StringDtor) {
        s_fn_StringDtor(&costString);
    }

    // Check if cell position changed (triggers tick sound)
    if (self->lastStartCol != startCol || self->lastStartRow != startRow) {
        self->lastStartCol = startCol;
        self->lastStartRow = startRow;
        if (s_fn_FUN_1400ffbf0) {
            s_fn_FUN_1400ffbf0(self->dialogPtr);
        }
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
void PlacementTool_Draw_Impl(PlacementTool* self, void* context, char param3) {
    EnsureFuncs();

    if (!s_fn_sub_1380) return;

    float halfCell = s_pFloat_halfCell ? *s_pFloat_halfCell : 0.5f;
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t gameWorld = GetGameWorld();

    // --- Section 1: Active placement preview (param3 == 0) ---
    if (param3 == 0) {
        int startCol = 0, startRow = 0;
        uint32_t endCol = 0;
        int endRow = 0;
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float yaw = 0.0f;

        bool valid = false;
        if (s_fn_sub_da00) {
            valid = s_fn_sub_da00(self, &startCol, &startRow,
                                  (int*)&endCol, &endRow, pos, &yaw);
        }

        if (valid) {
            // Read descComponent before CheckPtr (used for yawAngle and entityTemplate)
            uintptr_t descComp1 = (uintptr_t)self->descComponent;  // +0x28

            // CheckPtr on rotation IBP at +0x48
            if (s_fn_IBP_CheckPtr) {
                s_fn_IBP_CheckPtr((void*)(selfAddr + 0x48));
            }

            // Re-read descComponent after CheckPtr (used for terrainMask, ptrs)
            uintptr_t descComp2 = (uintptr_t)self->descComponent;  // +0x28
            uintptr_t mapComp = (uintptr_t)self->mapComponent;     // +0x30

            // Read rotation value: *(*(self+0x48) + 0x10) * 0x5a
            uintptr_t rotIBP = *(uintptr_t*)(selfAddr + 0x48);
            int yawInt = *(int*)(rotIBP + 0x10) * 0x5a;  // rotation * 90 degrees

            // yawAngle from first descComponent read (+0x40)
            int yawAngle = *(int*)(descComp1 + 0x40);

            s_fn_sub_1380(
                (void*)(mapComp + 0x28),        // param1: mapComponent+0x28
                context,                         // param2: render context
                startCol,                        // param3: startCol
                startRow,                        // param4: startRow
                endCol,                          // param5: endCol
                endRow,                          // param6: endRow
                *(uint32_t*)(descComp2 + 0x20), // param7: terrainMask
                (void*)(descComp2 + 0x30),      // param8: desc+0x30 (LEA)
                (void*)(descComp2 + 0x28),      // param9: desc+0x28 (LEA)
                0.0f,                            // param10
                halfCell,                        // param11: DAT_140172248
                0.0f,                            // param12
                0,                               // param13: forceRed = false
                0,                               // param14: allowOverlap = false
                (void*)(mapComp + 0x28),        // param15: mapComp+0x28 (same as param1)
                yawInt,                          // param16: rotation*90
                yawAngle,                        // param17: desc+0x40
                0,                               // param18: isExisting = false
                *(int64_t*)(descComp1 + 0x50)   // param19: entityTemplate from desc
            );
        }
    }

    // --- Section 2: Existing placement marker (if hasExistingPlacement) ---
    uintptr_t dialogCtrl = *(uintptr_t*)(selfAddr + 0x10);
    if (*(char*)(dialogCtrl + 0x80) != 0) {
        uintptr_t descComp = (uintptr_t)self->descComponent;  // +0x28
        int mapSz1 = **(int**)(gameWorld + 0xf8);
        int mapSz2 = **(int**)(gameWorld + 0xf8);  // re-read (matches original)

        int existStart = *(int*)(dialogCtrl + 0x98);
        int existEnd = *(int*)(dialogCtrl + 0x9c);
        uint32_t existYaw = *(uint32_t*)(dialogCtrl + 0xa0);

        s_fn_sub_1380(
            (void*)(uintptr_t)existYaw,         // param1: existYaw as uint->void*
            context,                             // param2: render context
            existStart % mapSz2,                // param3: startCol
            existStart / mapSz2,                // param4: startRow
            (uint32_t)(existEnd % mapSz1),      // param5: endCol
            existEnd / mapSz1,                  // param6: endRow
            *(uint32_t*)(descComp + 0x20),      // param7: terrainMask
            (void*)(descComp + 0x30),           // param8: desc+0x30
            (void*)(descComp + 0x28),           // param9: desc+0x28
            0.0f,                                // param10
            halfCell,                            // param11
            0.0f,                                // param12
            0,                                   // param13: forceRed = false
            0,                                   // param14: allowOverlap = false
            (void*)(*(uintptr_t*)(selfAddr + 0x30) + 0x28), // param15: mapComp+0x28
            existYaw,                            // param16: yawInt
            *(int*)(descComp + 0x40),           // param17: yaw angle from desc
            1,                                   // param18: isExisting = true
            *(int64_t*)(descComp + 0x50)        // param19: entityTemplate
        );
    }
}
