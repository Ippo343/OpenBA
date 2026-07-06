// code/Game/Player/edittool.cpp
// Clean reimplementation of EditTool vtable methods from Application-x64.exe.
//
// EditTool (0x38 bytes) extends Tool (0x28 bytes) directly.
// Handles entity selection and editing in-game.
//
// Vtable at 0x140161ca0 (13 slots). Four are reimplemented here:
//   Slot0   Dtor       (0x14002db00) — scalar deleting destructor
//   Slot6   OnKey      (0x1400fd030) — entity selection via ray picking
//   Slot7   Startup    (0x1400fcda0) — dialog positioning near entity
//   Slot10  Serialize  (0x1400fd290) — compare entity reference
//
// Decompiled from Application-x64.exe.

#include "Player/edittool_funcs.h"
#include "EditTool.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// BANISHED_SELECT_DIAG=1 traces the live EditTool pick path (A/B-safe: off by default).
static int SelDiagOn() {
    static int s = -1;
    if (s < 0) { const char* e = std::getenv("BANISHED_SELECT_DIAG"); s = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s;
}

#ifdef STANDALONE_EXE
// Forward decl (full header is included in the STANDALONE factory block at file end).
namespace Object { uint32_t GetTypeIdByName(const char* name); }
#endif

// ============================================================
// EditTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_EditTool = nullptr;
static void* s_vtable_Tool     = nullptr;

static void EnsureVtables() {
    if (!s_vtable_EditTool) {
        s_vtable_EditTool = (void*)ResolveAddr(Module::Exe, EditToolAddr::Vtable);
        s_vtable_Tool     = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// ============================================================
// Object::Internal::~Internal — called from base dtor
// Resolved via the function pointer slot at 0x140152770
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
// EditTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: EditTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002db00
void* EditTool_Dtor_Impl(EditTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_EditTool;
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
// EditTool::Serialize (vtable +0x50, Slot10) — Compare entity reference
// ============================================================
// Calls InternalBasePtr::CheckPtr on self->lastEntityRef (+0x30),
// then compares *param_2 with self->lastEntityRef.
// Returns true if the entity references match.
//
// This is used to check whether a given entity is the one currently
// being edited by this tool (e.g., for highlight/selection state).
//
// Original: 0x1400fd290
bool EditTool_Serialize_Impl(EditTool* self, uint64_t* param_2) {
    // CheckPtr on self->lastEntityRef to resolve lazy/weak pointers
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)&self->lastEntityRef);
    }

    // Compare: does *param_2 match self->lastEntityRef?
    return *param_2 == self->lastEntityRef;
}

// ============================================================
// Locally resolved function pointers for Startup and OnKey
// ============================================================

// Helper: Entity_impl_0380 (0x140100380) — get entity screen bounding rectangle
typedef bool (*fn_Entity_impl_0380_t)(uintptr_t entity, void* entityIBP, void* rectOut);
static fn_Entity_impl_0380_t s_fn_Entity_impl_0380 = nullptr;

// Helper: Entity_impl_cc90 (0x1400fcc90) — fallback positioning when entity rect fails
typedef void (*fn_Entity_impl_cc90_t)(uintptr_t self);
static fn_Entity_impl_cc90_t s_fn_Entity_impl_cc90 = nullptr;

// Helper: EditTool_helper_6800 (0x140066800) — toggle page setup
typedef void (*fn_EditTool_helper_6800_t)(uintptr_t controller);
static fn_EditTool_helper_6800_t s_fn_EditTool_helper_6800 = nullptr;

// Helper: FUN_1401005a0 — compute camera ray from mouse position
typedef void (*fn_CameraRay_t)(uintptr_t keyMap, void* rayOrigin, void* rayDir);
static fn_CameraRay_t s_fn_CameraRay = nullptr;

// Helper: QuarryComponent_impl_a530 (0x1400aa530) — map hit test (was a dropped-page
// typo 0x14010a530, mid-function in CitizenAI_State_Wander — Level-1 caller-bug fix,
// confirmed by the CALL at 0x1400fd12a in EditTool_Draw; pathtool.cpp uses it correctly)
// RCX=mapComponent, RDX=rayOrigin, R8=rayDir, R9=hitDist, [RSP+0x20]=hitPosOut, [RSP+0x28]=distOut
typedef bool (*fn_MapHitTest_t)(uintptr_t mapComponent, float* rayOrigin, void* rayDir,
                                 float* hitDist, float* hitPosOut, float* distOut);
static fn_MapHitTest_t s_fn_MapHitTest = nullptr;

// Helper: FUN_1400a0280 — get entity at world cell
typedef void* (*fn_EntityAtCell_t)(uintptr_t entityList, void* retIBP, int cellIndex);
static fn_EntityAtCell_t s_fn_EntityAtCell = nullptr;

// Picking::Scene::Intersect — IAT dispatch slot [0x140152428]
typedef int (*fn_PickingIntersect_t)(void* scene, void* rayOrigin, void* rayDir,
                                      float* maxDist, void* hitPosOut, void* hitNormalOut);
static fn_PickingIntersect_t s_fn_PickingIntersect = nullptr;

// Picking::Scene::GetUserData — IAT dispatch slot [0x140152890]
typedef void* (*fn_PickingGetUserData_t)(void* scene, int hitId);
static fn_PickingGetUserData_t s_fn_PickingGetUserData = nullptr;

// Volume::Rectangle::Ctor — IAT dispatch slot [0x1401528a8]
typedef void (*fn_RectangleCtor_t)(void* self);
static fn_RectangleCtor_t s_fn_RectangleCtor = nullptr;

// DialogInstance::GetRoot — IAT dispatch slot [0x1401517a8]
// Returns InternalPtr<Element> BY VALUE: RCX=dialog, RDX=hidden retbuf (Phase-4 ABI).
typedef void* (*fn_GetRoot_t)(void* dialog, void* retbuf);
static fn_GetRoot_t s_fn_GetRoot = nullptr;

// DialogInstance::GetController — IAT dispatch slot [0x140151768]
// Member, non-trivial return ABI: RCX=dialog, RDX=retval, R8=typeId
typedef void* (*fn_GetController_t)(void* dialog, void* retval, uint32_t typeId);
static fn_GetController_t s_fn_GetController = nullptr;

// IsDerivedFrom — IAT dispatch slot [0x140152a98]
typedef bool (*fn_IsDerived_t)(void* obj, uint32_t typeId);
static fn_IsDerived_t s_fn_IsDerived = nullptr;

// InternalBasePtr::Ctor — IAT dispatch slot [0x140152a90]
typedef void* (*fn_IBPCtor_t)(void* self, void* internal);
static fn_IBPCtor_t s_fn_IBPCtor = nullptr;

// InternalBasePtr::CheckPtr — IAT dispatch slot [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* self);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// Element::GetPaddedSize — IAT dispatch slot [0x140152750]
typedef void (*fn_GetPaddedSize_t)(void* element, int* outWidth, int* outHeight);
static fn_GetPaddedSize_t s_fn_GetPaddedSize = nullptr;

// DialogInstance::SetUpperLeft — IAT dispatch slot [0x140151dd0]
typedef void (*fn_SetUpperLeft_t)(void* dialog, int x, int y, bool animate);
static fn_SetUpperLeft_t s_fn_SetUpperLeft = nullptr;

// Math::Vector2::Ctor — IAT dispatch slot [0x1401522d8]
typedef void (*fn_Vector2Ctor_t)(void* self, float* x, float* y);
static fn_Vector2Ctor_t s_fn_Vector2Ctor = nullptr;

// KeyMap::Pressed — IAT dispatch slot [0x140151758]
typedef bool (*fn_KeyMapPressed_t)(uintptr_t keyMap, int slot, int keyId);
static fn_KeyMapPressed_t s_fn_KeyMapPressed = nullptr;

// Type IDs resolved from .rdata
static uint32_t* s_pTypeId_ToolUI = nullptr;       // DWORD at 0x1401c5788
static uint32_t* s_pTypeId_PageToggleUI = nullptr;  // DWORD at 0x1401c4f00

static bool s_editFuncsResolved = false;

static void EnsureEditFuncs() {
    if (s_editFuncsResolved) return;

    // IAT dispatch slots: read function pointer from .rdata indirect slot
    auto resolveIAT = [](uintptr_t ghidraAddr) -> void* {
        uintptr_t slot = ResolveAddr(Module::Exe, ghidraAddr);
        if (slot) return *reinterpret_cast<void**>(slot);
        return nullptr;
    };

    // IAT slots
    s_fn_CheckPtr         = (fn_CheckPtr_t)resolveIAT(0x140152ab0);
    s_fn_GetController    = (fn_GetController_t)resolveIAT(0x140151768);
    s_fn_IsDerived        = (fn_IsDerived_t)resolveIAT(0x140152a98);
    s_fn_IBPCtor          = (fn_IBPCtor_t)resolveIAT(0x140152a90);
    s_fn_RectangleCtor    = (fn_RectangleCtor_t)resolveIAT(0x1401528a8);
    s_fn_GetRoot          = (fn_GetRoot_t)resolveIAT(0x1401517a8);
    s_fn_GetPaddedSize    = (fn_GetPaddedSize_t)resolveIAT(0x140152750);
    s_fn_SetUpperLeft     = (fn_SetUpperLeft_t)resolveIAT(0x140151dd0);
    s_fn_Vector2Ctor      = (fn_Vector2Ctor_t)resolveIAT(0x1401522d8);
    s_fn_KeyMapPressed    = (fn_KeyMapPressed_t)resolveIAT(0x140151758);
    s_fn_PickingIntersect = (fn_PickingIntersect_t)resolveIAT(0x140152428);
    s_fn_PickingGetUserData = (fn_PickingGetUserData_t)resolveIAT(0x140152890);

    // Direct exe function addresses
    s_fn_Entity_impl_0380     = (fn_Entity_impl_0380_t)ResolveAddr(Module::Exe, 0x140100380);
    s_fn_Entity_impl_cc90     = (fn_Entity_impl_cc90_t)ResolveAddr(Module::Exe, 0x1400fcc90);
    s_fn_EditTool_helper_6800 = (fn_EditTool_helper_6800_t)ResolveAddr(Module::Exe, 0x140066800);
    s_fn_CameraRay            = (fn_CameraRay_t)ResolveAddr(Module::Exe, 0x1401005a0);
    s_fn_MapHitTest           = (fn_MapHitTest_t)ResolveAddr(Module::Exe, 0x1400aa530);
    s_fn_EntityAtCell         = (fn_EntityAtCell_t)ResolveAddr(Module::Exe, 0x1400a0280);

    // Type IDs
    s_pTypeId_ToolUI       = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c5788);
    s_pTypeId_PageToggleUI = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4f00);

