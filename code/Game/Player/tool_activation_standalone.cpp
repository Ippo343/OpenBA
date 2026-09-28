// code/Game/Player/tool_activation_standalone.cpp
// GUI_PLAN Keystone F — Phase 3: make a real toolbar build-button click activate a
// PlacementTool and drive it per-frame (preview follows the mouse, left-click places).
//
// STANDALONE-only. Three pieces, all faithful ports of paths that exist in the exe
// but are dead in the standalone build:
//
//   1. EntityUI_BuildToolModeTree_SA  — first-half of EntityUI::Startup (FUN_140101ee0).
//      Builds the tool-mode registry BST at EntityUI+0x10 so SetEditMode
//      (FUN_140102b60 / Toolbar_ShowGroup_Impl / EntityUI_SetEditMode_Impl) can
//      resolve a Tool from a clicked building descriptor. In standalone
//      fn_EntityUI_Startup (0x140101ee0) is NULL (gameupdateloop.cpp:1703 no-ops), so
//      the tree is never built and every build-button click dead-ends on the null tree.
//
//   2. EntityUI_ToolTick_SA  — the tool portion of EntityUI::Update (FUN_1401026a0) +
//      its input dispatcher helper_f6e0 (0x1400ff6e0). Per frame, when a tool is active
//      at EntityUI+0x28: OnKey (+0x30, dialog-open + rotate/flip + left-click confirm),
//      Serialize (+0x38, ghost-mesh preview), cursor (+0x60), and right-click cancel
//      (action 6 -> SetEditMode(pc,0)). EntityUI::Update itself is unregistered in
//      standalone (only the camera half was ported -> camera_input.cpp), so this is the
//      missing tool half.
//
//   3. EntityUI_ToolDraw_SA  — the tail of GameUpdateLoop::Draw: dispatch the active
//      tool's Draw (+0x40) to render the footprint validity overlay.
//
// The tool itself is created by the EXISTING SetEditMode reimpl the toolbar case-1
// click already invokes (Toolbar_ShowGroup_Impl); this file only supplies the tree it
// walks + the per-frame tick that drives the resulting tool. Gated by the caller behind
// BANISHED_LIVE_TOOLS so the default build is byte-identical.
//
// AVL node (0x38, from EntityUI_helper_ddb0_sub_df10 @0x14002df10): +0x00 key(u32
// triggerTypeId), +0x08 toolTypeId(u32), +0x10 settingIBP(8), +0x18 parent, +0x20 left,
// +0x28 right, +0x30 balance. Insert = helper_ddb0 (0x14002ddb0) walk+link + rebalance
// TradeWork_impl_4b40 (0x140014b40, already registered in standalone).


#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "Object/factory_registration.h"
#include "Player/Tool.h"
#include "Player/ToolSetting.h"
#include "Player/BridgeTool.h"
#include "Player/DragPlacementTool.h"
#include "Player/EditTool.h"
#include "Player/FencedTool.h"
#include "Player/PathTool.h"
#include "Player/PlacementTool.h"
#include "Player/RemovalTool.h"
#include "Player/RoadTool.h"
#include "Component/CreateBridgeDescription.h"
#include "Component/CreateDragDescription.h"
#include "Component/CreateFencedDescription.h"
#include "Component/CreatePathDescription.h"
#include "Component/CreatePlacedDescription.h"
#include "Component/CreateRemovalDescription.h"
#include "Component/CreateRoadDescription.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)

// Registered AVL retrace/rebalance for 0x38-byte nodes (reg_keystone_a_restree.cpp).
extern "C" void TradeWork_impl_4b40(int64_t* root, int64_t* newNode);

