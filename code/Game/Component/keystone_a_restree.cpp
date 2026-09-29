// code/Game/Component/keystone_a_restree.cpp
// Keystone A — ResidenceComponent residence-record self-balancing BST (AVL) cluster.
//
// This is the SAME engine BST template as code/Game/Work/tradework_serialize.cpp and
// code/Game/Player/tool_base_trade.cpp, but its NODE PAYLOAD differs: instead of an
// ExternalPtr at +0x08, each node embeds a *growable array struct* (deep-copied by the
// 5ec0 helper). The tree key is the node's low uint64 at +0x00.
//
// All functions are faithful 1:1 ports of the decompiled exe helpers, verified
// offset-by-offset against the Ghidra disassembly.
//
// AVL node layout (0x38 bytes) — confirmed from 0x140017460 disasm:
//   +0x00 uint64  key
//   +0x08 ptr     array.data      \
//   +0x10 int     array.count      } 0x10-byte growable-array struct (managed by 5ec0)
//   +0x14 int     array.capacity  /
//   +0x18 ptr     parent
//   +0x20 ptr     left child
//   +0x28 ptr     right child
//   +0x30 int16   balance (= rightHeight - leftHeight)
//   +0x32 int16   height
// Tree object: param_1[0] = root node ptr, *(int*)(param_1+0x08) = node count.
//
// The src record streamed into 7460/72f0 mirrors a node's first 0x18 bytes:
//   +0x00 uint64 key   +0x08 array.data   +0x10 array.count   +0x14 array.capacity
//
// Closure:
//   0x140017230 -> 0x140015ec0, 0x1400172f0, MemFree (slot 0x140152b68)
//   0x140015ec0 -> MemAlloc (slot 0x140152b60), MemFree (slot 0x140152b68)  [leaf otherwise]
//   0x1400172f0 -> 0x140017460, 0x140014b40
//   0x140017460 -> MemAlloc (slot 0x140152b60), 0x140015ec0
//   0x140014b40 (retrace) -> 0x140015000, 0x140014ed0
//
// The retrace 0x140014b40 (TradeWork_impl_4b40) and the two rotation leaves
// 0x140015000 / 0x140014ed0 (PlayerTrade_Rot_15000 / _14ed0) are the SHARED engine
// template — already ported in code/Game/Player/tool_base_highlight.cpp and
// code/Game/Player/tool_base_trade.cpp (byte-identical decompiles). We REUSE them via
// extern decls (NOT re-defined here, to avoid duplicate symbols / drift). The rotations
// are already registered by reg_player_trade.cpp; the retrace 0x140014b40 is registered
// by THIS keystone's reg file (reg_keystone_a_restree.cpp) — its own closure (15000 +
// 14ed0) is registered, so registering it is closure-safe (it was only previously
// "held" inside reg_player_highlight because that whole batch waited on ace40, an
// UNRELATED tessellation sibling — not a callee of 14b40).

#include <cstddef>
#include <cstdint>
#include "Component/exe_bodies.h"
#include "Player/exe_bodies.h"          // TradeWork_impl_4b40 (0x140014b40)
#include "system/MemoryAllocator.h"     // System::MemoryAllocator::MemAlloc / MemFree

namespace {

// System::MemoryAllocator::MemAlloc / MemFree are the statics the IAT slots
// 0x140152b60 / 0x140152b68 (?MemAlloc@ SAPEAX_KH / ?MemFree@ SAXPEAX) point at; the
// bodies below call them by name (B3R R9).

// The node payload: a growable array of 8-byte elements (Collection::Array, 0x10).
struct RecordArray {
    uint64_t* data;       // +0x00
    int32_t   count;      // +0x08
    int32_t   capacity;   // +0x0C
};
static_assert(sizeof(RecordArray) == 0x10, "RecordArray is 0x10 (0x140015ec0)");

// The record streamed into 7460 / 72f0: a node's first 0x18 bytes.
struct Record {
    uint64_t    key;      // +0x00
    RecordArray items;    // +0x08
};
static_assert(sizeof(Record) == 0x18, "Record is 0x18 (0x140017230's stack record)");

// The AVL node (0x38 bytes, 0x140017460).
struct RecordNode {
    uint64_t    key;       // +0x00
    RecordArray items;     // +0x08
    RecordNode* parent;    // +0x18
    RecordNode* left;      // +0x20
    RecordNode* right;     // +0x28
    int16_t     balance;   // +0x30 rightHeight - leftHeight
    int16_t     height;    // +0x32
};
static_assert(sizeof(RecordNode) == 0x38, "RecordNode is 0x38 (MemAlloc(0x38) at 0x140017460)");
static_assert(offsetof(RecordNode, parent) == 0x18, "RecordNode +0x18");
static_assert(offsetof(RecordNode, left) == 0x20, "RecordNode +0x20");
static_assert(offsetof(RecordNode, right) == 0x28, "RecordNode +0x28");
static_assert(offsetof(RecordNode, balance) == 0x30, "RecordNode +0x30");

// The tree object: the root and the node count.
struct RecordTree {
    RecordNode* root;    // +0x00
    int32_t     count;   // +0x08
};

// ============================================================
// 0x140015ec0 — ResidenceComponent_helper_5ec0. Growable-array deep-copy.
// Frees dst's buffer, then copies src's count 8-byte elements into a freshly
// allocated one. Returns dst.
// ============================================================
RecordArray* CopyRecordArray(RecordArray* dst, const RecordArray* src) {
    if (dst == src) return dst;

    System::MemoryAllocator::MemFree(dst->data);   // MemFree(*param_1)
    dst->count = 0;                                // +0x08 count/capacity qword := 0
    dst->capacity = 0;
    dst->data = nullptr;                           // +0x00 data := 0
    int cnt = src->count;                          // source count
    dst->count = cnt;
    if (cnt > 0) {
        dst->capacity = cnt;
        dst->data = static_cast<uint64_t*>(
            System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(cnt) << 3, 8));   // data := MemAlloc(cnt*8,8)
    }

