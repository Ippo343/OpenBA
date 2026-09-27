// code/Game/Work/work_base_cellbst.cpp
// Shared Work "cell BST" (AVL) family — ClearArea / cell-keyed work tree.
//
// Structurally identical to the simple BST in work_base_bst.cpp, but with a wider node
// (0x30 bytes) carrying an int cell key + an Object IBP, and shifted child/metadata
// offsets. Resolved via ResolveAddr(Module::Exe, 0xADDR) from clear-area / harvest /
// plant Work classes; registered via RegisterWorkCellBstBatch in reg_work_cellbst.cpp.
//
// Faithful 1:1 ports (Ghidra-verified).
//
// Node layout (0x30 bytes):
//   +0x00  int     key (cell index)
//   +0x08  ptr     value IBP
//   +0x10  ptr     parent
//   +0x18  ptr     left child
//   +0x20  ptr     right child
//   +0x28  int16   balance   (= rightHeight - leftHeight)
//   +0x2a  int16   height
// Root struct: root[0] = root node ptr, root[+0x8] = node count (int).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()

// ============================================================
// Internal AVL primitives (static; not registered)
// ============================================================

static void CellBST_UpdateHB(uintptr_t n) {
    if (n == 0) return;
    uintptr_t l = *reinterpret_cast<uintptr_t*>(n + 0x18);
    uintptr_t r = *reinterpret_cast<uintptr_t*>(n + 0x20);
    int16_t lh = (l == 0) ? 0 : static_cast<int16_t>(*reinterpret_cast<int16_t*>(l + 0x2a) + 1);
    int16_t rh = (r == 0) ? 0 : static_cast<int16_t>(*reinterpret_cast<int16_t*>(r + 0x2a) + 1);
    *reinterpret_cast<int16_t*>(n + 0x28) = static_cast<int16_t>(rh - lh);
    *reinterpret_cast<int16_t*>(n + 0x2a) = (rh < lh) ? lh : rh;
}

// Reimpl of FUN_14000e450 — left rotation (pivot = node->right @+0x20).
static uintptr_t CellBST_RotateLeft(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x20);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 0x10);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x18) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x20);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x20) = *reinterpret_cast<uintptr_t*>(node + 0x20);
    }
    uintptr_t pivot = *reinterpret_cast<uintptr_t*>(node + 0x20);
    *reinterpret_cast<uintptr_t*>(pivot + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);
    uintptr_t tmp = *reinterpret_cast<uintptr_t*>(pivot + 0x18);
    *reinterpret_cast<uintptr_t*>(node + 0x20) = tmp;
    if (tmp != 0) *reinterpret_cast<uintptr_t*>(tmp + 0x10) = node;
    *reinterpret_cast<uintptr_t*>(pivot + 0x18) = node;
    *reinterpret_cast<uintptr_t*>(node + 0x10) = pivot;
    CellBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x18));
    CellBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x20));
    CellBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_14000e580 — right rotation (pivot = node->left @+0x18).
static uintptr_t CellBST_RotateRight(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x18);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 0x10);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x18) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x18);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x20) = *reinterpret_cast<uintptr_t*>(node + 0x18);
    }
    uintptr_t pivot = *reinterpret_cast<uintptr_t*>(node + 0x18);
    *reinterpret_cast<uintptr_t*>(pivot + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);
    uintptr_t tmp = *reinterpret_cast<uintptr_t*>(pivot + 0x20);
    *reinterpret_cast<uintptr_t*>(node + 0x18) = tmp;
    if (tmp != 0) *reinterpret_cast<uintptr_t*>(tmp + 0x10) = node;
    *reinterpret_cast<uintptr_t*>(pivot + 0x20) = node;
    *reinterpret_cast<uintptr_t*>(node + 0x10) = pivot;
    CellBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x18));
    CellBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x20));
    CellBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_140013480 (TradeDescription_impl_3480) — AVL retrace.