#ifdef STANDALONE_EXE
    // In standalone the raw .rdata IAT dispatch slots above resolve to 0 (unregistered),
    // which would leave the pick + reposition pipeline dead (KeyMapPressed null -> OnKey
    // returns before ever picking). Fall back to the engine_funcs pointers, which ARE
    // resolved by mangled export name from Runtime-x64.dll and work in standalone (the same
    // ones placement/utility use). Only override when the slot came back null.
    if (!s_fn_CheckPtr && fn_InternalBasePtr_CheckPtr)
        s_fn_CheckPtr = (fn_CheckPtr_t)fn_InternalBasePtr_CheckPtr;
    if (!s_fn_GetController && fn_DialogInstance_GetController)
        s_fn_GetController = (fn_GetController_t)fn_DialogInstance_GetController;
    if (!s_fn_IsDerived && fn_IsDerivedFrom)
        s_fn_IsDerived = (fn_IsDerived_t)fn_IsDerivedFrom;
    if (!s_fn_IBPCtor && fn_InternalBasePtr_Ctor)
        s_fn_IBPCtor = (fn_IBPCtor_t)fn_InternalBasePtr_Ctor;
    if (!s_fn_GetRoot && fn_DialogInstance_GetRoot)
        s_fn_GetRoot = (fn_GetRoot_t)fn_DialogInstance_GetRoot;   // 2-arg (self, retbuf) ABI
    if (!s_fn_GetPaddedSize && fn_Element_GetPaddedSize)
        s_fn_GetPaddedSize = (fn_GetPaddedSize_t)fn_Element_GetPaddedSize;
    if (!s_fn_SetUpperLeft && fn_DialogInstance_SetUpperLeft)
        s_fn_SetUpperLeft = (fn_SetUpperLeft_t)fn_DialogInstance_SetUpperLeft;
    if (!s_fn_KeyMapPressed && fn_KeyMap_Pressed)
        s_fn_KeyMapPressed = (fn_KeyMapPressed_t)fn_KeyMap_Pressed;
    // Picking::Scene::Intersect / GetUserData — Runtime exports, resolved by mangled name.
    if (!s_fn_PickingIntersect)
        s_fn_PickingIntersect = (fn_PickingIntersect_t)ResolveExport(
            "?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z");
    if (!s_fn_PickingGetUserData)
        s_fn_PickingGetUserData = (fn_PickingGetUserData_t)ResolveExport(
            "?GetUserData@Scene@Picking@@QEBAPEAXI@Z");

    // Reposition (Startup +0x38) type-ids: raw .rdata is NULL in standalone. Fill by name so
    // the GetController(ToolUI)/(EntityUI) lookups resolve. 0x1401c5788="ToolUI",
    // 0x1401c4f00="EntityUI" (the s_pTypeId_PageToggleUI name is a legacy misnomer — the
    // RegisterType writer for 0x1401c4f00 names it "EntityUI").
    // (GetTypeIdByName returns 0xFFFFFFFF before Factory::Startup; retry until a real id.)
    static uint32_t s_tid_ToolUI_sa = 0, s_tid_EntityUI_sa = 0;
    auto validTid = [](uint32_t v) { return v != 0 && v != 0xFFFFFFFFu; };
    if (!s_pTypeId_ToolUI || !validTid(s_tid_ToolUI_sa)) {
        if (!validTid(s_tid_ToolUI_sa)) s_tid_ToolUI_sa = ::Object::GetTypeIdByName("ToolUI");
        s_pTypeId_ToolUI = &s_tid_ToolUI_sa;
    }
    if (!s_pTypeId_PageToggleUI || !validTid(s_tid_EntityUI_sa)) {
        if (!validTid(s_tid_EntityUI_sa)) s_tid_EntityUI_sa = ::Object::GetTypeIdByName("EntityUI");
        s_pTypeId_PageToggleUI = &s_tid_EntityUI_sa;
    }
