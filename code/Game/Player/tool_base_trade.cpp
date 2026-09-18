// code/Game/Player/tool_base_trade.cpp
// Player wave round 2c (R3-trade) — EditTool trade/track radius helpers.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from
// code/Game/Player/edittool.cpp (and reused by tradework.cpp / nomadcomponent.cpp /
// entityui.cpp). They resolve to 0 (silently skipped) unless registered. This file
// provides faithful 1:1 reimplementations and a single registration point
// (RegisterPlayerTradeBatch in standalone/reg_player_trade.cpp).
//
// Closure (registered bottom-up by RegisterPlayerTradeBatch):
//   0x140066800 EditTool_helper_6800       -> 0x1400666b0
//   0x1400666b0 EntityUI_helper_66b0       -> 0x140102e30, 0x14002c690, GetController
//   0x140102e30 TradeWork_impl_2e30 (BST insert) -> 0x140025920, static MemAlloc
//   0x14002c690 TradeWork_impl_c690 (BST find+remove) -> 0x140025740
//   0x140025740 TradeWork_helper_5740 (BST node remove) -> 0x140025920, static MemFree
//   0x140025920 TradeWork_helper_5740_sub_5920 (retrace+rebalance) -> 0x140015000, 0x140014ed0
//   0x140015000 rotate-left  (leaf)
//   0x140014ed0 rotate-right (leaf)
//
// The BST is an AVL tree of 0x38-byte nodes keyed by an entity pointer (the IBP key
// at param2[0]). Node layout (verified from disasm):
//   +0x00 key (entity ptr, 8 bytes)   +0x08 int payloadA   +0x0c int payloadB
//   +0x10 byte flag (=1 on insert)    +0x18 parent  +0x20 left  +0x28 right
//   +0x30 short balance (= rightH - leftH)   +0x32 short height
//
// Runtime deps (resolve at runtime; NO registration needed):
//   fn_InternalBasePtr_CheckPtr / _Ctor, fn_IsDerivedFrom, fn_DialogInstance_GetController
//   (engine_funcs.h), and System::MemoryAllocator::MemAlloc/MemFree (static, by export).
//
// Port is 1:1 with the decompile — do not "improve".

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

namespace {

// ---- static System::MemoryAllocator MemAlloc/MemFree (the 2-arg static form the
// original uses via slot 0x140152b60 / 0x140152b68 — NOT the 3-arg fn_MemAlloc) ----
typedef void* (*MemAllocStatic_t)(uint64_t size, uint32_t align);  // real export is SAPEAX_KH (uint64 size, int align); SAPEAXII does not exist
typedef void  (*MemFreeStatic_t)(void* ptr);                       // SAXPEAX
MemAllocStatic_t s_memAlloc = nullptr;
MemFreeStatic_t  s_memFree  = nullptr;
bool s_memInit = false;
void EnsureMem() {
    if (s_memInit) return;
    // Prefer the registered exe dispatch slot (stores the Runtime fn-ptr); fall back
    // to ResolveExport (mirrors code/Game/Action/action_pf_geom.cpp).
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x140152b60));
    if (!s_memAlloc) s_memAlloc = reinterpret_cast<MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x140152b68));
    if (!s_memFree) s_memFree = reinterpret_cast<MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_memInit = true;
}

// CheckPtr convenience (engine_funcs export; resolves at runtime).

// AVL node field accessors (offsets verified against decompile/disasm).
inline uint64_t& NodeKey(int64_t n)    { return *reinterpret_cast<uint64_t*>(n + 0x00); }
inline int64_t&  NodeParent(int64_t n) { return *reinterpret_cast<int64_t*>(n + 0x18); }
inline int64_t&  NodeLeft(int64_t n)   { return *reinterpret_cast<int64_t*>(n + 0x20); }
inline int64_t&  NodeRight(int64_t n)  { return *reinterpret_cast<int64_t*>(n + 0x28); }
inline int16_t&  NodeBal(int64_t n)    { return *reinterpret_cast<int16_t*>(n + 0x30); }
inline int16_t&  NodeHeight(int64_t n) { return *reinterpret_cast<int16_t*>(n + 0x32); }

