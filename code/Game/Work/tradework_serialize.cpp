// code/Game/Work/tradework_serialize.cpp
// Keystone B tail — TradeWork (CitizenComponent work-tracking) AVL serialize cascade.
//
// These exe sub-helpers are called by name from TradeWork_Serialize.
#include "Work/exe_bodies.h"
// Registration point: RegisterWorkTailTradeBatch in
// standalone/reg_work_tail_trade.cpp.
//
// All functions are faithful 1:1 ports of the decompiled exe helpers (Ghidra
// disassembly-verified, offset by offset).
//
// AVL node layout (0x38 bytes) — IDENTICAL to code/Game/Player/tool_base_trade.cpp
// (the same engine BST template; that file ports the rotation/retrace/remove leaves):
//   +0x00 uint64  key (entity / Object IBP value at IBP+0)
//   +0x08 ExternalPtr  (serialized via fn_ExternalPtr_BuildSerialize = 0x140002340)
//   +0x10 int     payload (node-local int field)
//   +0x18 ptr     parent
//   +0x20 ptr     left child
//   +0x28 ptr     right child
//   +0x30 int16   balance (= rightHeight - leftHeight)
//   +0x32 int16   height
// Tree object: param_1[0] = root node ptr, param_1[+0x08] = node count (int).
//
// The IBP record streamed for each node is a 0x18-byte triple:
//   +0x00 Object IBP (key/obj)   +0x08 ExternalPtr   +0x10 int
// matching node fields +0x00 / +0x08 / +0x10 (c710 copies param_3[0..0x10]).
//
// Type-id global (reads 0 until filled after Engine::Startup):
//   DWORD_1401c7158 = "CitizenComponent" (node-key object validation in c8a0).
//
// Closure (all sub-callees registered/available):
//   0x14002c8a0 -> 0x14002c710, fn_ExternalPtr_BuildSerialize (0x140002340, registered),
//                  InternalBasePtr::Serialize, IsDerivedFrom, Ser-int slot (engine_funcs)
//   0x14002c710 -> 0x14002c830 (ported here), 0x140025920 retrace (registered via
//                  reg_player_trade.cpp), static MemAlloc, InternalBasePtr CheckPtr
//   0x14002c830 -> static MemAlloc (leaf)

#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "Game/exe_bodies.h"        // B3R: 0x140002340
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()

namespace {


// AVL node field accessors (0x38-byte node; offsets disasm-verified).
inline uint64_t& NodeKey(int64_t n)    { return *reinterpret_cast<uint64_t*>(n + 0x00); }
inline int64_t&  NodeParent(int64_t n) { return *reinterpret_cast<int64_t*>(n + 0x18); }
inline int64_t&  NodeLeft(int64_t n)   { return *reinterpret_cast<int64_t*>(n + 0x20); }
inline int64_t&  NodeRight(int64_t n)  { return *reinterpret_cast<int64_t*>(n + 0x28); }

} // namespace

// The retrace/rebalance leaf (0x140025920) is ported in tool_base_trade.cpp and
// registered there; declare it for direct reuse from c710's insert.
extern "C" void PlayerTrade_Retrace_25920(int64_t* root, int64_t* cursor); // 0x140025920


// ============================================================
// 0x14002c830 — c710 node-alloc sub-helper. Allocates a 0x38-byte AVL node, copies
// the 0x18-byte IBP record (key/ExternalPtr/int) from `src`, links `parent`, and zeroes
// the child/balance fields. 1:1 port of TradeWork_helper_c710_sub_c830.
//   param_1 (RCX) is unused by the body; src=RDX, parent=R8.
// Returns the new node (or null on OOM).
// ============================================================
extern "C" void* TradeWork_NodeAlloc_c830(void* /*unused*/, uint64_t* src, int64_t parent) {
    uint64_t* n = reinterpret_cast<uint64_t*>(System::MemoryAllocator::MemAlloc(0x38, 8));
    if (n == nullptr) return nullptr;
    n[0] = 0;
    n[0] = src[0];   // +0x00 key
    n[1] = src[1];   // +0x08 ExternalPtr
    n[2] = src[2];   // +0x10 int payload (full qword copied as in disasm)
    *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x18) = parent; // +0x18 parent
    *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x20) = 0;      // +0x20 left
    *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x28) = 0;      // +0x28 right
    *reinterpret_cast<uint32_t*>(reinterpret_cast<int64_t>(n) + 0x30) = 0;     // +0x30 bal/height
    return n;
}

