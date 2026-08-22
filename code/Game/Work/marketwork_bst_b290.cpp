// code/Game/Work/marketwork_bst_b290.cpp
// MarketWork market-BST SERIALIZE pair — exe 0x14002b290 + 0x14002b5f0.
//
// These two are the SAVE/LOAD half of the MarketWork AVL/BST family (the insert half
// lives in marketwork_bst_a9f0.cpp, the cleanup half in work_leaf_market.cpp).
// MarketWork::Serialize (marketwork.cpp:MarketWork_Serialize_Impl) already dispatches
// to 0x14002b290 through an UNGUARDED function pointer, so until this file is
// registered that call is an EXEC-of-0 the first time a market save/load is reached.
//
// VERSION: MarketWork::Serialize constructs its IO::Serializer with version=1 and then
// branches on IO::Serializer::GetVersion:
//     version == 0 -> legacy simple-BST reader 0x1400296b0 + migration via MarketBst_a9f0
//     version != 0 -> 0x14002b290 (this file)
// So this pair implements the **v1 (current) format ONLY**. Nothing here is reachable
// for a v0 stream, and neither function contains a version test of its own.
//
// Node layout (0x38 bytes) and the on-disk record layout are documented in
// marketwork_bst_b290.h. Every offset below was read out of the disassembly, not the
// decompiler: Ghidra types the node pointer as `longlong*`/`InternalBasePtr*` and
// prints ELEMENT indices (`puVar3[4]`, `pIVar7 + 0x20`), which is the classic way this
// family gets silently mis-ported. The byte offsets confirmed from the raw
// instructions are:
//     14002b385  MOV [RBX+0x18],RDI    parent
//     14002b389  MOV [RBX+0x20],RDI    left
//     14002b38d  MOV [RBX+0x28],RDI    right
//     14002b391  MOV [RBX+0x30],EDI    balance (int32)
//     14002b36c  LEA RCX,[RBX+0x8]     payload array header
//     14002b4ad  INC dword ptr [R14+8] tree->count (int32)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/marketwork_bst_b290.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// External closure — every one of these is already ported + registered
// ============================================================

// 0x14000d9d0 (Ghidra: FUN_14000d9d0, ported as ActRm_VectorClone_d9d0)
//   deep-copies a 16-byte-stride dynamic array: dst[RCX] <- src[RDX].
// NOTE: defined as ordinary C++ (no extern "C") in code/Game/Action/action_base_rawmat.cpp,
// so it must be declared the same way here or the mangled names will not match at link.
extern void ActRm_VectorClone_d9d0(int64_t* dst, int64_t* src);

// 0x14000da70 (Ghidra: DynArray16_Resize, ported as ActRm_ArrayResize16_da70)
//   resizes a 16-byte-stride dynamic array to `count` elements.
extern void ActRm_ArrayResize16_da70(int64_t* arr, int count);

// 0x140025920 (ported as PlayerTrade_Retrace_25920) — the AVL retrace/rebalance.
//   NOTE the argument: retail passes a pointer to the PARENT node slot
//   (LEA RDX,[RBP+0x48], which at that point still holds the node the new child was
//   linked under), NOT the newly created node. See the b3a0 descent loop below.
extern "C" void PlayerTrade_Retrace_25920(int64_t* tree, int64_t* nodeSlot);

// 0x140002340 (Ghidra: Object_ExternalPtr_BuildComponentRef_Serialize)
//   serializes one ExternalPtr through the save-file string table.
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

// ============================================================
// Dispatch slots (.rdata) — resolved once
// ============================================================
// 0x140152ac8  IO::Serializer::Serialize(int*)  -- (serializer, int*, const char* name)
//              retail always passes R8D=0 for the name.
// 0x140152aa0  Object::InternalBasePtr::Serialize(this, serializer)
// 0x1401c7158  DWORD type id tested by IsDerivedFrom on the key. Ghidra labels this
//              g_BaseTypeInfo_CitizenComponent; other ports in this tree call the same
//              global "work entity type" (completetask.cpp) — the label is not
//              load-bearing here, only the DWORD value is.
// MemAlloc/MemFree are dispatch slots 0x140152b60/0x140152b68 in retail; we reach the
// same Runtime exports through the canonical fn_MemAllocStatic / fn_MemFreeStatic
// wrappers, exactly as work_leaf_market.cpp does.
typedef void (*MB_SerializeInt_t)(void* serializer, int* value, const char* name);
typedef void (*MB_IBP_Serialize_t)(void* ibp, void* serializer);