    // Copy `count` 8-byte elements from src into dst.
    if (dst->count > 0) {
        int i = 0;
        do {
            uint64_t* d = dst->data + i;
            if (d != nullptr) {
                *d = 0;
                *d = src->data[i];
            }
            ++i;
        } while (i < dst->count);
    }
    return dst;
}

// ============================================================
// 0x140017460 — ResidenceComponent_impl_7460. AVL node allocator.
// Allocates a 0x38-byte node, copies the key, zero-inits the embedded array struct, then
// deep-copies the source array via 5ec0, links the parent, and zeroes children + balance.
// Retail's first argument (RCX) is UNUSED by the body (verified in disasm — saved to
// stack, never read), so it is not carried.
// Returns the new node (or null on OOM).
// ============================================================
RecordNode* NewRecordNode(const Record* src, RecordNode* parent) {
    RecordNode* n = static_cast<RecordNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
    if (n != nullptr) {
        n->key = src->key;                  // +0x00 key := src key
        n->items.data = nullptr;            // +0x08 array.data := 0
        n->items.count = 0;                 // +0x10 array.count/capacity := 0
        n->items.capacity = 0;
        CopyRecordArray(&n->items, &src->items);   // 5ec0(node+0x08, src+0x08)
        n->parent = parent;                 // +0x18 parent
        n->left = nullptr;                  // +0x20 left
        n->right = nullptr;                 // +0x28 right
        n->balance = 0;                     // +0x30 balance/height (dword)
        n->height = 0;
    }
    return n;
}

// ============================================================
// 0x1400172f0 — ResidenceComponent_helper_72f0. AVL insert keyed by the node's key.
//   tree = the tree (root, count)
//   out  = receives the inserted/existing node
//   src  = the record to insert
// ============================================================
RecordNode** InsertRecord(RecordTree* tree, RecordNode** out, const Record* src) {
    RecordNode* root = tree->root;

    if (root == nullptr) {
        RecordNode* newNode = NewRecordNode(src, nullptr);
        tree->count = tree->count + 1;
        tree->root = newNode;
        *out = newNode;
        return out;
    }

    uint64_t key = src->key;
    RecordNode* cur = root;
    RecordNode* cursor;     // local_res8 — the descent node (parent of the new node)
    RecordNode* newNode = nullptr;
    for (;;) {
        // Descend left while key < node key.
        for (;;) {
            cursor = cur;
            if (!(key < cursor->key)) break;
            RecordNode* left = cursor->left;   // +0x20
            if (left == nullptr) {
                // insert as left child.
                newNode = NewRecordNode(src, cursor);
                cursor->left = newNode;
                goto rebalance;
            }
            cur = left;
        }
        if (key <= cursor->key) {
            // exact match — return existing node, no structural change.
            *out = cursor;
            return out;
        }
        // descend right (+0x28).
        cur = cursor->right;
        if (cur == nullptr) {
            newNode = NewRecordNode(src, cursor);
            cursor->right = newNode;
            goto rebalance;
        }
    }

rebalance:
    {
        RecordNode* retraceCursor = cursor;    // &local_res8 (the descent node)
        // LEFT: the shared retrace takes the tree and its cursor as int64_t* (PLAYER's body).
        TradeWork_impl_4b40(reinterpret_cast<int64_t*>(tree),
                            reinterpret_cast<int64_t*>(&retraceCursor));   // 0x140014b40
        tree->count = tree->count + 1;
        *out = newNode;
    }
    return out;
}

} // namespace

// ============================================================
// 0x140017230 — ResidenceComponent_impl_7230. 3-call wrapper:
//   local = {0,0}; 5ec0(&local, param_4); 72f0(param_1, param_2, &keyRecord); MemFree(local.data);
// where keyRecord on the stack is { *param_3 (key), local.data, local.count/capacity }.
// The deep-copied array (built by 5ec0 into the record) becomes the record's +0x08
// array struct, which 72f0->7460->5ec0 then deep-copies AGAIN into the node; the
// temporary is freed here. Returns param_2.
// ============================================================
extern "C" void* ResidenceComponent_impl_7230(int64_t* param_1, void** param_2,
                                              uint64_t* param_3, int64_t* param_4) {
    // LEFT: the caller (zonecomponent.cpp) hands the tree / out / array as untyped words.
    RecordTree* tree = reinterpret_cast<RecordTree*>(param_1);
    RecordNode** out = reinterpret_cast<RecordNode**>(param_2);
    const RecordArray* items = reinterpret_cast<const RecordArray*>(param_4);

    // Stack record: local_20 (key) / local_18 (array.data) / local_10 (array.count/cap).
    Record rec;
    rec.key = *param_3;
    rec.items.data = nullptr;
    rec.items.count = 0;
    rec.items.capacity = 0;

    CopyRecordArray(&rec.items, items);   // 5ec0(&local_18, param_4)
    InsertRecord(tree, out, &rec);        // 72f0(param_1, param_2, &local_20)
    System::MemoryAllocator::MemFree(rec.items.data);   // free the temporary array buffer
    return param_2;
}