// ============================================================
// 0x14002c710 — AVL insert keyed by the node's low uint64 key. 1:1 port of
// TradeWork_helper_c710.
//   param_1 = &tree (root at param_1[0], count int at param_1+0x08)
//   param_2 = out (receives inserted/existing node ptr)
//   param_3 = IBP record to insert (0x18-byte triple; key = *param_3)
// ============================================================
extern "C" void* TradeWork_Insert_c710(int64_t* param_1, void** param_2, int64_t param_3) {
    int64_t root = *param_1;

    if (root == 0) {
        // Empty tree: allocate the root node inline (matches the decompile's MemAlloc
        // here rather than via the c830 sub-helper).
        uint64_t* n = reinterpret_cast<uint64_t*>(System::MemoryAllocator::MemAlloc(0x38, 8));
        void* newNode = nullptr;
        if (n != nullptr) {
            n[0] = 0;
            n[0] = *reinterpret_cast<uint64_t*>(param_3 + 0x00);   // +0x00 key
            n[1] = *reinterpret_cast<uint64_t*>(param_3 + 0x08);   // +0x08 ExternalPtr
            n[2] = *reinterpret_cast<uint64_t*>(param_3 + 0x10);   // +0x10 int
            *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x18) = 0; // parent
            *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x20) = 0; // left
            *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(n) + 0x28) = 0; // right
            *reinterpret_cast<uint32_t*>(reinterpret_cast<int64_t>(n) + 0x30) = 0;
            newNode = n;
        }
        *param_1 = reinterpret_cast<int64_t>(newNode);
        *param_2 = newNode;
    } else {
        uint64_t key = *reinterpret_cast<uint64_t*>(param_3 + 0x00);
        int64_t cur = root;
        int64_t cursor;     // local_res8 — descent node (parent of the new node)
        void* newNode;
        for (;;) {
            cursor = cur;
            if (key < NodeKey(cur)) {
                // descend LEFT (+0x20)
                int64_t left = NodeLeft(cur);
                if (left == 0) {
                    // c830(RCX=cursor[unused], RDX=param_3, R8=cursor[parent]).
                    newNode = TradeWork_NodeAlloc_c830(reinterpret_cast<void*>(cursor),
                                                       reinterpret_cast<uint64_t*>(param_3),
                                                       cursor);
                    NodeLeft(cursor) = reinterpret_cast<int64_t>(newNode);
                    break;
                }
                cur = left;
                continue;
            }
            if (key <= NodeKey(cursor)) {
                // exact match — return the existing node, no structural change.
                *param_2 = reinterpret_cast<void*>(cursor);
                return param_2;
            }
            // descend RIGHT (+0x28)
            int64_t right = NodeRight(cursor);
            if (right == 0) {
                newNode = TradeWork_NodeAlloc_c830(reinterpret_cast<void*>(cursor),
                                                   reinterpret_cast<uint64_t*>(param_3),
                                                   cursor);
                NodeRight(cursor) = reinterpret_cast<int64_t>(newNode);
                break;
            }
            cur = right;
        }
        // Rebalance from the descent node (parent of the new node) up to the root.
        // Matches the disasm: retrace cursor = &local_res8 (the parent), not the new node.
        int64_t retraceCursor = cursor;
        PlayerTrade_Retrace_25920(param_1, &retraceCursor);
        *param_2 = newNode;
    }

    // count++
    *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) =
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) + 1;
    return param_2;
}