// height-of-child helper: child==0 -> 0, else child.height + 1
inline int16_t ChildH(int64_t child) {
    return (child == 0) ? (int16_t)0 : (int16_t)(NodeHeight(child) + 1);
}

} // namespace

// ============================================================
// 0x140015000 — single rotation, pivot = node.LEFT (+0x20). Node becomes pivot's
// RIGHT child. 1:1 port of TradeWork_helper_5740_sub_5920_sub_5000 (decompile uses
// *(node+0x20) as pivot). Returns the new subtree root (pivot).
// ============================================================
extern "C" int64_t PlayerTrade_Rot_15000(int64_t* root, int64_t node) {
    if (node == *root) {
        *root = NodeLeft(node);
    }
    int64_t parent = NodeParent(node);
    if (parent != 0) {
        if (NodeLeft(parent) == node) NodeLeft(parent) = NodeLeft(node);
        else                          NodeRight(parent) = NodeLeft(node);
    }
    int64_t pivot = NodeLeft(node);
    NodeParent(pivot) = NodeParent(node);
    int64_t pivotRight = NodeRight(pivot);
    NodeLeft(node) = pivotRight;
    if (pivotRight != 0) NodeParent(pivotRight) = node;
    NodeRight(pivot) = node;
    NodeParent(node) = pivot;

    // Recompute balance/height for pivot.left, pivot.right, then pivot.
    int64_t c = NodeLeft(pivot);
    if (c != 0) {
        int16_t lh = ChildH(NodeLeft(c));
        int16_t rh = ChildH(NodeRight(c));
        NodeBal(c) = (int16_t)(rh - lh);
        NodeHeight(c) = (lh < rh) ? rh : lh;
    }
    c = NodeRight(pivot);
    if (c != 0) {
        int16_t lh = ChildH(NodeLeft(c));
        int16_t rh = ChildH(NodeRight(c));
        NodeBal(c) = (int16_t)(rh - lh);
        NodeHeight(c) = (lh < rh) ? rh : lh;
    }
    {
        int16_t lh = ChildH(NodeLeft(pivot));
        int16_t rh = ChildH(NodeRight(pivot));
        NodeBal(pivot) = (int16_t)(rh - lh);
        NodeHeight(pivot) = (lh < rh) ? rh : lh;
    }
    return pivot;
}

// ============================================================
// 0x140014ed0 — single rotation, pivot = node.RIGHT (+0x28). Node becomes pivot's
// LEFT child. 1:1 port of TradeWork_helper_5740_sub_5920_sub_4ed0 (decompile uses
// *(node+0x28) as pivot). Returns the new subtree root (pivot).
// ============================================================
extern "C" int64_t PlayerTrade_Rot_14ed0(int64_t* root, int64_t node) {
    if (node == *root) {
        *root = NodeRight(node);
    }
    int64_t parent = NodeParent(node);
    if (parent != 0) {
        if (NodeLeft(parent) == node) NodeLeft(parent) = NodeRight(node);
        else                          NodeRight(parent) = NodeRight(node);
    }
    int64_t pivot = NodeRight(node);
    NodeParent(pivot) = NodeParent(node);
    int64_t pivotLeft = NodeLeft(pivot);
    NodeRight(node) = pivotLeft;
    if (pivotLeft != 0) NodeParent(pivotLeft) = node;
    NodeLeft(pivot) = node;
    NodeParent(node) = pivot;

    int64_t c = NodeLeft(pivot);
    if (c != 0) {
        int16_t lh = ChildH(NodeLeft(c));
        int16_t rh = ChildH(NodeRight(c));
        NodeBal(c) = (int16_t)(rh - lh);
        NodeHeight(c) = (lh < rh) ? rh : lh;
    }
    c = NodeRight(pivot);
    if (c != 0) {
        int16_t lh = ChildH(NodeLeft(c));
        int16_t rh = ChildH(NodeRight(c));
        NodeBal(c) = (int16_t)(rh - lh);
        NodeHeight(c) = (lh < rh) ? rh : lh;
    }
    {
        int16_t lh = ChildH(NodeLeft(pivot));
        int16_t rh = ChildH(NodeRight(pivot));
        NodeBal(pivot) = (int16_t)(rh - lh);
        NodeHeight(pivot) = (lh < rh) ? rh : lh;
    }
    return pivot;
}

