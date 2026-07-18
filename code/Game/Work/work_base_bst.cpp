// code/Game/Work/work_base_bst.cpp
// Shared Work "simple BST" (AVL) family — citizen-AI work-tracking tree.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from many
// code/Game/Work/*.cpp. In standalone mode they resolve to 0 (silently skipped) unless
// registered. Registration point: RegisterWorkBstBatch in standalone/reg_work_bst.cpp.
//
// All functions are faithful 1:1 ports of the decompiled exe helpers (Ghidra-verified).
//
// Node layout (0x28 bytes), an AVL node keyed on the low uint64 of an Object IBP:
//   +0x00  uint64  key          (== IBP value at IBP+0)
//   +0x08  ptr     parent
//   +0x10  ptr     left child
//   +0x18  ptr     right child
//   +0x20  int16   balance      (= rightHeight - leftHeight)
//   +0x22  int16   height       (leaf = 0; node = max(childHeight)+1 encoded as child.height+1)
// Tree root struct: root[0] = root node ptr, root[+0x8] = node count (int).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Internal AVL primitives (static; not registered)
// ============================================================

// Recompute balance(+0x20) and height(+0x22) of node `n` from its children.
static void SimpleBST_UpdateHB(uintptr_t n) {
    if (n == 0) return;
    uintptr_t l = *reinterpret_cast<uintptr_t*>(n + 0x10);
    uintptr_t r = *reinterpret_cast<uintptr_t*>(n + 0x18);
    int16_t lh = (l == 0) ? 0 : static_cast<int16_t>(*reinterpret_cast<int16_t*>(l + 0x22) + 1);
    int16_t rh = (r == 0) ? 0 : static_cast<int16_t>(*reinterpret_cast<int16_t*>(r + 0x22) + 1);
    *reinterpret_cast<int16_t*>(n + 0x20) = static_cast<int16_t>(rh - lh);
    *reinterpret_cast<int16_t*>(n + 0x22) = (rh < lh) ? lh : rh;
}

// Reimpl of FUN_140014c70 — left rotation about `node` (pivot = node->right).
static uintptr_t SimpleBST_RotateLeft(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x18);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x18);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x18);
    }
    uintptr_t pivot = *reinterpret_cast<uintptr_t*>(node + 0x18);
    *reinterpret_cast<uintptr_t*>(pivot + 8) = *reinterpret_cast<uintptr_t*>(node + 8);
    uintptr_t tmp = *reinterpret_cast<uintptr_t*>(pivot + 0x10);
    *reinterpret_cast<uintptr_t*>(node + 0x18) = tmp;
    if (tmp != 0) *reinterpret_cast<uintptr_t*>(tmp + 8) = node;
    *reinterpret_cast<uintptr_t*>(pivot + 0x10) = node;
    *reinterpret_cast<uintptr_t*>(node + 8) = pivot;
    SimpleBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x10));
    SimpleBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x18));
    SimpleBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_140014da0 — right rotation about `node` (pivot = node->left).
static uintptr_t SimpleBST_RotateRight(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x10);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x10);
    }
    uintptr_t pivot = *reinterpret_cast<uintptr_t*>(node + 0x10);
    *reinterpret_cast<uintptr_t*>(pivot + 8) = *reinterpret_cast<uintptr_t*>(node + 8);
    uintptr_t tmp = *reinterpret_cast<uintptr_t*>(pivot + 0x18);
    *reinterpret_cast<uintptr_t*>(node + 0x10) = tmp;
    if (tmp != 0) *reinterpret_cast<uintptr_t*>(tmp + 8) = node;
    *reinterpret_cast<uintptr_t*>(pivot + 0x18) = node;
    *reinterpret_cast<uintptr_t*>(node + 8) = pivot;
    SimpleBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x10));
    SimpleBST_UpdateHB(*reinterpret_cast<uintptr_t*>(pivot + 0x18));
    SimpleBST_UpdateHB(pivot);
    return pivot;
}

