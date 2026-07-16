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
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#endif
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
// Real signature: static Transform FromYawPitchOffset(Scalar& yaw, Scalar& pitch, Vector3& offset)
// RCX=retval, RDX=&yaw, R8=&pitch, R9=&offset.
typedef void (*fn_TransformFromYaw_t)(void* outTransform, const void* yaw, const void* pitch, const void* offset);
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
// 🔑 Returns Math::Transform BY VALUE (member fn, hidden retbuf ABI: RCX=this,
// RDX=retbuf). MUST be called with a retbuf — calling it (mesh) with no retbuf leaves
// RDX garbage and the fn writes the Transform through it → hard-fault (WRITE into
// Runtime code). Signature: void GetTransform(Geometry* this, Transform* retbuf).
typedef void (*fn_GeomGetTransform_t)(void* geometry, void* retbuf);
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
#ifdef STANDALONE_EXE
    // These IAT dispatch slots are unregistered in standalone (→ null). The resolved
    // engine_funcs globals are the canonical fallback (SetEditMode uses them). A null
    // IBP_Ctor silently makes an IBP wrapping a NON-null object come out 0 — e.g. the
    // rotation setting → rotationIBP(+0x48) null → sub_da00 reads rot+0x10 = crash.
    if (!s_fn_IBP_Ctor)     s_fn_IBP_Ctor     = reinterpret_cast<fn_IBP_Ctor_t>(fn_InternalBasePtr_Ctor);
    if (!s_fn_IBP_CheckPtr) s_fn_IBP_CheckPtr = reinterpret_cast<fn_IBP_CheckPtr_t>(fn_InternalBasePtr_CheckPtr);
    // CloseDialog slot 0x140151550 is likewise unfilled in standalone → null, so the
    // tool's Shutdown2/OnKey close paths no-op and the CreateBar never disappears when
    // the tool is dismissed (right-click). Fall back to the export-resolved global
    // (same fn DragTool_CloseToolDialog uses). Harmless in DLL mode (slot is filled).
    if (!s_fn_CloseDialog && fn_UI_CloseDialog)
        s_fn_CloseDialog = reinterpret_cast<fn_CloseDialog_t>(fn_UI_CloseDialog);
#endif
    s_fn_KeyMapPressed     = (fn_KeyMapPressed_t)resolveIAT(0x140151758);
    // Standalone: the raw IAT slot 0x140151758 is unfilled → null, which would make
    // EVERY key check (rotate/flip AND the key-5 place-confirm) no-op — the "can't place
    // the building" symptom. Fall back to the export-resolved engine_funcs global (the
    // same one the working camera-input path uses). Harmless in DLL mode (slot is filled).
    if (!s_fn_KeyMapPressed && fn_KeyMap_Pressed)
        s_fn_KeyMapPressed = reinterpret_cast<fn_KeyMapPressed_t>(fn_KeyMap_Pressed);
    s_fn_TransformFromYaw  = (fn_TransformFromYaw_t)resolveIAT(0x140151288);
    s_fn_GeomSetTransform  = (fn_GeomSetTransform_t)resolveIAT(0x1401512e0);
    s_fn_MeshUpdateHier    = (fn_MeshUpdateHier_t)resolveIAT(0x140152888);
    s_fn_GeomGetTransform  = (fn_GeomGetTransform_t)resolveIAT(0x140151460);
    s_fn_StringCtor        = (fn_StringCtor_t)resolveIAT(0x140151428);
    s_fn_StringDtor        = (fn_StringDtor_t)resolveIAT(0x140152af0);