static MB_SerializeInt_t  s_fn_SerializeInt = nullptr;   // slot 0x140152ac8
static MB_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;  // slot 0x140152aa0
static uint32_t*          s_TypeId_1401c7158 = nullptr;  // DAT_1401c7158
static bool               s_resolved = false;

static void MarketBst_b290_ResolveFuncs() {
    if (s_resolved) return;
    s_resolved = true;
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<MB_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa0);
    if (slot) s_fn_IBP_Serialize = *reinterpret_cast<MB_IBP_Serialize_t*>(slot);
    s_TypeId_1401c7158 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
}

// `*(int32*)(*(void**)serializer + 8) == 0` is the engine-wide "am I loading?" test
// (0 = loading, non-zero = saving). Same idiom as decaldescription.cpp et al.
// Retail re-reads it from memory at every use site; we do the same rather than
// caching, because the read is what the original does.
static inline bool SerIsLoading(void* serializer) {
    return *reinterpret_cast<int32_t*>(*reinterpret_cast<uintptr_t*>(serializer) + 8) == 0;
}

// Payload element = 0x10 bytes: { ExternalPtr ref @+0x00, int32 value @+0x08 }.
// Stride confirmed by `ADD RBX,0x10` at 14002b65b and `ADD RSI,0x10` at 14002b57b.
static const int kPayloadStride = 0x10;

// ============================================================
// 0x14002b5f0 — MarketBst_b5f0: serialize one payload dynamic array
// ============================================================
// TRUE signature (Ghidra prints `void __fastcall(longlong*, Serializer*)`, which is
// correct for once — 2 args, void return, confirmed: only RCX/RDX are read and RAX is
// dead at RET 14002b673).
//
//   arr [RCX] : { void* ptr @+0x00, int32 count @+0x08, int32 cap @+0x0C }
//               (`MOV EAX,dword ptr [RCX+0x8]` at 14002b5fa reads the count as a DWORD)
//   serializer [RDX]
//
// Wire format: int32 count, then `count` records of { ExternalPtr ref, int32 value }.
// On LOAD the array is resized to the stored count BEFORE the element loop; on SAVE
// the resize is skipped and the existing buffer is written out.
void MarketBst_b5f0(int64_t* arr, void* serializer) {
    MarketBst_b290_ResolveFuncs();

    // 14002b5fa..b612: copy the count into a stack slot and serialize it in place.
    // The stack slot is what gets read back on load — the array's own count field is
    // NOT the serialize target; the resize below is what commits it.
    int count = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(arr) + 0x08);
    s_fn_SerializeInt(serializer, &count, nullptr);

    // 14002b618..b628
    if (SerIsLoading(serializer)) {
        ActRm_ArrayResize16_da70(arr, count);
    }

    // 14002b62d..b662: element loop. Note retail reloads the buffer pointer AFTER the
    // resize (MOV RBX,qword ptr [RBX]) — reading it earlier would use the freed buffer.
    uintptr_t p = static_cast<uintptr_t>(static_cast<uint64_t>(arr[0]));
    for (int i = 0; i < count; ++i) {
        Reimpl_ExternalPtr_Serialize_StringTable(reinterpret_cast<void*>(p), serializer);
        s_fn_SerializeInt(serializer, reinterpret_cast<int*>(p + 0x08), nullptr);
        p += kPayloadStride;
    }
}

// ------------------------------------------------------------
// Node field accessors — byte offsets, verified above.
// ------------------------------------------------------------
static inline uintptr_t NodeKey(uintptr_t n)    { return *reinterpret_cast<uintptr_t*>(n + 0x00); }
static inline uintptr_t NodeParent(uintptr_t n) { return *reinterpret_cast<uintptr_t*>(n + 0x18); }
static inline uintptr_t NodeLeft(uintptr_t n)   { return *reinterpret_cast<uintptr_t*>(n + 0x20); }
static inline uintptr_t NodeRight(uintptr_t n)  { return *reinterpret_cast<uintptr_t*>(n + 0x28); }

