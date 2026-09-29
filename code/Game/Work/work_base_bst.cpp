// code/Game/Work/work_base_bst.cpp
// Shared Work "simple BST" (AVL) family — citizen-AI work-tracking tree.
//
// These are the exe's inlined Collection::Set instances for an 8-byte key (an
// Object pointer compared as an unsigned 64-bit value): EntityList's update trees,
// the Work leaves' citizen sets (WorkCitizenSetNode), the scheduler's order tree.
#include "Work/exe_bodies.h"
//
// All functions are faithful 1:1 ports of the decompiled exe helpers (Ghidra-verified).
//
// Node (Collection::Set<uint64_t>::Node, 0x28 bytes):
//   +0x00  uint64  element      (the key == the pointer an InternalPtr holds)
//   +0x08  ptr     parent
//   +0x10  ptr     left child
//   +0x18  ptr     right child
//   +0x20  int16   balance      (= rightHeight - leftHeight)
//   +0x22  int16   height       (leaf = 0; node = max(childHeight)+1 encoded as child.height+1)
// Tree: the Set's header, _root (+0x00) and _count (+0x08).
//
// LEFT (every entry point): the callers in other blocks hand the set as a
// uintptr_t* and the key / node as void*; each body views them as the Set and
// its node once, at the top.

#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"     // ENDGAME B.3.4 caller batch C
#include "object/Internal.h"   // ENDGAME B.3.4a closing: InternalBasePtr::Serialize
#include "collection/Collection.h"
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()
#include <cstdint>

namespace {
using KeySet = Collection::Set<uint64_t>;
using Node   = KeySet::Node;
}  // namespace

// ============================================================
// Internal AVL primitives (static; not registered)
// ============================================================

// Recompute balance(+0x20) and height(+0x22) of node `n` from its children.
static void SimpleBST_UpdateHB(Node* n) {
    if (n == nullptr) return;
    Node* l = n->left;
    Node* r = n->right;
    int16_t lh = (l == nullptr) ? 0 : static_cast<int16_t>(l->height + 1);
    int16_t rh = (r == nullptr) ? 0 : static_cast<int16_t>(r->height + 1);
    n->balance = static_cast<int16_t>(rh - lh);
    n->height = (rh < lh) ? lh : rh;
}