namespace {

// KeyMap via the triple-deref exref (same pattern as camera_input.cpp::CamKeyMap).
inline void* ToolKeyMap() {
    if (!g_pInputConfigSingletonExref || !*g_pInputConfigSingletonExref) return nullptr;
    void* l2 = *(void**)*g_pInputConfigSingletonExref;
    if (!l2) return nullptr;
    return *(void**)l2;
}

// Node-alloc = FUN_140102df10: MemAlloc(0x38,8), copy key/toolTypeId, zero the rest,
// set parent. settingIBP(+0x10) starts 0 -> SetEditMode lazily fills it via the tool's
// GetToolSettingTypeId (+0x58) on first activation.
void* TreeNode_Alloc(uint32_t key, uint32_t toolTypeId, void* parent) {
    void* n = System::MemoryAllocator::MemAlloc(0x38, 8);
    if (!n) return nullptr;
    uintptr_t p = (uintptr_t)n;
    *(uint32_t*)(p + 0x00) = key;
    *(uint32_t*)(p + 0x08) = toolTypeId;
    *(uint64_t*)(p + 0x10) = 0;       // settingIBP
    *(void**)(p + 0x18)    = parent;  // parent
    *(uint64_t*)(p + 0x20) = 0;       // left
    *(uint64_t*)(p + 0x28) = 0;       // right
    *(uint32_t*)(p + 0x30) = 0;       // balance
    return n;
}

// helper_ddb0 (0x14002ddb0) find-or-create insert. tree points at EntityUI+0x10:
// tree[0] = root node ptr, tree[1] low32 = node count.
void Tree_Insert(int64_t* tree, uint32_t key, uint32_t toolTypeId) {
    if (*tree == 0) {
        void* n = TreeNode_Alloc(key, toolTypeId, nullptr);
        if (!n) return;
        *(int32_t*)(tree + 1) = (int32_t)tree[1] + 1;
        *tree = (int64_t)n;
        return;
    }
    uintptr_t node = (uintptr_t)*tree;
    for (;;) {
        uint32_t nk = *(uint32_t*)node;
        if (key < nk) {
            uintptr_t left = *(uintptr_t*)(node + 0x20);
            if (left == 0) {
                void* n = TreeNode_Alloc(key, toolTypeId, (void*)node);
                if (!n) return;
                *(void**)(node + 0x20) = n;
                int64_t nn = (int64_t)n;
                TradeWork_impl_4b40(tree, &nn);   // rebalance (may rotate root)
                *(int32_t*)(tree + 1) = (int32_t)tree[1] + 1;
                return;
            }
            node = left;
        } else if (key > nk) {
            uintptr_t right = *(uintptr_t*)(node + 0x28);
            if (right == 0) {
                void* n = TreeNode_Alloc(key, toolTypeId, (void*)node);
                if (!n) return;
                *(void**)(node + 0x28) = n;
                int64_t nn = (int64_t)n;
                TradeWork_impl_4b40(tree, &nn);
                *(int32_t*)(tree + 1) = (int32_t)tree[1] + 1;
                return;
            }
            node = right;
        } else {
            return;  // key already present
        }
    }
}

}  // namespace

// ============================================================
// Piece 1b — EntityUI::Startup's SECOND half: open the dialogs registered in the
// BST at EntityUI+0x50 (retail 0x140101ee0, the loop right after the eight
// Tree_Insert calls Piece 1 stands in for).
// ============================================================
// This is the ONLY thing that opens the tutorial panel. FinishTutorialInstance::Slot4
// (0x1400d7a90) does not open a dialog — it calls 0x1401001f0, which merely records
// {key = the tutorial's Dialog External, x, y, wanted = 1} in this tree. Retail's
// EntityUI::Startup then walks the tree and calls the toggle (0x140101a80, ported as
// Toolbar_Utility_Reg) for every node whose `wanted` byte is set, and THAT constructs
// the DialogInstance and hands it to UserInterface::AddDialog.
//
// With only Piece 1 standing in, the tutorial's request sat in the tree forever: the
// step driver's FindDialogByControllerType (0x140100270) walked a dialog list the panel
// was never added to, returned null every tick, and took its silent early return — the
// tutorial world played as a sandbox with no panel, no goals and no steps.
//
// Node layout (from 0x1401001f0 + 0x140101a80):
//   +0x00 key (Dialog External*)   +0x08 int x   +0x0C int y   +0x10 byte wanted
//   +0x18 parent   +0x20 left   +0x28 right
// Retail walks it in order (leftmost, then successor). Not gated by BANISHED_LIVE_TOOLS:
// an empty tree — every non-scenario game — makes this a no-op.
extern "C" void* Toolbar_Utility_Reg(void* entityUI, void* outIbp, uint64_t key);