#endif

    s_editFuncsResolved = true;
}

// ============================================================
// EditTool::Startup (vtable +0x38, Slot7) — Dialog positioning near entity
// ============================================================
// Gets dialog controller via IBP at self+0x28, casts with IsDerivedFrom
// to ToolUI and PageToggleUI types. If ToolUI controller's flag at +0x40
// is false, gets entity screen rect and positions dialog near it.
//
// Original: 0x1400fcda0
void EditTool_Startup_Impl(EditTool* self) {
    EnsureEditFuncs();

    uintptr_t selfAddr = (uintptr_t)self;

    // Early out: if entity IBP at +0x28 is null, nothing to do
    if (self->entityRef == 0)
        return;

    void* entityIBP = (void*)&self->entityRef;  // &self+0x28

    // Resolve lazy/weak pointer
    if (s_fn_CheckPtr) s_fn_CheckPtr(entityIBP);

    // If the referenced info dialog was destroyed while this EditTool kept its
    // selection — in standalone the panel's close (ToolUI CloseDialog) does NOT tear
    // the tool down the way retail's EntityUI::Slot9 -> SetEditMode(0) does — CheckPtr
    // marks the IBP with the -1 "dead reference" sentinel (0xffffffffffffffff). The
    // code below dereferences it and calls GetController(-1,...), which faults deep in
    // the Runtime. Treat a dead / non-canonical ref as "nothing selected": clear it so
    // the per-frame tick becomes a safe no-op (line above already early-outs on 0).
    // In DLL mode the dialog is always live here (retail destroys the tool on close),
    // so this guard never triggers.
    {
        uintptr_t resolved = *(uintptr_t*)entityIBP;
        if (resolved < 0x10000 || resolved > 0x00007FFFFFFFFFFFULL) {
            self->entityRef = 0;
            return;
        }
    }

    // Get ToolUI controller from dialog
    uintptr_t dialogPtr = *(uintptr_t*)entityIBP;  // deref IBP to get dialog
    uintptr_t controllerResult = 0;
    uintptr_t toolUIObj = 0;

    if (s_fn_GetController && s_pTypeId_ToolUI) {
        s_fn_GetController((void*)dialogPtr, &controllerResult, *s_pTypeId_ToolUI);
        if (controllerResult != 0) {
            if (s_fn_CheckPtr) s_fn_CheckPtr(&controllerResult);
            void* ctrObj = (void*)controllerResult;
            if (s_fn_IsDerived && s_fn_IsDerived(ctrObj, *s_pTypeId_ToolUI)) {
                toolUIObj = controllerResult;
            }
        }
    }

    // Wrap in IBP (local_48 in decompilation)
    uintptr_t toolUIIBP = 0;
    if (s_fn_IBPCtor) s_fn_IBPCtor(&toolUIIBP, (void*)toolUIObj);

    // Get PageToggleUI controller from the same dialog
    if (s_fn_CheckPtr) s_fn_CheckPtr(entityIBP);
    dialogPtr = *(uintptr_t*)entityIBP;
    controllerResult = 0;
    uintptr_t pageToggleObj = 0;

    if (s_fn_GetController && s_pTypeId_PageToggleUI) {
        s_fn_GetController((void*)dialogPtr, &controllerResult, *s_pTypeId_PageToggleUI);
        if (controllerResult != 0) {
            if (s_fn_CheckPtr) s_fn_CheckPtr(&controllerResult);
            void* ctrObj = (void*)controllerResult;
            if (s_fn_IsDerived && s_fn_IsDerived(ctrObj, *s_pTypeId_PageToggleUI)) {
                pageToggleObj = controllerResult;
            }
        }
    }

    // Wrap in IBP (local_50 in decompilation)
    uintptr_t pageToggleIBP = 0;
    if (s_fn_IBPCtor) s_fn_IBPCtor(&pageToggleIBP, (void*)pageToggleObj);

    // Standalone safety: this reposition only runs once an entity's info dialog is open, a
    // path never exercised until GUI select landed. If either controller came back null (a
    // missing standalone controller, or an entity whose dialog lacks a ToolUI/EntityUI), we
    // cannot position — skip rather than deref a null IBP. The dialog still shows at its .rsc
    // default position. Retail always has both controllers.
    if (toolUIIBP == 0 || pageToggleIBP == 0)
        return;

    // Check ToolUI controller flag at +0x40
    if (s_fn_CheckPtr) s_fn_CheckPtr(&toolUIIBP);
    if (*(char*)(toolUIIBP + 0x40) != 0) {
        // Flag is set — skip positioning
        return;
    }

    // Construct a Volume::Rectangle for entity bounding rect
    uint8_t rectangle[32];
    memset(rectangle, 0, sizeof(rectangle));
    if (s_fn_RectangleCtor) s_fn_RectangleCtor(rectangle);

    // Get entity reference from PageToggleUI controller at +0x30
    if (s_fn_CheckPtr) s_fn_CheckPtr(&pageToggleIBP);
    uintptr_t entityObj = *(uintptr_t*)(pageToggleIBP + 0x30);

    // Get entity's screen bounding rectangle
    uintptr_t entityParam = entityObj;
    bool gotRect = false;
    if (s_fn_Entity_impl_0380) {
        gotRect = s_fn_Entity_impl_0380(entityObj, &entityParam, rectangle);
    }

    if (!gotRect) {
        // Fallback positioning
        if (s_fn_Entity_impl_cc90) s_fn_Entity_impl_cc90(selfAddr);
        return;
    }

    // Get dialog root element and its padded size
    if (s_fn_CheckPtr) s_fn_CheckPtr(entityIBP);
    dialogPtr = *(uintptr_t*)entityIBP;
    void* rootIBPPtr = nullptr;
    uintptr_t rootRetbuf = 0;  // InternalPtr<Element> return-by-value buffer
    if (s_fn_GetRoot) { s_fn_GetRoot((void*)dialogPtr, &rootRetbuf); rootIBPPtr = &rootRetbuf; }

    if (s_fn_CheckPtr && rootIBPPtr) s_fn_CheckPtr(rootIBPPtr);

    int dialogWidth = 0, dialogHeight = 0;
    uintptr_t rootElement = rootIBPPtr ? *(uintptr_t*)rootIBPPtr : 0;
    if (s_fn_GetPaddedSize && rootElement) {
        s_fn_GetPaddedSize((void*)rootElement, &dialogWidth, &dialogHeight);
    }

    // Calculate dialog position from entity rect
    // rectangle layout: +0x00=left(float), +0x04=top(float), +0x08..+0x10=more rect data,
    //                   +0x10=right?, etc.
    // From disasm: rect+0x00 and rect+0x10 are used (rect.left and rect.right)
    // local_res8._4_4_ = rect[+0x04] (y component)
    // local_res20 = rect[+0x00] (x component)
    // The formula: midX = (rect.left + rect.right) * 0.5
    float rectLeft = *(float*)&rectangle[0];
    float rectTop  = *(float*)&rectangle[4];
    float rectRight = *(float*)&rectangle[16]; // from local_28 mapping to rect+0x10

    // From disassembly analysis:
    // local_res20 = rect[0] (x), local_res8._4_4_ = rect[4] (y)
    // local_38._0_4_ = rect[16] (x2)
    // midX = (rect[0] + rect[16]) * 0.5f
    float midX = (rectLeft + rectRight) * 0.5f;

    // Construct Vector2 for dialog position
    float vec2[2];
    if (s_fn_Vector2Ctor) {
        s_fn_Vector2Ctor(vec2, &midX, &rectTop);
    } else {
        vec2[0] = midX;
        vec2[1] = rectTop;
    }

    // Position = (vec2.x - dialogWidth/2, vec2.y - dialogHeight) / uiScale
    int posX = (int)vec2[0] - (dialogWidth >> 1);
    int posY = (int)vec2[1] - dialogHeight;

    // Get UI scale factor from dialog's UI instance
    if (s_fn_CheckPtr) s_fn_CheckPtr(entityIBP);
    dialogPtr = *(uintptr_t*)entityIBP;
    // dialog+0x10 = UI instance, UI+0xdc = scale factor
    float uiScale = *(float*)(*(uintptr_t*)(dialogPtr + 0x10) + 0xdc);

    // Apply scale and set position
    int scaledX = (int)((float)posX / uiScale);
    int scaledY = (int)((float)posY / uiScale);

    if (s_fn_CheckPtr) s_fn_CheckPtr(entityIBP);
    dialogPtr = *(uintptr_t*)entityIBP;
    if (s_fn_SetUpperLeft) {
        s_fn_SetUpperLeft((void*)dialogPtr, scaledX, scaledY, true);
    }

    // If game world flag bit 10 is set and pageToggle controller exists, call helper
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uint32_t worldFlags = *(uint32_t*)(gameWorld + 0xd0);
    if (((worldFlags >> 10) & 1) != 0 && pageToggleIBP != 0) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(&pageToggleIBP);
        if (s_fn_EditTool_helper_6800) {
            s_fn_EditTool_helper_6800(pageToggleIBP);
        }
    }
}