// Reimpl of FUN_140014c70 — left rotation about `node` (pivot = node->right).
static Node* SimpleBST_RotateLeft(Node** root, Node* node) {
    if (node == *root) *root = node->right;
    Node* parent = node->parent;
    if (parent != nullptr) {
        if (parent->left == node)
            parent->left = node->right;
        else
            parent->right = node->right;
    }
    Node* pivot = node->right;
    pivot->parent = node->parent;
    Node* tmp = pivot->left;
    node->right = tmp;
    if (tmp != nullptr) tmp->parent = node;
    pivot->left = node;
    node->parent = pivot;
    SimpleBST_UpdateHB(pivot->left);
    SimpleBST_UpdateHB(pivot->right);
    SimpleBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_140014da0 — right rotation about `node` (pivot = node->left).
static Node* SimpleBST_RotateRight(Node** root, Node* node) {
    if (node == *root) *root = node->left;
    Node* parent = node->parent;
    if (parent != nullptr) {
        if (parent->left == node)
            parent->left = node->left;
        else
            parent->right = node->left;
    }
    Node* pivot = node->left;
    pivot->parent = node->parent;
    Node* tmp = pivot->right;
    node->left = tmp;
    if (tmp != nullptr) tmp->parent = node;
    pivot->right = node;
    node->parent = pivot;
    SimpleBST_UpdateHB(pivot->left);
    SimpleBST_UpdateHB(pivot->right);
    SimpleBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_14001a8c0 (CitizenAI_State_Leisure_sub_a8c0) — AVL retrace: walk up
// the parent chain from *pWalk, recomputing balance/height and rotating where the
// balance leaves [-1,1]. The parent is captured before rotations so the walk follows
// the original ancestry.
static void SimpleBST_Retrace(Node** root, Node** pWalk) {
    while (*pWalk != nullptr) {
        Node* node = *pWalk;
        Node* next = node->parent;   // parent, pre-rotation
        SimpleBST_UpdateHB(node);
        node = *pWalk;
        if (node->balance > 1 && node->right != nullptr) {
            Node* rc = node->right;
            if (rc->balance < 0) {       // right-left case
                SimpleBST_RotateRight(root, rc);
                node = *pWalk;
            }
            SimpleBST_RotateLeft(root, node);
        }
        node = *pWalk;
        if (node->balance < -1 && node->left != nullptr) {
            Node* lc = node->left;
            if (lc->balance > 0) {       // left-right case
                SimpleBST_RotateLeft(root, lc);
                node = *pWalk;
            }
            SimpleBST_RotateRight(root, node);
        }
        *pWalk = next;
    }
}

// Reimpl of FUN_140021b40 — allocate + init a 0x28-byte leaf (key from *key).
static Node* AllocNode_1b40(const uint64_t* key, Node* parent) {
    Node* node = static_cast<Node*>(System::MemoryAllocator::MemAlloc(0x28, 8));
    if (!node) return nullptr;
    node->element = *key;
    node->parent = parent;
    node->left = nullptr;
    node->right = nullptr;
    node->balance = 0;   // balance + height: one dword store in retail
    node->height = 0;
    return node;
}

// ============================================================
// Registered helpers
// ============================================================

// Reimpl of CitizenAI_impl_1a30 (FUN_140021a30) — AVL insert.
// Original prototype: void* fn(longlong* root, void* outResult, IBP* keyIBP).
// key = *keyIBP; the inserted (or pre-existing) node is written to *outResult; the
// function returns outResult. Most callers typedef it `void fn(root, outResult, key)`
// (ignoring the return); some as `void* fn(treeRoot, outResult, keyData)`.
void* WorkBST_RegisterWork_1a30(uintptr_t* root, void* outResult, void* keyIBP) {
    KeySet* set = reinterpret_cast<KeySet*>(root);
    const uint64_t* key = reinterpret_cast<const uint64_t*>(keyIBP);
    Node** pOut = reinterpret_cast<Node**>(outResult);
    if (set->_root == nullptr) {
        Node* node = AllocNode_1b40(key, nullptr);
        set->_root = node;
        *pOut = node;
    } else {
        Node* cur = set->_root;
        Node* newLeaf = nullptr;
        for (;;) {
            // descend left while key < node->key
            for (;;) {
                if (*key < cur->element) {
                    if (cur->left == nullptr) {
                        newLeaf = AllocNode_1b40(key, cur);
                        cur->left = newLeaf;
                        goto done_insert;
                    }
                    cur = cur->left;
                } else {
                    break;
                }
            }
            if (*key <= cur->element) {
                *pOut = cur;   // key already present
                return outResult;
            }
            if (cur->right == nullptr) {
                newLeaf = AllocNode_1b40(key, cur);
                cur->right = newLeaf;
                goto done_insert;
            }
            cur = cur->right;
        }
    done_insert:
        Node* retraceFrom = cur;   // parent of the new leaf
        SimpleBST_Retrace(&set->_root, &retraceFrom);
        *pOut = newLeaf;
    }
    set->_count += 1;   // count++
    return outResult;
}

// Reimpl of CitizenAI_impl_a6e0 (FUN_14001a6e0) — AVL delete of `nodeArg`.
// Handles the three classic cases (no right child; right child with a left subtree
// whose in-order predecessor is spliced in; right child but no left child), then
// retraces from the splice point. Caller: void fn(void* bstRoot, void* node).
void WorkBST_BSTRemove_a6e0(uintptr_t* root, void* nodeArg) {
    KeySet* set = reinterpret_cast<KeySet*>(root);
    Node* node = reinterpret_cast<Node*>(nodeArg);
    Node* local_res8 = node->parent;
    set->_count -= 1;   // count--
    Node* lVar5 = node->right;

    if (lVar5 == nullptr) {
        Node* parent = node->parent;
        Node* left   = node->left;
        if (parent == nullptr) set->_root = left;
        else if (parent->left == node) parent->left = left;
        else parent->right = left;
        lVar5 = node->left;          // surviving child = left
    } else {
        Node* lVar1 = node->left;
        if (lVar1 != nullptr) {
            // in-order predecessor = rightmost of left subtree
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
                SimpleBST_UpdateHB(local_res8);
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
                set->_root = pred;
                pred->parent = nullptr;
                local_res10 = set->_root;
            } else {
                if (nodeParent->left == node)
                    nodeParent->left = pred;
                else
                    nodeParent->right = pred;
                local_res10 = pred;
            }
            System::MemoryAllocator::MemFree(node);
            if (lVar5 == nullptr) SimpleBST_Retrace(&set->_root, &local_res10);
            else SimpleBST_Retrace(&set->_root, &local_res8);
            return;
        }
        // node->left == 0, node->right != 0
        Node* parent = node->parent;
        if (parent == nullptr) set->_root = lVar5;
        else if (parent->left == node) parent->left = lVar5;
        else parent->right = lVar5;
        lVar5 = node->right;          // surviving child = right
    }

    if (lVar5 != nullptr) lVar5->parent = node->parent;
    Node* local_res10 = lVar5;
    System::MemoryAllocator::MemFree(node);
    if (lVar5 == nullptr) SimpleBST_Retrace(&set->_root, &local_res8);
    else SimpleBST_Retrace(&set->_root, &local_res10);
}

// Reimpl of CitizenAI_impl_5520 (FUN_140025520) — find a node by key (uint64 at IBP+0)
// and remove it. Caller: void fn(void* bstRoot, void* keyIBP).
void WorkBST_BSTFindRemove_25520(uintptr_t* root, void* keyIBP) {
    KeySet* set = reinterpret_cast<KeySet*>(root);
    const uint64_t* key = reinterpret_cast<const uint64_t*>(keyIBP);
    Node* node = set->_root;
    if (node == nullptr) return;
    do {
        if (*key < node->element) {
            node = node->left;
        } else {
            if (*key <= node->element) {
                WorkBST_BSTRemove_a6e0(root, node);
                return;
            }
            node = node->right;
        }
    } while (node != nullptr);
}

// Reimpl of CitizenAI_impl_96b0 (FUN_1400296b0) — serialize the simple BST.
// Stream order: count(int), then `count` IBP entries. LOAD path reads each IBP
// (type-checked vs CitizenComponent 0x1401c7158) and inserts via RegisterWork. SAVE
// path is an iterative in-order traversal using parent pointers, writing each node's
// IBP (at node+0). Serializer read/write mode = *(int*)(*serializer + 8) == 0 (read).
// Caller: void fn(void* bstRoot, void* serializer).
void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer) {
    KeySet* set = reinterpret_cast<KeySet*>(root);
    IO::Serializer* ser = reinterpret_cast<IO::Serializer*>(serializer);
    int count = set->_count;
    ser->Serialize(count, nullptr);
    bool loading = (static_cast<int>(ser->GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint32_t citTid = CitizenComponent::TypeId();  // DAT_1401c7158

    if (loading) {
        for (int i = 0; i < count; i++) {
            CitizenComponent* keyIBP = nullptr;
            // The key is an InternalPtr retail serializes through InternalBasePtr::Serialize.
            reinterpret_cast<Object::InternalBasePtr*>(&keyIBP)->Serialize(*ser);
            if (keyIBP != nullptr && citTid &&
                !Object::IsDerivedFromNullSafe(keyIBP, citTid))
                keyIBP = nullptr;
            Node* out = nullptr;
            WorkBST_RegisterWork_1a30(root, &out, &keyIBP);
        }
        return;
    }

    // SAVE: iterative in-order traversal via parent links.
    Node* pIVar6 = set->_root;
    if (pIVar6 == nullptr) return;
    Node* pIVar3;
    Node* pIVar1;
    do { pIVar3 = pIVar6; pIVar6 = pIVar3->left; } while (pIVar6 != nullptr);  // leftmost
joined:
    while ((pIVar6 = pIVar3) != nullptr) {
        // The key is an InternalPtr retail serializes through InternalBasePtr::Serialize.
        reinterpret_cast<Object::InternalBasePtr*>(&pIVar6->element)->Serialize(*ser);
        if (pIVar6->element != 0 && citTid &&
            !Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(pIVar6->element), citTid))
            pIVar6->element = 0;
        pIVar1 = pIVar6->right;   // right child
        if (pIVar1 == nullptr) {
            pIVar3 = pIVar6->parent;  // parent
            if (pIVar3 != nullptr) goto succ;
            goto joined;                                         // root reached (pIVar3==0) -> terminate
        }
        do { pIVar3 = pIVar1; pIVar1 = pIVar3->left; } while (pIVar1 != nullptr);  // leftmost of right subtree
    }
    return;
succ:
    if (pIVar3->left != pIVar6) {
        for (;;) {
            pIVar1 = pIVar3;
            if (pIVar1 == nullptr || pIVar1->right != pIVar6) break;
            pIVar6 = pIVar1;
            pIVar3 = pIVar1->parent;
        }
        pIVar3 = pIVar6->parent;
    }
    goto joined;
}
