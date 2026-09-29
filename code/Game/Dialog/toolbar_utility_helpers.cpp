// code/Game/Dialog/toolbar_utility_helpers.cpp
// GUI_PLAN Keystone F, Phase 4 — Utility dialogs (toolbar case 2).
//
// Two exe helpers that Toolbar_Utility_Impl (toolbardialog.cpp, FUN_140101a80)
// depends on, ported so ResolveAddr(Module::Exe, addr) resolves them in the
// standalone exe. Both are unregistered otherwise → the engine_funcs pointers
// fn_FindDialogByControllerType / fn_BST_Insert_0x14 stay NULL → the very first
// calls in Toolbar_Utility_Impl are null-calls (EXEC of 0).
//
//   FUN_140100270  FindDialogByControllerType — walk the live UI dialog list and
//     return the open dialog whose ToolUI controller's +0x30 (its Description
//     pointer) == param. Used to detect an already-open utility dialog (toggle).
//   FUN_140014890  BST_Insert_0x14 — AVL find-or-insert of a 0x38-byte
//     utility-dialog position node keyed by the Description ptr; rebalances via
//     the already-registered retrace 0x140014b40.
//
// Faithful 1:1 against the Ghidra disassembly (offsets verified). Registered via
// standalone/reg_toolbar_utility.cpp.

#include "Player/exe_bodies.h"   // B3R: TradeWork_impl_4b40
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/PlayerToolManager.h"
#include "Object/factory_registration.h"
#include "ui/UserInterface.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "Dialog/ToolUI.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

namespace {

// Retrace/rebalance (0x140014b40, TradeWork_impl_4b40) — shared AVL retrace,
// called by name (B3R).

// The 0x38-byte node 0x140014890 inserts: a key and two data qwords (for the
// utility-dialog set, PlayerToolManager.h's TrackedEntityNode: the key, the
// packed x / y and the flags), then the tree links.
struct SetNode {
    uint64_t key;       // +0x00
    uint64_t data[2];   // +0x08 / +0x10
    SetNode* parent;    // +0x18
    SetNode* left;      // +0x20
    SetNode* right;     // +0x28
    uint32_t balance;   // +0x30
    uint32_t _pad34;    // +0x34
};
static_assert(sizeof(SetNode) == 0x38, "set node 0x38");
static_assert(offsetof(SetNode, parent) == 0x18, "");
static_assert(offsetof(SetNode, left) == 0x20, "");
static_assert(offsetof(SetNode, right) == 0x28, "");

// The set header the callers hand over: the root and the count.
struct SetHeader {
    SetNode* root;    // +0x00
    int32_t  count;   // +0x08
};

// sub_4a90 — allocate + init a new 0x38-byte AVL node (copies the 3 data qwords,
// sets parent, zeroes children + balance).
SetNode* BstNodeAlloc(const uint64_t* insertData, SetNode* parent) {
    SetNode* n = static_cast<SetNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
    if (!n) return nullptr;
    n->key = insertData[0];               // key (Description ptr)
    n->data[0] = insertData[1];           // packed (x,y)
    n->data[1] = insertData[2];           // flags
    n->parent = parent;
    n->left = nullptr;
    n->right = nullptr;
    n->balance = 0;
    return n;
}

}  // namespace