// ============================================================
// EditTool::OnKey (vtable +0x30, Slot6) — Entity selection via ray picking
// ============================================================
// If param2 is true and tool is in a valid state, reads KeyMap,
// computes camera ray, does map hit test + picking scene intersection
// to find entities under cursor. Calls fn_EditTool_SelectEntity to
// select/deselect the found entity.
//
// Original: 0x1400fd030
uint64_t EditTool_OnKey_Impl(EditTool* self, char param2) {
    EnsureEditFuncs();

    uintptr_t selfAddr = (uintptr_t)self;

    if (SelDiagOn()) {
        static int calls = 0;
        if (calls < 3) { calls++;
            printf("[SelDiag] OnKey#%d param2=%d KMP=%p CamRay=%p MapHit=%p AtCell=%p "
                   "Pick=%p GetUD=%p keyExref=%p\n", calls, param2,
                   (void*)s_fn_KeyMapPressed, (void*)s_fn_CameraRay, (void*)s_fn_MapHitTest,
                   (void*)s_fn_EntityAtCell, (void*)s_fn_PickingIntersect,
                   (void*)s_fn_PickingGetUserData, (void*)g_pKeyMapObjectExref);
            fflush(stdout);
        }
    }

    if (param2 == 0) return 1;

    // Check tool state: self+0x10 -> state struct
    uintptr_t statePtr = *(uintptr_t*)(selfAddr + 0x10);
    bool flag80 = *(char*)(statePtr + 0x80) != 0;
    if (flag80 && *(uintptr_t*)(statePtr + 0x90) == 0) {
        return 1;
    }

    // Get KeyMap via triple deref of external reference
    if (!g_pKeyMapObjectExref) return 1;
    uintptr_t keyMapExref = (uintptr_t)*g_pKeyMapObjectExref;
    uintptr_t keyMapPtr = *(uintptr_t*)keyMapExref;
    uintptr_t keyMap = *(uintptr_t*)keyMapPtr;

    // Check KeyMap::Pressed(keyMap, 0, 5)
    if (!s_fn_KeyMapPressed) return 1;
    bool pressed = s_fn_KeyMapPressed(keyMap, 0, 5);
    if (SelDiagOn() && pressed)
        printf("[SelDiag] action-5 PRESSED (keyMap=%p) — running pick\n", (void*)keyMap);
    if (!pressed) return 1;

    // Get world data pointers
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xf8);

    // Compute camera ray from mouse position
    // rayOrigin = 12 bytes (Vector3), rayDir = 12 bytes (Vector3)
    float rayOrigin[3] = {0.0f, 0.0f, 0.0f};
    float rayDir[3] = {0.0f, 0.0f, 0.0f};
    float maxDist = 0.0f;

    if (s_fn_CameraRay) {
        s_fn_CameraRay(keyMap, rayOrigin, rayDir);
    }

    // Map hit test to find world position under cursor
    float hitPos[2] = {0.0f, 0.0f};
    float hitDistBuf[2] = {0.0f, 0.0f};  // extra space for output
    float hitMaxDist = 400.0f;   // 0x43c80000
    uintptr_t foundEntity = 0;

    uintptr_t mapComponent = *(uintptr_t*)(gameWorld + 0x110);
    int* mapSize = *(int**)(gameWorld + 0xf8);

    if (s_fn_MapHitTest && mapComponent) {
        float maxDistArr[2];
        maxDistArr[0] = 400.0f;  // 0x43c80000
        maxDistArr[1] = 0.0f;

        bool mapHit = s_fn_MapHitTest(mapComponent, rayOrigin, rayDir,
                                       maxDistArr, hitPos, &maxDist);

        if (mapHit) {
            int gridSize = *mapSize;
            int halfGrid = gridSize >> 1;

            // Convert world position to grid cell
            int cellX = (int)(hitPos[0] + (float)halfGrid);
            int cellY = (int)(hitPos[1] + (float)halfGrid);

            // Clamp to grid bounds
            if (cellX < 0) cellX = 0;
            if (cellX >= gridSize) cellX = gridSize - 1;
            if (cellY < 0) cellY = 0;
            if (cellY >= gridSize) cellY = gridSize - 1;

            int cellIndex = gridSize * cellY + cellX;

            if (cellIndex > 0 && s_fn_EntityAtCell) {
                uintptr_t retIBP = 0;
                void* result = s_fn_EntityAtCell(entityList, &retIBP, cellIndex);
                if (result) foundEntity = *(uintptr_t*)result;
            }
            if (SelDiagOn())
                printf("[SelDiag] mapHit=1 hitPos=(%.2f,%.2f) cell=(%d,%d) cellEntity=%p\n",
                       hitPos[0], hitPos[1], cellX, cellY, (void*)foundEntity);
        } else if (SelDiagOn()) {
            printf("[SelDiag] mapHit=0 (ray missed terrain) ray o=(%.2f,%.2f,%.2f) d=(%.2f,%.2f,%.2f)\n",
                   rayOrigin[0], rayOrigin[1], rayOrigin[2], rayDir[0], rayDir[1], rayDir[2]);
        }
    } else if (SelDiagOn()) {
        printf("[SelDiag] MapHitTest skipped (MapHit=%p mapComp=%p)\n",
               (void*)s_fn_MapHitTest, (void*)mapComponent);
    }

    // Picking scene intersection (3D ray picking for precise entity selection)
    void* pickingScene = *(void**)(gameWorld + 0x70);
    float hitNormal[3] = {0.0f, 0.0f, 0.0f};
    float hitPoint[3] = {0.0f, 0.0f, 0.0f};

    if (s_fn_PickingIntersect && pickingScene) {
        maxDist = 400.0f;  // reset max distance for picking
        int hitId = s_fn_PickingIntersect(pickingScene, rayOrigin, rayDir,
                                           &maxDist, hitPoint, hitNormal);
        if (hitId >= 0) {
            // Get the entity associated with this picking hit
            if (s_fn_PickingGetUserData) {
                void* userData = s_fn_PickingGetUserData(pickingScene, hitId);
                uintptr_t pickIBP = 0;
                if (s_fn_IBPCtor) s_fn_IBPCtor(&pickIBP, userData);
                foundEntity = pickIBP;
            }
        }
        if (SelDiagOn())
            printf("[SelDiag] Picking hitId=%d -> foundEntity=%p\n", hitId, (void*)foundEntity);
    } else if (SelDiagOn()) {
        printf("[SelDiag] Picking skipped (Pick=%p scene=%p)\n",
               (void*)s_fn_PickingIntersect, pickingScene);
    }

    // Select the found entity if valid and different from current
    if (foundEntity != 0) {
        // If flag80 is set, verify entity matches expected type
        if (flag80) {
            if (s_fn_CheckPtr) s_fn_CheckPtr(&foundEntity);
            uintptr_t entityTypeRef = *(uintptr_t*)(foundEntity + 0x28);
            uintptr_t expectedType = *(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x10) + 0x90);
            if (entityTypeRef != expectedType) {
                return 1;  // type mismatch, skip
            }
        }

        // Check if entity is different from last selected
        if (s_fn_CheckPtr) s_fn_CheckPtr(&foundEntity);
        if (self->lastEntityRef != foundEntity) {
            // Call EditTool_SelectEntity (0x1400fd3b0)
            uintptr_t entityParam = foundEntity;
            if (fn_EditTool_SelectEntity) {
                fn_EditTool_SelectEntity((void*)selfAddr, &entityParam);
            }
            if (SelDiagOn())
                printf("[SelDiag] SELECT entity=%p -> dialog(self+0x28)=%p\n",
                       (void*)foundEntity, *(void**)(selfAddr + 0x28));
        }
    } else if (SelDiagOn()) {
        printf("[SelDiag] no entity under cursor — nothing selected\n");
    }

    return 1;
}

