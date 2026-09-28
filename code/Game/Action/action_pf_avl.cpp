// code/Game/Action/action_pf_avl.cpp
// Pathfinding/Action closure — AVL/BST node-pool + rotation family.
//
// Shared by the road A*, region graph, and work BST. 10 exe sub-helpers, faithfully
#include "Action/exe_bodies.h"
// transcribed 1:1 from the Ghidra decompile (cross-checked against disassembly for the
// rotation pointer surgery, the insert child-pointer offsets, and the balance-factor /
// height fixups). None of this is exercised on the autostart path — a wrong port would be
// undetectable silent corruption — so it is a LITERAL transcription, not an "improvement".
//
// The rebalance drivers and inserts call the rotations / allocators / rebalances of this
// batch by name (Action/exe_bodies.h, B3R) rather than inlining them; the registration
// (standalone/reg_pf_avl.cpp -> RegisterPfAvlBatch()) stays for readers other blocks have.
//
// ===========================================================================
// TWO node layouts (both AVL, different field offsets):
//
//   Layout A — 0x28-byte node  (a4f0 / a680 / a8c0 / 14c70 / 14da0):
//     +0x00  int    key           (single int key)
//     +0x08  ptr    parent
//     +0x10  ptr    left
//     +0x18  ptr    right
//     +0x20  short  balance       (= height(right) - height(left))
//     +0x22  short  height        (= 1 + max(height(left), height(right)); leaf-relative)
//
//   Layout B — 0x30-byte node  (c940 / cc40 / 13480 / e450 / e580):
//     +0x00  int    key1          (3-int key prefix: key1, key2, key3)
//     +0x04  int    key2
//     +0x08  int    key3
//     +0x10  ptr    parent
//     +0x18  ptr    left
//     +0x20  ptr    right
//     +0x28  short  balance
//     +0x2a  short  height
//
// Height convention (both): a node's "subtree height as seen from its parent" is read as
//   child ? (child->height + 1) : 0
// and stored at the node as height = max(leftH, rightH); balance = rightH - leftH.
// ===========================================================================

#include "Component/exe_bodies.h"      // CitizenAI_Reg_helper_e360
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc
#include <cstdint>

namespace {

// ---- byte-offset field accessors -----------------------------------------
static inline int64_t&  P64(uintptr_t base, uintptr_t off) { return *reinterpret_cast<int64_t*>(base + off); }
static inline int32_t&  P32(uintptr_t base, uintptr_t off) { return *reinterpret_cast<int32_t*>(base + off); }
static inline int16_t&  P16(uintptr_t base, uintptr_t off) { return *reinterpret_cast<int16_t*>(base + off); }

} // namespace

// ===========================================================================
// Layout B rotations (0x30-node: parent +0x10, left +0x18, right +0x20,
//                                  balance +0x28, height +0x2a)
// ===========================================================================

// 0x14000e450 — rotate subtree (left child promoted up the right link).
// Returns the promoted node (the old right child, lVar1).
extern "C" int64_t Pf_AvlRotateB_e450(int64_t* root, int64_t node)
{
    int64_t pivot;   // lVar1
    int64_t tmp;     // lVar2
    int16_t s3, s4, s5, s6;

    if (node == *root) {
        *root = P64((uintptr_t)node, 0x20);
    }
    pivot = P64((uintptr_t)node, 0x10);            // parent
    if (pivot != 0) {
        if (P64((uintptr_t)pivot, 0x18) == node) {
            P64((uintptr_t)pivot, 0x18) = P64((uintptr_t)node, 0x20);
        } else {
            P64((uintptr_t)pivot, 0x20) = P64((uintptr_t)node, 0x20);
        }
    }
    pivot = P64((uintptr_t)node, 0x20);            // right child -> becomes new subtree root
    P64((uintptr_t)pivot, 0x10) = P64((uintptr_t)node, 0x10);
    tmp = P64((uintptr_t)pivot, 0x18);             // pivot->left
    P64((uintptr_t)node, 0x20) = tmp;
    if (tmp != 0) {
        P64((uintptr_t)tmp, 0x10) = node;
    }
    P64((uintptr_t)pivot, 0x18) = node;
    P64((uintptr_t)node, 0x10) = pivot;

    // height/balance fixup for pivot->left
    tmp = P64((uintptr_t)pivot, 0x18);
    s5 = 0;
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x2a) + 1);
        s4 = (P64((uintptr_t)tmp, 0x20) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x20), 0x2a) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x28) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x2a) = s3;
    }
    // height/balance fixup for pivot->right
    tmp = P64((uintptr_t)pivot, 0x20);
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x2a) + 1);
        s4 = (P64((uintptr_t)tmp, 0x20) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x20), 0x2a) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x28) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x2a) = s3;
    }
    // height/balance fixup for pivot itself
    s6 = (P64((uintptr_t)pivot, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)pivot, 0x18), 0x2a) + 1);
    if (P64((uintptr_t)pivot, 0x20) != 0) {
        s5 = (int16_t)(P16(P64((uintptr_t)pivot, 0x20), 0x2a) + 1);
    }
    s4 = s5;
    if (s5 < s6) s4 = s6;
    P16((uintptr_t)pivot, 0x28) = (int16_t)(s5 - s6);
    P16((uintptr_t)pivot, 0x2a) = s4;
    return pivot;
}