// ============================================================
// 0x14002c8a0 — TradeWork BST serialize (in-order). 1:1 port of TradeWork_helper_c8a0.
//   param_1 = &tree (root at param_1[0], count int at param_1+0x08)
//   param_2 = IO::Serializer*
// Read mode: *(int*)(*(int64*)param_2 + 8) == 0.
// ============================================================
extern "C" void TradeWork_Serialize_c8a0(int64_t* param_1, void* param_2) {
    // Serialize the node count (read or write).
    int32_t count = *reinterpret_cast<int32_t*>(reinterpret_cast<int64_t>(param_1) + 0x08);
    reinterpret_cast<IO::Serializer*>(param_2)->Serialize(*reinterpret_cast<int*>(&count), nullptr);   // B.3.4 bridge

    bool readMode = (static_cast<int>(reinterpret_cast<IO::Serializer*>(param_2)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)

    if (readMode) {
        // ---- read: deserialize `count` records, rebuild the tree ----
        for (int i = 0; i < count; ++i) {
            // 0x18-byte IBP record on the stack: obj/IBP +0x00, ExternalPtr +0x08, int +0x10.
            uint64_t rec[3];
            rec[0] = 0; rec[1] = 0; rec[2] = 0;

            reinterpret_cast<Object::InternalBasePtr*>(&rec[0])->Serialize(*reinterpret_cast<IO::Serializer*>(param_2));   // B.3.4 bridge
            // Validate the deserialized object against CitizenComponent (DWORD_1401c7158,
            // ENDGAME B.3.9 D5: by cell, read directly).
            if (rec[0] != 0 &&
                !Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(rec[0]), CitizenComponent::TypeId())) {
                rec[0] = 0;
            }
            // int payload at record +0x10.
            reinterpret_cast<IO::Serializer*>(param_2)->Serialize(*reinterpret_cast<int*>(&rec[2]), nullptr);   // B.3.4 bridge
            // ExternalPtr at record +0x08 (0x140002340 = fn_ExternalPtr_BuildSerialize).
            Reimpl_ExternalPtr_Serialize_StringTable(&rec[1], param_2);

            // Insert into the tree.
            void* out = nullptr;
            TradeWork_Insert_c710(param_1, &out, reinterpret_cast<int64_t>(&rec[0]));
        }
    } else {
        // ---- write: walk the AVL in-order, serializing each node ----
        int64_t node = *param_1;
        if (node == 0) return;
        // Descend to the leftmost (minimum) node.
        int64_t n = node;
        do {
            node = n;
            n = NodeLeft(node);
        } while (n != 0);

        while (node != 0) {
            // Serialize this node's IBP (+0x00), int (+0x10), ExternalPtr (+0x08).
            reinterpret_cast<Object::InternalBasePtr*>(reinterpret_cast<void*>(node))->Serialize(*reinterpret_cast<IO::Serializer*>(param_2));   // B.3.4 bridge
            if (*reinterpret_cast<int64_t*>(node) != 0 &&
                !Object::IsDerivedFromNullSafe(*reinterpret_cast<void**>(node), CitizenComponent::TypeId())) {
                *reinterpret_cast<int64_t*>(node) = 0;
            }
            reinterpret_cast<IO::Serializer*>(param_2)->Serialize(*reinterpret_cast<int*>(node + 0x10), nullptr);   // B.3.4 bridge
            Reimpl_ExternalPtr_Serialize_StringTable(reinterpret_cast<void*>(node + 0x08), param_2);

            // In-order successor.
            int64_t right = NodeRight(node);
            if (right != 0) {
                // leftmost of the right subtree
                int64_t s = right;
                do {
                    node = s;
                    s = NodeLeft(node);
                } while (s != 0);
            } else {
                // climb until we come up from a left child
                int64_t parent = NodeParent(node);
                if (parent == 0) {
                    node = 0;
                } else if (NodeLeft(parent) == node) {
                    node = parent;
                } else {
                    // climb while node is a right child
                    int64_t p = parent;
                    while (p != 0 && NodeRight(p) == node) {
                        node = p;
                        p = NodeParent(p);
                    }
                    node = NodeParent(node);
                }
            }
        }
    }
}