// ============================================================
// Standalone factory + vtable (factory cascade).
// EditTool: size 0x38, parent Tool, 13-slot vtable @ 0x140161ca0.
// ============================================================
#ifdef STANDALONE_EXE
#include "tool_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t EditTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("EditTool");
    return c;
}
// Slot11 (+0x58) GetSettingTypeId — EditTool has no ToolSetting. Retail (0x140004950)
// writes 0 into the caller's retbuf and returns it; SetEditMode reads *result and skips
// setting creation when 0. A plain Tool_Ret0 (returns the *value* 0 = null ptr) would make
// SetEditMode deref null — the same trap Phase 3 fixed for PlacementTool. Return the retbuf.
static uint32_t* EditTool_GetSettingTypeId(void*, uint32_t* retbuf) {
    if (retbuf) *retbuf = 0;
    return retbuf;
}
static void* g_EditTool_Vtable[13] = {
    (void*)&EditTool_Dtor_Impl,      // +0x00 Dtor
    (void*)&Tool_Slot1,              // +0x08
    (void*)&EditTool_GetTypeId,      // +0x10 GetTypeId
    (void*)&Tool_Persist,            // +0x18 Serialize (base)
    (void*)&Tool_Ret0,               // +0x20
    (void*)&Tool_Ret0,               // +0x28
    (void*)&EditTool_OnKey_Impl,     // +0x30 OnKey
    (void*)&EditTool_Startup_Impl,   // +0x38 Startup
    (void*)&Tool_Ret0,               // +0x40
    (void*)&Tool_Ret0,               // +0x48
    (void*)&EditTool_Serialize_Impl, // +0x50 Serialize (entity ref)
    (void*)&EditTool_GetSettingTypeId, // +0x58 GetSettingTypeId (writes 0 to retbuf)
    (void*)&Tool_Ret0,               // +0x60
};
extern "C" void RegisterEditToolVtable() {
    RegisterExeAddr(0x140161ca0, (uintptr_t)g_EditTool_Vtable);
}