static void CellBST_Retrace(uintptr_t* root, uintptr_t* pWalk) {
    while (*pWalk != 0) {
        uintptr_t node = *pWalk;
        uintptr_t next = *reinterpret_cast<uintptr_t*>(node + 0x10);   // parent, pre-rotation
        CellBST_UpdateHB(node);
        node = *pWalk;
        if (*reinterpret_cast<int16_t*>(node + 0x28) > 1 && *reinterpret_cast<uintptr_t*>(node + 0x20) != 0) {
            uintptr_t rc = *reinterpret_cast<uintptr_t*>(node + 0x20);
            if (*reinterpret_cast<int16_t*>(rc + 0x28) < 0) { CellBST_RotateRight(root, rc); node = *pWalk; }
            CellBST_RotateLeft(root, node);
        }
        node = *pWalk;
        if (*reinterpret_cast<int16_t*>(node + 0x28) < -1 && *reinterpret_cast<uintptr_t*>(node + 0x18) != 0) {
            uintptr_t lc = *reinterpret_cast<uintptr_t*>(node + 0x18);
            if (*reinterpret_cast<int16_t*>(lc + 0x28) > 0) { CellBST_RotateLeft(root, lc); node = *pWalk; }
            CellBST_RotateRight(root, node);
        }
        *pWalk = next;
    }
}

// Reimpl of FUN_140029bb0 — allocate + init a 0x30-byte node. keyRec is {int key@+0;
// ptr IBP@+8} (the int key and the value IBP to copy in).
static void* CellBST_AllocNode(const void* keyRec, void* parent) {
    void* node = (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8) : nullptr;
    if (!node) return nullptr;
    uintptr_t n = reinterpret_cast<uintptr_t>(node);
    *reinterpret_cast<int*>(n + 0x00) = *reinterpret_cast<const int*>(keyRec);
    *reinterpret_cast<uint64_t*>(n + 0x08) = *reinterpret_cast<const uint64_t*>(reinterpret_cast<uintptr_t>(keyRec) + 8);
    *reinterpret_cast<uintptr_t*>(n + 0x10) = reinterpret_cast<uintptr_t>(parent);
    *reinterpret_cast<uintptr_t*>(n + 0x18) = 0;
    *reinterpret_cast<uintptr_t*>(n + 0x20) = 0;
    *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
    return node;
}

// ============================================================
// Registered helpers
// ============================================================

// Reimpl of FUN_140029a40 (TradeDescription_impl_9a40) — AVL insert keyed on the int
// cell index. Prototype: void* fn(longlong* root, void* outResult, uint* keyRec).
// keyRec = {int key@+0; ptr IBP@+8}; key = *keyRec; the node is written to *outResult.
void* WorkCellBST_Insert_9a40(uintptr_t* root, void* outResult, void* keyRec) {
    uint64_t* pOut = reinterpret_cast<uint64_t*>(outResult);
    int key = *reinterpret_cast<const int*>(keyRec);
    if (*root == 0) {
        void* node = CellBST_AllocNode(keyRec, nullptr);
        *root = reinterpret_cast<uintptr_t>(node);
        *pOut = reinterpret_cast<uint64_t>(node);
    } else {
        uintptr_t cur = *root;
        void* newLeaf = nullptr;
        for (;;) {
            // descend right while node->key <= key
            while (*reinterpret_cast<int*>(cur) <= key) {
                if (key <= *reinterpret_cast<int*>(cur)) { *pOut = static_cast<uint64_t>(cur); return outResult; }
                uintptr_t* rightPtr = reinterpret_cast<uintptr_t*>(cur + 0x20);
                if (*rightPtr == 0) {
                    newLeaf = CellBST_AllocNode(keyRec, reinterpret_cast<void*>(cur));
                    *rightPtr = reinterpret_cast<uintptr_t>(newLeaf);
                    goto done;
                }
                cur = *rightPtr;
            }
            uintptr_t* leftPtr = reinterpret_cast<uintptr_t*>(cur + 0x18);
            if (*leftPtr == 0) {
                newLeaf = CellBST_AllocNode(keyRec, reinterpret_cast<void*>(cur));
                *leftPtr = reinterpret_cast<uintptr_t>(newLeaf);
                goto done;
            }
            cur = *leftPtr;
        }
    done:
        uintptr_t retraceFrom = cur;
        CellBST_Retrace(root, &retraceFrom);
        *pOut = reinterpret_cast<uint64_t>(newLeaf);
    }
    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8) += 1;
    return outResult;
}