// 14002b35a..b391 / b408..b43f / b463..b49a — three byte-identical copies of the node
// constructor emitted by the compiler (empty-tree, right-child, left-child). Folded
// into one helper here; the stores and their WIDTHS are exact:
//   [n+0x00] = 0 (8 bytes) then = key (8 bytes)   -- retail really does write it twice
//   [n+0x08] = 0 (8 bytes), [n+0x10] = 0 (8 bytes) -- the *8-byte* zero clears BOTH the
//                                                     int32 count and the int32 cap.
//                                                     Narrowing this to 4 bytes would
//                                                     leave a stale capacity behind and
//                                                     silently strand the array.
//   ActRm_VectorClone_d9d0(n+0x08, srcArray)
//   [n+0x18] = parent (8), [n+0x20] = 0 (8), [n+0x28] = 0 (8), [n+0x30] = 0 (4)
// Returns 0 when MemAlloc fails — retail then links the NULL in anyway (see callers).
static uintptr_t MarketBst_MakeNode(uintptr_t key, int64_t* srcArray, uintptr_t parent) {
    // 14002b3ed: MOV EDX,0x8 / LEA ECX,[RDX+0x30] -> MemAlloc(size=0x38, align=8)
    void* mem = fn_MemAllocStatic(0x38, 8);
    if (mem == nullptr) return 0;
    uintptr_t n = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uintptr_t*>(n + 0x00) = 0;
    *reinterpret_cast<uintptr_t*>(n + 0x00) = key;
    *reinterpret_cast<uintptr_t*>(n + 0x08) = 0;
    *reinterpret_cast<uintptr_t*>(n + 0x10) = 0;   // 8-byte zero: count AND cap
    ActRm_VectorClone_d9d0(reinterpret_cast<int64_t*>(n + 0x08), srcArray);
    *reinterpret_cast<uintptr_t*>(n + 0x18) = parent;
    *reinterpret_cast<uintptr_t*>(n + 0x20) = 0;
    *reinterpret_cast<uintptr_t*>(n + 0x28) = 0;
    *reinterpret_cast<int32_t*>(n + 0x30)   = 0;
    return n;
}

