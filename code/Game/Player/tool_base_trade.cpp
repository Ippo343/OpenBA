// code/Game/Player/tool_base_trade.cpp
// The EntityUI pin helpers and the AVL set primitives of the 0x38-byte nodes.
//
//   0x140066800 EntityUI::SetPinned        -> 0x1400666b0
//   0x1400666b0 EntityUI::SyncPin          -> 0x140102e30, 0x14002c690, GetController
//   0x140102e30 TrackManagerPinEntity (Set insert into the tool manager's +0x60 set)
//   0x14002c690 Avl38FindRemove (Set find + remove)  -> 0x140025740
//   0x140025740 Avl38Remove (Set node remove)        -> 0x140025920, static MemFree
//   0x140025920 Avl38Retrace (retrace + rebalance)   -> 0x140015000, 0x140014ed0
//   0x140015000 Avl38RotateRight, 0x140014ed0 Avl38RotateLeft (Player/exe_bodies.h)
//
// The set is an AVL tree of 0x38-byte nodes (an 0x18-byte element: here
// TrackedEntityNode, PlayerToolManager.h): +0x00 key (the entity), +0x08 / +0x0C
// ints, +0x10 byte flag (=1 on insert), +0x18 parent, +0x20 left, +0x28 right,
// +0x30 short balance (= rightH - leftH), +0x32 short height. Retail's bodies
// serve every 0x18-element set (B.4 B13): they are templates over the node.
//
// Port is 1:1 with the decompile — do not "improve".

#include "ui/UserInterface.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/PlayerToolManager.h"   // TrackedEntityNode
#include "Action/PathAvl.h"           // AvlTreeHead
#include "system/MemoryAllocator.h"
#include "object/TypeInfo.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "runtime/types/ui/CheckElement.h"
#include "Dialog/ToolUI.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/EntityUI.h"
#include "Player/exe_bodies.h"