// Reimpl of FUN_14001ca60 (TradeDescription_impl_ca60) — AVL delete of `nodeArg`.
// Same three-case structure as the simple-BST delete, cell offsets.
// Caller: void fn(void* bstRoot, void* node).
void WorkCellBST_Remove_ca60(uintptr_t* root, void* nodeArg) {
    uintptr_t node = reinterpret_cast<uintptr_t>(nodeArg);
    uintptr_t local_res8 = *reinterpret_cast<uintptr_t*>(node + 0x10);   // node->parent
    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8) -= 1; // count--
    uintptr_t lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x20);        // node->right

    if (lVar5 == 0) {
        uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 0x10);
        uintptr_t left   = *reinterpret_cast<uintptr_t*>(node + 0x18);
        if (parent == 0) *root = left;
        else if (*reinterpret_cast<uintptr_t*>(parent + 0x18) == node) *reinterpret_cast<uintptr_t*>(parent + 0x18) = left;
        else *reinterpret_cast<uintptr_t*>(parent + 0x20) = left;
        lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x18);             // surviving child = left
    } else {
        uintptr_t lVar1 = *reinterpret_cast<uintptr_t*>(node + 0x18);   // node->left
        if (lVar1 != 0) {
            uintptr_t pred = lVar1;
            uintptr_t walk = lVar1;
            do { pred = walk; walk = *reinterpret_cast<uintptr_t*>(pred + 0x20); } while (*reinterpret_cast<uintptr_t*>(pred + 0x20) != 0);

            local_res8 = 0;
            if (pred != lVar1) {
                lVar5 = *reinterpret_cast<uintptr_t*>(pred + 0x18);     // pred->left
                local_res8 = *reinterpret_cast<uintptr_t*>(pred + 0x10);// pred->parent
                *reinterpret_cast<uintptr_t*>(local_res8 + 0x20) = lVar5;          // pred->parent->right = pred->left
                if (lVar5 != 0) *reinterpret_cast<uintptr_t*>(lVar5 + 0x10) = local_res8;
                *reinterpret_cast<uintptr_t*>(pred + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x18);  // pred->left = node->left
                CellBST_UpdateHB(local_res8);
            }
            lVar5 = local_res8;
            *reinterpret_cast<uintptr_t*>(pred + 0x20) = *reinterpret_cast<uintptr_t*>(node + 0x20);  // pred->right = node->right
            if (*reinterpret_cast<uintptr_t*>(pred + 0x18) != 0)
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(pred + 0x18) + 0x10) = pred;
            if (*reinterpret_cast<uintptr_t*>(pred + 0x20) != 0)
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(pred + 0x20) + 0x10) = pred;
            uintptr_t nodeParent = *reinterpret_cast<uintptr_t*>(node + 0x10);
            *reinterpret_cast<uintptr_t*>(pred + 0x10) = nodeParent;
            uintptr_t local_res10;
            if (nodeParent == 0) {
                *root = pred;
                *reinterpret_cast<uintptr_t*>(pred + 0x10) = 0;
                local_res10 = *root;
            } else {
                if (*reinterpret_cast<uintptr_t*>(nodeParent + 0x18) == node)
                    *reinterpret_cast<uintptr_t*>(nodeParent + 0x18) = pred;
                else
                    *reinterpret_cast<uintptr_t*>(nodeParent + 0x20) = pred;
                local_res10 = pred;
            }
            if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(node));
            if (lVar5 == 0) CellBST_Retrace(root, &local_res10);
            else CellBST_Retrace(root, &local_res8);
            return;
        }
        // node->left == 0, node->right != 0
        uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 0x10);
        if (parent == 0) *root = lVar5;
        else if (*reinterpret_cast<uintptr_t*>(parent + 0x18) == node) *reinterpret_cast<uintptr_t*>(parent + 0x18) = lVar5;
        else *reinterpret_cast<uintptr_t*>(parent + 0x20) = lVar5;
        lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x20);            // surviving child = right
    }

    if (lVar5 != 0) *reinterpret_cast<uintptr_t*>(lVar5 + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);
    uintptr_t local_res10 = lVar5;
    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(node));
    if (lVar5 == 0) CellBST_Retrace(root, &local_res8);
    else CellBST_Retrace(root, &local_res10);
}

