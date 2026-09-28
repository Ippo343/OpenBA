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

#include "resolve.h"
#include <cstdint>
#include "Component/exe_bodies.h"

// Shared AVL retrace/rebalance — 0x140014b40, ported in tool_base_highlight.cpp.
//   param_1 = &tree (root at [0]); param_2 = &cursor (start node; climbs to root).
extern "C" void TradeWork_impl_4b40(int64_t* param_1, int64_t* param_2);

namespace {

// ---- static System::MemoryAllocator MemAlloc/MemFree (2-arg / 1-arg static forms,
// via IAT slots 0x140152b60 / 0x140152b68, ResolveExport fallback) — mirrors
// code/Game/Work/tradework_serialize.cpp + code/Game/Player/tool_base_trade.cpp. ----
typedef void* (*MemAllocStatic_t)(uint64_t size, uint32_t align);  // real export is SAPEAX_KH (uint64 size, int align); SAPEAXII does not exist
typedef void  (*MemFreeStatic_t)(void* ptr);                       // SAXPEAX
MemAllocStatic_t s_memAlloc = nullptr;
MemFreeStatic_t  s_memFree  = nullptr;
bool s_memInit = false;

inline uintptr_t slot(uintptr_t a) {
    uintptr_t s = ResolveAddr(Module::Exe, a);
    return s ? *reinterpret_cast<uintptr_t*>(s) : 0;
}

void EnsureMem() {
    if (s_memInit) return;
    s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x140152b60));
    if (!s_memAlloc)
        s_memAlloc = reinterpret_cast<MemAllocStatic_t>(
            ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    if (!s_memAlloc)   // proven-working overload (size in RCX / align in RDX — ABI-compatible)
        s_memAlloc = reinterpret_cast<MemAllocStatic_t>(
            ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x140152b68));
    if (!s_memFree)
        s_memFree = reinterpret_cast<MemFreeStatic_t>(
            ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_memInit = true;
}

inline void* MemAlloc(uint64_t size, uint32_t align) {
    return s_memAlloc ? s_memAlloc(size, align) : nullptr;
}
inline void MemFree(void* p) {
    if (s_memFree) s_memFree(p);
}

// AVL node field accessors (0x38-byte node; offsets disasm-verified).
inline uint64_t& NodeKey(int64_t n)    { return *reinterpret_cast<uint64_t*>(n + 0x00); }
inline int64_t&  NodeParent(int64_t n) { return *reinterpret_cast<int64_t*>(n + 0x18); }
inline int64_t&  NodeLeft(int64_t n)   { return *reinterpret_cast<int64_t*>(n + 0x20); }
inline int64_t&  NodeRight(int64_t n)  { return *reinterpret_cast<int64_t*>(n + 0x28); }

} // namespace

// ============================================================
// 0x140015ec0 — ResidenceComponent_helper_5ec0. Growable-array deep-copy.
// Frees *param_1, then copies param_2's count 8-byte elements into a freshly allocated
// buffer. Array struct: +0x00 data ptr, +0x08 count(int), +0x0C capacity(int).
//   if(param_1 != param_2){ MemFree(*param_1); param_1[1]=0; *param_1=0;
//                           cnt=(int)param_2[1]; *(int*)(param_1+1)=cnt;
//                           if(cnt>0){ *(int*)((char*)param_1+0xC)=cnt;
//                                      *param_1=MemAlloc(cnt<<3,8); }
//                           loop copy 8-byte elems }
// Returns param_1.
// ============================================================
extern "C" int64_t* ResidenceComponent_helper_5ec0(int64_t* param_1, int64_t* param_2) {
    EnsureMem();
    if (param_1 == param_2) return param_1;

    MemFree(reinterpret_cast<void*>(*param_1));   // MemFree(*param_1)
    param_1[1] = 0;                               // +0x08 count/capacity qword := 0
    *param_1 = 0;                                 // +0x00 data := 0
    int cnt = static_cast<int>(param_2[1]);       // (int)param_2[1] = source count (+0x08 low)
    *reinterpret_cast<int*>(param_1 + 1) = cnt;   // +0x08 count := cnt
    if (cnt > 0) {
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0xC) = cnt; // +0x0C capacity
        *param_1 = reinterpret_cast<int64_t>(
            MemAlloc(static_cast<uint64_t>(cnt) << 3, 8));                       // data := MemAlloc(cnt*8,8)
    }

    // Copy `count` 8-byte elements from *param_2 into *param_1.
    if (static_cast<int>(param_1[1]) > 0) {
        uint64_t off = 0;
        int i = 0;
        do {
            uint64_t* dst = reinterpret_cast<uint64_t*>(*param_1 + static_cast<int64_t>(off));
            if (dst != nullptr) {
                int64_t srcData = *param_2;
                *dst = 0;
                *dst = *reinterpret_cast<uint64_t*>(off + srcData);
            }
            ++i;
            off += 8;
        } while (i < static_cast<int>(param_1[1]));
    }
    return param_1;
}

