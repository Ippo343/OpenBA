// code/Game/Player/bridgetool.cpp
// Clean reimplementation of BridgeTool vtable methods from Application-x64.exe.
//
// BridgeTool (0x40 bytes) extends DragTool (0x38 bytes).
// Handles bridge placement: path calculation, validation, and cost estimation.
//
// Vtable at 0x140161b00 (15 slots). Four are reimplemented here:
//   Slot0  Dtor     (0x14002d7f0) — scalar deleting destructor
//   Slot4  Startup  (0x1400fb7a0) — find BridgeDescription component on entity
//   Slot13 CalcPath (0x1400f9cd0) — build bridge segment entities along path
//   Slot14 CalcCost (0x1400facf0) — validate path and return cost/length
//
// Helper functions (exe-internal, resolved at runtime):
//   SnapEndCell   (0x1400fb710) — snaps end cell to same row or column as drag start
//   ValidatePath  (0x1400fb2d0) — validates path and returns bridge body length
//
// Decompiled from Application-x64.exe.

#include "Player/bridgetool_funcs.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// BridgeTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_BridgeTool = nullptr;
static void* s_vtable_DragTool   = nullptr;
static void* s_vtable_Tool       = nullptr;

static void EnsureVtables() {
    if (!s_vtable_BridgeTool) {
        s_vtable_BridgeTool = (void*)ResolveAddr(Module::Exe, BridgeToolAddr::Vtable);
        s_vtable_DragTool   = (void*)ResolveAddr(Module::Exe, DragToolAddr::Vtable);
        s_vtable_Tool       = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
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
        // The dtor calls through [0x140152770] which is a resolved func ptr slot
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// BridgeTool::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: BridgeTool -> DragTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002d7f0
void* BridgeTool_Dtor(BridgeTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_BridgeTool;
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
// BridgeTool::Startup (vtable +0x20, Slot4)
// ============================================================
// Searches the entity's component list for a component matching the
// CreateBridgeDescription type ID (DAT_1401c73f8). Stores the found
// component pointer (or nullptr) in self->bridgeDesc (+0x38).
//
// The entity at self->entity (+0x20) is a ComponentEntity with:
//   +0x28: void** componentArray  — array of component pointers
//   +0x30: int    componentCount  — number of components
//
// Each component has a vtable with GetTypeId at +0x10 (returns uint32_t).
// After finding a match by type ID, fn_IsDerivedFrom confirms the cast.
//
// Original: 0x1400fb7a0
void BridgeTool_Startup(BridgeTool* self) {
    uintptr_t entity = (uintptr_t)self->descriptionPtr;
    void* result = nullptr;

    if (!entity || !g_TypeId_CreateBridgeDescription || !fn_IsDerivedFrom) {
        self->bridgeDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *g_TypeId_CreateBridgeDescription;

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // Call vtable slot +0x10: GetTypeId() -> uint32_t
        typedef uint32_t (*fn_GetTypeId_t)(void* self);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom before casting
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                result = comp;
            }
            break;
        }
    }

    self->bridgeDesc = result;
}

// ============================================================
// BridgeTool::CalcCost (vtable +0x70, Slot14)
// ============================================================
// Calculates bridge placement cost between startCell and endCell.
//
// Steps:
//   1. Initialize output params to defaults (valid=1, costA=0, costB=0, length=1)
//   2. If either cell is negative (invalid), return 3 (failure)
//   3. Snap endCell to same row or column using SnapEndCell helper
//   4. Validate path using ValidatePath helper
//   5. On success: set all cost outputs to bridge body length, return 0
//   6. On failure: return 3
//
// Return value semantics:
//   0 = success (bridge can be placed)
//   3 = failure (invalid cells or path validation failed)
//
// Original: 0x1400facf0
uint64_t BridgeTool_CalcCost(BridgeTool* self, int startCell, int endCell,
                              uint32_t* outValid, int* outCostB, int* outCostA, int* outLength) {
    // Initialize outputs to defaults
    *outCostA = 0;
    *outCostB = 0;
    *outValid = 1;
    *outLength = 1;

    // Validate cell indices
    if (startCell < 0 || endCell < 0) {
        return 3;
    }

    // Snap end cell to same row or column as drag start
    if (!fn_BridgeTool_SnapEndCell) return 3;
    int snappedEnd = fn_BridgeTool_SnapEndCell(self, endCell);

    // Validate the bridge path
    if (!fn_BridgeTool_ValidatePath) return 3;
    int bridgeLength = 0;
    int valid = fn_BridgeTool_ValidatePath(self, startCell, snappedEnd, &bridgeLength);

    // Diagnostics: confirm the two helpers are bound to the ports (they resolved to 0 before
    // reg_player_bridge.cpp existed, which made every drag return 3 = invalid). Logs the
    // first call, the first real (non-degenerate) drag, and the first validating span —
    // enough to tell "never runs" from "runs but always rejects".
    {
        static bool s_first = false, s_drag = false, s_valid = false;
        const bool isDrag = (startCell != snappedEnd);
        if (!s_first || (isDrag && !s_drag) || (valid && !s_valid)) {
            if (!s_first) s_first = true;
            if (isDrag)   s_drag  = true;
            if (valid)    s_valid = true;
            uintptr_t d = (uintptr_t)self->bridgeDesc;
            fprintf(stderr, "[Bridge] CalcCost: start=%d end=%d snapped=%d -> valid=%d len=%d"
                    "  (Snap=%p Validate=%p desc=%p body=%08x approach=%08x req=%08x "
                    "neigh=%08x radius=%d)\n",
                    startCell, endCell, snappedEnd, valid, bridgeLength,
                    (void*)fn_BridgeTool_SnapEndCell, (void*)fn_BridgeTool_ValidatePath,
                    (void*)d,
                    d ? *(uint32_t*)(d + 0x20) : 0, d ? *(uint32_t*)(d + 0x24) : 0,
                    d ? *(uint32_t*)(d + 0x2c) : 0, d ? *(uint32_t*)(d + 0x30) : 0,
                    d ? *(int*)(d + 0x34) : -1);
            fflush(stderr);
        }
    }

    if ((char)valid != '\0') {
        // Success: all cost outputs = bridge body length
        *outCostB = bridgeLength;
        *outCostA = bridgeLength;
        *outLength = bridgeLength;
        return 0;
    }

    return 3;
}

// ============================================================
// BridgeTool::CalcPath (vtable +0x68, Slot13)
// ============================================================
// The most complex tool function (~400 lines decompiled, 1066 instructions).
// Builds a bridge path from startCell to endCell by:
//
//   1. Snapping the end cell via SnapEndCell helper
//   2. Validating the path via ValidatePath helper
//   3. Building a doubly-linked list of segment descriptors, each specifying
//      a segment size (0, 1, 2, 4, or 8 cells). The algorithm fills the bridge
//      body from center outward with the largest fitting segments.
//   4. For each segment in the linked list:
//      a. Copy-constructs an entity from BridgeDescription segment templates
//         (indexed by segment size: +0x38[0..4])
//      b. Finds MapComponent, ModelComponent, MultiObjectComponent on the entity
//      c. Sets cell coordinates (col, row, width, height) and rotation on MapComponent
//      d. Computes terrain-aligned position via CharacterComponent_impl_f3e0
//      e. Builds a rotation transform via Math::Transform::FromYawPitchOffset
//      f. Sets transform on ModelComponent and PickingComponent
//      g. Registers picking user data with Picking::Scene::SetMeshUserData
//      h. Appends the entity to the MultiObjectComponent on the master entity
//      i. Registers the entity with the world entity list
//   5. Finally registers the master entity with the entity list
//   6. Frees the linked list nodes
//
// Full reimplementation is blocked on typing ~20 unresolved helper functions:
//   - BridgeTool_sub_b710 (SnapEndCell), BridgeTool_sub_b2d0 (ValidatePath)
//   - System::MemoryAllocator::MemAlloc/MemFree for linked list nodes
//   - ComponentEntity_CopyCtor, fn_InternalBasePtr_Ctor/CheckPtr
//   - Component search loops (MapComponent, ModelComponent, MultiObjectComponent,
//     PickingComponent) — each repeated 4+ times with IsDerivedFrom checks
//   - CharacterComponent_impl_f3e0 (terrain height query)
//   - Math::Vector3::Vector3, Math::Transform::FromYawPitchOffset, operator=
//   - Graphics::Geometry::SetTransform
//   - Picking::Scene::SetMeshUserData
//   - FUN_140003110 (append to internal list)
//   - FUN_1400c48a0 (register entity with world entity list)
//
// Delegates to the original exe function via a resolved function pointer.
// Original: 0x1400f9cd0
typedef uint64_t (*fn_BridgeTool_CalcPath_t)(void* self, int startCell, int endCell);
static fn_BridgeTool_CalcPath_t s_fn_BridgeTool_CalcPath = nullptr;

// PORTED 2026-07-26 — bridgetool_calcpath.cpp. The delegation below was a leftover from
// the removed proxy-DLL mode: in the standalone exe ResolveAddr(Exe, 0x1400f9cd0) has no
// original to point at, so it returned 0 and every released bridge drag logged
// "CalcPath function pointer is NULL" and built nothing.
extern "C" uint64_t BridgeTool_CalcPath_Impl(void* self, int startCell, int endCell);

uint64_t BridgeTool_CalcPath(BridgeTool* self, int startCell, int endCell) {
    return BridgeTool_CalcPath_Impl(self, startCell, endCell);
}

// ============================================================
// Standalone factory + vtable (factory cascade). See
// tool_base_standalone.h + notes/FACTORY_CASCADE_145_BRIEF.md.
// BridgeTool: size 0x40, parent DragTool, 15-slot vtable @ 0x140161b00.
// ============================================================
#include "tool_base_standalone.h"
#include "Player/dragtool.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t BridgeTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("BridgeTool");
    return c;
}
// +0x58 (Slot11) GetToolSettingTypeId — retail BridgeTool slot 11 is 0x140004950, the
// SAME shared `*ret = 0; return ret;` thunk RoadTool/RemovalTool/PathTool use (BridgeTool
// has no tool-setting; only PlacementTool's RotationSetting does). SetEditMode
// (FUN_140102b60 / Toolbar_ShowGroup_Impl) DEREFERENCES the returned pointer to decide
// whether to build a ToolSetting, so the previous Tool_Ret0 stub (returns nullptr -> *0)
// faulted the instant a bridge button click created the tool. This was the reason the
// tool-mode tree node could not simply be added on its own.
static uint32_t* BridgeTool_GetSettingType(void*, uint32_t* ret) {
    if (ret) *ret = 0;
    return ret;
}
// +0x60 (Slot12) cursor id — retail 0x1400fc9d0 `return 5`, shared with RoadTool/FencedTool.
// EntityUI_ToolTick_SA feeds this to SetCursor, so Tool_Ret0 gave the wrong (id 0) cursor.
static int BridgeTool_Slot12(void*) { return 5; }
// +0x48 (Slot9) cancel/"can-deactivate?" — retail 0x1400fc640, the shared DragTool-family
// right-click handler (cancels an active drag + closes the tool dialog, returns 1 to keep
// the tool; 0 when idle so EntityUI proceeds with teardown). Same fn RoadTool slot 9 uses.
extern "C" int DragTool_CancelDrag_Impl(void* self);
// +0x40 (Slot8) Draw — retail 0x1400fadc0, the red/green drag preview. Ported in
// bridgetool_draw.cpp. While this was Tool_Ret0 a bridge drag rendered NOTHING: the
// placement worked and CalcCost reported valid=1 every frame, but the player had no
// on-map feedback at all. The headless harness never renders, so it could not see it.
extern "C" void BridgeTool_Draw_Impl(void* self, void* context, char skip);
static void* g_BridgeTool_Vtable[15] = {
    (void*)&BridgeTool_Dtor,      // +0x00 Dtor
    (void*)&Tool_Slot1,           // +0x08 slot1 (null)
    (void*)&BridgeTool_GetTypeId, // +0x10 GetTypeId
    (void*)&Tool_Persist,         // +0x18 Serialize (base)
    (void*)&BridgeTool_Startup,   // +0x20 Startup
    (void*)&Tool_Ret0,            // +0x28
    (void*)&DragTool_OnKey,       // +0x30 OnKey — retail 0x1400fc6b0 IS DragTool::OnKey (shared)
    (void*)&Tool_Ret0,            // +0x38 retail 0x1400fb2c0 = empty `return;`
    (void*)&BridgeTool_Draw_Impl, // +0x40 Draw — retail 0x1400fadc0 (red/green drag preview)
    (void*)&DragTool_CancelDrag_Impl, // +0x48 Slot9 cancel drag / allow-teardown (0x1400fc640)
    (void*)&Tool_Ret0,            // +0x50 retail 0x1400268a0 = shared `return 0`
    (void*)&BridgeTool_GetSettingType, // +0x58 Slot11 GetToolSettingTypeId (*ret=0)
    (void*)&BridgeTool_Slot12,    // +0x60 cursor id (retail returns 5)
    (void*)&BridgeTool_CalcPath,  // +0x68 CalcPath
    (void*)&BridgeTool_CalcCost,  // +0x70 CalcCost
};
extern "C" void RegisterBridgeToolVtable() {
    RegisterExeAddr(0x140161b00, (uintptr_t)g_BridgeTool_Vtable);
}
extern "C" void* BridgeTool_CreateInPlace_SA(void* mem) {    return Tool_InitInPlace(mem, 0x40, g_BridgeTool_Vtable, 1);}
extern "C" void* BridgeTool_Factory_SA(void) {
    return Tool_Alloc(0x40, g_BridgeTool_Vtable, 1);
}
REGISTER_TYPE(BridgeTool, "DragTool", &BridgeTool_Factory_SA, &BridgeTool_CreateInPlace_SA);