#ifdef STANDALONE_EXE
    // The String copy-ctor / dtor IAT slots (0x140151428 / 0x140152af0) are unregistered in
    // standalone → null, so the cost-string tooltip below silently skips (costString stays
    // 0 → dialog shows a blank cost). Fall back to the export-resolved System::String
    // ctor/dtor (same slot->export pattern as DragTool). Harmless in DLL mode (slots filled).
    if (!s_fn_StringCtor && fn_String_Ctor_Copy)
        s_fn_StringCtor = reinterpret_cast<fn_StringCtor_t>(fn_String_Ctor_Copy);
    if (!s_fn_StringDtor && fn_String_Dtor)
        s_fn_StringDtor = reinterpret_cast<fn_StringDtor_t>(fn_String_Dtor);
    // Math::Transform::FromYawPitchOffset is a STATIC fn returning Transform by value
    // (hidden retbuf: RCX=retbuf, RDX=yaw, R8=pitch, R9=offset) — matches the 4-arg
    // typedef. Its exe IAT slot 0x140151288 is registered in standalone via
    // address_registry (RegisterDispatchSlot), so resolveIAT above normally returns it;
    // this ResolveExport is a belt-and-suspenders fallback if the slot is ever empty.
    // Without a valid pointer the preview mesh gets a zeroed (degenerate) transform →
    // invisible ghost. (Name recovered from the exe slot name-table.) Likewise
    // GeomGetTransform (0x140151460) is now registered; keep an export fallback.
    if (!s_fn_TransformFromYaw)
        s_fn_TransformFromYaw = reinterpret_cast<fn_TransformFromYaw_t>(
            ResolveExport("?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z"));
    if (!s_fn_GeomGetTransform)
        s_fn_GeomGetTransform = reinterpret_cast<fn_GeomGetTransform_t>(
            ResolveExport("?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ"));
#endif

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
#ifdef STANDALONE_EXE
    { const char* e = std::getenv("BANISHED_TOOL_DIAG");
      if (e && e[0] && e[0] != '0' && rotCCW) { fprintf(stderr, "[OnKey] rotate-CCW key(0x13) PRESSED on this frame\n"); fflush(stderr); } }
#endif
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
#ifdef STANDALONE_EXE
        { const char* e = std::getenv("BANISHED_TOOL_DIAG");
          if (e && e[0] && e[0] != '0' && rotCW) { fprintf(stderr, "[OnKey] rotate-CW key(0x14) PRESSED on this frame\n"); fflush(stderr); } }
