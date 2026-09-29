// code/Game/Work/work_base_cellbst.cpp
// Shared Work "cell BST" (AVL) family — ClearArea / cell-keyed work tree.
//
// Structurally identical to the simple BST in work_base_bst.cpp, but with a wider node
// (0x30 bytes) carrying an int cell key + an Object pointer: the exe's inlined
// Collection::Set instance over WorkCellAssignment (Work/Work.h), used by the area
// works (PlantArea, PlantOrchard, ClearArea, HarvestArea ...) and, with a Work in
// the pointer slot, by the scheduler's id map.
#include "Work/exe_bodies.h"
//
// Faithful 1:1 ports (Ghidra-verified).
//
// Node (WorkCellSetNode = Collection::Set<WorkCellAssignment>::Node, 0x30 bytes):
//   +0x00  int     element.cell (the key)
//   +0x08  ptr     element.citizen (the value pointer)
//   +0x10  ptr     parent
//   +0x18  ptr     left child
//   +0x20  ptr     right child
//   +0x28  int16   balance   (= rightHeight - leftHeight)
//   +0x2a  int16   height
// Tree: the Set's header, _root (+0x00) and _count (+0x08).
//
// LEFT (every entry point): the callers in other blocks hand the set as a
// uintptr_t* and the record / node as void*; each body views them as the Set and
// its node once, at the top.

#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Work/Work.h"         // WorkCellAssignment / WorkCellSetNode
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()

namespace {
using CellSet = Collection::Set<WorkCellAssignment>;
using Node    = WorkCellSetNode;
}  // namespace

// ============================================================
// Internal AVL primitives (static; not registered)
// ============================================================

static void CellBST_UpdateHB(Node* n) {
    if (n == nullptr) return;
    Node* l = n->left;
    Node* r = n->right;
    int16_t lh = (l == nullptr) ? 0 : static_cast<int16_t>(l->height + 1);
    int16_t rh = (r == nullptr) ? 0 : static_cast<int16_t>(r->height + 1);
    n->balance = static_cast<int16_t>(rh - lh);
    n->height = (rh < lh) ? lh : rh;
}