// ---- FUN_140100270: FindDialogByControllerType ------------------------------
// The open dialog whose ToolUI controller's +0x30 (its description) is
// `description`, or null. Used to detect an already-open utility dialog (toggle).
UI::DialogInstance* PlayerToolManager::FindUtilityDialog(const ComponentDescription* description) {
    GameUpdateLoop* game = GameUpdateLoop::_singleton;
    if (!game) return nullptr;
    UI::UserInterface* ui = game->m_userInterface;
    if (!ui) return nullptr;

    // ToolUI type-id (DWORD_1401c5788, RegisterType_ToolUI writer), cached.
    static uint32_t s_toolUITid = 0;
    if (s_toolUITid == 0) s_toolUITid = ToolUI::TypeId();
    const uint32_t toolUITid = s_toolUITid;

    // Walk the dialog linked list. `_dialogs.GetFirst()` IS retail's
    // GetFirstDialog (0x180031150 = `*retval = this->_dialogs._head`), and the
    // iterator is the bare node pointer, so `it.node` is the address retail
    // hands to CheckPtr and `it.node->element` is the DialogInstance.
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        // LEFT: the list's element is the runtime's `DialogInstancePtr`, a void*
        // (UserInterface.h).
        UI::DialogInstance* dialog = reinterpret_cast<UI::DialogInstance*>(it.node->element);

        // GetController(ToolUI tid), narrowed to a ToolUI
        const ToolUI* controller = nullptr;
        if (dialog) {
            UI::DialogController* c = dialog->GetController(toolUITid).Get();
            if (c && ::Object::IsDerivedFromNullSafe(c, toolUITid)) controller = static_cast<const ToolUI*>(c);
        }

        if (controller != nullptr && controller->description == description) {
            return dialog;
        }
    }
    return nullptr;
}

extern "C" {

// B4 FORWARD: tutorial_wave.cpp (TUT) passes the retail shape — its own object
// in RCX (unread), the InternalPtr slot, the key as a qword.
void ToolbarUtility_FindDialogByControllerType_0270(void* /*toolController*/,
                                                    void* retBuf, uint64_t param) {
    *reinterpret_cast<UI::DialogInstance**>(retBuf) =
        PlayerToolManager::FindUtilityDialog(reinterpret_cast<const ComponentDescription*>(param));  // CAST-OK: ptr_int
}

// ---- FUN_140014890: BST_Insert_0x14 ----------------------------------------
// (RCX=rootHolder, RDX=retNode, R8=insertData)
//   rootHolder: the set header, root node and int32 count
//   retNode:    *retNode = the found-or-inserted node
//   insertData: [0]=key, [1] / [2] the node's two data qwords
// Faithful AVL find-or-insert; rebalances from the insertion point via 0x140014b40.
// B4 FORWARD: the int64_t* / void** / uint64_t* signature is kept for
// townhallui.cpp (DLG-e, running) and PlayerToolManager::ToggleUtilityDialog.
void ToolbarUtility_BstInsert_4890(int64_t* rootHolder, void** retNode, uint64_t* insertData) {
    SetHeader* set = reinterpret_cast<SetHeader*>(rootHolder);
    if (set->root == nullptr) {
        // Empty tree — the new node becomes the root.
        SetNode* n = static_cast<SetNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
        if (!n) {
            set->root = nullptr;
            *retNode = nullptr;
        } else {
            n->key = insertData[0];
            n->data[0] = insertData[1];
            n->data[1] = insertData[2];
            n->parent = nullptr; n->left = nullptr; n->right = nullptr;
            n->balance = 0;
            *retNode = n;
            set->root = n;
        }
    } else {
        const uint64_t key = insertData[0];
        SetNode* next = set->root;
        SetNode* node = nullptr;      // current node examined (== insertion parent)
        SetNode* newNode = nullptr;
        bool inserted = false;

        while (!inserted) {
            node = next;
            // Walk right while node->key < key.
            while (node->key <= key) {
                if (key <= node->key) {      // equal → existing node found (no count++)
                    *retNode = node;
                    return;
                }
                if (node->right == nullptr) {   // right child null → insert right
                    newNode = BstNodeAlloc(insertData, node);
                    node->right = newNode;
                    inserted = true;
                    break;
                }
                next = node->right;
                node = next;
            }
            if (inserted) break;
            // node->key > key → go left.
            if (node->left == nullptr) {     // left child null → insert left
                newNode = BstNodeAlloc(insertData, node);
                node->left = newNode;
                inserted = true;
                break;
            }
            next = node->left;
        }

        // LEFT: 0x140014b40's declaration (Player/exe_bodies.h) takes int64_t*.
        TradeWork_impl_4b40(rootHolder, reinterpret_cast<int64_t*>(&node));   // 0x140014b40
        *retNode = newNode;
    }

    // count — incremented on every non-early-return path, matching the exe
    // (even the alloc-failure sub-case).
    set->count += 1;
}

}  // extern "C"