#endif
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
#ifdef STANDALONE_EXE
        {
            static int s_ok = -1;
            if (s_ok < 0) { const char* e = std::getenv("BANISHED_TOOL_DIAG"); s_ok = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (s_ok) {
                static bool s_once = false;
                if (!s_once) { s_once = true;
                    fprintf(stderr, "[OnKey] active: keyMap=%p KeyMapPressed=%p (place-confirm armed)\n",
                            keyMap, (void*)s_fn_KeyMapPressed); fflush(stderr); }
                if (placeKey) { fprintf(stderr, "[OnKey] key-5 PRESSED (left-click) — running confirm\n"); fflush(stderr); }
            }
        }
#endif
        if (placeKey) {
            // Calculate placement position
            int startCol = 0, startRow = 0, endCol = 0, endRow = 0;
            float pos[3] = {0.0f, 0.0f, 0.0f};
            float yaw[2] = {0.0f, 0.0f};

            if (s_fn_sub_da00) {
                bool valid = s_fn_sub_da00(self, &startCol, &startRow,
                                            &endCol, &endRow, pos, yaw);
#ifdef STANDALONE_EXE
                { const char* e = std::getenv("BANISHED_TOOL_DIAG");
                  if (e && e[0] && e[0] != '0') {
                    fprintf(stderr, "[OnKey] confirm: sub_da00 valid=%d cell=(%d,%d)-(%d,%d) helper_d650=%p\n",
                            valid, startCol, startRow, endCol, endRow, (void*)s_fn_helper_d650); fflush(stderr); } }
#endif
                if (valid) {
                    // Try to place via helper_d650
                    if (s_fn_helper_d650) {
                        bool placed = s_fn_helper_d650(self, startCol, startRow,
                                                        endCol, endRow, pos, yaw[0]);
#ifdef STANDALONE_EXE
                        { const char* e = std::getenv("BANISHED_TOOL_DIAG");
                          if (e && e[0] && e[0] != '0') {
                            fprintf(stderr, "[OnKey] helper_d650 placed=%d\n", placed); fflush(stderr); } }
#endif
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

#ifdef STANDALONE_EXE
    // Diagnostic (BANISHED_TOOL_DIAG=1): throttled log of the per-frame Serialize state.
    // The ghost mesh is only in the scene while sub_da00 returns VALID (cursor over the
    // map); an invalid frame removes it. This tells us whether the cursor→terrain pick
    // actually resolves in-game (the linchpin for both the ghost AND operator_fc60).
    {
        static int s_diagOn = -1;
        if (s_diagOn < 0) { const char* e = std::getenv("BANISHED_TOOL_DIAG"); s_diagOn = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (s_diagOn) {
            static int s_frame = 0;
            if ((s_frame++ % 30) == 0) {
                fprintf(stderr, "[SerDiag] active=%d valid=%d cell=(%d,%d)-(%d,%d) pos=(%.2f,%.2f,%.2f) "
                        "yaw=%.2f sceneObjIdx=%d mesh=%p\n",
                        (int)param3, (int)valid, startCol, startRow, endCol, endRow,
                        pos[0], pos[1], pos[2], yaw[0], (int)self->sceneObjectIdx, self->meshInstance);
                fflush(stderr);
            }
        }
    }
#endif

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

    // Set transform: FromYawPitchOffset(retbuf, &yaw, &pitch=0, &offset).
    // 🔑 Ghidra (0x1400fdff3): R9=offset = the cursor WORLD position from sub_da00
    // (pos[]), NOT zero — that is what makes the ghost mesh follow the cursor. R8=pitch
    // is zeroed ([RBP-0x70]).
    uint8_t localTransform[96];
    memset(localTransform, 0, sizeof(localTransform));
    float zeroPitch = 0.f;

    // yaw[0] already has the float yaw value from sub_da00.
    if (s_fn_TransformFromYaw) {
        float yawVal = yaw[0];
        s_fn_TransformFromYaw(localTransform, &yawVal, &zeroPitch, pos);
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
#ifdef STANDALONE_EXE
    // Log each DISTINCT costResult (dedupe by pointer) so the trace captures the real
    // building being placed, not just the first (often-empty) default tool state.
    {
        const char* e = std::getenv("BANISHED_TOOL_DIAG");
        if (e && e[0] && e[0] != '0' && costResult) {
            static uintptr_t s_seen[16] = {0};
            static int s_seenN = 0;
            bool known = false;
            for (int k = 0; k < s_seenN; ++k) if (s_seen[k] == costResult) { known = true; break; }
            if (!known && s_seenN < 16) {
                s_seen[s_seenN++] = costResult;
                int arrCount = *(int*)(costResult + 0x58);
                uintptr_t arrBase = *(uintptr_t*)(costResult + 0x50);
                uint64_t str0 = (arrBase && arrCount > 0) ? *(uint64_t*)(arrBase) : 0;
                uint64_t str1 = (arrBase && arrCount > 1) ? *(uint64_t*)(arrBase + 8) : 0;
                fprintf(stderr, "[cost] costResult(ToolbarDescription)=%p arr@+0x50=%p count@+0x58=%d "
                        "str0=\"%.32s\" str1=\"%.32s\"\n", (void*)costResult, (void*)arrBase, arrCount,
                        str0 ? (const char*)str0 : "", str1 ? (const char*)str1 : "");
                fflush(stderr);
            }
        }
    }
#endif

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

    // Get the mesh transform (by value → hidden retbuf; RCX=mesh, RDX=&retbuf).
    uint8_t meshTransformBuf[96];
    memset(meshTransformBuf, 0, sizeof(meshTransformBuf));
    void* meshTransform = nullptr;
    if (s_fn_GeomGetTransform && mesh) {
        s_fn_GeomGetTransform(mesh, meshTransformBuf);
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

// Local function pointers for Startup (resolved from IAT dispatch slots)
typedef void (*fn_MeshInstanceCtor_t)(void* self);
typedef void (*fn_MeshInstanceSetMesh_t)(void* self, void* mesh);

static fn_MeshInstanceCtor_t s_fn_MeshInstanceCtor = nullptr;
static fn_MeshInstanceSetMesh_t s_fn_MeshInstanceSetMesh = nullptr;
static uint32_t* s_pTypeId_RotationSetting = nullptr;
static void** s_pMeshInstanceVtable = nullptr;
static bool s_startupFuncsResolved = false;

static void EnsureStartupFuncs() {
    if (s_startupFuncsResolved) return;
    EnsureFuncs();  // resolve common funcs (IBP_Ctor, IBP_CheckPtr, etc.)

    auto resolveIAT = [](uintptr_t ghidraAddr) -> void* {
        uintptr_t slot = ResolveAddr(Module::Exe, ghidraAddr);
        if (slot) return *reinterpret_cast<void**>(slot);
        return nullptr;
    };

    s_fn_MeshInstanceCtor = (fn_MeshInstanceCtor_t)resolveIAT(0x140151490);
    s_fn_MeshInstanceSetMesh = (fn_MeshInstanceSetMesh_t)resolveIAT(0x140151498);

    // RotationSetting type ID at 0x1401c63c8 (read as uint32_t*)
    s_pTypeId_RotationSetting = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c63c8);
#ifdef STANDALONE_EXE
    // 0x1401c63c8 is a raw .rdata constant → NULL in standalone. Without it Startup's
    // IsDerivedFrom(setting, RotationSetting) is skipped → rotationIBP (+0x48) wraps
    // null → sub_da00 crashes on rot+0x10 (the orientation read). Fill from the live
    // factory tree, same as the *Description type-ids above.
    if (!s_pTypeId_RotationSetting) {
        static uint32_t s_tid_RotationSetting = 0;
        s_tid_RotationSetting = ::Object::ResolveTypeIdByNameLazy("RotationSetting");
        s_pTypeId_RotationSetting = &s_tid_RotationSetting;
    }
#endif

    // MeshInstance vtable at 0x14015e240 (LEA, not deref -- direct address in .rdata)
    s_pMeshInstanceVtable = (void**)ResolveAddr(Module::Exe, 0x14015e240);

#ifdef STANDALONE_EXE
    // The four *Description type-id globals Startup's findComp reads are filled in
    // engine_funcs.cpp via ResolveAddr(Exe, .rdata) — which is NULL in the standalone
    // exe (raw .rdata constants don't resolve, see reference_standalone_rdata_null_fallback).
    // They are read by dormant tools that have never run in standalone, so they are
    // unfilled here. Point them at local storage filled from the LIVE factory tree
    // (ResolveTypeIdByNameLazy), the proven ToolbarStandalone_Init pattern. The `if
    // (!global)` guard makes this a no-op in DLL mode (already filled).
    // 🔑 The engine_funcs LABELS for these 4 globals are WRONG (Ghidra-verified via the
    // RegisterType DATA writers, 2026-07-02). True types, per slot:
    //   +0x28 descComponent   = DAT_1401c7468 = CreatePlacedDescription (NOT "SizeDescription")
    //   +0x30 mapComponent    = DAT_1401c7a18 = MapDescription          (NOT "AreaDescription")
    //   +0x38 createComponent = DAT_1401c7a88 = ModelDescription        (NOT "FenceDescription");
    //         Startup derefs createComponent+0x28 (mesh count) UNGUARDED — MUST be present.
    //   +0x40 radiusComponent = DWORD_1401c4870 = RadiusDescription     (NOT "RadiusComponent")
    static uint32_t s_tid_CreatePlaced = 0, s_tid_Map = 0, s_tid_Model = 0, s_tid_Radius = 0;
    s_tid_CreatePlaced = ::Object::ResolveTypeIdByNameLazy("CreatePlacedDescription");
    s_tid_Map          = ::Object::ResolveTypeIdByNameLazy("MapDescription");
    s_tid_Model        = ::Object::ResolveTypeIdByNameLazy("ModelDescription");
    s_tid_Radius       = ::Object::ResolveTypeIdByNameLazy("RadiusDescription");
    if (!g_TypeId_SizeDescription) g_TypeId_SizeDescription = &s_tid_CreatePlaced; // +0x28
    if (!g_TypeId_AreaDescription) g_TypeId_AreaDescription = &s_tid_Map;          // +0x30
    if (!g_TypeId_FenceDescription) g_TypeId_FenceDescription = &s_tid_Model;      // +0x38
    if (!g_TypeId_RadiusComponent) g_TypeId_RadiusComponent = &s_tid_Radius;      // +0x40
    fprintf(stderr, "[Placement] type-ids: CreatePlaced=%u Map=%u Model=%u Radius=%u\n",
            s_tid_CreatePlaced, s_tid_Map, s_tid_Model, s_tid_Radius);
    fflush(stderr);
#endif

    s_startupFuncsResolved = true;
}

void PlacementTool_Startup_Impl(PlacementTool* self) {
    EnsureStartupFuncs();

    uintptr_t entity = (uintptr_t)self->descriptionPtr;  // +0x20

    // --- Find 4 components via GetTypeId + IsDerivedFrom ---
    // Component list is at entity+0x28 (pointer array), count at entity+0x30.
    // The original code loops through components, calls vtable[+0x10] (GetTypeId),
    // compares, then confirms with IsDerivedFrom. On first type match that fails
    // IsDerivedFrom, it stops (returns nullptr), matching original break-on-match.

    auto findComp = [&](uint32_t targetTypeId) -> void* {
        if (!entity || !fn_IsDerivedFrom) return nullptr;
        int count = *(int*)(entity + 0x30);
        void** components = *(void***)(entity + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = components[i];
            if (!comp) continue;
            typedef uint32_t (*fn_GetTypeId_t)(void* self);
            fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
            uint32_t typeId = getTypeId(comp);
            if (typeId == targetTypeId) {
                if (fn_IsDerivedFrom(comp, targetTypeId)) {
                    return comp;
                }
                return nullptr;
            }
        }
        return nullptr;
    };

    // 1. SizeDescription (DAT_1401c7468) -> descComponent (+0x28)
    self->descComponent = findComp(*g_TypeId_SizeDescription);

    // 2. AreaDescription (DAT_1401c7a18) -> mapComponent (+0x30)
    self->mapComponent = findComp(*g_TypeId_AreaDescription);

    // 3. FenceDescription (DAT_1401c7a88) -> createComponent (+0x38)
    self->createComponent = findComp(*g_TypeId_FenceDescription);

    // 4. RadiusComponent (DWORD_1401c4870) -> radiusComponent (+0x40)
    self->radiusComponent = findComp(*g_TypeId_RadiusComponent);

    // --- Resolve RotationSetting IBP ---
    // CheckPtr on IBP at +0x18
    if (s_fn_IBP_CheckPtr) {
        s_fn_IBP_CheckPtr((void*)((uintptr_t)self + 0x18));
    }

    // Read resolved pointer, check IsDerivedFrom RotationSetting
    void* ibpVal = self->_field18;
    void* rotSetting = nullptr;
    if (fn_IsDerivedFrom && s_pTypeId_RotationSetting && ibpVal) {
        bool isDerived = fn_IsDerivedFrom(ibpVal, *s_pTypeId_RotationSetting);
        if (isDerived) {
            rotSetting = ibpVal;
        }
    }

    // Construct IBP with the resolved RotationSetting, store at +0x48
    uint64_t localIBP = 0;
    if (s_fn_IBP_Ctor) {
        s_fn_IBP_Ctor(&localIBP, rotSetting);
    }
    self->rotationIBP = localIBP;

    // --- Allocate and construct MeshInstance (0x120 bytes) ---
    void* meshMem = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        meshMem = fn_MemAlloc(g_pMemAllocSingleton, 0x120, 8);
    }

    void* meshInst = nullptr;
    if (meshMem) {
        if (s_fn_MeshInstanceCtor) {
            s_fn_MeshInstanceCtor(meshMem);
        }
        // Overwrite vtable to 0x14015e240 (specific MeshInstance subclass vtable)
        if (s_pMeshInstanceVtable) {
            *(void**)meshMem = s_pMeshInstanceVtable;
        }
        meshInst = meshMem;
    }
    self->meshInstance = meshInst;

    // --- Pick random mesh variant ---
    // createComponent (+0x38) has mesh count at +0x28
    uintptr_t modelComp = (uintptr_t)self->createComponent;
    uint32_t meshCount = *(uint32_t*)(modelComp + 0x28);

    // Get game world's Random at +0xd8
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    void* rng = *(void**)(gameWorld + 0xd8);

    uint32_t variant = 0;
    if (fn_Random_Rand && rng) {
        variant = fn_Random_Rand(rng, meshCount);
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

    if (s_fn_MeshInstanceSetMesh && self->meshInstance) {
        s_fn_MeshInstanceSetMesh(self->meshInstance, mesh);
    }

    // --- Update hierarchy ---
    if (s_fn_MeshUpdateHier && self->meshInstance) {
        s_fn_MeshUpdateHier(self->meshInstance);
    }

    // Store variant index
    self->meshVariant = (int32_t)variant;

#ifdef STANDALONE_EXE
    // Diagnostic: dump the built preview mesh's state so we can see why it may not
    // render. s_pMeshInstanceVtable is NULL in standalone (0x14015e240 is a raw .rdata
    // vtable → ResolveAddr(Exe, .rdata)=0), so the exe's vtable override is SKIPPED and
    // the mesh keeps the base Graphics::MeshInstance vtable. Log the bounds (mesh+0x34,
    // 6 floats) — if zero, the renderer frustum-culls it → invisible.
    if (self->meshInstance) {
        uintptr_t m = (uintptr_t)self->meshInstance;
        const float* b = (const float*)(m + 0x34);
        fprintf(stderr, "[Placement] mesh built: mesh=%p vtable=%p (override %s) meshPtr=%p "
                "count=%u variant=%u bounds=[%.2f %.2f %.2f .. %.2f %.2f %.2f]\n",
                self->meshInstance, *(void**)m,
                s_pMeshInstanceVtable ? "APPLIED" : "SKIPPED(null in SA!)",
                mesh, meshCount, variant, b[0], b[1], b[2], b[3], b[4], b[5]);
        fflush(stderr);
    }
#endif
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// PlacementTool: size 0x70, parent Tool, 13-slot vtable @ 0x140161d20.
// (PlacementTool_Ctor's ~25-resource slot-map init is elided; dormant type.)
// ============================================================
#ifdef STANDALONE_EXE
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PlacementTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PlacementTool");
    return c;
}
// +0x58 (Slot11) GetToolSettingTypeId — faithful port of FencedTool_Slot11
// (0x140026920): `*ret = TypeInfo_RotationSetting; return ret;`. SetEditMode
// (FUN_140102b60 / Toolbar_ShowGroup_Impl) calls this on first activation and
// DEREFERENCES the returned pointer, so a Tool_Ret0 stub (returns 0 → *0) would
// crash. Returns the live RotationSetting factory type-id.
static uint32_t* PlacementTool_GetSettingType(void*, uint32_t* ret) {
    static uint32_t s_tid = 0;
    if (!s_tid) s_tid = ::Object::GetTypeIdByName("RotationSetting");
    if (ret) *ret = s_tid;
    return ret;
}
// +0x60 (Slot12) GetCursorId — exe body is literally `MOV EAX,5; RET`.
static int PlacementTool_GetCursor(void*) { return 5; }
static void* g_PlacementTool_Vtable[13] = {
    (void*)&PlacementTool_Dtor_Impl,      // +0x00 Dtor
    (void*)&Tool_Slot1,                   // +0x08
    (void*)&PlacementTool_GetTypeId,      // +0x10 GetTypeId
    (void*)&Tool_Persist,                 // +0x18 Serialize (base)
    (void*)&PlacementTool_Startup_Impl,   // +0x20 Startup
    (void*)&PlacementTool_Shutdown2_Impl, // +0x28 Shutdown2
    (void*)&PlacementTool_OnKey_Impl,     // +0x30 OnKey
    (void*)&PlacementTool_Serialize_Impl, // +0x38 Serialize (preview)
    (void*)&PlacementTool_Draw_Impl,      // +0x40 Draw
    (void*)&Tool_Ret0,                    // +0x48 Slot9 (can-deactivate? returns 0)
    (void*)&Tool_Ret0,                    // +0x50 Slot10
    (void*)&PlacementTool_GetSettingType, // +0x58 Slot11 GetToolSettingTypeId
    (void*)&PlacementTool_GetCursor,      // +0x60 Slot12 GetCursorId (=5)
};
extern "C" void RegisterPlacementToolVtable() {
    RegisterExeAddr(0x140161d20, (uintptr_t)g_PlacementTool_Vtable);
}
// PlacementTool_Ctor (0x1400fe850) inits the preview-state caches to -1: +0x50/+0x54
// (lastStartCol/Row), +0x58 lastCenterCell, +0x5c radiusDisplayIdx, +0x60 sceneObjectIdx.
// Tool_Alloc/InitInPlace only ZEROES [0x10,size) — leaving these 0, which Serialize
// reads as a live scene/highlight index and removes → crash on the bogus index 0.
static void PlacementTool_InitCtorFields_SA(void* p) {
    if (!p) return;
    uint8_t* t = static_cast<uint8_t*>(p);
    *reinterpret_cast<int64_t*>(t + 0x50) = -1;   // lastStartCol / lastStartRow
    *reinterpret_cast<int64_t*>(t + 0x58) = -1;   // lastCenterCell / radiusDisplayIdx
    *reinterpret_cast<int32_t*>(t + 0x60) = -1;   // sceneObjectIdx
}
extern "C" void* PlacementTool_CreateInPlace_SA(void* mem) {
    void* p = Tool_InitInPlace(mem, 0x70, g_PlacementTool_Vtable, 0);
    PlacementTool_InitCtorFields_SA(p);
    return p;
}
extern "C" void* PlacementTool_Factory_SA(void) {
    void* p = Tool_Alloc(0x70, g_PlacementTool_Vtable, 0);
    PlacementTool_InitCtorFields_SA(p);
    return p;
}
REGISTER_TYPE(PlacementTool, "Tool", &PlacementTool_Factory_SA, &PlacementTool_CreateInPlace_SA);

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
    EnsureStartupFuncs();  // fills the 4 g_TypeId_* globals (and logs them)

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

    uint32_t tCreate = g_TypeId_SizeDescription  ? *g_TypeId_SizeDescription  : 0; // CreatePlacedDescription
    uint32_t tMap    = g_TypeId_AreaDescription   ? *g_TypeId_AreaDescription   : 0; // MapDescription
    uint32_t tModel  = g_TypeId_FenceDescription  ? *g_TypeId_FenceDescription  : 0; // ModelDescription
    uint32_t tRadius = g_TypeId_RadiusComponent   ? *g_TypeId_RadiusComponent   : 0; // RadiusDescription

    bool haveCreate = false, haveMap = false, haveModel = false, haveRadius = false;
    if (comps && count > 0 && count < 256) {
        for (int i = 0; i < count; i++) {
            void* c = comps[i];
            if (!c) continue;
            typedef uint32_t (*getid_t)(void*);
            getid_t gid = *(getid_t*)(*(uintptr_t*)c + 0x10);
            uint32_t id = gid(c);
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
    PlacementTool_Startup_Impl(self);
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
// Requires PlacementTool_Startup_Impl to have run first (descComponent/mapComponent/
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
    typedef float* (*fn_GridToWorld_t)(int*, float*, float*, char, void*);
    static fn_GridToWorld_t gridToWorld = nullptr;
    if (!gridToWorld) gridToWorld = (fn_GridToWorld_t)ResolveAddr(Module::Exe, 0x14009f3e0);
    const float halfCell = 0.5f;
    pos[0] = pos[1] = pos[2] = 0.0f;
    if (gridToWorld) {
        int gw = grid[0];
        float centerOff = (float)(gw >> 1) - halfCell;
        int idxA = startRow * gw + startCol;
        float inA[3] = { (float)(idxA % gw) - centerOff, (float)(idxA / gw) - centerOff, 0.0f };
        float worldA[3] = {0.0f, 0.0f, 0.0f};
        gridToWorld(grid, worldA, inA, '\0', nullptr);

        int idxB = endRow * gw + endCol;
        float inB[3] = { (float)(idxB % gw) - centerOff, (float)(idxB / gw) - centerOff, 0.0f };
        float worldB[3] = {0.0f, 0.0f, 0.0f};
        gridToWorld(grid, worldB, inB, '\0', nullptr);

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

    typedef uint64_t (*fn_helper_d650_t)(void* self, int, int, int, int, const void*, float);
    static fn_helper_d650_t create = nullptr;
    if (!create) create = (fn_helper_d650_t)ResolveAddr(Module::Exe, 0x1400fd650);
    if (!create) return 0;
    uint64_t placed = create(self, startCol, startRow, endCol, endRow, pos, 0.0f /*yaw*/);
    return (int)(placed & 0xff);
}

extern "C" int PlacementTool_HarnessPlaceAtCell(void* toolSelf, int centerCell) {
    PlacementTool* self = (PlacementTool*)toolSelf;
    EnsureFuncs();

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
                    if (s_fn_FUN_1401000f0) s_fn_FUN_1401000f0(self);  // finalize (place sound)
                    return 1;
                }
            }
        }
    }
    if (s_fn_FUN_140100140) s_fn_FUN_140100140(self);  // fail (error sound)
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
    EnsureFuncs();

    if (!self->descComponent || !self->meshInstance) {
        fprintf(stderr, "[Preview] descComponent(+0x28)=%p meshInstance(+0x68)=%p — run Startup first\n",
                (void*)self->descComponent, (void*)self->meshInstance);
        fflush(stderr);
        return 0;
    }

    fprintf(stderr, "[Preview] fns: TransformFromYaw=%p SceneAddObject=%p GeomSetTransform=%p "
            "MeshUpdateHier=%p\n",
            (void*)s_fn_TransformFromYaw, (void*)s_fn_SceneAddObject,
            (void*)s_fn_GeomSetTransform, (void*)s_fn_MeshUpdateHier);
    fflush(stderr);

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
    if ((int)self->sceneObjectIdx < 0 && s_fn_SceneAddObject && scene && self->meshInstance) {
        self->sceneObjectIdx = (int)s_fn_SceneAddObject(scene, self->meshInstance);
    }

    // 2) Build the placement transform from yaw + cursor world offset (the fix:
    //    TransformFromYaw was null in standalone → zero transform → invisible).
    uint8_t localTransform[96];
    memset(localTransform, 0, sizeof(localTransform));
    float yawVal = 0.0f, zeroPitch = 0.0f;
    if (s_fn_TransformFromYaw) s_fn_TransformFromYaw(localTransform, &yawVal, &zeroPitch, pos);

    // Log the first 12 floats so a degenerate (all-zero) transform is obvious and the
    // translation (which should carry pos) is visible.
    const float* xf = reinterpret_cast<const float*>(localTransform);
    fprintf(stderr, "[Preview] transform[0..11]= %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f | "
            "%.2f %.2f %.2f %.2f\n",
            xf[0], xf[1], xf[2], xf[3], xf[4], xf[5], xf[6], xf[7], xf[8], xf[9], xf[10], xf[11]);

    if (s_fn_GeomSetTransform && self->meshInstance) s_fn_GeomSetTransform(self->meshInstance, localTransform);
    if (s_fn_MeshUpdateHier && self->meshInstance)   s_fn_MeshUpdateHier(self->meshInstance);

    bool degenerate = (xf[0] == 0.0f && xf[1] == 0.0f && xf[2] == 0.0f &&
                       xf[4] == 0.0f && xf[5] == 0.0f && xf[6] == 0.0f);
    fprintf(stderr, "[Preview] sceneObjectIdx=%d transform=%s — ghost mesh %s in the live scene\n",
            (int)self->sceneObjectIdx, degenerate ? "DEGENERATE(all-zero!)" : "valid",
            (self->sceneObjectIdx >= 0 && !degenerate) ? "SUBMITTED" : "NOT submitted");
    fflush(stderr);
    return (self->sceneObjectIdx >= 0 && !degenerate) ? 1 : 0;
}
#endif // STANDALONE_EXE