// Cursor-free select harness (BANISHED_TOOL_HARNESS=select). The live pick needs the OS
// cursor over an entity, which the headless settle-frame lacks. This mirrors OnKey's
// cell-fallback: scan the region map around the start-town centre via EntityAtCell and
// drive fn_EditTool_SelectEntity on each occupant until one opens an info dialog
// (editTool+0x28 goes non-null). Entities without a UIDescription (trees/rocks) safely
// no-op, so the first building/citizen hit is what opens. Returns 1 if a dialog opened.
// Drive the select path on ONE specific entity (a definitive UIDescription-bearing building
// from the founding-house capture). Returns 1 if an info dialog opened (editTool+0x28 set).
extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity) {
    EnsureEditFuncs();
    if (!editTool || !entity) return 0;
    if (!fn_EditTool_SelectEntity) { printf("[EditSel] fn_EditTool_SelectEntity null (M2 unported?)\n"); return 0; }
    // Clear any prior selection ref so d3b0 doesn't early-out on a repeat.
    uintptr_t entityParam = (uintptr_t)entity;
    fn_EditTool_SelectEntity(editTool, &entityParam);
    void* dlg = *(void**)((uintptr_t)editTool + 0x28);
    printf("[EditSel] direct select entity=%p -> dialog=%p (%s)\n",
           entity, dlg, dlg ? "OPENED" : "no dialog");
    return dlg != 0 ? 1 : 0;
}