// ============================================================
// 0x140025920 — retrace from *cursor up to the root, recomputing balance/height
// and applying single/double rotations. 1:1 port of TradeWork_helper_5740_sub_5920.
// param_2 (cursor) is an in/out slot holding the current node; the loop walks parents.
// ============================================================
extern "C" void PlayerTrade_Retrace_25920(int64_t* root, int64_t* cursor) {
    while (*cursor != 0) {
        int64_t node = *cursor;
        int64_t up = NodeParent(node);  // saved parent (next iteration target)

        int16_t leftH  = ChildH(NodeLeft(node));
        int16_t rightH = ChildH(NodeRight(node));
        int16_t maxH = (rightH < leftH) ? leftH : rightH;
        // balance = leftH(rightChild) - leftH(leftChild)? Match disasm exactly:
        //   *(node+0x30) = rightH - leftH ; *(node+0x32) = max(rightH,leftH)
        NodeBal(node) = (int16_t)(rightH - leftH);
        NodeHeight(node) = maxH;

        node = *cursor;
        // Right-heavy (bal > 1). Disasm @1400259ad/b8: if right-child bal < 0,
        // CALL 0x140015000 on the right child first, then CALL 0x140014ed0 on node.
        if (NodeBal(node) > 1 && NodeRight(node) != 0) {
            int64_t rc = NodeRight(node);
            if (NodeBal(rc) <= 0 && NodeBal(rc) < 0) {  // JG/JNS skip => bal_rc < 0
                PlayerTrade_Rot_15000(root, rc);
                node = *cursor;
            }
            PlayerTrade_Rot_14ed0(root, node);
        }
        node = *cursor;
        // Left-heavy (bal < -1). Disasm @1400259e1/ec: if left-child bal > 0,
        // CALL 0x140014ed0 on the left child first, then CALL 0x140015000 on node.
        if (NodeBal(node) < -1 && NodeLeft(node) != 0) {
            int64_t lc = NodeLeft(node);
            if (NodeBal(lc) >= 0 && NodeBal(lc) > 0) {  // JS/JLE skip => bal_lc > 0
                PlayerTrade_Rot_14ed0(root, lc);
                node = *cursor;
            }
            PlayerTrade_Rot_15000(root, node);
        }
        *cursor = up;
    }
}