// Reimpl of FUN_14001a8c0 (CitizenAI_State_Leisure_sub_a8c0) — AVL retrace: walk up
// the parent chain from *pWalk, recomputing balance/height and rotating where the
// balance leaves [-1,1]. The parent is captured before rotations so the walk follows
// the original ancestry.
static void SimpleBST_Retrace(uintptr_t* root, uintptr_t* pWalk) {
    while (*pWalk != 0) {
        uintptr_t node = *pWalk;
        uintptr_t next = *reinterpret_cast<uintptr_t*>(node + 8);   // parent, pre-rotation
        SimpleBST_UpdateHB(node);
        node = *pWalk;
        if (*reinterpret_cast<int16_t*>(node + 0x20) > 1 && *reinterpret_cast<uintptr_t*>(node + 0x18) != 0) {
            uintptr_t rc = *reinterpret_cast<uintptr_t*>(node + 0x18);
            if (*reinterpret_cast<int16_t*>(rc + 0x20) < 0) {       // right-left case
                SimpleBST_RotateRight(root, rc);
                node = *pWalk;
            }
            SimpleBST_RotateLeft(root, node);
        }
        node = *pWalk;
        if (*reinterpret_cast<int16_t*>(node + 0x20) < -1 && *reinterpret_cast<uintptr_t*>(node + 0x10) != 0) {
            uintptr_t lc = *reinterpret_cast<uintptr_t*>(node + 0x10);
            if (*reinterpret_cast<int16_t*>(lc + 0x20) > 0) {       // left-right case
                SimpleBST_RotateLeft(root, lc);
                node = *pWalk;
            }
            SimpleBST_RotateRight(root, node);
        }
        *pWalk = next;
    }
}

// Reimpl of FUN_140021b40 — allocate + init a 0x28-byte leaf (key from *keyIBP).
static void* AllocNode_1b40(const void* keyIBP, void* parent) {
    void* node = (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8) : nullptr;
    if (!node) return nullptr;
    uintptr_t n = reinterpret_cast<uintptr_t>(node);
    *reinterpret_cast<uint64_t*>(n + 0x00) = *reinterpret_cast<const uint64_t*>(keyIBP);
    *reinterpret_cast<uintptr_t*>(n + 0x08) = reinterpret_cast<uintptr_t>(parent);
    *reinterpret_cast<uintptr_t*>(n + 0x10) = 0;
    *reinterpret_cast<uintptr_t*>(n + 0x18) = 0;
    *reinterpret_cast<uint32_t*>(n + 0x20) = 0;   // balance + height
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
    uint64_t* pOut = reinterpret_cast<uint64_t*>(outResult);
    if (*root == 0) {
        void* node = AllocNode_1b40(keyIBP, nullptr);
        *root = reinterpret_cast<uintptr_t>(node);
        *pOut = reinterpret_cast<uint64_t>(node);
    } else {
        uintptr_t cur = *root;
        void* newLeaf = nullptr;
        for (;;) {
            // descend left while key < node->key
            for (;;) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(cur));
                if (*reinterpret_cast<uint64_t*>(keyIBP) < *reinterpret_cast<uint64_t*>(cur)) {
                    uintptr_t* leftPtr = reinterpret_cast<uintptr_t*>(cur + 0x10);
                    if (*leftPtr == 0) {
                        newLeaf = AllocNode_1b40(keyIBP, reinterpret_cast<void*>(cur));
                        *leftPtr = reinterpret_cast<uintptr_t>(newLeaf);
                        goto done_insert;
                    }
                    cur = *leftPtr;
                } else {
                    break;
                }
            }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(keyIBP);
            if (*reinterpret_cast<uint64_t*>(keyIBP) <= *reinterpret_cast<uint64_t*>(cur)) {
                *pOut = static_cast<uint64_t>(cur);   // key already present
                return outResult;
            }
            uintptr_t* rightPtr = reinterpret_cast<uintptr_t*>(cur + 0x18);
            if (*rightPtr == 0) {
                newLeaf = AllocNode_1b40(keyIBP, reinterpret_cast<void*>(cur));
                *rightPtr = reinterpret_cast<uintptr_t>(newLeaf);
                goto done_insert;
            }
            cur = *rightPtr;
        }
    done_insert:
        uintptr_t retraceFrom = cur;   // parent of the new leaf
        SimpleBST_Retrace(root, &retraceFrom);
        *pOut = reinterpret_cast<uint64_t>(newLeaf);
    }
    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8) += 1;   // count++
    return outResult;
}