namespace {

// ============================================================
// 0x140025740 — remove an AVL node and rebalance. 1:1 port of TradeWork_helper_5740.
// tree = {root, count}; node = the node to remove.
// ============================================================
template <class Node>
void Avl38Remove(AvlTreeHead<Node>* tree, Node* node) {
    Node** root = &tree->root;
    Node* local_res8;   // retrace cursor candidate A
    Node* local_res10;  // retrace cursor candidate B

    local_res8 = node->parent;
    tree->count = tree->count - 1;

    Node* rightChild = node->right;
    Node* fixup;  // lVar5 in decompile

    if (rightChild == nullptr) {
        // No right child: splice in the left child.
        Node* parent = node->parent;
        Node* left   = node->left;
        if (parent == nullptr) {
            *root = left;
        } else if (parent->left == node) {
            parent->left = left;
        } else {
            parent->right = left;
        }
        fixup = node->left;
    } else {
        Node* leftChild = node->left;
        if (leftChild != nullptr) {
            // Two children: find in-order predecessor (rightmost of left subtree).
            Node* pred = leftChild;
            do {
                local_res10 = pred;
                pred = local_res10->right;
            } while (local_res10->right != nullptr);

            local_res8 = nullptr;
            if (local_res10 != leftChild) {
                // Detach predecessor from its parent, re-link.
                Node* predLeft = local_res10->left;
                local_res8 = local_res10->parent;
                local_res8->right = predLeft;
                if (predLeft != nullptr) predLeft->parent = local_res8;
                local_res10->left = node->left;

                int16_t lh = Avl38ChildHeight(local_res8->left);
                int16_t rh = Avl38ChildHeight(local_res8->right);
                local_res8->balance = (int16_t)(rh - lh);
                local_res8->height = (lh < rh) ? rh : lh;
            }
            Node* fix2 = local_res8;  // lVar5 = local_res8
            local_res10->right = node->right;
            if (local_res10->left != nullptr)  local_res10->left->parent  = local_res10;
            if (local_res10->right != nullptr) local_res10->right->parent = local_res10;

            Node* parent = node->parent;
            local_res10->parent = parent;
            if (parent == nullptr) {
                *root = local_res10;
                local_res10->parent = nullptr;
                local_res10 = *root;
            } else if (parent->left == node) {
                parent->left = local_res10;
            } else {
                parent->right = local_res10;
            }
            System::MemoryAllocator::MemFree(node);
            Node** cursor = (fix2 == nullptr) ? &local_res10 : &local_res8;
            Avl38Retrace(root, cursor);
            return;
        }
        // Right child only.
        Node* parent = node->parent;
        if (parent == nullptr) {
            *root = rightChild;
            fixup = node->right;
        } else if (parent->left == node) {
            parent->left = rightChild;
            fixup = node->right;
        } else {
            parent->right = rightChild;
            fixup = node->right;
        }
    }

    if (fixup != nullptr) {
        fixup->parent = node->parent;
    }
    local_res10 = fixup;
    System::MemoryAllocator::MemFree(node);
    if (fixup == nullptr) {
        Avl38Retrace(root, &local_res8);
    } else {
        Avl38Retrace(root, &local_res10);
    }
}

// ============================================================
// 0x14002c690 — find a node by key (the entity) and remove it. 1:1 port of
// TradeWork_impl_c690. The keys compare as unsigned 64-bit values.
// ============================================================
template <class Node, class Key>
void Avl38FindRemove(AvlTreeHead<Node>* tree, const Key* keyRef) {
    Node* self = tree->root;
    if (self == nullptr) return;
    Key key = *keyRef;
    do {
        if (key < self->key) {
            self = self->right;
        } else {
            if (key <= self->key) {
                Avl38Remove(tree, self);
                return;
            }
            self = self->left;
        }
    } while (self != nullptr);
}

// ============================================================
// 0x140102e30 — insert / update the entity in the tool manager's +0x60 set (root
// +0x60, count +0x68). key = the entity, x / y = the node's two ints.
// LEFT a free function: its receiver is PlayerToolManager, whose header is GAME-b's.
// ============================================================
void TrackManagerPinEntity(PlayerToolManager* manager, ComponentEntity* const* keyRef,
                           int32_t x, int32_t y) {
    TrackedEntityNode** rootSlot = &manager->field_0x60;
    TrackedEntityNode* cur = *rootSlot;
    ComponentEntity* key = *keyRef;

    // First pass: search; if an existing node matches the key, update in place.
    while (cur != nullptr) {
        if (key < cur->key) {
            cur = cur->right;
        } else {
            if (key <= cur->key) {
                // exact match -> update flag + payloads, no structural change.
                cur->active = 1;
                cur->x = x;
                cur->y = y;
                return;
            }
            cur = cur->left;
        }
    }

    // Not found: allocate + link a new node. Staged values from decompile:
    ComponentEntity* newKey = key;   // local_38
    uint8_t  newFlag = 1;            // local_28 (low byte of local_40)
    int32_t  newX = x;               // local_30
    int32_t  newY = y;               // local_2c

    TrackedEntityNode* newNode;
    if (*rootSlot == nullptr) {
        // Empty tree: new root.
        TrackedEntityNode* n = static_cast<TrackedEntityNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
        newNode = nullptr;
        if (n != nullptr) {
            n->key = nullptr;
            n->key = newKey;
            n->x = newX;
            n->y = newY;
            n->active = newFlag;
            n->parent = nullptr;
            n->left   = nullptr;
            n->right  = nullptr;
            n->balance = 0;   // balance(short) + height(short): one dword store in the tree
            n->height  = 0;
            newNode = n;
        }
        *rootSlot = newNode;
    } else {
        // Walk to the insertion point. local_res8 tracks the current parent.
        TrackedEntityNode* walk = *rootSlot;
        TrackedEntityNode* parent;
        for (;;) {
            // Inner loop: descend right while node.key <= newKey (and check exact dup).
            for (;;) {
                parent = walk;
                if (parent->key > newKey) break;  // go left branch below
                if (newKey <= parent->key) {
                    return;  // duplicate appeared mid-walk; nothing to do
                }
                walk = parent->right;
                if (parent->right == nullptr) {
                    // attach as right child
                    TrackedEntityNode* n = static_cast<TrackedEntityNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
                    newNode = nullptr;
                    if (n != nullptr) {
                        n->key = nullptr;
                        n->key = newKey;
                        n->x = newX;
                        n->y = newY;
                        n->active = newFlag;
                        n->left  = nullptr;
                        n->right = nullptr;
                        n->balance = 0;
                        n->height  = 0;
                        n->parent = parent;
                        newNode = n;
                    }
                    parent->right = newNode;
                    goto rebalance;
                }
            }
            // node.key > newKey: descend left; if no left child, attach there.
            walk = parent->left;
            if (parent->left == nullptr) {
                TrackedEntityNode* n = static_cast<TrackedEntityNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
                newNode = nullptr;
                if (n != nullptr) {
                    n->key = nullptr;
                    n->key = newKey;
                    n->x = newX;
                    n->y = newY;
                    n->active = newFlag;
                    n->left  = nullptr;
                    n->right = nullptr;
                    n->balance = 0;
                    n->height  = 0;
                    n->parent = parent;
                    newNode = n;
                }
                parent->left = newNode;
                goto rebalance;
            }
        }
    rebalance:
        {
            TrackedEntityNode* cursor = parent;
            Avl38Retrace(rootSlot, &cursor);
        }
    }

    // count++
    manager->field_0x68 = manager->field_0x68 + 1;
}

} // namespace

