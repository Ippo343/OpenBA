// code/Game/Work/marketwork_bst_aaa0.cpp
// Wave (MarketWork BST) -- the ERASE cluster 0x14002aaa0 -> 0x14002ac40 -> 0x14002aee0.
//
// These three functions are one dependency chain and implement ERASE from MarketWork's
// self-balancing (AVL) binary search tree.  They are the mirror image of the insert
// cluster ported in marketwork_bst_a9f0.cpp.
//
//   0x14002aaa0  erase-BY-KEY  : unsigned-64 search from the root; on an exact key match
//                                hands the node to 0x14002ac40.  Silent no-op if absent.
//   0x14002ac40  erase-BY-NODE : decrement the count, unlink the node (three classic BST
//                                cases), destroy it, then AVL-retrace from the lowest
//                                node whose subtree changed shape (0x140025920 -> the
//                                "PlayerTrade_Rot_*" rotations 0x140014ed0 / 0x140015000).
//   0x14002aee0  destroy-node  : free the node's payload array, zero it, free the node.
//
// ---------------------------------------------------------------------------
// ROLE CORRECTION -- 0x14002aaa0 IS NOT AN INSERT
// ---------------------------------------------------------------------------
// The wave brief (and the shape of its call sites) suggested "BST insert".  It is not.
// The function allocates nothing, calls no allocator, never writes a child pointer, and
// its single non-CheckPtr callee is 0x14002ac40, which frees memory and decrements the
// element count.  It is erase(key).  Both of its callers are one-line vtable thunks that
// forward the SAME tree object:
//   0x1400eff50 (Ghidra "MarketWork_OnCreate", MarketWork vtable Slot6)
//        -> MarketWork_sub_aaa0(this + 0x48, keyIBP); returns void
//   0x1400eff60 (Ghidra "MarketWork_Draw",     MarketWork vtable Slot7)
//        -> MarketWork_sub_aaa0(this + 0x48, keyIBP); returns 0 (undefined1 -> bool false)
// So Slot6 and Slot7 do the same work and differ only in the return value -- the classic
// Work "CancelWork(entity)" / "RemoveWork(entity) -> bool" pair.  Naming either of them
// "insert" in marketwork.cpp's typedef table would be wrong, and Ghidra's undefined1
// return on 0x1400eff60 is a REAL return value, not void (trap #3).
//
// ---------------------------------------------------------------------------
// Node layout (0x38) -- every offset below re-derived from the DISASSEMBLY
// ---------------------------------------------------------------------------
//   +0x00 key (InternalBasePtr, compared UNSIGNED)
//   +0x08 payload array ptr        +0x10 count:int32 / cap:int32  (zeroed as one 8-byte 0)
//   +0x18 parent   +0x20 left   +0x28 right
//   +0x30 balance:int16            +0x32 height:int16
// Tree header (MarketWork+0x48):   +0x00 root (8 bytes)   +0x08 count (int32)
//
// The int16 balance/height pair is proved by 0x14002ac40's own rebalance fixup:
//   adb9  MOV word ptr [RBX + 0x32],AX     ; height  = max(childH_left, childH_right)
//   adc1  MOV word ptr [RBX + 0x30],R8W    ; balance = rightH - leftH
// (marketwork_bst_a9f0.cpp calls +0x30 an int32 balance; the insert path only ever ZEROES
// that word pair, so both descriptions agree on the bytes it writes.  For erase the
// distinction matters: a 4-byte store to +0x30 here would clobber the height.)
//
// ---------------------------------------------------------------------------
// GHIDRA DECOMPILER-vs-DISASSEMBLY DISAGREEMENTS
// ---------------------------------------------------------------------------
// * Trap #1 (elements vs bytes).  The decompiler renders 0x14002aaa0's children as
//   self[4] / self[5] on a ulonglong* -- BYTE offsets 0x20 / 0x28.  Likewise
//   0x14002ac40's param_2[3]/[4]/[5] are parent/left/right at 0x18/0x20/0x28.  Reading
//   those as byte offsets 3/4/5 would land inside the key and the payload pointer.  Every
//   access below matches a literal MOV ... [reg + 0x18/0x20/0x28] in the listing.
// * The decompiler's tail for 0x14002ac40 is wrong-looking but harmless: it hoists a
//   single "if (lVar2 == 0)" out of three different predecessors, and lVar2 aliases a
//   different register on each path (it is uninitialised on one of them as written).  The
//   disassembly is unambiguous -- each path does its own TEST on its own register and
//   CMOVZ/JZ picks between the two stack slots [RSP+0x50] and [RSP+0x58].  Ported as an
//   explicit per-path useB flag.
// * Ghidra shows 0x14002aee0 as (undefined8 param_1, void *param_2) and never uses
//   param_1.  That is correct and deliberate: aee5 homes RDX, and the RCX home slot is
//   never loaded back.  RCX is a DEAD parameter (the callee is a node destructor that
//   ignores its "this").  Kept in the exported signature so the registered function is
//   ABI-identical to retail.
// * 0x14002ac40's "right == 0" path does NOT call 0x14002aee0 -- the compiler INLINED the
//   destructor there (acbc-acd1).  Same stores, same order; shared here as DestroyNodeBody.
//
// ---------------------------------------------------------------------------
// Cross-check
// ---------------------------------------------------------------------------
// 0x14002ac40 is the 0x38-node twin of 0x140025740 (PlayerTrade_BSTRemove_25740, ported
// in code/Game/Player/tool_base_trade.cpp), which does the identical unlink dance on the
// simpler 0x20-node tree.  The only structural difference is the payload free at +0x08.
// The two ports were derived independently and agree instruction-for-instruction on the
// unlink/rebalance logic, which is the strongest available check on this one.
//
// ---------------------------------------------------------------------------
// Runtime primitives (reached through .rdata dispatch slots, exactly as retail does)
// ---------------------------------------------------------------------------
//   [0x140152b68] System::MemoryAllocator::MemFree  (static 1-arg)
// B3R: called by name. The REDZONE / ALLOC_STATS instruments sit on the allocator's
// own hook inside MemAlloc / MemFree (W0-A0), so a direct call is instrumented.
// 0x140025920 (retrace) is our own PlayerTrade_Retrace_25920, called by name.