// 0x14000e580 — rotate subtree (mirror of e450: right child promoted up the left link).
extern "C" int64_t Pf_AvlRotateB_e580(int64_t* root, int64_t node)
{
    int64_t pivot;
    int64_t tmp;
    int16_t s3, s4, s5, s6;

    if (node == *root) {
        *root = P64((uintptr_t)node, 0x18);
    }
    pivot = P64((uintptr_t)node, 0x10);            // parent
    if (pivot != 0) {
        if (P64((uintptr_t)pivot, 0x18) == node) {
            P64((uintptr_t)pivot, 0x18) = P64((uintptr_t)node, 0x18);
        } else {
            P64((uintptr_t)pivot, 0x20) = P64((uintptr_t)node, 0x18);
        }
    }
    pivot = P64((uintptr_t)node, 0x18);            // left child -> becomes new subtree root
    P64((uintptr_t)pivot, 0x10) = P64((uintptr_t)node, 0x10);
    tmp = P64((uintptr_t)pivot, 0x20);             // pivot->right
    P64((uintptr_t)node, 0x18) = tmp;
    if (tmp != 0) {
        P64((uintptr_t)tmp, 0x10) = node;
    }
    P64((uintptr_t)pivot, 0x20) = node;
    P64((uintptr_t)node, 0x10) = pivot;

    tmp = P64((uintptr_t)pivot, 0x18);
    s5 = 0;
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x2a) + 1);
        s4 = (P64((uintptr_t)tmp, 0x20) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x20), 0x2a) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x28) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x2a) = s3;
    }
    tmp = P64((uintptr_t)pivot, 0x20);
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x2a) + 1);
        s4 = (P64((uintptr_t)tmp, 0x20) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x20), 0x2a) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x28) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x2a) = s3;
    }
    s6 = (P64((uintptr_t)pivot, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)pivot, 0x18), 0x2a) + 1);
    if (P64((uintptr_t)pivot, 0x20) != 0) {
        s5 = (int16_t)(P16(P64((uintptr_t)pivot, 0x20), 0x2a) + 1);
    }
    s4 = s5;
    if (s5 < s6) s4 = s6;
    P16((uintptr_t)pivot, 0x28) = (int16_t)(s5 - s6);
    P16((uintptr_t)pivot, 0x2a) = s4;
    return pivot;
}

// ===========================================================================
// Layout A rotations (0x28-node: parent +0x08, left +0x10, right +0x18,
//                                  balance +0x20, height +0x22)
// ===========================================================================