extern "C" void EntityUI_OpenRegisteredDialogs_SA(void* entityUI) {
    if (!entityUI) return;
    uint64_t* node = *reinterpret_cast<uint64_t**>((uintptr_t)entityUI + 0x50);
    if (!node) return;

    while (node[4]) node = reinterpret_cast<uint64_t*>(node[4]);   // leftmost

    int opened = 0;
    while (node) {
        if (*reinterpret_cast<const uint8_t*>(&node[2]) != 0) {
            uint64_t outIbp = 0;
            Toolbar_Utility_Reg(entityUI, &outIbp, node[0]);
            ++opened;
        }
        // successor: leftmost of the right subtree, else up until we come from a left child
        if (node[5]) {
            node = reinterpret_cast<uint64_t*>(node[5]);
            while (node[4]) node = reinterpret_cast<uint64_t*>(node[4]);
        } else {
            uint64_t* parent = reinterpret_cast<uint64_t*>(node[3]);
            while (parent && reinterpret_cast<uint64_t*>(parent[5]) == node) {
                node = parent;
                parent = reinterpret_cast<uint64_t*>(node[3]);
            }
            node = parent;
        }
    }
    if (opened) {
        fprintf(stderr, "[EntityUI] opened %d registered dialog(s) from the +0x50 tree\n", opened);
        fflush(stderr);
    }
}