#include "Work/marketwork_bst_aaa0.h"
#include "Player/exe_bodies.h"          // B3R: PlayerTrade_Retrace_25920 (0x140025920)
#include "system/MemoryAllocator.h"    // B3R: slot 0x140152b68 = System::MemoryAllocator::MemFree
#include <cstdint>

namespace {

// --- node field accessors (byte offsets, see header comment) -----------------
inline uint64_t& NodeKey(int64_t n)    { return *reinterpret_cast<uint64_t*>(n + 0x00); }
inline int64_t&  NodeParent(int64_t n) { return *reinterpret_cast<int64_t*>(n + 0x18); }
inline int64_t&  NodeLeft(int64_t n)   { return *reinterpret_cast<int64_t*>(n + 0x20); }
inline int64_t&  NodeRight(int64_t n)  { return *reinterpret_cast<int64_t*>(n + 0x28); }
inline int16_t&  NodeBal(int64_t n)    { return *reinterpret_cast<int16_t*>(n + 0x30); }
inline int16_t&  NodeHeight(int64_t n) { return *reinterpret_cast<int16_t*>(n + 0x32); }

// height-of-child: null -> 0, else child.height + 1
// (ad87 MOVZX R9D,[RAX+0x32] / INC R9W, with the null arm taking EDX, which is 0 there.)
inline int16_t ChildH(int64_t child) {
    return (child == 0) ? (int16_t)0 : (int16_t)(NodeHeight(child) + 1);
}

// The body of 0x14002aee0, minus its null test.  Retail emits these exact four steps
// twice: once inlined at 0x14002acbc-acd1, once as the standalone callee.
//   free(node->payload); node->payload = 0; node->count/cap = 0; free(node);
// The two zero stores are 8 BYTES EACH (MOV qword ptr [RBX],RDX / [RBX+8],RDX with
// RBX = node+8).  Narrowing either to 4 bytes would leave the array capacity stale --
// exactly the desync trap #2 warns about.  The block is freed immediately afterwards, but
// the redzone allocator inspects freed blocks, so keep the stores faithful.
void DestroyNodeBody(int64_t node) {
    void* payload = *reinterpret_cast<void**>(node + 0x08);
    System::MemoryAllocator::MemFree(payload);
    *reinterpret_cast<int64_t*>(node + 0x08) = 0;
    *reinterpret_cast<int64_t*>(node + 0x10) = 0;
    System::MemoryAllocator::MemFree(reinterpret_cast<void*>(node));
}

} // namespace