// 0x140014c70 — AVL left rotation + height fixup (right child promoted up the right link).
extern "C" int64_t Pf_AvlRotateL_14c70(int64_t* root, int64_t node)
{
    int64_t pivot;
    int64_t tmp;
    int16_t s3, s4, s5, s6;

    if (node == *root) {
        *root = P64((uintptr_t)node, 0x18);
    }
    pivot = P64((uintptr_t)node, 0x08);            // parent
    if (pivot != 0) {
        if (P64((uintptr_t)pivot, 0x10) == node) {
            P64((uintptr_t)pivot, 0x10) = P64((uintptr_t)node, 0x18);
        } else {
            P64((uintptr_t)pivot, 0x18) = P64((uintptr_t)node, 0x18);
        }
    }
    pivot = P64((uintptr_t)node, 0x18);            // right child
    P64((uintptr_t)pivot, 0x08) = P64((uintptr_t)node, 0x08);
    tmp = P64((uintptr_t)pivot, 0x10);             // pivot->left
    P64((uintptr_t)node, 0x18) = tmp;
    if (tmp != 0) {
        P64((uintptr_t)tmp, 0x08) = node;
    }
    P64((uintptr_t)pivot, 0x10) = node;
    P64((uintptr_t)node, 0x08) = pivot;

    tmp = P64((uintptr_t)pivot, 0x10);
    s5 = 0;
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x10), 0x22) + 1);
        s4 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x22) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x20) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x22) = s3;
    }
    tmp = P64((uintptr_t)pivot, 0x18);
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x10), 0x22) + 1);
        s4 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x22) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x20) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x22) = s3;
    }
    s6 = (P64((uintptr_t)pivot, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)pivot, 0x10), 0x22) + 1);
    if (P64((uintptr_t)pivot, 0x18) != 0) {
        s5 = (int16_t)(P16(P64((uintptr_t)pivot, 0x18), 0x22) + 1);
    }
    s4 = s5;
    if (s5 < s6) s4 = s6;
    P16((uintptr_t)pivot, 0x20) = (int16_t)(s5 - s6);
    P16((uintptr_t)pivot, 0x22) = s4;
    return pivot;
}

// 0x140014da0 — AVL right rotation + height fixup (left child promoted up the left link).
extern "C" int64_t Pf_AvlRotateR_14da0(int64_t* root, int64_t node)
{
    int64_t pivot;
    int64_t tmp;
    int16_t s3, s4, s5, s6;

    if (node == *root) {
        *root = P64((uintptr_t)node, 0x10);
    }
    pivot = P64((uintptr_t)node, 0x08);            // parent
    if (pivot != 0) {
        if (P64((uintptr_t)pivot, 0x10) == node) {
            P64((uintptr_t)pivot, 0x10) = P64((uintptr_t)node, 0x10);
        } else {
            P64((uintptr_t)pivot, 0x18) = P64((uintptr_t)node, 0x10);
        }
    }
    pivot = P64((uintptr_t)node, 0x10);            // left child
    P64((uintptr_t)pivot, 0x08) = P64((uintptr_t)node, 0x08);
    tmp = P64((uintptr_t)pivot, 0x18);             // pivot->right
    P64((uintptr_t)node, 0x10) = tmp;
    if (tmp != 0) {
        P64((uintptr_t)tmp, 0x08) = node;
    }
    P64((uintptr_t)pivot, 0x18) = node;
    P64((uintptr_t)node, 0x08) = pivot;

    tmp = P64((uintptr_t)pivot, 0x10);
    s5 = 0;
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x10), 0x22) + 1);
        s4 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x22) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x20) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x22) = s3;
    }
    tmp = P64((uintptr_t)pivot, 0x18);
    if (tmp != 0) {
        s6 = (P64((uintptr_t)tmp, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x10), 0x22) + 1);
        s4 = (P64((uintptr_t)tmp, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)tmp, 0x18), 0x22) + 1);
        s3 = s4;
        if (s4 < s6) s3 = s6;
        P16((uintptr_t)tmp, 0x20) = (int16_t)(s4 - s6);
        P16((uintptr_t)tmp, 0x22) = s3;
    }
    s6 = (P64((uintptr_t)pivot, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)pivot, 0x10), 0x22) + 1);
    if (P64((uintptr_t)pivot, 0x18) != 0) {
        s5 = (int16_t)(P16(P64((uintptr_t)pivot, 0x18), 0x22) + 1);
    }
    s4 = s5;
    if (s5 < s6) s4 = s6;
    P16((uintptr_t)pivot, 0x20) = (int16_t)(s5 - s6);
    P16((uintptr_t)pivot, 0x22) = s4;
    return pivot;
}

// ===========================================================================
// Rebalance drivers — walk up from the inserted node toward the root, recompute
// height/balance, and apply rotations on imbalance. The rotations are called by name:
// Layout B (0x30-node) 0x14000e450 Pf_AvlRotateB_e450 / 0x14000e580 Pf_AvlRotateB_e580,
// Layout A (0x28-node) 0x140014c70 Pf_AvlRotateL_14c70 / 0x140014da0 Pf_AvlRotateR_14da0.
// ===========================================================================