// ============================================================
// 0x140025740 — remove an AVL node and rebalance. 1:1 port of TradeWork_helper_5740.
// param_1 = &tree.root (and param_1[1] = count). param_2 = node to remove.
// ============================================================
extern "C" void PlayerTrade_BSTRemove_25740(int64_t* root, void* nodeArg) {
    EnsureMem();
    int64_t node = reinterpret_cast<int64_t>(nodeArg);

    int64_t local_res8;   // retrace cursor candidate A
    int64_t local_res10;  // retrace cursor candidate B

    local_res8 = NodeParent(node);
    // count--  (root[1] is an int field overlapping the second pointer slot)
    *reinterpret_cast<int*>(root + 1) = *reinterpret_cast<int*>(root + 1) - 1;

    int64_t rightChild = NodeRight(node);
    int64_t fixup;  // lVar5 in decompile

    if (rightChild == 0) {
        // No right child: splice in the left child.
        int64_t parent = NodeParent(node);
        int64_t left   = NodeLeft(node);
        if (parent == 0) {
            *root = left;
        } else if (NodeLeft(parent) == reinterpret_cast<int64_t>(nodeArg)) {
            NodeLeft(parent) = left;
        } else {
            NodeRight(parent) = left;
        }
        fixup = NodeLeft(node);
    } else {
        int64_t leftChild = NodeLeft(node);
        if (leftChild != 0) {
            // Two children: find in-order predecessor (rightmost of left subtree).
            int64_t pred = leftChild;
            do {
                local_res10 = pred;
                pred = NodeRight(local_res10);
            } while (NodeRight(local_res10) != 0);

            local_res8 = 0;
            if (local_res10 != leftChild) {
                // Detach predecessor from its parent, re-link.
                int64_t predLeft = NodeLeft(local_res10);
                local_res8 = NodeParent(local_res10);
                NodeRight(local_res8) = predLeft;
                if (predLeft != 0) NodeParent(predLeft) = local_res8;
                NodeLeft(local_res10) = NodeLeft(node);

                int16_t lh = ChildH(NodeLeft(local_res8));
                int16_t rh = ChildH(NodeRight(local_res8));
                NodeBal(local_res8) = (int16_t)(rh - lh);
                NodeHeight(local_res8) = (lh < rh) ? rh : lh;
            }
            int64_t fix2 = local_res8;  // lVar5 = local_res8
            NodeRight(local_res10) = NodeRight(node);
            if (NodeLeft(local_res10) != 0)  NodeParent(NodeLeft(local_res10))  = local_res10;
            if (NodeRight(local_res10) != 0) NodeParent(NodeRight(local_res10)) = local_res10;

            int64_t parent = NodeParent(node);
            NodeParent(local_res10) = parent;
            if (parent == 0) {
                *root = local_res10;
                NodeParent(local_res10) = 0;
                local_res10 = *root;
            } else if (NodeLeft(parent) == reinterpret_cast<int64_t>(nodeArg)) {
                NodeLeft(parent) = local_res10;
            } else {
                NodeRight(parent) = local_res10;
            }
            if (s_memFree) s_memFree(nodeArg);
            int64_t* cursor = (fix2 == 0) ? &local_res10 : &local_res8;
            PlayerTrade_Retrace_25920(root, cursor);
            return;
        }
        // Right child only.
        int64_t parent = NodeParent(node);
        if (parent == 0) {
            *root = rightChild;
            fixup = NodeRight(node);
        } else if (NodeLeft(parent) == reinterpret_cast<int64_t>(nodeArg)) {
            NodeLeft(parent) = rightChild;
            fixup = NodeRight(node);
        } else {
            NodeRight(parent) = rightChild;
            fixup = NodeRight(node);
        }
    }

    if (fixup != 0) {
        NodeParent(fixup) = NodeParent(node);
    }
    local_res10 = fixup;
    if (s_memFree) s_memFree(nodeArg);
    if (fixup == 0) {
        PlayerTrade_Retrace_25920(root, &local_res8);
    } else {
        PlayerTrade_Retrace_25920(root, &local_res10);
    }
}

