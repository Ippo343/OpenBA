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

// Helper: QuarryComponent_impl_a530 (0x14010a530) — map hit test
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
typedef void* (*fn_GetRoot_t)(void* dialog);
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
    s_fn_MapHitTest           = (fn_MapHitTest_t)ResolveAddr(Module::Exe, 0x14010a530);
    s_fn_EntityAtCell         = (fn_EntityAtCell_t)ResolveAddr(Module::Exe, 0x1400a0280);

    // Type IDs
    s_pTypeId_ToolUI       = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c5788);
    s_pTypeId_PageToggleUI = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4f00);

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
    if (s_fn_GetRoot) rootIBPPtr = s_fn_GetRoot((void*)dialogPtr);

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
                foundEntity = *(uintptr_t*)result;
            }
        }
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
        }
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
    (void*)&Tool_Ret0,               // +0x58
    (void*)&Tool_Ret0,               // +0x60
};
extern "C" void RegisterEditToolVtable() {
    RegisterExeAddr(0x140161ca0, (uintptr_t)g_EditTool_Vtable);
}
extern "C" void* EditTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x38, g_EditTool_Vtable, 0);}
extern "C" void* EditTool_Factory_SA(void) {
    return Tool_Alloc(0x38, g_EditTool_Vtable, 0);
}
REGISTER_TYPE(EditTool, "Tool", &EditTool_Factory_SA, &EditTool_CreateInPlace_SA);
#endif // STANDALONE_EXE