// 0x140013480 — AVL rebalance driver for Layout B (0x30-node). Walks from *node up to
// the root, recomputing balance/height and applying e450 (left) / e580 (right) rotations,
// with the double-rotation pre-step when the child leans the opposite way.
extern "C" void Pf_AvlRebalanceB_13480(int64_t* root, int64_t* node)
{
    int64_t lVar1;
    int64_t lVar2;
    int16_t s3, s4, s5;
    int64_t cur;   // lVar6


    lVar1 = *node;
    while (lVar1 != 0) {
        cur = *node;
        lVar1 = P64((uintptr_t)cur, 0x10);          // parent (next step up)
        s5 = (P64((uintptr_t)cur, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)cur, 0x18), 0x2a) + 1);
        s4 = (P64((uintptr_t)cur, 0x20) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)cur, 0x20), 0x2a) + 1);
        s3 = s4;
        if (s4 < s5) s3 = s5;
        P16((uintptr_t)cur, 0x28) = (int16_t)(s4 - s5);
        P16((uintptr_t)cur, 0x2a) = s3;

        cur = *node;
        if (1 < P16((uintptr_t)cur, 0x28)) {
            lVar2 = P64((uintptr_t)cur, 0x20);      // right child
            if (lVar2 != 0) {
                if ((P16((uintptr_t)lVar2, 0x28) < 1) && (P16((uintptr_t)lVar2, 0x28) < 0)) {
                    Pf_AvlRotateB_e580(root, lVar2);
                    cur = *node;
                }
                Pf_AvlRotateB_e450(root, cur);
            }
        }
        cur = *node;
        if (P16((uintptr_t)cur, 0x28) < -1) {
            lVar2 = P64((uintptr_t)cur, 0x18);      // left child
            if (lVar2 != 0) {
                if ((-1 < P16((uintptr_t)lVar2, 0x28)) && (0 < P16((uintptr_t)lVar2, 0x28))) {
                    Pf_AvlRotateB_e450(root, lVar2);
                    cur = *node;
                }
                Pf_AvlRotateB_e580(root, cur);
            }
        }
        *node = lVar1;
    }
}

// 0x14001a8c0 — AVL rebalance loop for Layout A (0x28-node). Mirror of 13480 but with
// the 0x28-node offsets and the 14c70 (left) / 14da0 (right) rotations.
extern "C" void Pf_AvlRebalanceA_a8c0(int64_t* root, int64_t* node)
{
    int64_t lVar1;
    int64_t lVar2;
    int16_t s3, s4, s5;
    int64_t cur;


    lVar1 = *node;
    while (lVar1 != 0) {
        cur = *node;
        lVar1 = P64((uintptr_t)cur, 0x08);          // parent (next step up)
        s5 = (P64((uintptr_t)cur, 0x10) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)cur, 0x10), 0x22) + 1);
        s4 = (P64((uintptr_t)cur, 0x18) == 0) ? 0 : (int16_t)(P16(P64((uintptr_t)cur, 0x18), 0x22) + 1);
        s3 = s4;
        if (s4 < s5) s3 = s5;
        P16((uintptr_t)cur, 0x20) = (int16_t)(s4 - s5);
        P16((uintptr_t)cur, 0x22) = s3;

        cur = *node;
        if (1 < P16((uintptr_t)cur, 0x20)) {
            lVar2 = P64((uintptr_t)cur, 0x18);      // right child
            if (lVar2 != 0) {
                if ((P16((uintptr_t)lVar2, 0x20) < 1) && (P16((uintptr_t)lVar2, 0x20) < 0)) {
                    Pf_AvlRotateR_14da0(root, lVar2);
                    cur = *node;
                }
                Pf_AvlRotateL_14c70(root, cur);
            }
        }
        cur = *node;
        if (P16((uintptr_t)cur, 0x20) < -1) {
            lVar2 = P64((uintptr_t)cur, 0x10);      // left child
            if (lVar2 != 0) {
                if ((-1 < P16((uintptr_t)lVar2, 0x20)) && (0 < P16((uintptr_t)lVar2, 0x20))) {
                    Pf_AvlRotateL_14c70(root, lVar2);
                    cur = *node;
                }
                Pf_AvlRotateR_14da0(root, cur);
            }
        }
        *node = lVar1;
    }
}

// ===========================================================================
// Node allocators
// ===========================================================================