// Dump a dialog's element tree with geometry + flags so we can see WHY injected
// content isn't drawing (zero size? off-screen? visibility flag?). Element layout:
//   +0x10 name (System::String) · +0x34 flags · +0x38..0x58 rect fields
//   +0xa0 children array · +0xa8 child count · vtable[2]=GetTypeId
static void DumpCitizenElemTree(void* elem, int depth, int* counter) {
    if (!elem || depth > 10 || *counter > 200) return;
    (*counter)++;
    uintptr_t e = (uintptr_t)elem;
    char* nameData = *(char**)(e + 0x10);
    int len = nameData ? *(int*)(nameData - 8) : 0;
    uint32_t tid = 0;
    { void** vt = *(void***)elem;
      if (vt && vt[2]) { typedef uint32_t(*G)(void*); tid = ((G)vt[2])(elem); } }
    uint32_t flags = *(uint32_t*)(e + 0x34);
    // Element bounds: +0x68 x, +0x6c y, +0x70 w, +0x74 h (ints) — from
    // TabElement::UpdateContainers geometry math.
    int bx = *(int*)(e + 0x68), by = *(int*)(e + 0x6c);
    int bw = *(int*)(e + 0x70), bh = *(int*)(e + 0x74);
    printf("[CitTree] %*selem=%p tid=%u flags=0x%x name='%.*s' | rect=(%d,%d %dx%d)\n",
           depth * 2, "", elem, tid, flags, len > 0 ? len : 0, nameData ? nameData : "",
           bx, by, bw, bh);
    void** kids = *(void***)(e + 0xa0);
    int kc = *(int*)(e + 0xa8);
    for (int i = 0; kids && i < kc; i++) DumpCitizenElemTree(kids[i], depth + 1, counter);
}

