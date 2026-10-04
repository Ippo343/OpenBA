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


#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"
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
#include "Game/PlayerToolManager.h"
#include "Dialog/ToolRegistryNode.h"   // the +0x10 registry's node
#include <cstddef>

#include "Player/exe_bodies.h"   // Avl38Retrace (0x140014b40)

namespace {

// The tool-mode registry at PlayerToolManager +0x10 (root) / +0x18 (count): the
// nodes are ToolRegistryNode (Dialog/ToolRegistryNode.h), as the node allocator
// 0x14002df10 builds them (MemAlloc(0x38)): the trigger type id, the Tool class's
// type id, the ToolSetting SetEditMode creates lazily, the AVL links and the
// balance/height pair the shared retrace 0x140014b40 maintains.

// Node-alloc = FUN_140102df10: MemAlloc(0x38,8), copy key/toolTypeId, zero the rest,
// set parent. setting (+0x10) starts 0 -> SetEditMode lazily fills it via the tool's
// GetSettingType (+0x58) on first activation.
ToolRegistryNode* TreeNode_Alloc(uint32_t key, uint32_t toolTypeId, ToolRegistryNode* parent) {
    ToolRegistryNode* n = static_cast<ToolRegistryNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
    if (!n) return nullptr;
    n->typeId = key;
    n->factoryTypeId = toolTypeId;
    n->toolSetting = nullptr;
    n->parent = parent;
    n->left = nullptr;
    n->right = nullptr;
    n->balance = 0;   // +0x30 dword 0
    n->height = 0;
    return n;
}

// The shared AVL retrace 0x140014b40 (Avl38Retrace, Player/exe_bodies.h).
void Retrace(PlayerToolManager* mgr, ToolRegistryNode* node) {
    ToolRegistryNode* nn = node;
    Avl38Retrace(&mgr->field_0x10, &nn);
}

// helper_ddb0 (0x14002ddb0) find-or-create insert.
void Tree_Insert(PlayerToolManager* mgr, uint32_t key, uint32_t toolTypeId) {
    if (mgr->field_0x10 == nullptr) {
        ToolRegistryNode* n = TreeNode_Alloc(key, toolTypeId, nullptr);
        if (!n) return;
        mgr->field_0x18 = mgr->field_0x18 + 1;
        mgr->field_0x10 = n;
        return;
    }
    ToolRegistryNode* node = mgr->field_0x10;
    for (;;) {
        uint32_t nk = node->typeId;
        if (key < nk) {
            ToolRegistryNode* left = node->left;
            if (left == nullptr) {
                ToolRegistryNode* n = TreeNode_Alloc(key, toolTypeId, node);
                if (!n) return;
                node->left = n;
                Retrace(mgr, n);   // rebalance (may rotate root)
                mgr->field_0x18 = mgr->field_0x18 + 1;
                return;
            }
            node = left;
        } else if (key > nk) {
            ToolRegistryNode* right = node->right;
            if (right == nullptr) {
                ToolRegistryNode* n = TreeNode_Alloc(key, toolTypeId, node);
                if (!n) return;
                node->right = n;
                Retrace(mgr, n);
                mgr->field_0x18 = mgr->field_0x18 + 1;
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
// BST at +0x50 (retail 0x140101ee0, the loop right after the eight Tree_Insert
// calls Piece 1 stands in for).
// ============================================================
// This is the ONLY thing that opens the tutorial panel. FinishTutorialInstance::Slot4
// (0x1400d7a90) does not open a dialog — it calls 0x1401001f0, which merely records
// {key = the tutorial's Dialog External, x, y, wanted = 1} in this tree. Retail's
// EntityUI::Startup then walks the tree and calls the toggle (0x140101a80) for every
// node whose `wanted` byte is set, and THAT constructs the DialogInstance and hands it
// to UserInterface::AddDialog.
//
// Node layout (from 0x1401001f0 + 0x140101a80): TrackedEntityNode
// (Game/PlayerToolManager.h): +0x00 key, +0x08 int x, +0x0C int y, +0x10 byte wanted,
// +0x18 parent, +0x20 left, +0x28 right. Retail walks it in order (leftmost, then
// successor). An empty tree — every non-scenario game — makes this a no-op.
extern "C" void EntityUI_OpenRegisteredDialogs_SA(PlayerToolManager* mgr) {
    if (!mgr) return;
    TrackedEntityNode* node = mgr->field_0x50;
    if (!node) return;

    while (node->left) node = node->left;   // leftmost

    int opened = 0;
    while (node) {
        if (node->active != 0) {
            // The toggle 0x140101a80 (was its Toolbar_Utility_Reg forwarder; the returned
            // InternalPtr was discarded). LEFT: the node's key is declared ComponentEntity*
            // (GAME-a's TrackedEntityNode); 0x1401001f0 stores the dialog's description there.
            mgr->ToggleUtilityDialog(reinterpret_cast<ComponentDescription*>(node->key));   // LEFT: Game/PlayerToolManager.h TrackedEntityNode::key
            ++opened;
        }
        // successor: leftmost of the right subtree, else up until we come from a left child
        if (node->right) {
            node = node->right;
            while (node->left) node = node->left;
        } else {
            TrackedEntityNode* parent = node->parent;
            while (parent && parent->right == node) {
                node = parent;
                parent = node->parent;
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
// Piece 1 — build the tool-mode tree on the tool manager (== GameWorld+0x120).
// ============================================================
// Idempotent (no-op if a tree already exists). We register PlacementTool, which covers
// ALL placeable buildings — their description carries a CreatePlacedDescription (the
// trigger the SetEditMode component-scan matches), and the other 7 tools that retail's
// EntityUI::Startup registers (Edit/Path/DragPlacement/Road/Bridge/Removal/Fenced).
extern "C" void EntityUI_BuildToolModeTree_SA(PlayerToolManager* entityUI) {
    if (!entityUI) return;
    PlayerToolManager* mgr = entityUI;
    if (mgr->field_0x10 != nullptr) return;  // already built (the +0x10 registry root)

    // The "Tool" / "ToolSetting" base type ids the SetEditMode reimpls IsDerivedFrom
    // (cells 0x1401c6eb8 / 0x1401c6ef0), kept here only for the trace line below.
    static uint32_t s_tid_Tool = 0, s_tid_ToolSetting = 0;
    s_tid_Tool        = Tool::TypeId();
    s_tid_ToolSetting = ToolSetting::TypeId();

    uint32_t tid_CreatePlaced  = CreatePlacedDescription::TypeId();
    uint32_t tid_PlacementTool = PlacementTool::TypeId();
    if (tid_CreatePlaced && tid_PlacementTool)
        Tree_Insert(mgr, tid_CreatePlaced, tid_PlacementTool);

    // EditTool: the default world-SELECTION tool. Retail's EntityUI::Startup inserts it
    // as node key=0 (the very first insert) — SetEditMode(pc, 0) resolves this node to
    // install EditTool as the idle tool.
    uint32_t tid_EditTool = EditTool::TypeId();
    if (tid_EditTool)
        Tree_Insert(mgr, 0, tid_EditTool);

    // RemovalTool, keyed on CreateRemovalDescription (0x1401c74a0).
    uint32_t tid_CreateRemoval = CreateRemovalDescription::TypeId();
    uint32_t tid_RemovalTool   = RemovalTool::TypeId();
    if (tid_CreateRemoval && tid_RemovalTool)
        Tree_Insert(mgr, tid_CreateRemoval, tid_RemovalTool);

    // PathTool (the "Paths" report tool), keyed on CreatePathDescription.
    uint32_t tid_CreatePath = CreatePathDescription::TypeId();
    uint32_t tid_PathTool   = PathTool::TypeId();
    if (tid_CreatePath && tid_PathTool)
        Tree_Insert(mgr, tid_CreatePath, tid_PathTool);

    // RoadTool, keyed on CreateRoadDescription (0x1401c74d8) — the same type-id
    // RoadTool::Startup (0x1401056e0) scans the descriptor for.
    uint32_t tid_CreateRoad = CreateRoadDescription::TypeId();
    uint32_t tid_RoadTool   = RoadTool::TypeId();
    if (tid_CreateRoad && tid_RoadTool)
        Tree_Insert(mgr, tid_CreateRoad, tid_RoadTool);

    // BridgeTool, keyed on CreateBridgeDescription (0x1401c73f8).
    uint32_t tid_CreateBridge = CreateBridgeDescription::TypeId();
    uint32_t tid_BridgeTool   = BridgeTool::TypeId();
    if (tid_CreateBridge && tid_BridgeTool)
        Tree_Insert(mgr, tid_CreateBridge, tid_BridgeTool);
    fprintf(stderr, "[Tools]   bridge node: CreateBridge=%u -> BridgeTool=%u\n",
            tid_CreateBridge, tid_BridgeTool);

    // FencedTool (pasture / cemetery), keyed on CreateFencedDescription (0x1401c4100).
    // (CropField.rsc declares `createdrag`: it routes to DragPlacementTool below.)
    uint32_t tid_CreateFenced = CreateFencedDescription::TypeId();
    uint32_t tid_FencedTool   = FencedTool::TypeId();
    if (tid_CreateFenced && tid_FencedTool)
        Tree_Insert(mgr, tid_CreateFenced, tid_FencedTool);
    fprintf(stderr, "[Tools]   fenced node: CreateFenced=%u -> FencedTool=%u\n",
            tid_CreateFenced, tid_FencedTool);

    // DragPlacementTool (orchard / crop-field drag-rectangle tool), keyed on
    // CreateDragDescription (0x1401c7430).
    uint32_t tid_CreateDrag      = CreateDragDescription::TypeId();
    uint32_t tid_DragPlacement   = DragPlacementTool::TypeId();
    if (tid_CreateDrag && tid_DragPlacement)
        Tree_Insert(mgr, tid_CreateDrag, tid_DragPlacement);
    fprintf(stderr, "[Tools]   drag node: CreateDrag=%u -> DragPlacementTool=%u\n",
            tid_CreateDrag, tid_DragPlacement);

    fprintf(stderr, "[Tools] BuildToolModeTree: entityUI=%p Tool=%u ToolSetting=%u "
            "node{CreatePlaced=%u -> PlacementTool=%u} node{0 -> EditTool=%u} "
            "node{CreateRemoval=%u -> RemovalTool=%u} node{CreatePath=%u -> PathTool=%u} "
            "node{CreateRoad=%u -> RoadTool=%u} count=%d\n",
            entityUI, s_tid_Tool, s_tid_ToolSetting, tid_CreatePlaced, tid_PlacementTool,
            tid_EditTool, tid_CreateRemoval, tid_RemovalTool, tid_CreatePath, tid_PathTool,
            tid_CreateRoad, tid_RoadTool,
            mgr->field_0x18);
    fflush(stderr);
}

// ============================================================
// Piece 2 — per-frame tool tick (tool half of EntityUI::Update + helper_f6e0).
// ============================================================
// Returns the tool's cursor id (from +0x60) when a tool is active, else -1 (caller
// keeps its default). activeFlag = noInputFocus (UserInterface+0xd9 == 0).
extern "C" int EntityUI_ToolTick_SA(PlayerToolManager* entityUI, int activeFlag, int64_t time) {
    if (!entityUI) return -1;
    PlayerToolManager* mgr = entityUI;
    if (mgr->currentTool == nullptr) {
        // No active tool — install the default EditTool (retail EntityUI::Update does this
        // via SetEditMode(pc, 0)). Guarded to run only when idle so it does NOT churn
        // (destroy+recreate) each frame.
        mgr->SelectTool(nullptr);
        if (mgr->currentTool == nullptr) return -1;  // still none (EditTool node missing)
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
    // step did not come after a toolbar click. IsA is an EXACT type match (not
    // IsDerivedFrom): a live PlacementTool/RemovalTool must not be torn down and
    // rebuilt every frame, only the idle EditTool is replaced.
    if (mgr->field_0x80 && mgr->field_0x88 != nullptr) {
        const uint32_t tidEditTool = EditTool::TypeId();

        Tool* cur = mgr->currentTool;
        bool replaceable = (cur == nullptr) ||
                           (tidEditTool && cur->IsA(tidEditTool));
        if (replaceable) {
            mgr->SelectTool(mgr->field_0x88);
            if (mgr->currentTool == nullptr) return -1;
        }
    }

    // (1) OnKey (+0x30): open/close CreateBar dialog, rotate/flip, left-click confirm.
    Tool* tool = mgr->currentTool;
    if (!tool) return -1;
    char onKeyResult = tool->OnKey((char)activeFlag);  // slot 6, +0x30

    // Right-click cancel (action 6 = MouseRight) when active and not tool-locked
    // (+0x80 == 0): tool slot 9 "can-deactivate?" -> clear.
    // Retail (0x1400ff6e0) gates the input branches on OnKey returning NON-ZERO:
    //     cVar2 = (**(vt + 0x30))();
    //     if (cVar2 != 0) { ...save-view hotkeys...; ...right-click cancel... }
    // Scoped to this branch ONLY: helper_f6e0 is input dispatch, while the
    // Update (+0x38) preview and cursor id below come from EntityUI::Update.
    if (onKeyResult != 0 && activeFlag != 0 && !mgr->field_0x80) {
        const Input::KeyMap* km =
            Input::Configuration::_singleton ? Input::Configuration::_singleton->_keyMap : nullptr;
        if (km && km->Pressed(0, 6)) {
            Tool* t2 = mgr->currentTool;
            int canStay = 1;
            if (t2) canStay = t2->OnCancel();  // slot 9, +0x48
            if (canStay == 0) {
                mgr->SelectTool(nullptr);
                return -1;  // tool gone this frame
            }
        }
    }

    // (2) Update (+0x38): update the ghost-mesh preview + CreateBar cost/validity.
    tool = mgr->currentTool;
    if (!tool) return -1;
    {
        int64_t timeLocal = time;
        tool->Update(&timeLocal, (char)activeFlag);  // slot 7, +0x38
    }

    // (3) cursor (+0x60): id used for SetCursor.
    tool = mgr->currentTool;
    if (!tool) return -1;
    return tool->GetCursor();  // slot 12, +0x60
}

// ============================================================
// Piece 3 — draw-phase footprint overlay (tool +0x40).
// ============================================================
extern "C" void EntityUI_ToolDraw_SA(PlayerToolManager* entityUI, Video::Context* context, int activeFlag) {
    if (!entityUI) return;
    PlayerToolManager* mgr = entityUI;
    if (mgr->currentTool == nullptr) return;
    Tool* tool = mgr->currentTool;
    if (!tool) return;
    tool->Draw(context, (char)activeFlag);  // slot 8, +0x40
}