// Reimpl of CitizenAI_impl_a6e0 (FUN_14001a6e0) — AVL delete of `nodeArg`.
// Handles the three classic cases (no right child; right child with a left subtree
// whose in-order predecessor is spliced in; right child but no left child), then
// retraces from the splice point. Caller: void fn(void* bstRoot, void* node).
void WorkBST_BSTRemove_a6e0(uintptr_t* root, void* nodeArg) {
    uintptr_t node = reinterpret_cast<uintptr_t>(nodeArg);
    uintptr_t local_res8 = *reinterpret_cast<uintptr_t*>(node + 8);   // node->parent
    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8) -= 1;   // count--
    uintptr_t lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x18);     // node->right

    if (lVar5 == 0) {
        uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
        uintptr_t left   = *reinterpret_cast<uintptr_t*>(node + 0x10);
        if (parent == 0) *root = left;
        else if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node) *reinterpret_cast<uintptr_t*>(parent + 0x10) = left;
        else *reinterpret_cast<uintptr_t*>(parent + 0x18) = left;
        lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x10);          // surviving child = left
    } else {
        uintptr_t lVar1 = *reinterpret_cast<uintptr_t*>(node + 0x10);   // node->left
        if (lVar1 != 0) {
            // in-order predecessor = rightmost of left subtree
            uintptr_t pred = lVar1;
            uintptr_t walk = lVar1;
            do { pred = walk; walk = *reinterpret_cast<uintptr_t*>(pred + 0x18); } while (*reinterpret_cast<uintptr_t*>(pred + 0x18) != 0);

            local_res8 = 0;
            if (pred != lVar1) {
                lVar5 = *reinterpret_cast<uintptr_t*>(pred + 0x10);     // pred->left
                local_res8 = *reinterpret_cast<uintptr_t*>(pred + 8);  // pred->parent
                *reinterpret_cast<uintptr_t*>(local_res8 + 0x18) = lVar5;          // pred->parent->right = pred->left
                if (lVar5 != 0) *reinterpret_cast<uintptr_t*>(lVar5 + 8) = local_res8;
                *reinterpret_cast<uintptr_t*>(pred + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);  // pred->left = node->left
                SimpleBST_UpdateHB(local_res8);
            }
            lVar5 = local_res8;
            *reinterpret_cast<uintptr_t*>(pred + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x18);  // pred->right = node->right
            if (*reinterpret_cast<uintptr_t*>(pred + 0x10) != 0)
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(pred + 0x10) + 8) = pred;
            if (*reinterpret_cast<uintptr_t*>(pred + 0x18) != 0)
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(pred + 0x18) + 8) = pred;
            uintptr_t nodeParent = *reinterpret_cast<uintptr_t*>(node + 8);
            *reinterpret_cast<uintptr_t*>(pred + 8) = nodeParent;
            uintptr_t local_res10;
            if (nodeParent == 0) {
                *root = pred;
                *reinterpret_cast<uintptr_t*>(pred + 8) = 0;
                local_res10 = *root;
            } else {
                if (*reinterpret_cast<uintptr_t*>(nodeParent + 0x10) == node)
                    *reinterpret_cast<uintptr_t*>(nodeParent + 0x10) = pred;
                else
                    *reinterpret_cast<uintptr_t*>(nodeParent + 0x18) = pred;
                local_res10 = pred;
            }
            if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(node));
            if (lVar5 == 0) SimpleBST_Retrace(root, &local_res10);
            else SimpleBST_Retrace(root, &local_res8);
            return;
        }
        // node->left == 0, node->right != 0
        uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
        if (parent == 0) *root = lVar5;
        else if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node) *reinterpret_cast<uintptr_t*>(parent + 0x10) = lVar5;
        else *reinterpret_cast<uintptr_t*>(parent + 0x18) = lVar5;
        lVar5 = *reinterpret_cast<uintptr_t*>(node + 0x18);          // surviving child = right
    }

    if (lVar5 != 0) *reinterpret_cast<uintptr_t*>(lVar5 + 8) = *reinterpret_cast<uintptr_t*>(node + 8);
    uintptr_t local_res10 = lVar5;
    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(node));
    if (lVar5 == 0) SimpleBST_Retrace(root, &local_res8);
    else SimpleBST_Retrace(root, &local_res10);
}