// ============================================================
// 0x140017460 — ResidenceComponent_impl_7460. AVL node allocator.
// Allocates a 0x38-byte node, copies the key, zero-inits the embedded array struct, then
// deep-copies the source array via 5ec0, links the parent, and zeroes children + balance.
//   param_1 (RCX) is UNUSED by the body (verified in disasm — saved to stack, never read).
//   param_2 (RDX) = src record (key at +0x00, array struct at +0x08).
//   param_3 (R8)  = parent node ptr.
// Returns the new node (or null on OOM).
// ============================================================
extern "C" void* ResidenceComponent_impl_7460(int64_t /*unused*/, int64_t* param_2, int64_t param_3) {
    EnsureMem();
    int64_t* n = reinterpret_cast<int64_t*>(MemAlloc(0x38, 8));
    if (n != nullptr) {
        n[0] = *param_2;        // +0x00 key := src key
        n[1] = 0;               // +0x08 array.data/count := 0
        n[2] = 0;               // +0x10 array.count/capacity := 0
        // 5ec0(node+0x08, src+0x08) — deep-copy the embedded array struct.
        ResidenceComponent_helper_5ec0(n + 1, param_2 + 1);
        n[3] = param_3;         // +0x18 parent
        n[4] = 0;               // +0x20 left
        n[5] = 0;               // +0x28 right
        *reinterpret_cast<int*>(n + 6) = 0;  // +0x30 balance/height (dword)
    }
    return n;
}

// ============================================================
// 0x1400172f0 — ResidenceComponent_helper_72f0. AVL insert keyed by the node's low uint64.
//   param_1 = &tree (root at param_1[0], count int at param_1+0x08)
//   param_2 = out (receives inserted/existing node ptr)
//   param_3 = src record to insert (key = *param_3)
// 1:1 port. Note: 7460's arg1 is ignored, so the empty-vs-nonempty arg1 difference in the
// decompile is immaterial (passes param_1 when empty, key when non-empty — both unused).
// ============================================================
extern "C" void** ResidenceComponent_helper_72f0(int64_t* param_1, void** param_2, uint64_t* param_3) {
    int64_t root = *param_1;

    if (root == 0) {
        void* newNode = ResidenceComponent_impl_7460(reinterpret_cast<int64_t>(param_1),
                                                     reinterpret_cast<int64_t*>(param_3), 0);
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) =
            *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) + 1;  // count++
        *param_1 = reinterpret_cast<int64_t>(newNode);
        *param_2 = newNode;
        return param_2;
    }

    uint64_t key = *param_3;
    int64_t cur = root;
    int64_t cursor;     // local_res8 — the descent node (parent of the new node)
    void* newNode = nullptr;
    for (;;) {
        // Descend left while key < node key.
        for (;;) {
            cursor = cur;
            if (!(key < NodeKey(cursor))) break;
            int64_t left = NodeLeft(cursor);   // +0x20
            if (left == 0) {
                // insert as left child. 7460(arg1=key[unused], src=param_3, parent=cursor).
                newNode = ResidenceComponent_impl_7460(static_cast<int64_t>(key),
                                                       reinterpret_cast<int64_t*>(param_3), cursor);
                NodeLeft(cursor) = reinterpret_cast<int64_t>(newNode);
                goto rebalance;
            }
            cur = left;
        }
        if (key <= NodeKey(cursor)) {
            // exact match — return existing node, no structural change.
            *param_2 = reinterpret_cast<void*>(cursor);
            return param_2;
        }
        // descend right (+0x28).
        cur = NodeRight(cursor);
        if (cur == 0) {
            newNode = ResidenceComponent_impl_7460(static_cast<int64_t>(key),
                                                   reinterpret_cast<int64_t*>(param_3), cursor);
            NodeRight(cursor) = reinterpret_cast<int64_t>(newNode);
            goto rebalance;
        }
    }

rebalance:
    {
        int64_t retraceCursor = cursor;    // &local_res8 (the descent node)
        TradeWork_impl_4b40(param_1, &retraceCursor);  // shared retrace (0x140014b40)
        *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) =
            *reinterpret_cast<int*>(reinterpret_cast<int64_t>(param_1) + 0x08) + 1;  // count++
        *param_2 = newNode;
    }
    return param_2;
}

// ============================================================
// 0x140017230 — ResidenceComponent_impl_7230. 3-call wrapper:
//   local = {0,0}; 5ec0(&local, param_4); 72f0(param_1, param_2, &keyRecord); MemFree(local.data);
// where keyRecord on the stack is { *param_3 (key), local.data, local.count/capacity },
// i.e. local_20=*param_3, local_18=array.data, local_10=array.count/capacity. The deep-copied
// array (built by 5ec0 into local_18/local_10) becomes the record's +0x08 array struct, which
// 72f0->7460->5ec0 then deep-copies AGAIN into the node; the temporary is freed here.
// Returns param_2.
// ============================================================
extern "C" void* ResidenceComponent_impl_7230(int64_t* param_1, void** param_2,
                                              uint64_t* param_3, int64_t* param_4) {
    EnsureMem();
    // Stack record: local_20 (key) / local_18 (array.data) / local_10 (array.count/cap).
    uint64_t rec[3];
    rec[0] = *param_3;   // local_20 = *param_3 (key)
    rec[1] = 0;          // local_18 = array.data
    rec[2] = 0;          // local_10 = array.count/capacity

    // 5ec0(&local_18, param_4) — note the array struct begins at local_18 (rec+1).
    ResidenceComponent_helper_5ec0(reinterpret_cast<int64_t*>(&rec[1]), param_4);
    // 72f0(param_1, param_2, &local_20) — the full record (key at rec[0], array at rec[1..]).
    ResidenceComponent_helper_72f0(param_1, param_2, &rec[0]);
    // MemFree(local_18) — free the temporary array buffer.
    MemFree(reinterpret_cast<void*>(rec[1]));
    return param_2;
}
