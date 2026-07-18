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
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Registered AVL retrace/rebalance for 0x38-byte nodes (reg_keystone_a_restree.cpp).
extern "C" void TradeWork_impl_4b40(int64_t* root, int64_t* newNode);

namespace {

// KeyMap via the triple-deref exref (same pattern as camera_input.cpp::CamKeyMap).
inline void* ToolKeyMap() {
    if (!g_pKeyMapObjectExref || !*g_pKeyMapObjectExref) return nullptr;
    void* l2 = *(void**)*g_pKeyMapObjectExref;
    if (!l2) return nullptr;
    return *(void**)l2;
}

// Node-alloc = FUN_140102df10: MemAlloc(0x38,8), copy key/toolTypeId, zero the rest,
// set parent. settingIBP(+0x10) starts 0 -> SetEditMode lazily fills it via the tool's
// GetToolSettingTypeId (+0x58) on first activation.
void* TreeNode_Alloc(uint32_t key, uint32_t toolTypeId, void* parent) {
    void* n = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
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

    // Fill the "Tool" / "ToolSetting" base type-ids that the SetEditMode reimpls
    // IsDerivedFrom (0x1401c6eb8="Tool", 0x1401c6ef0="ToolSetting"; both resolve to
    // NULL pointers in standalone since they are raw .rdata constants).
    static uint32_t s_tid_Tool = 0, s_tid_ToolSetting = 0;
    s_tid_Tool        = ::Object::ResolveTypeIdByNameLazy("Tool");
    s_tid_ToolSetting = ::Object::ResolveTypeIdByNameLazy("ToolSetting");
    if (!g_TypeId_ToolController) g_TypeId_ToolController = &s_tid_Tool;
    if (!g_TypeId_ToolSetting)    g_TypeId_ToolSetting    = &s_tid_ToolSetting;

    uint32_t tid_CreatePlaced  = ::Object::ResolveTypeIdByNameLazy("CreatePlacedDescription");
    uint32_t tid_PlacementTool = ::Object::ResolveTypeIdByNameLazy("PlacementTool");
    if (tid_CreatePlaced && tid_PlacementTool)
        Tree_Insert(tree, tid_CreatePlaced, tid_PlacementTool);

    // EditTool: the default world-SELECTION tool. Retail's EntityUI::Startup inserts it
    // as node key=0 (the very first insert) — SetEditMode(pc, 0) resolves this node to
    // install EditTool as the idle tool, whose OnKey (+0x30) picks the entity under the
    // cursor and opens its info dialog. Without this node the select path is dead in SA.
    uint32_t tid_EditTool = ::Object::ResolveTypeIdByNameLazy("EditTool");
    if (tid_EditTool)
        Tree_Insert(tree, 0, tid_EditTool);

    // RemovalTool (the clearance / remove tool). Retail's EntityUI::Startup keys it on
    // CreateRemovalDescription (0x1401c74a0). The preloaded Dialog/Toolbar.rsc "Remove"
    // button carries that component at item+0x58, so SetEditMode's component-scan matches
    // this node and creates a RemovalTool. RemovalTool extends DragTool, so DragTool_OnKey
    // drives the drag and calls slot 13 (RemovalTool_CalcPath -> ported RemovalCalc_Slot13)
    // on drag-release — the whole clearance path. CalcPath cluster is registered
    // (reg_keystone_a_removal.cpp); the 74a0 type-id is wired (reg_economy_typeids.cpp).
    uint32_t tid_CreateRemoval = ::Object::ResolveTypeIdByNameLazy("CreateRemovalDescription");
    uint32_t tid_RemovalTool   = ::Object::ResolveTypeIdByNameLazy("RemovalTool");
    if (tid_CreateRemoval && tid_RemovalTool)
        Tree_Insert(tree, tid_CreateRemoval, tid_RemovalTool);

    // PathTool (the "Paths" report tool: shows worker home<->workplace routes). The
    // reports-group "path" toolbar item (Game/Toolbar.rsc) carries Template/UtilityPath.rsc,
    // whose CreatePathDescription is the trigger the ShowGroup component-scan matches. Add
    // the node so a Paths click resolves+activates a PathTool. PathTool extends Tool
    // directly (no ToolSetting; its +0x58 GetSettingType returns ptr-to-0). Vtable/factory
    // registered in pathtool.cpp; type-ids resolve from the live factory tree.
    uint32_t tid_CreatePath = ::Object::ResolveTypeIdByNameLazy("CreatePathDescription");
    uint32_t tid_PathTool   = ::Object::ResolveTypeIdByNameLazy("PathTool");
    if (tid_CreatePath && tid_PathTool)
        Tree_Insert(tree, tid_CreatePath, tid_PathTool);

    fprintf(stderr, "[Tools] BuildToolModeTree: entityUI=%p Tool=%u ToolSetting=%u "
            "node{CreatePlaced=%u -> PlacementTool=%u} node{0 -> EditTool=%u} "
            "node{CreateRemoval=%u -> RemovalTool=%u} node{CreatePath=%u -> PathTool=%u} count=%d\n",
            entityUI, s_tid_Tool, s_tid_ToolSetting, tid_CreatePlaced, tid_PlacementTool,
            tid_EditTool, tid_CreateRemoval, tid_RemovalTool, tid_CreatePath, tid_PathTool,
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

    // (1) OnKey (+0x30): open/close CreateBar dialog, rotate/flip, left-click confirm.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
    void* tool = *(void**)toolSlot;
    if (!tool) return -1;
    {
        void** vt = *(void***)tool;
        typedef char (*OnKey_t)(void*, char);
        ((OnKey_t)vt[6])(tool, (char)activeFlag);   // vt[6] = +0x30
    }

    // Right-click cancel (action 6 = MouseRight) when active and not tool-locked
    // (EntityUI+0x80 == 0): tool +0x48 "can-deactivate?" (our stub returns 0) -> clear.
    if (activeFlag != 0 && *(char*)(pc + 0x80) == 0) {
        void* km = ToolKeyMap();
        if (km && fn_KeyMap_Pressed && fn_KeyMap_Pressed(km, 0, 6)) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
            void* t2 = *(void**)toolSlot;
            int canStay = 1;
            if (t2) { void** vt2 = *(void***)t2; typedef int (*Slot9_t)(void*); canStay = ((Slot9_t)vt2[9])(t2); }
            if (canStay == 0) {
                EntityUI_SetEditMode_Impl(entityUI, 0);
                return -1;  // tool gone this frame
            }
        }
    }

    // (2) Serialize (+0x38): update the ghost-mesh preview + CreateBar cost/validity.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
    tool = *(void**)toolSlot;
    if (!tool) return -1;
    {
        void** vt = *(void***)tool;
        typedef void (*Serialize_t)(void*, void*, char);
        int64_t timeLocal = time;
        ((Serialize_t)vt[7])(tool, &timeLocal, (char)activeFlag);   // vt[7] = +0x38
    }

    // (3) cursor (+0x60): id used for SetCursor.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
    tool = *(void**)toolSlot;
    if (!tool) return -1;
    {
        void** vt = *(void***)tool;
        typedef int (*Cursor_t)(void*);
        return ((Cursor_t)vt[12])(tool);   // vt[12] = +0x60
    }
}

// ============================================================
// Piece 3 — draw-phase footprint overlay (tool +0x40).
// ============================================================
extern "C" void EntityUI_ToolDraw_SA(void* entityUI, void* context, int activeFlag) {
    if (!entityUI) return;
    uintptr_t pc = (uintptr_t)entityUI;
    void* toolSlot = (void*)(pc + 0x28);
    if (*(void**)toolSlot == nullptr) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
    void* tool = *(void**)toolSlot;
    if (!tool) return;
    void** vt = *(void***)tool;
    typedef void (*Draw_t)(void*, void*, char);
    ((Draw_t)vt[8])(tool, context, (char)activeFlag);   // vt[8] = +0x40
}