// ============================================================
// 0x140102e30 — AVL insert/update keyed by entity ptr. 1:1 port of TradeWork_impl_2e30.
// param_1 = tree object (root at param_1+0x60, count at param_1+0x68).
// param_2 = key IBP (key = *param2). param_3/param_4 = int payloads (-> node +0x08/+0x0c).
// ============================================================
extern "C" void PlayerTrade_BSTInsert_102e30(int64_t param_1, void* keyIBP,
                                             uint32_t payloadA, uint32_t payloadB) {
    EnsureMem();
    int64_t* rootSlot = reinterpret_cast<int64_t*>(param_1 + 0x60);
    int64_t cur = *rootSlot;
    uint64_t key = *reinterpret_cast<uint64_t*>(keyIBP);

    // First pass: search; if an existing node matches the key, update in place.
    while (cur != 0) {
        if (key < NodeKey(cur)) {
            cur = NodeRight(cur);
        } else {
            if (key <= NodeKey(cur)) {
                // exact match -> update flag + payloads, no structural change.
                *reinterpret_cast<uint8_t*>(cur + 0x10) = 1;
                *reinterpret_cast<uint32_t*>(cur + 0x08) = payloadA;
                *reinterpret_cast<uint32_t*>(cur + 0x0c) = payloadB;
                return;
            }
            cur = NodeLeft(cur);
        }
    }

    // Not found: allocate + link a new node. Staged values from decompile:
    uint64_t newKey = key;       // local_38
    uint8_t  newFlag = 1;        // local_28 (low byte of local_40)
    uint32_t newA = payloadA;    // local_30
    uint32_t newB = payloadB;    // local_2c

    int64_t newNode;
    if (*rootSlot == 0) {
        // Empty tree: new root.
        int64_t n = reinterpret_cast<int64_t>(s_memAlloc ? s_memAlloc(0x38, 8) : nullptr);
        newNode = 0;
        if (n != 0) {
            NodeKey(n) = 0;
            NodeKey(n) = newKey;
            *reinterpret_cast<uint32_t*>(n + 0x08) = newA;
            *reinterpret_cast<uint32_t*>(n + 0x0c) = newB;
            *reinterpret_cast<uint8_t*>(n + 0x10) = newFlag;
            NodeParent(n) = 0;
            NodeLeft(n)   = 0;
            NodeRight(n)  = 0;
            *reinterpret_cast<uint32_t*>(n + 0x30) = 0;  // balance(short)+height(short)
            newNode = n;
        }
        *rootSlot = newNode;
    } else {
        // Walk to the insertion point. local_res8 tracks the current parent.
        int64_t walk = *rootSlot;
        int64_t parent;
        for (;;) {
            // Inner loop: descend right while node.key <= newKey (and check exact dup).
            for (;;) {
                parent = walk;
                if (NodeKey(parent) > newKey) break;  // go left branch below
                if (newKey <= NodeKey(parent)) {
                    return;  // duplicate appeared mid-walk; nothing to do
                }
                walk = NodeRight(parent);
                if (NodeRight(parent) == 0) {
                    // attach as right child
                    int64_t n = reinterpret_cast<int64_t>(s_memAlloc ? s_memAlloc(0x38, 8) : nullptr);
                    newNode = 0;
                    if (n != 0) {
                        NodeKey(n) = 0;
                        NodeKey(n) = newKey;
                        *reinterpret_cast<uint32_t*>(n + 0x08) = newA;
                        *reinterpret_cast<uint32_t*>(n + 0x0c) = newB;
                        *reinterpret_cast<uint8_t*>(n + 0x10) = newFlag;
                        NodeLeft(n)  = 0;
                        NodeRight(n) = 0;
                        *reinterpret_cast<uint32_t*>(n + 0x30) = 0;
                        NodeParent(n) = parent;
                        newNode = n;
                    }
                    NodeRight(parent) = newNode;
                    goto rebalance;
                }
            }
            // node.key > newKey: descend left; if no left child, attach there.
            walk = NodeLeft(parent);
            if (NodeLeft(parent) == 0) {
                int64_t n = reinterpret_cast<int64_t>(s_memAlloc ? s_memAlloc(0x38, 8) : nullptr);
                newNode = 0;
                if (n != 0) {
                    NodeKey(n) = 0;
                    NodeKey(n) = newKey;
                    *reinterpret_cast<uint32_t*>(n + 0x08) = newA;
                    *reinterpret_cast<uint32_t*>(n + 0x0c) = newB;
                    *reinterpret_cast<uint8_t*>(n + 0x10) = newFlag;
                    NodeLeft(n)  = 0;
                    NodeRight(n) = 0;
                    *reinterpret_cast<uint32_t*>(n + 0x30) = 0;
                    NodeParent(n) = parent;
                    newNode = n;
                }
                NodeLeft(parent) = newNode;
                goto rebalance;
            }
        }
    rebalance:
        {
            int64_t cursor = parent;
            PlayerTrade_Retrace_25920(rootSlot, &cursor);
        }
    }

    // count++
    *reinterpret_cast<int*>(param_1 + 0x68) = *reinterpret_cast<int*>(param_1 + 0x68) + 1;
}