// ============================================================
// Piece 1 — build the tool-mode tree on the EntityUI (== GameWorld+0x120).
// ============================================================
// Idempotent (no-op if a tree already exists). We register PlacementTool, which covers
// ALL placeable buildings — their description carries a CreatePlacedDescription (the
// trigger the SetEditMode component-scan matches). The other 7 tools that retail's
// EntityUI::Startup registers (Edit/Path/DragPlacement/Road/Bridge/Removal/Fenced) are
// added here as each is proven crash-free in standalone.
extern "C" void EntityUI_BuildToolModeTree_SA(void* entityUI) {
    if (!entityUI) return;
    int64_t* tree = (int64_t*)((uintptr_t)entityUI + 0x10);
    if (*tree != 0) return;  // already built

    // The "Tool" / "ToolSetting" base type ids the SetEditMode reimpls IsDerivedFrom
    // (cells 0x1401c6eb8 / 0x1401c6ef0) are the classes' own records (ENDGAME B.3.9 RF);
    // kept here only for the trace line below. (ENDGAME B.3.9 TR: the null-fill of the
    // two engine_funcs pointers that stood here went with them.)
    static uint32_t s_tid_Tool = 0, s_tid_ToolSetting = 0;
    s_tid_Tool        = Tool::TypeId();
    s_tid_ToolSetting = ToolSetting::TypeId();

    uint32_t tid_CreatePlaced  = CreatePlacedDescription::TypeId();
    uint32_t tid_PlacementTool = PlacementTool::TypeId();
    if (tid_CreatePlaced && tid_PlacementTool)
        Tree_Insert(tree, tid_CreatePlaced, tid_PlacementTool);

    // EditTool: the default world-SELECTION tool. Retail's EntityUI::Startup inserts it
    // as node key=0 (the very first insert) — SetEditMode(pc, 0) resolves this node to
    // install EditTool as the idle tool, whose OnKey (+0x30) picks the entity under the
    // cursor and opens its info dialog. Without this node the select path is dead in SA.
    uint32_t tid_EditTool = EditTool::TypeId();
    if (tid_EditTool)
        Tree_Insert(tree, 0, tid_EditTool);

    // RemovalTool (the clearance / remove tool). Retail's EntityUI::Startup keys it on
    // CreateRemovalDescription (0x1401c74a0). The preloaded Dialog/Toolbar.rsc "Remove"
    // button carries that component at item+0x58, so SetEditMode's component-scan matches
    // this node and creates a RemovalTool. RemovalTool extends DragTool, so DragTool_OnKey
    // drives the drag and calls slot 13 (RemovalTool::CalcPath, keystone_a_removalcalcpath.cpp)
    // on drag-release — the whole clearance path. CalcPath cluster is registered
    // (reg_keystone_a_removal.cpp); the 74a0 type-id is wired (reg_economy_typeids.cpp).
    uint32_t tid_CreateRemoval = CreateRemovalDescription::TypeId();
    uint32_t tid_RemovalTool   = RemovalTool::TypeId();
    if (tid_CreateRemoval && tid_RemovalTool)
        Tree_Insert(tree, tid_CreateRemoval, tid_RemovalTool);

    // PathTool (the "Paths" report tool: shows worker home<->workplace routes). The
    // reports-group "path" toolbar item (Game/Toolbar.rsc) carries Template/UtilityPath.rsc,
    // whose CreatePathDescription is the trigger the ShowGroup component-scan matches. Add
    // the node so a Paths click resolves+activates a PathTool. PathTool extends Tool
    // directly (no ToolSetting; its +0x58 GetSettingType returns ptr-to-0). Vtable/factory
    // registered in pathtool.cpp; type-ids resolve from the live factory tree.
    uint32_t tid_CreatePath = CreatePathDescription::TypeId();
    uint32_t tid_PathTool   = PathTool::TypeId();
    if (tid_CreatePath && tid_PathTool)
        Tree_Insert(tree, tid_CreatePath, tid_PathTool);

    // RoadTool (the road/stone-road drawing tool). Retail's EntityUI::Startup
    // (0x140101ee0) keys it on CreateRoadDescription (0x1401c74d8) — the same type-id
    // RoadTool::Startup (0x1401056e0) scans the descriptor for and caches at tool+0x38.
    // The Toolbar.rsc road buttons carry that component at item+0x58, so without this
    // node SetEditMode's component-scan matched nothing and a road click silently
    // dead-ended (no tool, no cursor, no preview). RoadTool extends DragTool, so
    // DragTool_OnKey drives the drag and dispatches slot 14 (CalcCost, ported) for the
    // per-frame cost/validity preview and slot 13 (CalcPath) on release to commit.
    uint32_t tid_CreateRoad = CreateRoadDescription::TypeId();
    uint32_t tid_RoadTool   = RoadTool::TypeId();
    if (tid_CreateRoad && tid_RoadTool)
        Tree_Insert(tree, tid_CreateRoad, tid_RoadTool);

    // BridgeTool (the river/water bridge drawing tool). Retail's EntityUI::Startup keys it
    // on CreateBridgeDescription (0x1401c73f8). Without this node the toolbar's bridge
    // button dead-ended exactly like road did before 311e46f: Toolbar_ShowGroup_Impl's
    // component-scan matched none of the bridge description's 18 components, fell through
    // to foundTypeId=0, and silently cleared the tool (no crash, no cursor, no preview).
    // BridgeTool extends DragTool, so DragTool_OnKey (retail 0x1400fc6b0, its real slot-6)
    // drives the drag and dispatches slot 14 (CalcCost) per frame and slot 13 (CalcPath)
    // on release. NOTE: slot 11 had to be un-stubbed first — SetEditMode derefs its return.
    uint32_t tid_CreateBridge = CreateBridgeDescription::TypeId();
    uint32_t tid_BridgeTool   = BridgeTool::TypeId();
    if (tid_CreateBridge && tid_BridgeTool)
        Tree_Insert(tree, tid_CreateBridge, tid_BridgeTool);
    fprintf(stderr, "[Tools]   bridge node: CreateBridge=%u -> BridgeTool=%u\n",
            tid_CreateBridge, tid_BridgeTool);

    // FencedTool (the pasture / cemetery fenced-area tool). Retail's EntityUI::Startup
    // keys it on CreateFencedDescription (0x1401c4100). Pasture.rsc declares
    // `createfenced` and carries that component, so without this node a pasture click
    // dead-ended the same way road/bridge once did.
    //
    // This comment used to name CropField.rsc here alongside Pasture.rsc. That was
    // WRONG and it contradicted the DragPlacementTool block fourteen lines below, which
    // has always had it right: CropField.rsc declares `createdrag`, so the crop field is
    // keyed on CreateDragDescription and routes to DragPlacementTool, never to
    // FencedTool. Corrected 2026-09-06. The error was inert (the fenced node is correct
    // for Pasture either way) but it is exactly the kind of cross-reference that sends
    // the next reader looking for a crop-field bug in the wrong tool. FencedTool extends DragTool; slot 14 (CalcCost,
    // ported) previews the rect, slot 13 (CalcPath, ported in fencedtool_calcpath.cpp)
    // commits the master entity + shelter + fence pieces on drag-release.
    uint32_t tid_CreateFenced = CreateFencedDescription::TypeId();
    uint32_t tid_FencedTool   = FencedTool::TypeId();
    if (tid_CreateFenced && tid_FencedTool)
        Tree_Insert(tree, tid_CreateFenced, tid_FencedTool);
    fprintf(stderr, "[Tools]   fenced node: CreateFenced=%u -> FencedTool=%u\n",
            tid_CreateFenced, tid_FencedTool);

    // DragPlacementTool (orchard / crop-field drag-rectangle tool). Retail's
    // EntityUI::Startup keys it on CreateDragDescription (0x1401c7430). Orchard.rsc and
    // CropField.rsc declare `createdrag`, NOT the `createfenced` that Pasture.rsc uses —
    // so with this node missing, Toolbar_ShowGroup_Impl's component-scan matched nothing,
    // fell through to foundTypeId=0, and the lower-bound search resolved key 0 to the
    // EditTool node. An orchard click therefore installed the world-SELECTION tool: no
    // drag state, so no slot-8 red/green tile overlay and no CreateBar size/cost tooltip,
    // while pasture (FencedTool) had both. DragPlacementTool extends DragTool, so
    // DragTool_OnKey drives the drag, slot 14 (CalcCost) previews and slot 13 (CalcPath)
    // commits. Slots 9/11/12 had to be un-stubbed first — SetEditMode derefs slot 11's
    // return, exactly as it did for BridgeTool.
    uint32_t tid_CreateDrag      = CreateDragDescription::TypeId();
    uint32_t tid_DragPlacement   = DragPlacementTool::TypeId();
    if (tid_CreateDrag && tid_DragPlacement)
        Tree_Insert(tree, tid_CreateDrag, tid_DragPlacement);
    fprintf(stderr, "[Tools]   drag node: CreateDrag=%u -> DragPlacementTool=%u\n",
            tid_CreateDrag, tid_DragPlacement);

    fprintf(stderr, "[Tools] BuildToolModeTree: entityUI=%p Tool=%u ToolSetting=%u "
            "node{CreatePlaced=%u -> PlacementTool=%u} node{0 -> EditTool=%u} "
            "node{CreateRemoval=%u -> RemovalTool=%u} node{CreatePath=%u -> PathTool=%u} "
            "node{CreateRoad=%u -> RoadTool=%u} count=%d\n",
            entityUI, s_tid_Tool, s_tid_ToolSetting, tid_CreatePlaced, tid_PlacementTool,
            tid_EditTool, tid_CreateRemoval, tid_RemovalTool, tid_CreatePath, tid_PathTool,
            tid_CreateRoad, tid_RoadTool,
            *(int32_t*)(tree + 1));
    fflush(stderr);
}