// ============================================================================
// 0x14002aee0 -- destroy one market BST node
// ============================================================================
// __fastcall void MarketWork_sub_ac40_sub_aee0(void* unusedRCX, void* node [RDX])
//
//   aee0  TEST RDX,RDX / JZ af33      ; the whole body is under a null test on the NODE
//   aee5  MOV [RSP+0x10],RDX          ; RDX homed; the RCX home slot is never read back
//   af05  LEA RBX,[RDX+8] / af0e MOV RCX,[RBX] / af11 CALL [0x140152b68]  ; free payload
//   af19  MOV [RBX],RAX / af1c MOV [RBX+8],RAX                            ; zero 0x08/0x10
//   af20  MOV RCX,RDI / af23 CALL [0x140152b68]                           ; free node
//
// The null test is RETAIL's, not a defensive addition -- 0x14002ac40 only ever reaches
// this with a value it has already proven non-null, so retail is being generous, not us.
extern "C" void MarketBst_aee0(void* /*unusedRcx*/, void* node) {
    if (node == nullptr) return;
    DestroyNodeBody(reinterpret_cast<int64_t>(node));
}

// ============================================================================
// 0x14002ac40 -- erase one KNOWN node from the tree, then AVL-retrace
// ============================================================================
// __fastcall void MarketWork_sub_ac40(int64_t* tree [RCX], void* node [RDX])
//
// tree points at the header {root:int64, count:int32} living at MarketWork+0x48.
//
// Two stack slots carry the retrace start point; the tail picks one of them BY ADDRESS
// because 0x140025920 takes a pointer to a cursor it walks upward (and writes through):
//   [RSP+0x50] "cursorA" -- the parent of whatever left the tree
//   [RSP+0x58] "cursorB" -- the node that took the erased node's place
extern "C" void MarketBst_ac40(int64_t* tree, void* nodeArg) {
    const int64_t z = reinterpret_cast<int64_t>(nodeArg);

    int64_t cursorA = NodeParent(z);   // ac61: MOV RAX,[RDX+0x18] -> [RSP+0x50]
    int64_t cursorB = 0;               // [RSP+0x58]
    bool    useB    = false;

    // ac6a: DEC dword ptr [RCX+8].  The count really is a 32-bit field sharing the
    // header's second 8-byte slot -- this is a 4-byte RMW in retail, NOT a narrowed
    // 8-byte store (trap #2 cuts both ways: widening it would smash the upper half).
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(tree) + 8) -= 1;

    const int64_t right = NodeRight(z);
    if (right == 0) {
        // ---- Case 1: no right child -- splice the left child into z's place --------
        const int64_t parent = NodeParent(z);
        const int64_t leftAtSplice = NodeLeft(z);            // ac7a
        if (parent == 0) {
            *tree = leftAtSplice;                            // ac95: MOV [RSI],RAX
        } else if (NodeLeft(parent) == z) {
            NodeLeft(parent) = leftAtSplice;                 // ac89
        } else {
            NodeRight(parent) = leftAtSplice;                // ac8f
        }
        // ac98 re-reads z->left after the parent fixup (same value; kept literal).
        const int64_t left = NodeLeft(z);
        cursorB = left;
        if (left != 0) NodeParent(left) = NodeParent(z);     // acaa
        DestroyNodeBody(z);                                  // inlined 0x14002aee0
        useB = (left != 0);                                  // acd7: TEST RBP,RBP
    } else {
        const int64_t left = NodeLeft(z);
        if (left == 0) {
            // ---- Case 2: right child only -- splice it in -------------------------
            const int64_t parent = NodeParent(z);            // acfc
            if (parent == 0) {
                *tree = right;                               // ad17
            } else if (NodeLeft(parent) == z) {
                NodeLeft(parent) = right;                    // ad0b
            } else {
                NodeRight(parent) = right;                   // ad11
            }
            const int64_t r = NodeRight(z);                  // ad1a (re-read, same value)
            cursorB = r;
            if (r != 0) NodeParent(r) = NodeParent(z);       // ad2c
            // ad30: CALL 0x14002aee0 with RCX still holding `right` -- a dead argument,
            // passed here only to keep the call ABI-identical.
            MarketBst_aee0(reinterpret_cast<void*>(right), nodeArg);
            useB = (r != 0);                                 // ad35: TEST RBX,RBX
        } else {
            // ---- Case 3: two children ---------------------------------------------
            // Replacement = in-order PREDECESSOR: rightmost node of the LEFT subtree.
            // ad40: RAX=[RCX+0x28]; while (RAX) { RCX=RAX; ... }
            int64_t s = left;
            for (int64_t nxt = NodeRight(s); nxt != 0; nxt = NodeRight(s)) s = nxt;

            int64_t detachParent = 0;   // RBX -- 0 means "s WAS z->left, nothing detached"
            cursorA = 0;                // ad52: [RSP+0x50] = 0

            if (s != left) {            // ad57: CMP RCX,R8
                // Lift s out of its current spot: its parent adopts s's left subtree.
                detachParent = NodeParent(s);                      // ad5c
                const int64_t sLeft = NodeLeft(s);                 // ad60
                NodeRight(detachParent) = sLeft;                   // ad64
                if (sLeft != 0) NodeParent(sLeft) = detachParent;  // ad6d
                NodeLeft(s) = NodeLeft(z);                         // ad71/ad75
                cursorA = detachParent;                            // ad79

                // Fix detachParent's own metrics before the retrace walks past it.
                // Order is literal: height (+0x32) first, then balance (+0x30).
                const int16_t lh = ChildH(NodeLeft(detachParent));
                const int16_t rh = ChildH(NodeRight(detachParent));
                NodeHeight(detachParent) = (lh < rh) ? rh : lh;    // adb9, CMOVG (signed)
                NodeBal(detachParent) = (int16_t)(rh - lh);        // adc1, SUB R8W,R9W
            }

            NodeRight(s) = NodeRight(z);                           // adc6/adca
            if (NodeLeft(s)  != 0) NodeParent(NodeLeft(s))  = s;   // add7
            if (NodeRight(s) != 0) NodeParent(NodeRight(s)) = s;   // ade4

            const int64_t parent = NodeParent(z);                  // ade8
            NodeParent(s) = parent;                                // adec
            if (parent == 0) {
                *tree = s;                                         // adf6
                NodeParent(s) = 0;                                 // adf9
                cursorB = *tree;                                   // adfd/ae00
            } else {
                cursorB = s;                                       // ae07
                if (NodeLeft(parent) == z) NodeLeft(parent) = s;   // ae12
                else                       NodeRight(parent) = s;  // ae18
            }
            // ae1f: CALL 0x14002aee0 with RCX = s (dead argument again).
            // NOTE: s keeps its OWN balance/height here -- retail does not copy z's.
            // The retrace is what re-derives them.
            MarketBst_aee0(reinterpret_cast<void*>(s), nodeArg);
            useB = (detachParent == 0);                            // ae2e/ae31: TEST/CMOVZ
        }
    }

    // ae38: CALL 0x140025920(tree, &cursor).
    PlayerTrade_Retrace_25920(tree, useB ? &cursorB : &cursorA);
}