// Reimpl of CitizenAI_impl_5520 (FUN_140025520) — find a node by key (uint64 at IBP+0)
// and remove it. Caller: void fn(void* bstRoot, void* keyIBP).
void WorkBST_BSTFindRemove_25520(uintptr_t* root, void* keyIBP) {
    uintptr_t node = *root;
    if (node == 0) return;
    do {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(node));
        if (*reinterpret_cast<uint64_t*>(keyIBP) < *reinterpret_cast<uint64_t*>(node)) {
            node = *reinterpret_cast<uintptr_t*>(node + 0x10);      // left
        } else {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(keyIBP);
            if (*reinterpret_cast<uint64_t*>(keyIBP) <= *reinterpret_cast<uint64_t*>(node)) {
                WorkBST_BSTRemove_a6e0(root, reinterpret_cast<void*>(node));
                return;
            }
            node = *reinterpret_cast<uintptr_t*>(node + 0x18);      // right
        }
    } while (node != 0);
}

// Reimpl of CitizenAI_impl_96b0 (FUN_1400296b0) — serialize the simple BST.
// Stream order: count(int), then `count` IBP entries. LOAD path reads each IBP
// (type-checked vs CitizenAIComponent 0x1401c7158) and inserts via RegisterWork. SAVE
// path is an iterative in-order traversal using parent pointers, writing each node's
// IBP (at node+0). Serializer read/write mode = *(int*)(*serializer + 8) == 0 (read).
// Caller: void fn(void* bstRoot, void* serializer).
void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer) {
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(root) + 8);
    if (g_fn_Serialize_Int) g_fn_Serialize_Int(serializer, &count, nullptr);
    bool loading = (*reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(serializer) + 8) == 0);
    uint32_t citTid = g_TypeId_CitizenAIComponent_FC ? *g_TypeId_CitizenAIComponent_FC : 0;

    if (loading) {
        for (int i = 0; i < count; i++) {
            uintptr_t keyIBP = 0;
            if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(&keyIBP, serializer);
            if (keyIBP != 0 && fn_IsDerivedFrom && citTid &&
                !fn_IsDerivedFrom(reinterpret_cast<void*>(keyIBP), citTid))
                keyIBP = 0;
            uintptr_t out = 0;
            WorkBST_RegisterWork_1a30(root, &out, &keyIBP);
        }
        return;
    }

    // SAVE: iterative in-order traversal via parent links.
    uintptr_t pIVar6 = *root;
    if (pIVar6 == 0) return;
    uintptr_t pIVar3, pIVar1;
    do { pIVar3 = pIVar6; pIVar6 = *reinterpret_cast<uintptr_t*>(pIVar3 + 0x10); } while (pIVar6 != 0);  // leftmost
joined:
    while ((pIVar6 = pIVar3) != 0) {
        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(reinterpret_cast<void*>(pIVar6), serializer);
        if (*reinterpret_cast<uintptr_t*>(pIVar6) != 0 && fn_IsDerivedFrom && citTid &&
            !fn_IsDerivedFrom(*reinterpret_cast<void**>(pIVar6), citTid))
            *reinterpret_cast<uintptr_t*>(pIVar6) = 0;
        pIVar1 = *reinterpret_cast<uintptr_t*>(pIVar6 + 0x18);   // right child
        if (pIVar1 == 0) {
            pIVar3 = *reinterpret_cast<uintptr_t*>(pIVar6 + 8);  // parent
            if (pIVar3 != 0) goto succ;
            goto joined;                                         // root reached (pIVar3==0) -> terminate
        }
        do { pIVar3 = pIVar1; pIVar1 = *reinterpret_cast<uintptr_t*>(pIVar3 + 0x10); } while (pIVar1 != 0);  // leftmost of right subtree
    }
    return;
succ:
    if (*reinterpret_cast<uintptr_t*>(pIVar3 + 0x10) != pIVar6) {
        for (;;) {
            pIVar1 = pIVar3;
            if (pIVar1 == 0 || *reinterpret_cast<uintptr_t*>(pIVar1 + 0x18) != pIVar6) break;
            pIVar6 = pIVar1;
            pIVar3 = *reinterpret_cast<uintptr_t*>(pIVar1 + 8);
        }
        pIVar3 = *reinterpret_cast<uintptr_t*>(pIVar6 + 8);
    }
    goto joined;
}
