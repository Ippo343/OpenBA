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

#include "Player/BridgeTool.h"
#include "Player/dragtool.h"   // Tool + DragTool (ENDGAME B.3.8 TOOL)
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// BridgeTool::DeletingDtor (vtable +0x00) — 0x14002d7f0
// ============================================================
// ENDGAME B.3.8 TOOL: the hand-rolled BridgeTool_Dtor restamped the Bridge,
// DragTool and Tool tables read by address — the last two UNREGISTERED, so it
// stored a NULL vptr before ~Internal (B38 D9 #3). The compiler's dtor chain
// restamps the real tables, then runs Object::Internal::~Internal.
Object::Base* BridgeTool::DeletingDtor(unsigned flags) {
    this->~BridgeTool();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
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
void BridgeTool::Startup() {
    uintptr_t entity = (uintptr_t)descriptionPtr;
    void* result = nullptr;

    if (!entity || !g_TypeId_CreateBridgeDescription || !fn_IsDerivedFrom) {
        bridgeDesc = nullptr;
        return;
    }

    int count = *(int*)(entity + 0x30);
    void** components = *(void***)(entity + 0x28);
    uint32_t targetTypeId = *g_TypeId_CreateBridgeDescription;

    for (int i = 0; i < count; i++) {
        void* comp = components[i];
        if (!comp) continue;

        // Virtual slot 2 (+0x10): Type()
        uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom before casting
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                result = comp;
            }
            break;
        }
    }

    bridgeDesc = result;
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
int BridgeTool::CalcCost(int startCell, int endCell, uint32_t* outValid,
                         int* outCostB, int* outCostA, int* outLength) {
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
    int snappedEnd = fn_BridgeTool_SnapEndCell(this, endCell);

    // Validate the bridge path
    if (!fn_BridgeTool_ValidatePath) return 3;
    int bridgeLength = 0;
    int valid = fn_BridgeTool_ValidatePath(this, startCell, snappedEnd, &bridgeLength);

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
            uintptr_t d = (uintptr_t)bridgeDesc;
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
// PORTED 2026-07-26 — the body is BridgeTool::CalcPath in bridgetool_calcpath.cpp.
// Original: 0x1400f9cd0

// ============================================================
// BridgeTool members (BridgeTool.h) — ENDGAME B.3.8 TOOL.
// The hand-built g_BridgeTool_Vtable[15], its slot stubs, the _SA factories
// and RegisterBridgeToolVtable are gone: the compiler builds the table, and
// nothing in the tree reads 0x140161b00 back by address (the old dtor was the
// only reader), so the exe address is no longer registered.
// Inherited from the bases, as in retail: [6] DragTool::OnKey (0x1400fc6b0),
// [9] DragTool::OnCancel (0x1400fc640), [10] false, [11] no setting
// (0x140004950 `*ret = 0`: BridgeTool has no ToolSetting — SetEditMode
// dereferences the returned slot), [12] cursor 5 (0x1400fc9d0).
// ============================================================
#include "Object/factory_registration.h"
#include <new>

// [4] 0x1400fb7a0 Startup and [14] 0x1400facf0 CalcCost are above; [8] 0x1400fadc0
// Draw (the red/green drag preview) is in bridgetool_draw.cpp and [13] 0x1400f9cd0
// CalcPath in bridgetool_calcpath.cpp.

// create 0x14002d7a0: MemAlloc(0x40, 8), then the ctor 0x1400fb890.
void* BridgeTool::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BridgeTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BridgeTool());
}
// createInPlace 0x14002d7e0: `TEST RCX,RCX; JNZ 0x1400fb890 (the ctor);
// XOR EAX,EAX; RET` — the tools, unlike the components, DO build in place.
void* BridgeTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BridgeTool());
}

DEFINE_EXE_VTABLE(BridgeTool, "_ZTV10BridgeTool")
DEFINE_EXE_TYPE(BridgeTool, "DragTool")