// ============================================================================
// 0x14002aaa0 -- erase BY KEY (MarketWork vtable Slot6 / Slot7 helper)
// ============================================================================
// __fastcall void MarketWork_sub_aaa0(int64_t* tree [RCX], void* keyIBP [RDX])
//
// Plain unbalanced descent -- all balancing happens inside 0x14002ac40.  Keys compare
// UNSIGNED: aacf is JC (below) and aae0 is JNC (above-or-equal), not JL/JGE.
//
//   aac0  MOV RCX,RBX / CALL [0x140152ab0]   ; CheckPtr(node)  -- node+0x00 IS the IBP
//   aac9  MOV RAX,[RBX] / aacc CMP [RDI],RAX / aacf JC aae8   ; key < node.key -> LEFT
//   aad1  MOV RCX,RDI / CALL [0x140152ab0]   ; CheckPtr(keyIBP)  (only on this arm)
//   aada  MOV RAX,[RDI] / aadd CMP [RBX],RAX / aae0 JNC ab01  ; node.key >= key -> HIT
//   aae2  MOV RBX,[RBX+0x28]                 ; else RIGHT
//
// Both CheckPtr calls sit BEFORE the read they protect, and CheckPtr may rewrite the
// InternalBasePtr in place (that is its whole job -- it revalidates a weak handle).  So
// both keys are RE-READ after each CheckPtr rather than cached in a local; caching would
// compare stale handles.  The asymmetry (node revalidated every iteration, keyIBP only on
// the >= arm) is retail's, and is preserved.
//
// Reaching the hit arm means key >= node.key AND node.key >= key, i.e. equality.
extern "C" void MarketBst_aaa0(int64_t* tree, void* keyIBP) {

    int64_t node = *tree;
    if (node == 0) return;

    do {
        if (*static_cast<uint64_t*>(keyIBP) < NodeKey(node)) {
            node = NodeLeft(node);
        } else {
            if (NodeKey(node) >= *static_cast<uint64_t*>(keyIBP)) {
                MarketBst_ac40(tree, reinterpret_cast<void*>(node));   // ab07
                return;
            }
            node = NodeRight(node);
        }
    } while (node != 0);
}