// ============================================================
// Piece 2 — per-frame tool tick (tool half of EntityUI::Update + helper_f6e0).
// ============================================================
// Returns the tool's cursor id (from +0x60) when a tool is active, else -1 (caller
// keeps its default). activeFlag = noInputFocus (UserInterface+0xd9 == 0).
extern "C" int EntityUI_ToolTick_SA(void* entityUI, int activeFlag, int64_t time) {
    if (!entityUI) return -1;
    uintptr_t pc = (uintptr_t)entityUI;
    void* toolSlot = (void*)(pc + 0x28);        // InternalBasePtr to the active tool
    if (*(void**)toolSlot == nullptr) {
        // No active tool — install the default EditTool (retail EntityUI::Update does this
        // via SetEditMode(pc, 0)). EditTool is the idle world-selection tool: its per-frame
        // OnKey picks the entity under the cursor on left-click and opens its info dialog.
        // Guarded to run only when idle so it does NOT churn (destroy+recreate) each frame.
        EntityUI_SetEditMode_Impl(entityUI, 0);
        if (*(void**)toolSlot == nullptr) return -1;  // still none (EditTool node missing)
    }

    // (0b) Tutorial restricted-tool RE-ARM — retail EntityUI::Update (0x1401026a0),
    // the branch immediately after the "no tool -> SetEditMode(pc,0)" above:
    //
    //     if (pc[0x80] != 0 && pc[0x88] != 0) {
    //         if (pc[0x28] == 0 || Object::Base::IsA(pc[0x28], TypeInfo_EditTool))
    //             SetEditMode(pc, pc[0x88]);
    //     }
    //
    // This is the ONLY thing that gives a tutorial step its placement tool when the
    // step did not come after a toolbar click. TutorialSetupRestrictPlayer::Undo
    // (0x1400e2e90) ends in SetEditMode(pc, 0) whenever the step named an _object,
    // so EVERY restricted placement step hands the next step a bare EditTool. The
    // next step's Apply only writes pc+0x80/+0x88 — it never creates a tool. Retail
    // closes that gap here, on the following frame.
    //
    // Symptom without it: TutorialGettingStarted stepMoreHouses3/4 (place houses 3
    // and 4) show no building ghost and no yellow target rect, and the tutorial
    // stalls at "count=2 target=3" forever — the whole toolbar is disabled by
    // setupExitOnly's exclusion list, so the player cannot re-pick the tool either.
    //
    // IsA is an EXACT type match (not IsDerivedFrom): a live PlacementTool/RemovalTool
    // must not be torn down and rebuilt every frame, only the idle EditTool is replaced.
    if (*(char*)(pc + 0x80) != 0 && *(void**)(pc + 0x88) != nullptr) {
        const uint32_t tidEditTool = EditTool::TypeId();

        void* cur = *(void**)toolSlot;
        bool replaceable = (cur == nullptr) ||
                           (tidEditTool &&
                            reinterpret_cast<const Object::Base*>(cur)->IsA(tidEditTool));   // B.3.x bridge
        if (replaceable) {
            EntityUI_SetEditMode_Impl(entityUI, *(uint64_t*)(pc + 0x88));
            if (*(void**)toolSlot == nullptr) return -1;
        }
    }

    // (1) OnKey (+0x30): open/close CreateBar dialog, rotate/flip, left-click confirm.
    void* tool = *(void**)toolSlot;
    if (!tool) return -1;
    char onKeyResult = reinterpret_cast<Tool*>(tool)->OnKey((char)activeFlag);  // B.3.8 bridge (slot 6, +0x30)

    // Right-click cancel (action 6 = MouseRight) when active and not tool-locked
    // (EntityUI+0x80 == 0): tool +0x48 "can-deactivate?" (our stub returns 0) -> clear.
    // Retail (0x1400ff6e0) gates the input branches on OnKey returning NON-ZERO:
    //     cVar2 = (**(vt + 0x30))();
    //     if (cVar2 != 0) { ...save-view hotkeys...; ...right-click cancel... }
    // We discarded that return and ran the cancel check every frame, so a
    // right-DRAG (how the camera is rotated) reached the slot-9 test while a
    // tool was armed. PlacementTool slot 9 is Tool_Ret0 => "cannot stay" =>
    // SetEditMode(pc,0) tore the tool down, which also clears EntityUI+0x20 and
    // so unchecks the toolbar button. Right-click DOES cancel placement in
    // retail; the defect was reaching that test on a frame OnKey vetoed.
    //
    // Scoped to this branch ONLY: helper_f6e0 is input dispatch, while the
    // Serialize (+0x38) preview update and cursor id below come from
    // EntityUI::Update, which this function also fronts. Gating those too
    // would freeze the ghost mesh on any frame OnKey returned 0.
    if (onKeyResult != 0 && activeFlag != 0 && *(char*)(pc + 0x80) == 0) {
        void* km = ToolKeyMap();
        if (km && reinterpret_cast<const Input::KeyMap*>(km)->Pressed(0, 6)) {  // B.3.7 bridge
            void* t2 = *(void**)toolSlot;
            int canStay = 1;
            if (t2) canStay = reinterpret_cast<Tool*>(t2)->OnCancel();  // B.3.8 bridge (slot 9, +0x48)
            if (canStay == 0) {
                EntityUI_SetEditMode_Impl(entityUI, 0);
                return -1;  // tool gone this frame
            }
        }
    }

    // (2) Serialize (+0x38): update the ghost-mesh preview + CreateBar cost/validity.
    tool = *(void**)toolSlot;
    if (!tool) return -1;
    {
        int64_t timeLocal = time;
        reinterpret_cast<Tool*>(tool)->Update(&timeLocal, (char)activeFlag);  // B.3.8 bridge (slot 7, +0x38)
    }

    // (3) cursor (+0x60): id used for SetCursor.
    tool = *(void**)toolSlot;
    if (!tool) return -1;
    return reinterpret_cast<Tool*>(tool)->GetCursor();  // B.3.8 bridge (slot 12, +0x60)
}

// ============================================================
// Piece 3 — draw-phase footprint overlay (tool +0x40).
// ============================================================
extern "C" void EntityUI_ToolDraw_SA(void* entityUI, void* context, int activeFlag) {
    if (!entityUI) return;
    uintptr_t pc = (uintptr_t)entityUI;
    void* toolSlot = (void*)(pc + 0x28);
    if (*(void**)toolSlot == nullptr) return;
    void* tool = *(void**)toolSlot;
    if (!tool) return;
    reinterpret_cast<Tool*>(tool)->Draw(context, (char)activeFlag);  // B.3.8 bridge (slot 8, +0x40)
}

