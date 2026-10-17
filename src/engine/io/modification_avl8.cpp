// ============================================================================
// src/engine/io/modification_avl8.cpp -- the width-8 AVL core of Set<System::Filename>
//   0x180027f50 rotate-right, 0x180027e20 rotate-left, 0x1800274c0 the retrace
// ============================================================================
// Moved verbatim out of modification_refresh.cpp (2026-10-17) so the host unit test can
// link the ported core (tests/unit/set_ported_test.cpp) without Modification::RefreshList
// and the package / file-system classes that file stands on. Nothing in the bodies
// changed; the banner below is the one they carried there.
// ============================================================================
#include <cstdint>

#include "io/Modification.h"   // RtIO::ModSetNode

namespace RtIO {

// ===========================================================================
// PART 1 — the Set<Filename> AVL internals
//   0x180027f50 rotate-right, 0x180027e20 rotate-left, 0x1800274c0 the retrace
// ===========================================================================
//
// The node is `ModSetNode` (Modification.h): key +0x00, parent +0x08, left
// +0x10, right +0x18, and TWO int16 at +0x20 / +0x22. The pair is not a guess —
// all three bodies below compute them with the same five instructions:
//
//     lh = left  ? left->[+0x22]  + 1 : 0      (MOVZX ; INC r16)
//     rh = right ? right->[+0x22] + 1 : 0
//     [+0x22] = max(lh, rh)                    (CMP ; MOVZX ; CMOVG — SIGNED)
//     [+0x20] = rh - lh                        (SUB r16, r16)
//
// so +0x22 is a HEIGHT (a leaf is 0) and +0x20 is a BALANCE FACTOR, signed,
// right-heavy positive. The retrace's tests read them signed (`CMP CX,1 / JLE`,
// `CMP word ptr [RDX+0x20],-1 / JGE`, `TEST CX,CX / JS`) — an independent
// second pin on the signedness.
//
// The increments and the subtraction are 16-BIT and retail lets them wrap
// (`INC R8W`, `SUB CX,R8W`). Reproduced by computing in uint16_t and
// reinterpreting, so a pathological tree wraps where retail wraps.

// The node is `ModSetNode` itself (src/engine/io/Modification.h): the key's
// buffer at +0x00, parent / left / right at +0x08 / +0x10 / +0x18, the signed
// balance / height pair at +0x20 / +0x22, each pinned there by this core's own
// instructions.
using AvlNode = ModSetNode;

static inline int16_t ChildHeight(const AvlNode* c) {
    if (c == nullptr) return 0;                                  // XOR r,r
    return static_cast<int16_t>(static_cast<uint16_t>(
        static_cast<uint16_t>(c->height) + 1u));                 // MOVZX ; INC r16
}

static void AvlUpdate(AvlNode* n) {
    const int16_t lh = ChildHeight(n->left);
    const int16_t rh = ChildHeight(n->right);
    n->height  = (lh > rh) ? lh : rh;                            // CMOVG, signed
    n->balance = static_cast<int16_t>(static_cast<uint16_t>(
        static_cast<uint16_t>(rh) - static_cast<uint16_t>(lh))); // SUB r16, r16
}

// 0x180027f50 — rotate RIGHT: the LEFT child (+0x10) is promoted.
// `CMP RDX,[RCX]` at entry is the root fix-up: RCX is `&set.root`, so a rotation
// at the root retargets the set's own root pointer. That is the whole reason
// the retrace is handed `&set.root` rather than the set.
static void* AvlRotateRight(AvlNode** root, AvlNode* n) {
    if (n == *root) *root = n->left;
    AvlNode* const up = n->parent;
    if (up != nullptr) {
        AvlNode* const l = n->left;
        if (up->left == n) up->left = l; else up->right = l;
    }
    AvlNode* const pivot = n->left;
    pivot->parent = n->parent;
    n->left = pivot->right;
    if (n->left != nullptr) n->left->parent = n;
    pivot->right = n;
    n->parent = pivot;
    // Retail recomputes the pivot's two children and then the pivot, in that
    // order, each guarded on non-null. `pivot->right` is `n`, so it can never
    // be null here; the guard is transcribed anyway because retail emits it.
    if (pivot->left  != nullptr) AvlUpdate(pivot->left);
    if (pivot->right != nullptr) AvlUpdate(pivot->right);
    AvlUpdate(pivot);
    return pivot;                                                // MOV RAX,R8
}

// 0x180027e20 — rotate LEFT: the RIGHT child (+0x18) is promoted. The mirror of
// the above; the only differences in the instruction stream are +0x10 <-> +0x18
// in the three places that pick the pivot and re-hang it.
static void* AvlRotateLeft(AvlNode** root, AvlNode* n) {
    if (n == *root) *root = n->right;
    AvlNode* const up = n->parent;
    if (up != nullptr) {
        AvlNode* const r = n->right;
        if (up->left == n) up->left = r; else up->right = r;
    }
    AvlNode* const pivot = n->right;
    pivot->parent = n->parent;
    n->right = pivot->left;
    if (n->right != nullptr) n->right->parent = n;
    pivot->left = n;
    n->parent = pivot;
    if (pivot->left  != nullptr) AvlUpdate(pivot->left);
    if (pivot->right != nullptr) AvlUpdate(pivot->right);
    AvlUpdate(pivot);
    return pivot;
}

// 0x1800274c0 — the retrace. `cursor` is a stack slot holding the node to start
// from, and the loop OVERWRITES it with each node's parent, so on return it is
// null. The parent is read BEFORE the rotations (0x1800274e6 `MOV RSI,[RAX+8]`),
// which is load-bearing: a rotation at `n` changes `n->parent`, and retracing
// from the NEW parent would revisit the subtree it just fixed.
//
// Both rebalance arms are guarded on the balance factor AND the child being
// non-null, in that order, and the second arm RE-READS `*cursor` because the
// first arm may have rotated that node out from under it.
extern "C" void RtColl_SetInsertFixup_1800274c0(AvlNode** root, AvlNode** cursor) {
    if (*cursor == nullptr) return;                    // CMP qword [RDX],0 ; JZ
    do {
        AvlNode* n = *cursor;
        AvlNode* const up = n->parent;                 // captured first — see above
        AvlUpdate(n);

        if (n->balance > 1) {                          // CMP CX,1 ; JLE
            AvlNode* const c = n->right;               // MOV RAX,[RDX+0x18] ; JZ
            if (c != nullptr) {
                if (c->balance < 0) {                  // TEST CX,CX ; JG ; JNS
                    AvlRotateRight(root, c);
                    n = *cursor;                       // MOV RDX,[RDI]
                }
                AvlRotateLeft(root, n);
            }
        }

        n = *cursor;                                   // MOV RDX,[RDI]
        if (n->balance < -1) {                         // CMP word [RDX+0x20],-1 ; JGE
            AvlNode* const c = n->left;                // MOV RAX,[RDX+0x10] ; JZ
            if (c != nullptr) {
                if (c->balance > 0) {                  // TEST CX,CX ; JS ; JLE
                    AvlRotateLeft(root, c);
                    n = *cursor;
                }
                AvlRotateRight(root, n);
            }
        }

        *cursor = up;                                  // MOV [RDI],RSI
    } while (*cursor != nullptr);                      // TEST RSI,RSI ; JNZ
}

}  // namespace RtIO