// Drive the select path on a CITIZEN. Citizens aren't in the region-map cell array
// (they move) — they register in the Picking::Scene via PickingComponent OnAdded /
// the move-tick (see citizenai_executor.cpp RegisterCitizenPickMesh). Their pick
// userData is the entity backref we can recover with Picking::Scene::GetUserData.
// This bypasses the cursor ray (headless harness) but exercises the exact chain a
// real click uses: pick userData -> EditTool_SelectEntity -> CitizenUI dialog.
extern "C" int EditTool_HarnessSelectCitizen(void* editTool) {
    EnsureEditFuncs();
    if (!editTool) { printf("[EditSel] no active EditTool\n"); return 0; }
    if (!fn_EditTool_SelectEntity || !s_fn_PickingGetUserData) {
        printf("[EditSel] citizen select unavailable (SelectEntity=%p GetUserData=%p)\n",
               (void*)fn_EditTool_SelectEntity, (void*)s_fn_PickingGetUserData);
        return 0;
    }
    void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x70);
    if (!scene) { printf("[EditSel] picking scene null\n"); return 0; }

    // Citizens register with low pick-ids (0..N). Probe the first several; the first
    // that yields a live userData entity gets selected.
    for (int id = 0; id < 16; id++) {
        void* userData = s_fn_PickingGetUserData(scene, id);
        if (!userData) continue;
        printf("[EditSel] citizen pick-id %d -> entity=%p; selecting...\n", id, userData);
        uintptr_t entityParam = (uintptr_t)userData;
        fn_EditTool_SelectEntity(editTool, &entityParam);
        void* dlg = *(void**)((uintptr_t)editTool + 0x28);
        printf("[EditSel] citizen select entity=%p -> dialog=%p (%s)\n",
               userData, dlg, dlg ? "OPENED" : "no dialog");
        if (dlg) {
            if (std::getenv("BANISHED_DIALOG_TRACE")) {
                void* root = *(void**)((uintptr_t)dlg + 0x50);
                printf("[CitTree] dialog=%p root=%p — element tree:\n", dlg, root);
                int counter = 0;
                DumpCitizenElemTree(root, 0, &counter);
                printf("[CitTree] (%d elements)\n", counter);
            }
            return 1;
        }
    }
    printf("[EditSel] no citizen pick userData found in ids 0..15\n");
    return 0;
}

extern "C" int EditTool_HarnessSelectNearby(void* editTool) {
    EnsureEditFuncs();
    if (!editTool) { printf("[EditSel] no active EditTool\n"); return 0; }
    if (!fn_EditTool_SelectEntity) { printf("[EditSel] fn_EditTool_SelectEntity null (M2 unported?)\n"); return 0; }

    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xf8);
    int* mapSize = *(int**)(gameWorld + 0xf8);
    int gridSize = (mapSize && *mapSize > 0) ? *mapSize : 256;
    int tried = 0, found = 0;

    // Expanding Chebyshev rings around centre (124,124), same valley the placement/
    // removal harnesses target.
    for (int ring = 0; ring <= 16; ring++) {
        for (int dy = -ring; dy <= ring; dy++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (ring > 0 && dx > -ring && dx < ring && dy > -ring && dy < ring)
                    continue;  // ring perimeter only
                int cx = 124 + dx, cy = 124 + dy;
                if (cx < 0 || cx >= gridSize || cy < 0 || cy >= gridSize) continue;
                int cellIndex = gridSize * cy + cx;
                if (cellIndex <= 0 || !s_fn_EntityAtCell) continue;

                uintptr_t retIBP = 0;
                void* result = s_fn_EntityAtCell(entityList, &retIBP, cellIndex);
                uintptr_t entity = result ? *(uintptr_t*)result : 0;
                if (!entity) continue;
                found++;

                uintptr_t entityParam = entity;
                fn_EditTool_SelectEntity(editTool, &entityParam);
                tried++;
                void* dlg = *(void**)((uintptr_t)editTool + 0x28);
                if (dlg != 0) {
                    printf("[EditSel] cell(%d,%d) entity=%p OPENED info dialog=%p "
                           "(tried=%d occupants=%d)\n", cx, cy, (void*)entity, dlg, tried, found);
                    return 1;
                }
            }
        }
    }
    printf("[EditSel] no dialog opened after scanning rings (occupants=%d tried=%d) — "
           "entities may lack a UIDescription\n", found, tried);
    return 0;
}
extern "C" void* EditTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x38, g_EditTool_Vtable, 0);}
extern "C" void* EditTool_Factory_SA(void) {
    return Tool_Alloc(0x38, g_EditTool_Vtable, 0);
}
REGISTER_TYPE(EditTool, "Tool", &EditTool_Factory_SA, &EditTool_CreateInPlace_SA);
#endif // STANDALONE_EXE