// ============================================================
// 0x14002b290 — MarketBst_b290: serialize the whole market BST (v1)
// ============================================================
// TRUE signature: `void __fastcall(longlong* tree, Serializer*)`. Ghidra's `undefined`
// return is genuinely void here (RAX dead at the RET, 14002b5e1) and the arg count is
// genuinely 2 (R8/R9 are only ever used as scratch). This is NOT one of the family's
// under-counted prototypes — 0x14002a9f0 is (it takes 4).
//
//   tree [RCX] : { node* root @+0x00, int32 count @+0x08 }  == MarketWork+0x48
//   serializer [RDX]
void MarketBst_b290(int64_t* tree, void* serializer) {
    MarketBst_b290_ResolveFuncs();

    uintptr_t hdr = reinterpret_cast<uintptr_t>(tree);

    // 14002b2b0..b2c0: node count in/out.
    int nodeCount = *reinterpret_cast<int32_t*>(hdr + 0x08);
    s_fn_SerializeInt(serializer, &nodeCount, nullptr);

    if (SerIsLoading(serializer)) {
        // ---------------- LOAD: rebuild the tree, node by node ----------------
        // 14002b2d3..b4d0.
        // Two dead stores live here in retail (14002b2e1..b2ed store &payload into
        // RBP+0x50 and RBP+0x58 and never read either back) — inlining debris, not
        // omitted behaviour.
        for (int i = 0; i < nodeCount; ++i) {
            // 14002b2f1..b2f9: key + payload header cleared every iteration (3 qwords).
            uintptr_t key = 0;
            int64_t   payload[2] = { 0, 0 };   // { ptr, count|cap } — 16 bytes, the exact
                                               // size ActRm_VectorClone_d9d0 reads.

            // 14002b2fd..b329: read the key, then drop it unless it derives from the
            // expected type. Retail overwrites the key with 0 on a type mismatch and
            // still inserts the node — a null-keyed node, not a skipped one.
            s_fn_IBP_Serialize(&key, serializer);
            if (key != 0 && !fn_IsDerivedFrom(reinterpret_cast<void*>(key), *s_TypeId_1401c7158)) {
                key = 0;
            }

            // 14002b334: the per-node payload block.
            MarketBst_b5f0(payload, serializer);

            uintptr_t root = static_cast<uintptr_t>(static_cast<uint64_t>(tree[0]));
            if (root == 0) {
                // 14002b341..b397: empty tree. NOTE the faithful oddity — when MemAlloc
                // fails retail stores the NULL straight into the root (MOV [R14],RBX
                // with RBX=0 at 14002b394) and still bumps the count. No retrace here.
                tree[0] = static_cast<int64_t>(MarketBst_MakeNode(key, payload, 0));
                *reinterpret_cast<int32_t*>(hdr + 0x08) += 1;   // 14002b4ad
            } else {
                // 14002b39c..b4ad: unsigned-key descent, insert-if-absent, then retrace.
                uintptr_t cur = root;
                bool inserted = false;
                for (;;) {
                    // 14002b3a3: CheckPtr on the CURRENT NODE (the node pointer itself,
                    // which doubles as the address of its key field).
                    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(cur));
                    // 14002b3ac/b3b0: CMP [RBP-0x20],RAX / JC -> unsigned "key < node->key".
                    if (key < NodeKey(cur)) {
                        uintptr_t left = NodeLeft(cur);
                        if (left != 0) { cur = left; continue; }
                        // 14002b448..b49d: link a new LEFT child (null on alloc failure).
                        *reinterpret_cast<uintptr_t*>(cur + 0x20) =
                            MarketBst_MakeNode(key, payload, cur);
                        inserted = true;
                        break;
                    }
                    // 14002b3b2: CheckPtr on the KEY slot this time (&local key), not the node.
                    fn_InternalBasePtr_CheckPtr(&key);
                    // 14002b3c0/b3c3: JNC when node->key >= key -> duplicate, nothing to do.
                    if (NodeKey(cur) >= key) break;
                    uintptr_t right = NodeRight(cur);
                    if (right != 0) { cur = right; continue; }
                    // 14002b3ed..b442: link a new RIGHT child.
                    *reinterpret_cast<uintptr_t*>(cur + 0x28) =
                        MarketBst_MakeNode(key, payload, cur);
                    inserted = true;
                    break;
                }
                if (inserted) {
                    // 14002b4a1: retrace from the PARENT slot (RBP+0x48 still holds the
                    // node we linked under), NOT from the new child.
                    int64_t parentSlot = static_cast<int64_t>(cur);
                    PlayerTrade_Retrace_25920(tree, &parentSlot);
                    *reinterpret_cast<int32_t*>(hdr + 0x08) += 1;   // 14002b4ad
                }
                // Duplicate key falls through to 14002b4b1 WITHOUT bumping the count and
                // without taking ownership of the payload — it is freed just below.
            }

            // 14002b4b1..b4bf: the loaded payload buffer is always freed here. On the
            // insert paths the node owns a CLONE (d9d0), so this frees the temporary;
            // on the duplicate path it drops the record entirely. Both zero stores are
            // 8 bytes wide.
            fn_MemFreeStatic(reinterpret_cast<void*>(static_cast<uintptr_t>(
                static_cast<uint64_t>(payload[0]))));
            payload[0] = 0;
            payload[1] = 0;
        }
    } else {
        // ---------------- SAVE: in-order walk, write each node ----------------
        // 14002b4d5..b5cd.
        uintptr_t node = static_cast<uintptr_t>(static_cast<uint64_t>(tree[0]));
        if (node == 0) return;
        while (NodeLeft(node) != 0) node = NodeLeft(node);   // 14002b4e1: leftmost

        while (node != 0) {
            // 14002b506..b524: write the key, and null it out in memory if it no longer
            // derives from the expected type. This MUTATES the live tree on save —
            // faithful, and deliberate in retail (it is how stale refs get culled).
            s_fn_IBP_Serialize(reinterpret_cast<void*>(node), serializer);
            uintptr_t key = NodeKey(node);
            if (key != 0 && !fn_IsDerivedFrom(reinterpret_cast<void*>(key), *s_TypeId_1401c7158)) {
                *reinterpret_cast<uintptr_t*>(node + 0x00) = 0;
            }

            // 14002b527..b582: the payload block, written INLINE rather than by calling
            // 0x14002b5f0 — same wire format, the compiler just duplicated the body.
            int count = *reinterpret_cast<int32_t*>(node + 0x10);
            s_fn_SerializeInt(serializer, &count, nullptr);
            if (SerIsLoading(serializer)) {
                // 14002b540..b54c. Dead in this arm (we only get here when NOT loading),
                // but present in retail, so it is kept rather than "cleaned up".
                ActRm_ArrayResize16_da70(reinterpret_cast<int64_t*>(node + 0x08), count);
            }
            uintptr_t p = *reinterpret_cast<uintptr_t*>(node + 0x08);
            for (int i = 0; i < count; ++i) {
                Reimpl_ExternalPtr_Serialize_StringTable(reinterpret_cast<void*>(p), serializer);
                s_fn_SerializeInt(serializer, reinterpret_cast<int*>(p + 0x08), nullptr);
                p += kPayloadStride;
            }

            // 14002b584..b5cd: in-order successor.
            uintptr_t right = NodeRight(node);
            if (right != 0) {
                node = right;
                while (NodeLeft(node) != 0) node = NodeLeft(node);
            } else {
                uintptr_t parent = NodeParent(node);
                if (parent != 0 && NodeLeft(parent) == node) {
                    node = parent;                       // 14002b5aa
                } else {
                    while (parent != 0 && NodeRight(parent) == node) {
                        node = parent;                   // 14002b5ba
                        parent = NodeParent(parent);
                    }
                    node = NodeParent(node);             // 14002b5c6
                }
            }
        }
    }
}