// Reimpl of FUN_140029de0 (TradeDescription_impl_9de0) — serialize the cell BST.
// Stream order: count(int), then `count` (int key, IBP) pairs. LOAD inserts each pair
// (IBP type-checked vs CitizenComponent 0x1401c7158). SAVE is an iterative in-order
// parent-walk writing each node's key(int @+0) then IBP(@+8). Caller: void fn(root, ser).
void WorkCellBST_Serialize_9de0(uintptr_t* root, void* serializer) {
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&count), nullptr);   // B.3.4 bridge
    bool loading = (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    uint32_t citTid = CitizenComponent::TypeId();  // DAT_1401c7158

    if (loading) {
        struct CellKeyRec { int key; int pad; uint64_t ibp; };
        for (int i = 0; i < count; i++) {
            CellKeyRec rec; rec.key = 0; rec.pad = 0; rec.ibp = 0;
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&rec.key), nullptr);   // B.3.4 bridge
            uintptr_t keyIBP = 0;
            reinterpret_cast<Object::InternalBasePtr*>(&keyIBP)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            if (keyIBP != 0 && fn_IsDerivedFrom && citTid &&
                !fn_IsDerivedFrom(reinterpret_cast<void*>(keyIBP), citTid))
                keyIBP = 0;
            rec.ibp = keyIBP;
            uintptr_t out = 0;
            WorkCellBST_Insert_9a40(root, &out, &rec);
        }
        return;
    }

    // SAVE: iterative in-order traversal via parent links (cell offsets).
    uintptr_t pIVar5 = *root;
    if (pIVar5 == 0) return;
    uintptr_t pIVar7, pIVar3;
    do { pIVar7 = pIVar5; pIVar5 = *reinterpret_cast<uintptr_t*>(pIVar7 + 0x18); } while (pIVar5 != 0);  // leftmost
joined:
    while ((pIVar5 = pIVar7) != 0) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(pIVar5), nullptr);   // key @+0   (B.3.4 bridge)
        reinterpret_cast<Object::InternalBasePtr*>(reinterpret_cast<void*>(pIVar5 + 8))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));  // IBP @+8   // B.3.4 bridge
        if (*reinterpret_cast<uintptr_t*>(pIVar5 + 8) != 0 && fn_IsDerivedFrom && citTid &&
            !fn_IsDerivedFrom(*reinterpret_cast<void**>(pIVar5 + 8), citTid))
            *reinterpret_cast<uint64_t*>(pIVar5 + 8) = 0;
        pIVar3 = *reinterpret_cast<uintptr_t*>(pIVar5 + 0x20);   // right child
        if (pIVar3 == 0) {
            pIVar7 = *reinterpret_cast<uintptr_t*>(pIVar5 + 0x10);   // parent
            if (pIVar7 != 0) goto succ;
            goto joined;                                            // root reached -> terminate
        }
        do { pIVar7 = pIVar3; pIVar3 = *reinterpret_cast<uintptr_t*>(pIVar7 + 0x18); } while (pIVar3 != 0);  // leftmost of right subtree
    }
    return;
succ:
    if (*reinterpret_cast<uintptr_t*>(pIVar7 + 0x18) != pIVar5) {
        for (;;) {
            pIVar3 = pIVar7;
            if (pIVar3 == 0 || *reinterpret_cast<uintptr_t*>(pIVar3 + 0x20) != pIVar5) break;
            pIVar5 = pIVar3;
            pIVar7 = *reinterpret_cast<uintptr_t*>(pIVar3 + 0x10);
        }
        pIVar7 = *reinterpret_cast<uintptr_t*>(pIVar5 + 0x10);
    }
    goto joined;
}