// ---- the C entry points other blocks' files still bind (B4 FORWARD) ----------
// Their callers hold the set and its nodes as integers / void* (WORK's
// tradework.cpp / tradework_serialize.cpp / marketwork_bst_*.cpp, GAME's
// gameupdateloop_serialize.cpp, DLG's entityui.cpp, COMP's nomadcomponent.cpp):
// the casts below are LEFT until those callers hold typed sets. Their nodes are
// the generic Avl38Node (Player/exe_bodies.h): retail runs the same bodies on
// every 0x18-element set.

// B4 FORWARD: 5 outside callers (Dialog/entityui.cpp, Game/gameupdateloop_serialize.cpp,
// Work/tradework_serialize.cpp, Work/marketwork_bst_*.cpp)
extern "C" void PlayerTrade_Retrace_25920(int64_t* root, int64_t* cursor) {
    Avl38Retrace(reinterpret_cast<Avl38Node**>(root), reinterpret_cast<Avl38Node**>(cursor));   // LEFT: B4 FORWARD
}

// B4 FORWARD: 2 outside callers (Dialog/entityui.cpp, Work/tradework.cpp)
extern "C" void PlayerTrade_BSTRemove_25740(int64_t* root, void* nodeArg) {
    Avl38Remove(reinterpret_cast<AvlTreeHead<Avl38Node>*>(root), reinterpret_cast<Avl38Node*>(nodeArg));   // LEFT: B4 FORWARD
}

// B4 FORWARD: 2 outside callers (Work/tradework.cpp, Component/nomadcomponent.cpp)
extern "C" void PlayerTrade_BSTFindRemove_2c690(int64_t* root, void* keyIBP) {
    Avl38FindRemove(reinterpret_cast<AvlTreeHead<Avl38Node>*>(root), reinterpret_cast<const void* const*>(keyIBP));   // LEFT: B4 FORWARD
}

// ============================================================
// 0x1400666b0 — EntityUI: read the dialog's ToolUI, copy the pin check box
// (CheckElement +0xd8) into its pinned flag (+0x40), then add or remove this
// entity in the tool manager's +0x60 set.
// ============================================================
void EntityUI::SyncPin() {
    // Resolve the dialog's ToolUI via DialogInstance::GetController.
    // 0x1401c5788 is ToolUI's cell (Table 1).
    uint32_t ctrlTypeId = ToolUI::TypeId();

    Object::InternalPtr<UI::DialogController> controller = _dialog->GetController(ctrlTypeId);
    ToolUI* toolUI = nullptr;
    if (controller.Get() != nullptr) {
        if (controller->IsDerivedFrom(ctrlTypeId)) {
            toolUI = static_cast<ToolUI*>(controller.Get());
        }
    }

    // Copy the pin flag into the ToolUI (+0x40). No null test: retail has none.
    uint8_t flag = checkPin->_checked;
    toolUI->pinned = flag;
    if (flag == 0 && toolUI->dragging != 0) {
        toolUI->dragging = 0;
    }

    // Add (flag set) or remove (flag clear) this entity in the tool manager's set.
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return;
    PlayerToolManager* manager = world->m_toolManager;

    if (checkPin->_checked != 0) {
        int32_t y = _dialog->_upperLeftY;
        int32_t x = _dialog->_upperLeftX;
        ComponentEntity* key = entity;  // local_res8 = entity key
        TrackManagerPinEntity(manager, &key, x, y);
    } else {
        ComponentEntity* key = entity;
        // LEFT: PlayerToolManager (GAME-b) declares retail's +0x60 Set as root + count.
        Avl38FindRemove(reinterpret_cast<AvlTreeHead<TrackedEntityNode>*>(&manager->field_0x60), &key);
    }
}

// ============================================================
// 0x140066800 — check the pin box (+0xd8 of the CheckElement at +0x38) then
// run SyncPin. 1:1 port of EditTool_helper_6800.
// ============================================================
void EntityUI::SetPinned() {
    checkPin->_checked = 1;
    SyncPin();
}

// B4 FORWARD: 1 outside caller (Dialog/toolbardialog.cpp) holds the EntityUI as an integer.
extern "C" void PlayerTrade_EntityUI_66b0(int64_t param_1) {
    reinterpret_cast<EntityUI*>(param_1)->SyncPin();   // LEFT: B4 FORWARD
}

// Until edittool.cpp holds its pin page typed (this block, below).
extern "C" void PlayerTrade_EditTool_66800(int64_t param_1) {
    reinterpret_cast<EntityUI*>(param_1)->SetPinned();   // LEFT: edittool.cpp's integer page handle
}