// ============================================================
// 0x14002c690 — find a node by key (entity ptr) and remove it. 1:1 port of
// TradeWork_impl_c690. param_1 = &tree.root; param_2 = key IBP (key = *param2).
// ============================================================
extern "C" void PlayerTrade_BSTFindRemove_2c690(int64_t* root, void* keyIBP) {
    int64_t self = *root;
    if (self == 0) return;
    uint64_t key = *reinterpret_cast<uint64_t*>(keyIBP);
    do {
        if (key < NodeKey(self)) {
            self = NodeRight(self);
        } else {
            if (key <= NodeKey(self)) {
                PlayerTrade_BSTRemove_25740(root, reinterpret_cast<void*>(self));
                return;
            }
            self = NodeLeft(self);
        }
    } while (self != 0);
}

// ============================================================
// 0x1400666b0 — EntityUI helper: read the dialog controller, copy the edit/track
// flag (+0xd8 of the EntityList sub-object at param_1+0x38) into it, then add or
// remove this entity from the controller's tracking BST.
// 1:1 port of EntityUI_helper_66b0.
//   param_1 + 0x18 = DialogInstance IBP
//   param_1 + 0x38 = EntityList-ish sub-object IBP (flag at +0xd8)
//   param_1 + 0x30 = entity key IBP slot (raw ptr, used by reference)
// gameWorld tracking tree: *(g_pGameWorldSingleton+0x120) is the controller/tree obj;
//   its BST root is at +0x60.
// ============================================================
extern "C" void PlayerTrade_EntityUI_66b0(int64_t param_1) {
    // Resolve the dialog controller via DialogInstance::GetController.
    static uint32_t* s_pCtrlTypeId = nullptr;
    static bool s_tidInit = false;
    if (!s_tidInit) {
        s_pCtrlTypeId = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5788));
        s_tidInit = true;
    }
    uint32_t ctrlTypeId = s_pCtrlTypeId ? *s_pCtrlTypeId : 0;


    void* controllerRaw = nullptr;   // local_res8
    if (fn_DialogInstance_GetController) {
        void* dialog = *reinterpret_cast<void**>(param_1 + 0x18);
        fn_DialogInstance_GetController(dialog, &controllerRaw, ctrlTypeId);
    }

    void* ctrlPtr = nullptr;
    if (controllerRaw != nullptr) {
        if (fn_IsDerivedFrom && fn_IsDerivedFrom(controllerRaw, ctrlTypeId)) {
            ctrlPtr = controllerRaw;
        }
    }

    int64_t controllerIBP = 0;  // local_res10
    if (fn_InternalBasePtr_Ctor) new (&controllerIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(ctrlPtr));   // B.3.3 bridge

    // Copy the edit/track flag (+0xd8) into the controller (+0x40).
    char flag = *reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(param_1 + 0x38) + 0xd8);
    *reinterpret_cast<char*>(controllerIBP + 0x40) = flag;
    if (flag == 0 && *reinterpret_cast<char*>(controllerIBP + 0x41) != 0) {
        *reinterpret_cast<char*>(controllerIBP + 0x41) = 0;
    }

    // Add (flag set) or remove (flag clear) this entity in the world tracking BST.
    void* worldBase = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        ? *g_pGameWorldSingleton : nullptr;
    if (!worldBase) return;
    int64_t treeObj = *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(worldBase) + 0x120);

    if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(param_1 + 0x38) + 0xd8) != 0) {
        uint32_t payloadB = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x18) + 0x8c);
        uint32_t payloadA = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x18) + 0x88);
        void* keyIBP = *reinterpret_cast<void**>(param_1 + 0x30);  // local_res8 = entity key
        PlayerTrade_BSTInsert_102e30(treeObj, &keyIBP, payloadA, payloadB);
    } else {
        void* keyIBP = *reinterpret_cast<void**>(param_1 + 0x30);
        PlayerTrade_BSTFindRemove_2c690(reinterpret_cast<int64_t*>(treeObj + 0x60), &keyIBP);
    }
}

// ============================================================
// 0x140066800 — EditTool entry: set the edit/track flag (+0xd8) on the EntityList
// sub-object then run the EntityUI helper. 1:1 port of EditTool_helper_6800.
// ============================================================
extern "C" void PlayerTrade_EditTool_66800(int64_t param_1) {
    *reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x38) + 0xd8) = 1;
    PlayerTrade_EntityUI_66b0(param_1);
}