// 0x14001a680 — alloc+init a 0x28 AVL node (Layout A). param_2 -> int key, param_3 -> parent.
// Returns the new node, or null on OOM.
extern "C" uint32_t* Pf_AvlNewNodeA_a680(uint64_t /*param_1*/, uint32_t* param_2, uint64_t param_3)
{
    uint32_t* node = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x28, 8));
    if (node == nullptr) {
        return nullptr;
    }
    uint32_t key = *param_2;
    *reinterpret_cast<uint64_t*>(node + 2) = param_3;   // +0x08 parent
    *node = key;                                          // +0x00 key
    *reinterpret_cast<uint64_t*>(node + 4) = 0;          // +0x10 left
    *reinterpret_cast<uint64_t*>(node + 6) = 0;          // +0x18 right
    node[8] = 0;                                          // +0x20 balance/height (4 bytes)
    return node;
}

// 0x14001cc40 — alloc+init a 0x30 AVL node (Layout B). param_2 -> 3-int key prefix,
// param_3 -> parent. Returns the new node, or null on OOM.
extern "C" uint32_t* Pf_AvlNewNodeB_cc40(uint64_t /*param_1*/, uint32_t* param_2, uint64_t param_3)
{
    uint32_t* node = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x30, 8));
    if (node == nullptr) {
        return nullptr;
    }
    node[0] = param_2[0];                                // +0x00 key1
    node[1] = param_2[1];                                // +0x04 key2
    node[2] = param_2[2];                                // +0x08 key3
    *reinterpret_cast<uint64_t*>(node + 4) = param_3;    // +0x10 parent
    *reinterpret_cast<uint64_t*>(node + 6) = 0;          // +0x18 left
    *reinterpret_cast<uint64_t*>(node + 8) = 0;          // +0x20 right
    node[10] = 0;                                        // +0x28 balance/height (4 bytes)
    return node;
}

// ===========================================================================
// Inserts
// ===========================================================================

// The inserts call 0x14001cc40 Pf_AvlNewNodeB_cc40 / 0x140013480 Pf_AvlRebalanceB_13480
// (Layout B) and 0x14001a680 Pf_AvlNewNodeA_a680 / 0x14001a8c0 Pf_AvlRebalanceA_a8c0
// (Layout A) by name.

