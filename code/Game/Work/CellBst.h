// code/Game/Work/CellBst.h
//
// retail 0x14001ca60 -- the 0x30-node AVL remove (`TradeDescription_impl_ca60`), as a
// TEMPLATE over the node type (B7 LX, B7_RULES D7b; the shape of AvlRebalance<>,
// Action/PathAvl.h). MSVC folded one body over every 0x10-element Set instantiation the
// exe removes from (ICF): the area works' cell sets (Set<WorkCellAssignment>), ClearArea's
// entity set (Set<WorkEntityAssignment>), the work scheduler's id trees and bucket entries
// (WorkSchedulerTree / WorkSchedulerIdNode), the pathfinder's region-pair index
// (AvlTreeHead<AvlNode30>) and Goals' id tree. Every layout has the same links -- parent
// +0x10, left +0x18, right +0x20, balance +0x28, height +0x2a -- and the body reads only
// those, the header's root (+0x00) and its dword count (+0x08): each caller instantiates
// it on its own node type, and no header or node is viewed as another's.
//
// Retail takes the header in RCX ([RCX] the root, dword [RCX+8] the count) and the node in
// RDX. The three header spellings in the tree name those two fields differently
// (Collection::Set's `_root` / `_count`, AvlTreeHead's and WorkSchedulerTree's `root` /
// `count`), so the core takes the two fields and one overload per header hands them over.
//
// The retrace is 0x140013480, `AvlRebalance<Node>` (action_pf_avl.cpp: one explicit
// instantiation per node type). The node is freed with MemFree; its element is NOT
// destroyed (retail has no element destructor in this body).
#pragma once

#include <cstdint>
#include "Action/PathAvl.h"        // AvlRebalance (0x140013480), AvlTreeHead
#include "Game/WorkScheduler.h"    // WorkSchedulerTree
#include "collection/Collection.h"
#include "system/MemoryAllocator.h"

// The balance / height recompute retail inlines into 0x14001ca60 (the predecessor's old
// parent, before the splice is retraced): a child contributes height + 1, a missing child
// 0; balance = right - left, height = max.
template <class Node>
inline void WorkCellBST_UpdateHB(Node* n) {
    if (n == nullptr) return;
    Node* l = n->left;
    Node* r = n->right;
    int16_t lh = (l == nullptr) ? 0 : static_cast<int16_t>(l->height + 1);
    int16_t rh = (r == nullptr) ? 0 : static_cast<int16_t>(r->height + 1);
    n->balance = static_cast<int16_t>(rh - lh);
    n->height = (rh < lh) ? lh : rh;
}

// retail 0x14001ca60 -- unlink `node` (three cases; case 3 splices the PREDECESSOR, the
// rightmost node of node->left), free it, retrace.
template <class Node>
inline void WorkCellBST_Remove_ca60(Node*& root, int32_t& count, Node* node) {
    Node* local_res8 = node->parent;
    count -= 1;
    Node* lVar5 = node->right;

    if (lVar5 == nullptr) {
        Node* parent = node->parent;
        Node* left   = node->left;
        if (parent == nullptr) root = left;
        else if (parent->left == node) parent->left = left;
        else parent->right = left;
        lVar5 = node->left;             // surviving child = left
    } else {
        Node* lVar1 = node->left;
        if (lVar1 != nullptr) {
            Node* pred = lVar1;
            Node* walk = lVar1;
            do { pred = walk; walk = pred->right; } while (pred->right != nullptr);

            local_res8 = nullptr;
            if (pred != lVar1) {
                lVar5 = pred->left;
                local_res8 = pred->parent;
                local_res8->right = lVar5;          // pred->parent->right = pred->left
                if (lVar5 != nullptr) lVar5->parent = local_res8;
                pred->left = node->left;
                WorkCellBST_UpdateHB(local_res8);
            }
            lVar5 = local_res8;
            pred->right = node->right;
            if (pred->left != nullptr)
                pred->left->parent = pred;
            if (pred->right != nullptr)
                pred->right->parent = pred;
            Node* nodeParent = node->parent;
            pred->parent = nodeParent;
            Node* local_res10;
            if (nodeParent == nullptr) {
                root = pred;
                pred->parent = nullptr;
                local_res10 = root;
            } else {
                if (nodeParent->left == node)
                    nodeParent->left = pred;
                else
                    nodeParent->right = pred;
                local_res10 = pred;
            }
            System::MemoryAllocator::MemFree(node);
            if (lVar5 == nullptr) AvlRebalance(&root, &local_res10);
            else AvlRebalance(&root, &local_res8);
            return;
        }
        // node->left == 0, node->right != 0
        Node* parent = node->parent;
        if (parent == nullptr) root = lVar5;
        else if (parent->left == node) parent->left = lVar5;
        else parent->right = lVar5;
        lVar5 = node->right;            // surviving child = right
    }

    if (lVar5 != nullptr) lVar5->parent = node->parent;
    Node* local_res10 = lVar5;
    System::MemoryAllocator::MemFree(node);
    if (lVar5 == nullptr) AvlRebalance(&root, &local_res8);
    else AvlRebalance(&root, &local_res10);
}

// The header each caller holds: retail's RCX, its root and count handed to the core.
template <class T>
inline void WorkCellBST_Remove_ca60(Collection::Set<T>* set, typename Collection::Set<T>::Node* node) {
    static_assert(sizeof(T) == 0x10, "0x14001ca60's element is 0x10 bytes (the 0x30 node)");
    WorkCellBST_Remove_ca60(set->_root, set->_count, node);
}
template <class Node>
inline void WorkCellBST_Remove_ca60(AvlTreeHead<Node>* head, Node* node) {
    WorkCellBST_Remove_ca60(head->root, head->count, node);
}
// A template so a translation unit that never removes from a scheduler tree does not need
// WorkSchedulerIdNode complete (Game/WorkSchedulerNodes.h).
template <class Node>
inline void WorkCellBST_Remove_ca60(WorkSchedulerTree* tree, Node* node) {
    WorkCellBST_Remove_ca60(tree->root, tree->count, node);
}