// Reimpl of FUN_14000e450 — left rotation (pivot = node->right @+0x20).
static Node* CellBST_RotateLeft(Node** root, Node* node) {
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
    CellBST_UpdateHB(pivot->left);
    CellBST_UpdateHB(pivot->right);
    CellBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_14000e580 — right rotation (pivot = node->left @+0x18).
static Node* CellBST_RotateRight(Node** root, Node* node) {
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
    CellBST_UpdateHB(pivot->left);
    CellBST_UpdateHB(pivot->right);
    CellBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_140013480 (TradeDescription_impl_3480) — AVL retrace.
static void CellBST_Retrace(Node** root, Node** pWalk) {
    while (*pWalk != nullptr) {
        Node* node = *pWalk;
        Node* next = node->parent;   // parent, pre-rotation
        CellBST_UpdateHB(node);
        node = *pWalk;
        if (node->balance > 1 && node->right != nullptr) {
            Node* rc = node->right;
            if (rc->balance < 0) { CellBST_RotateRight(root, rc); node = *pWalk; }
            CellBST_RotateLeft(root, node);
        }
        node = *pWalk;
        if (node->balance < -1 && node->left != nullptr) {
            Node* lc = node->left;
            if (lc->balance > 0) { CellBST_RotateLeft(root, lc); node = *pWalk; }
            CellBST_RotateRight(root, node);
        }
        *pWalk = next;
    }
}

// Reimpl of FUN_140029bb0 — allocate + init a 0x30-byte node. `rec` is {int key@+0;
// ptr @+8} (the int key and the value pointer to copy in; +0x04 is not copied).
static Node* CellBST_AllocNode(const WorkCellAssignment* rec, Node* parent) {
    Node* node = static_cast<Node*>(System::MemoryAllocator::MemAlloc(0x30, 8));
    if (!node) return nullptr;
    node->element.cell = rec->cell;
    node->element.citizen = rec->citizen;
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

// Reimpl of FUN_140029a40 (TradeDescription_impl_9a40) — AVL insert keyed on the int
// cell index. Prototype: void* fn(longlong* root, void* outResult, uint* keyRec).
// keyRec = {int key@+0; ptr IBP@+8}; key = *keyRec; the node is written to *outResult.
void* WorkCellBST_Insert_9a40(uintptr_t* root, void* outResult, void* keyRec) {
    CellSet* set = reinterpret_cast<CellSet*>(root);
    const WorkCellAssignment* rec = reinterpret_cast<const WorkCellAssignment*>(keyRec);
    Node** pOut = reinterpret_cast<Node**>(outResult);
    int key = rec->cell;
    if (set->_root == nullptr) {
        Node* node = CellBST_AllocNode(rec, nullptr);
        set->_root = node;
        *pOut = node;
    } else {
        Node* cur = set->_root;
        Node* newLeaf = nullptr;
        for (;;) {
            // descend right while node->key <= key
            while (cur->element.cell <= key) {
                if (key <= cur->element.cell) { *pOut = cur; return outResult; }
                if (cur->right == nullptr) {
                    newLeaf = CellBST_AllocNode(rec, cur);
                    cur->right = newLeaf;
                    goto done;
                }
                cur = cur->right;
            }
            if (cur->left == nullptr) {
                newLeaf = CellBST_AllocNode(rec, cur);
                cur->left = newLeaf;
                goto done;
            }
            cur = cur->left;
        }
    done:
        Node* retraceFrom = cur;
        CellBST_Retrace(&set->_root, &retraceFrom);
        *pOut = newLeaf;
    }
    set->_count += 1;
    return outResult;
}

// Reimpl of FUN_14001ca60 (TradeDescription_impl_ca60) — AVL delete of `nodeArg`.
// Same three-case structure as the simple-BST delete, cell offsets.
// Caller: void fn(void* bstRoot, void* node).
void WorkCellBST_Remove_ca60(uintptr_t* root, void* nodeArg) {
    CellSet* set = reinterpret_cast<CellSet*>(root);
    Node* node = reinterpret_cast<Node*>(nodeArg);
    Node* local_res8 = node->parent;
    set->_count -= 1; // count--
    Node* lVar5 = node->right;

    if (lVar5 == nullptr) {
        Node* parent = node->parent;
        Node* left   = node->left;
        if (parent == nullptr) set->_root = left;
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
                CellBST_UpdateHB(local_res8);
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
            if (lVar5 == nullptr) CellBST_Retrace(&set->_root, &local_res10);
            else CellBST_Retrace(&set->_root, &local_res8);
            return;
        }
        // node->left == 0, node->right != 0
        Node* parent = node->parent;
        if (parent == nullptr) set->_root = lVar5;
        else if (parent->left == node) parent->left = lVar5;
        else parent->right = lVar5;
        lVar5 = node->right;            // surviving child = right
    }

    if (lVar5 != nullptr) lVar5->parent = node->parent;
    Node* local_res10 = lVar5;
    System::MemoryAllocator::MemFree(node);
    if (lVar5 == nullptr) CellBST_Retrace(&set->_root, &local_res8);
    else CellBST_Retrace(&set->_root, &local_res10);
}

// Reimpl of FUN_140029de0 (TradeDescription_impl_9de0) — serialize the cell BST.
// Stream order: count(int), then `count` (int key, IBP) pairs. LOAD inserts each pair
// (IBP type-checked vs CitizenComponent 0x1401c7158). SAVE is an iterative in-order
// parent-walk writing each node's key(int @+0) then IBP(@+8). Caller: void fn(root, ser).
void WorkCellBST_Serialize_9de0(uintptr_t* root, void* serializer) {
    CellSet* set = reinterpret_cast<CellSet*>(root);
    IO::Serializer* ser = reinterpret_cast<IO::Serializer*>(serializer);
    int count = set->_count;
    ser->Serialize(count, nullptr);
    bool loading = (static_cast<int>(ser->GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint32_t citTid = CitizenComponent::TypeId();  // DAT_1401c7158

    if (loading) {
        for (int i = 0; i < count; i++) {
            WorkCellAssignment rec; rec.cell = 0; rec._pad04 = 0; rec.citizen = nullptr;
            ser->Serialize(rec.cell, nullptr);
            CitizenComponent* keyIBP = nullptr;
            // The value is an InternalPtr retail serializes through InternalBasePtr::Serialize.
            reinterpret_cast<Object::InternalBasePtr*>(&keyIBP)->Serialize(*ser);
            if (keyIBP != nullptr && citTid &&
                !Object::IsDerivedFromNullSafe(keyIBP, citTid))
                keyIBP = nullptr;
            rec.citizen = keyIBP;
            Node* out = nullptr;
            WorkCellBST_Insert_9a40(root, &out, &rec);
        }
        return;
    }

    // SAVE: iterative in-order traversal via parent links (cell offsets).
    Node* pIVar5 = set->_root;
    if (pIVar5 == nullptr) return;
    Node* pIVar7;
    Node* pIVar3;
    do { pIVar7 = pIVar5; pIVar5 = pIVar7->left; } while (pIVar5 != nullptr);  // leftmost
joined:
    while ((pIVar5 = pIVar7) != nullptr) {
        ser->Serialize(pIVar5->element.cell, nullptr);   // key @+0
        // The value is an InternalPtr retail serializes through InternalBasePtr::Serialize.
        reinterpret_cast<Object::InternalBasePtr*>(&pIVar5->element.citizen)->Serialize(*ser);  // IBP @+8
        if (pIVar5->element.citizen != nullptr && citTid &&
            !Object::IsDerivedFromNullSafe(pIVar5->element.citizen, citTid))
            pIVar5->element.citizen = nullptr;
        pIVar3 = pIVar5->right;   // right child
        if (pIVar3 == nullptr) {
            pIVar7 = pIVar5->parent;   // parent
            if (pIVar7 != nullptr) goto succ;
            goto joined;                                            // root reached -> terminate
        }
        do { pIVar7 = pIVar3; pIVar3 = pIVar7->left; } while (pIVar3 != nullptr);  // leftmost of right subtree
    }
    return;
succ:
    if (pIVar7->left != pIVar5) {
        for (;;) {
            pIVar3 = pIVar7;
            if (pIVar3 == nullptr || pIVar3->right != pIVar5) break;
            pIVar5 = pIVar3;
            pIVar7 = pIVar3->parent;
        }
        pIVar7 = pIVar5->parent;
    }
    goto joined;
}