// 0x14001c940 — BST insert + rebalance for Layout B (0x30-node). param_1 = tree head
// (root ptr at [0], count int at [1] == +0x08); param_2 = out-node; param_3 = key prefix
// (uint key1, key2, key3). Ordered by (key1, key2). Calls cc40 (alloc) and 13480 (rebalance).
extern "C" uint64_t* Pf_BstInsertB_c940(int64_t* param_1, uint64_t* param_2, uint32_t* param_3)
{
    uint32_t  key;     // uVar1
    int32_t   curKey;  // iVar2
    int32_t*  cur;     // piVar3
    int32_t*  walk;    // piVar4
    uint32_t* fresh;   // puVar5
    uint32_t* nn;      // puVar6
    int32_t*  local_res8;


    if (reinterpret_cast<int32_t*>(*param_1) == nullptr) {
        fresh = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x30, 8));
        if (fresh == nullptr) {
            *param_1 = 0;
            *param_2 = 0;
        } else {
            key = *param_3;
            *param_2 = reinterpret_cast<uint64_t>(fresh);
            fresh[0] = key;
            fresh[1] = param_3[1];
            fresh[2] = param_3[2];
            fresh[4] = 0;   // +0x10 parent
            fresh[5] = 0;
            fresh[6] = 0;   // +0x18 left
            fresh[7] = 0;
            fresh[8] = 0;   // +0x20 right
            fresh[9] = 0;
            fresh[10] = 0;  // +0x28 balance/height
            *param_1 = reinterpret_cast<int64_t>(fresh);
        }
    } else {
        key = *param_3;
        walk = reinterpret_cast<int32_t*>(*param_1);
        do {
            while (local_res8 = walk, cur = local_res8, curKey = *local_res8,
                   curKey <= (int32_t)key) {
                if ((int32_t)key <= curKey) {
                    if ((int32_t)param_3[1] < local_res8[1]) break;
                    if (((int32_t)key <= curKey) &&
                        (((int32_t)key < curKey) || ((int32_t)param_3[1] <= local_res8[1]))) {
                        *param_2 = reinterpret_cast<uint64_t>(local_res8);
                        return param_2;
                    }
                }
                walk = *reinterpret_cast<int32_t**>(local_res8 + 8);   // +0x20 right
                if (*reinterpret_cast<int32_t**>(local_res8 + 8) == nullptr) {
                    nn = Pf_AvlNewNodeB_cc40((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
                    *reinterpret_cast<uint32_t**>(cur + 8) = nn;       // cur->right = nn
                    goto LAB_14001ca2a;
                }
            }
            walk = *reinterpret_cast<int32_t**>(local_res8 + 6);       // +0x18 left
        } while (*reinterpret_cast<int32_t**>(local_res8 + 6) != nullptr);
        nn = Pf_AvlNewNodeB_cc40((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
        *reinterpret_cast<uint32_t**>(cur + 6) = nn;                   // cur->left = nn
LAB_14001ca2a:
        Pf_AvlRebalanceB_13480(param_1, reinterpret_cast<int64_t*>(&local_res8));
        *param_2 = reinterpret_cast<uint64_t>(nn);
    }
    *reinterpret_cast<int32_t*>(param_1 + 1) = (int32_t)param_1[1] + 1;   // ++count at +0x08
    return param_2;
}

// 0x14001a4f0 — AVL insert recursive + rebalance for Layout A (0x28-node). param_1 = tree
// head (root ptr at [0], count int at +0x08); param_2 = out-node; param_3 = single uint key.
// Calls a680 (alloc) and a8c0 (rebalance).
extern "C" uint64_t* Pf_AvlInsertA_a4f0(int64_t* param_1, uint64_t* param_2, uint32_t* param_3)
{
    uint32_t  key;     // uVar1
    int32_t*  cur;     // piVar2
    int32_t*  walk;    // piVar3
    uint32_t* fresh;   // puVar4
    uint32_t* nn;      // puVar5
    int32_t*  local_res8;


    if (reinterpret_cast<int32_t*>(*param_1) == nullptr) {
        fresh = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x28, 8));
        if (fresh == nullptr) {
            *param_1 = 0;
            *param_2 = 0;
        } else {
            fresh[0] = *param_3;   // +0x00 key
            fresh[2] = 0;          // +0x08 parent
            fresh[3] = 0;
            fresh[4] = 0;          // +0x10 left
            fresh[5] = 0;
            fresh[6] = 0;          // +0x18 right
            fresh[7] = 0;
            fresh[8] = 0;          // +0x20 balance/height
            *param_1 = reinterpret_cast<int64_t>(fresh);
            *param_2 = reinterpret_cast<uint64_t>(fresh);
        }
    } else {
        key = *param_3;
        walk = reinterpret_cast<int32_t*>(*param_1);
        do {
            while (local_res8 = walk, cur = local_res8, *local_res8 <= (int32_t)key) {
                if ((int32_t)key <= *local_res8) {
                    *param_2 = reinterpret_cast<uint64_t>(local_res8);
                    return param_2;
                }
                walk = *reinterpret_cast<int32_t**>(local_res8 + 6);   // +0x18 right
                if (*reinterpret_cast<int32_t**>(local_res8 + 6) == nullptr) {
                    nn = Pf_AvlNewNodeA_a680((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
                    *reinterpret_cast<uint32_t**>(cur + 6) = nn;       // cur->right = nn
                    goto LAB_14001a5b5;
                }
            }
            walk = *reinterpret_cast<int32_t**>(local_res8 + 4);       // +0x10 left
        } while (*reinterpret_cast<int32_t**>(local_res8 + 4) != nullptr);
        nn = Pf_AvlNewNodeA_a680((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
        *reinterpret_cast<uint32_t**>(cur + 4) = nn;                   // cur->left = nn
LAB_14001a5b5:
        Pf_AvlRebalanceA_a8c0(param_1, reinterpret_cast<int64_t*>(&local_res8));
        *param_2 = reinterpret_cast<uint64_t>(nn);
    }
    *reinterpret_cast<int32_t*>(param_1 + 1) = (int32_t)param_1[1] + 1;   // ++count at +0x08
    return param_2;
}

// ===========================================================================
// PathGraph cost-cache helpers (CitizenBehavior path-distance graph)
// ===========================================================================
// These two live with the AVL/BST family because they operate on the same
// Layout-B (0x30) nodes and the pairs scratch header used by the pathgraph.
// Consumed by CitizenBehavior_ProcessPaths / _BuildPathGraph
// (code/Game/Entity/citizenbehavior.cpp), registered via
// standalone/reg_citizen_25_tick.cpp.

// PathBST_Insert calls 0x14001cc40 Pf_AvlNewNodeB_cc40 and 0x14000e360
// CitizenAI_Reg_helper_e360 (the AVL rebalance walker, Layout B) by name.

// 0x140025a10 — PathBST_Insert: insert-or-find in the region-pair cost-cache tree
// at pg+0x18 (Layout B 0x30-node). param_1 = tree head {root@+0, count@+8};
// param_2 = out-node; param_3 = 3-int key {regionHigh, regionLow, cost}. Ordered by
// (key0, key1). Structurally identical to Pf_BstInsertB_c940 but allocates via cc40
// and rebalances via TradeDescription_impl_e360 (0x14000e360) rather than 13480.
extern "C" uint64_t* PathBST_Insert_25a10(int64_t* param_1, uint64_t* param_2, uint32_t* param_3)
{
    uint32_t  key;
    int32_t   curKey;
    int32_t*  cur;
    int32_t*  walk;
    uint32_t* fresh;
    uint32_t* nn;
    int32_t*  local_res8;


    if (reinterpret_cast<int32_t*>(*param_1) == nullptr) {
        fresh = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x30, 8));
        if (fresh == nullptr) {
            *param_1 = 0;
            *param_2 = 0;
        } else {
            key = *param_3;
            *param_2 = reinterpret_cast<uint64_t>(fresh);
            fresh[0] = key;
            fresh[1] = param_3[1];
            fresh[2] = param_3[2];
            fresh[4] = 0;   // +0x10 parent
            fresh[5] = 0;
            fresh[6] = 0;   // +0x18 left
            fresh[7] = 0;
            fresh[8] = 0;   // +0x20 right
            fresh[9] = 0;
            fresh[10] = 0;  // +0x28 balance/height
            *param_1 = reinterpret_cast<int64_t>(fresh);
        }
    } else {
        key = *param_3;
        walk = reinterpret_cast<int32_t*>(*param_1);
        do {
            while (local_res8 = walk, cur = local_res8, curKey = *local_res8,
                   curKey <= (int32_t)key) {
                if ((int32_t)key <= curKey) {
                    if ((int32_t)param_3[1] < local_res8[1]) break;
                    if (((int32_t)key <= curKey) &&
                        (((int32_t)key < curKey) || ((int32_t)param_3[1] <= local_res8[1]))) {
                        *param_2 = reinterpret_cast<uint64_t>(local_res8);
                        return param_2;
                    }
                }
                walk = *reinterpret_cast<int32_t**>(local_res8 + 8);   // +0x20 right
                if (*reinterpret_cast<int32_t**>(local_res8 + 8) == nullptr) {
                    nn = Pf_AvlNewNodeB_cc40((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
                    *reinterpret_cast<uint32_t**>(cur + 8) = nn;       // cur->right = nn
                    goto LAB_140025afa;
                }
            }
            walk = *reinterpret_cast<int32_t**>(local_res8 + 6);       // +0x18 left
        } while (*reinterpret_cast<int32_t**>(local_res8 + 6) != nullptr);
        nn = Pf_AvlNewNodeB_cc40((uint64_t)key, param_3, reinterpret_cast<uint64_t>(local_res8));
        *reinterpret_cast<uint32_t**>(cur + 6) = nn;                   // cur->left = nn
LAB_140025afa:
        CitizenAI_Reg_helper_e360(param_1, reinterpret_cast<int64_t*>(&local_res8));   // 0x14000e360
        *param_2 = reinterpret_cast<uint64_t>(nn);
    }
    *reinterpret_cast<int32_t*>(param_1 + 1) = (int32_t)param_1[1] + 1;   // ++count at +0x08
    return param_2;
}

// 0x140025b30 — PathPairs_Clear: reset the pairs scratch array (pg+0x00 header
// {data@+0, count@+8, capacity@+0xC}) count to 0. Ghidra dropped the second
// parameter (newCount, EDX); both live callers (ProcessPaths @1400d70aa,
// BuildPathGraph @1400d7266) pass newCount==0, for which the faithful behavior is
// a single count reset — capacity (>=0) is never < newCount(0), so there is no
// realloc/copy/free and the decompiled `< 0` growth loops are pure signedness
// artifacts of the lost argument (intentionally NOT transcribed).
extern "C" void PathPairs_Clear_25b30(int64_t* param_1)
{
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(param_1) + 0x8) = 0;
}
