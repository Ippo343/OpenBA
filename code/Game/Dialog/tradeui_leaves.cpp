// code/Game/Dialog/tradeui_leaves.cpp
// Wave A leaves — the small exe helpers code/Game/Dialog/tradeui.cpp calls that were
// never ported, and which keep the TradeUI promotion gated (DepsReady() in
// ui_dialog_stub_factories.cpp refuses to wire the real slots while any is null).
//
// Leaves land here as they are ported; the ones still outstanding are listed in the
// status block at the BOTTOM of this file, which is the single place to update. (This
// header deliberately does not name a count — a comment that states today's tally
// becomes a lie the moment someone adds a function, which is the same trap that hid
// the missing river-polyline copy in worldgen.cpp for a month.)
//
// The gate needs ALL ten leaves, so landing a subset changes nothing observable yet:
// the panel stays stubbed and the gate keeps printing what is still missing. That is
// intended — partial progress must not read as success.
//
// ── The three clears ─────────────────────────────────────────────────────────
//   0x140016050  fn_DynArray_Clear_0x20      (sell entries, stride 0x20)
//   0x140016130  fn_DynArray_Clear_0x18_Buy  (buy entries,  stride 0x18)
//   0x140016200  fn_DynArray_Clear_0x18      (storage/order entries, stride 0x18)
//
// All three are the SAME generic array-resize body with newCount folded to 0 by the
// compiler. Decompiled (2026-07-30) they look substantial — a realloc branch, a
// copy/zero loop, a free — but every one of those paths is provably dead at
// newCount == 0:
//
//   iVar1 = count;  if (count != 0) {
//       old = data;  count = 0;
//       if (cap < 0)            { ... MemAlloc(0,8) ... }   // cap is never negative
//       iVar3 = (old == data) ? count_old : 0;              // data unchanged -> old count
//       if (iVar3 < 0) { ...copy/zero loop... }             // old count is never negative
//       if (old != data) MemFree(old);                      // data unchanged -> never
//   }
//
// So the entire observable effect is `count = 0`. Capacity and the buffer are
// deliberately RETAINED (this is a vector::clear(), not a shrink) — which is why the
// element strides differ between the three yet none of them appears below: the stride
// only ever mattered to the dead grow/copy path.
//
// Header layout, shared by all three (the standard engine dynamic-array header):
//   +0x00 void* data
//   +0x08 int32 count
//   +0x0c int32 capacity
//
// ⚠ If a future change makes any caller pass a non-zero newCount, these are NOT the
// right functions to extend — the real generic resize (with the live grow/copy path)
// already exists in the tree as Reimpl_Array_Resize_* in serialize_resize_helpers.cpp.
// These three are the clear-only specialisations retail emitted separately.
#include "resolve.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "Dialog/dialog_array_bridge.h"
#include <cstdint>
#include <new>

// ── B.3.2: the three clears are Collection::Array<T>::Clear() ────────────────
// `Clear()` is `SetSize(0)`, and rtcoll::SetSizeCore at newSize == 0 does
// exactly what the banner above says the retail bodies do and no more: the
// `newSize == oldSize` early return covers the `count != 0` test, nothing is
// reallocated (0 is never > capacity), the copy/fill loop does not run, and the
// blob element's spec has `destroy == nullptr` so no element is torn down. The
// buffer and the capacity are retained. The strides, which mattered only to the
// dead grow path, are now the element widths — kept per entry point so the type
// says which array each one is.

// 0x140016050 — clear the sell-entry array (stride 0x20).
extern "C" void TradeUI_DynArrayClear_16050(int64_t* header) {
    if (!header) return;                     // retail derefs unconditionally; callers
                                             // always pass &self->field, so this guard
                                             // is standalone-only belt-and-braces.
    DlgArray<0x20>(reinterpret_cast<uintptr_t>(header), 0).Clear();
}

// 0x140016130 — clear the buy-entry array (stride 0x18).
extern "C" void TradeUI_DynArrayClear_16130(int64_t* header) {
    if (!header) return;                     // see 16050
    DlgArray<0x18>(reinterpret_cast<uintptr_t>(header), 0).Clear();
}

// 0x140016200 — clear the storage/order-entry array (stride 0x18).
extern "C" void TradeUI_DynArrayClear_16200(int64_t* header) {
    if (!header) return;                     // see 16050
    DlgArray<0x18>(reinterpret_cast<uintptr_t>(header), 0).Clear();
}

// ── 0x140015fd0 — fn_Building_GetOrderName ───────────────────────────────────
// Returns (by hidden out-param, MSVC by-value String convention) the name String of
// the CURRENTLY SELECTED order, or an empty String when nothing is selected.
//
//   tradeState + 0x2c  int32  selected order index; NEGATIVE means "no selection"
//   tradeState + 0x30  void*  order array base, element stride 0x20, String at +0x00
//
// Retail (FUN_140015fd0):
//   if (idx < 0)  *(uint64*)out = 0;                       // raw 8-byte zero, NOT a
//                                                          // default-ctor call
//   else          String::String(out, &orderArray[idx]);   // copy-ctor
//   return out;
//
// A System::String is 8 bytes here, which is why the empty case is a single zeroed
// qword and why the copy is a real copy-ctor (it refcounts / duplicates the buffer)
// rather than a memcpy. fn_String_Ctor_Copy resolves
// "??0String@System@@QEAA@AEBV01@@Z" — ordinal 412, verified against
// `objdump -p Runtime-x64.dll` rather than assumed, because a mistyped mangling
// silently falls back to a wrong ordinal (see the CitizenList 2.9ms regression).
//
// Returning `out` matters: callers use the return value, not their own buffer
// pointer, so dropping the return would leave them reading an uninitialised local.
extern "C" void* TradeUI_GetOrderName_15fd0(int64_t tradeState, void* out) {
    if (!out) return out;
    const int32_t idx = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(tradeState) + 0x2c);
    if (idx < 0) {
        new (out) System::String();
        return out;
    }
    const uintptr_t base = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uint8_t*>(tradeState) + 0x30);
    if (!base) {      // standalone guard: retail derefs blind
        new (out) System::String();
        return out;
    }
    const System::String* src = reinterpret_cast<const System::String*>(
        base + static_cast<uintptr_t>(idx) * 0x20);
    new (out) System::String(*src);
    return out;
}

// ── 0x140059490 — fn_Building_AddPurchaseOrder ───────────────────────────────
// TOGGLES a resource in the purchase-order list of the CURRENTLY SELECTED order.
//
//   tradeState + 0x2c  int32  selected order index
//   tradeState + 0x30  void*  order array base, element stride 0x20
//
// and within the selected order record (base + idx*0x20):
//   rec + 0x10  int64*  resource-entity pointer array (element stride 8)
//   rec + 0x18  int32   count
//   rec + 0x1c  int32   capacity
//
// The name says "Add" but retail (FUN_140059490) is a toggle, and the branch is the
// part worth reading twice:
//
//   present = (resourceEntity is somewhere in arr[0..count))
//   if (addFlag == 0 || present)  erase EVERY occurrence of resourceEntity
//   else                          append resourceEntity
//
// So calling it with addFlag=1 on a resource that is already listed REMOVES it. A
// port that read the flag as "1 means add" and appended unconditionally would give
// a purchase list that silently grows duplicates — a believable-looking list that
// is wrong, not a crash. Verified against the disassembly at 0x1400594e6:
//   TEST R8B,R8B ; JZ erase      (addFlag == 0  -> erase)
//   TEST AL,AL   ; JNZ erase     (present       -> erase)
//
// The erase is a plain remove-all: shift the tail down one, decrement count, then
// RE-TEST the same index (retail does `DEC ESI` / `SUB R9,8` immediately before the
// unconditional `INC ESI` / `ADD R9,8`, which is how it avoids skipping an element
// that shifted into the hole).
//
// Growth on append is the engine's standard amortized rule, matching GrowTo in
// code/Game/Player/tool_base_render.cpp: capacity = newCount, then newCount*3 when
// the array was non-empty. The allocator call is the STATIC two-arg form —
// disassembly at 0x140059533 is `MOVSXD RCX,cap ; MOV EDX,8 ; SHL RCX,3 ; CALL
// [0x140152b60]`, i.e. MemAlloc(cap * 8, 8) with NO hidden header byte. That was
// checked against the disassembly rather than trusted from the decompile, because a
// mis-sized allocation here would corrupt the neighbouring order record instead of
// faulting.
extern "C" void TradeUI_AddPurchaseOrder_59490(int64_t tradeState,
                                               int64_t resourceEntity,
                                               int8_t  addFlag) {
    if (!tradeState) return;                 // standalone-only; retail derefs blind.
    const int32_t orderIdx = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(tradeState) + 0x2c);
    const uintptr_t orderBase = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uint8_t*>(tradeState) + 0x30);
    // Retail indexes unconditionally; a negative index (no order selected) would read
    // behind the array. Callers gate on the selection, so this only guards standalone.
    if (orderIdx < 0 || !orderBase) return;

    uint8_t* rec = reinterpret_cast<uint8_t*>(orderBase) +
                   static_cast<uintptr_t>(orderIdx) * 0x20;
    int64_t** items = reinterpret_cast<int64_t**>(rec + 0x10);
    int32_t*  count = reinterpret_cast<int32_t*>(rec + 0x18);
    int32_t*  cap   = reinterpret_cast<int32_t*>(rec + 0x1c);

    bool present = false;
    if (*items) {
        for (int32_t i = 0; i < *count; ++i) {
            if ((*items)[i] == resourceEntity) { present = true; break; }
        }
    }

    if (addFlag == 0 || present) {
        // Remove every occurrence, compacting in place.
        if (!*items) return;
        for (int32_t i = 0; i < *count; ) {
            if ((*items)[i] == resourceEntity) {
                for (int32_t j = i; j + 1 < *count; ++j) (*items)[j] = (*items)[j + 1];
                --*count;
                continue;                    // re-test i: the tail shifted into it
            }
            ++i;
        }
        return;
    }

    // Append.
    // B.3.2 NOT CONVERTED to Collection::Array<T>::SetSize: the `if (!grown)`
    // arm below restores count and cap and returns, and `SetSize` has no
    // failure path to hang that on — rtcoll::SetSizeCore commits the new size
    // before it allocates, exactly as retail does. The guard is standalone-only
    // (retail never checks) and deleting it to fit the template would be a
    // behaviour change in the direction of a wild write, so this growth stays
    // hand-rolled until the guard can go.
    const int32_t oldCount = *count;
    const int32_t oldCap   = *cap;
    const int32_t newCount = oldCount + 1;
    int64_t* oldItems = *items;
    *count = newCount;
    if (*cap < newCount) {
        *cap = newCount;
        if (oldCount != 0) *cap = newCount * 3;
        int64_t* grown = fn_MemAllocStatic
            ? static_cast<int64_t*>(fn_MemAllocStatic(
                  static_cast<uint64_t>(*cap) * 8, 8))
            : nullptr;
        if (!grown) {                        // standalone-only: retail never checks.
            *count = oldCount;               // leave the array exactly as we found it
            *cap   = oldCap;
            return;
        }
        *items = grown;
    }
    for (int32_t i = (oldItems == *items) ? oldCount : 0; i < newCount; ++i) {
        (*items)[i] = (i < oldCount) ? oldItems[i] : resourceEntity;
    }
    if (oldItems != *items && fn_MemFreeStatic) fn_MemFreeStatic(oldItems);
}

// ── 0x14000e2c0 / 0x14000e1c0 — the storage AVL insert ───────────────────────
// Node layout (0x30 bytes, allocated 8-aligned) — read off the disassembly at
// 0x14000e216, which zeroes +0x10/+0x18/+0x20 with one XOR'd register and +0x28
// with its low dword:
//   +0x00  uint64  key
//   +0x08  uint64  value
//   +0x10  node*   parent
//   +0x18  node*   left
//   +0x20  node*   right
//   +0x28  int16   balance   \  written as one dword by the ctor; the rebalance
//   +0x2a  int16   height    /  in Helper_e360 treats them as two int16s
//
// Tree header (param_1):
//   +0x00  node*   root
//   +0x08  int32   count
//
// ⚠ Keys compare UNSIGNED. The disassembly at 0x14000e23a is `CMP RCX,RAX ; JC`
// then `CMP RAX,RCX ; JNC` — carry-flag branches, not JL/JGE. Ghidra's `ulonglong`
// agrees, but this was checked against the instructions because a signed compare
// would put any key with bit 63 set on the wrong side of the tree and produce a
// silently mis-ordered (but structurally valid) BST.
namespace {
// 0x14000e360 — the AVL rebalance walk. Ported in citizenai_housing_idle.cpp and
// registered by reg_citizenai_housing_idle.cpp; pulled through the address registry
// so this file does not hard-link another translation unit's internal symbol.
typedef void (*fn_Rebalance_e360_t)(int64_t* rootSlot, int64_t* nodeSlot);
fn_Rebalance_e360_t RebalanceE360() {
    static fn_Rebalance_e360_t s_fn = nullptr;
    static bool s_tried = false;
    if (!s_tried) {
        s_tried = true;
        s_fn = reinterpret_cast<fn_Rebalance_e360_t>(
            ResolveAddr(Module::Exe, 0x14000e360));
    }
    return s_fn;
}
} // namespace

extern "C" void* TradeUI_StorageTreeNodeNew_e2c0(uint64_t /*key*/,
                                                 const uint64_t* entry,
                                                 void* parent) {
    // The key argument is genuinely unused: retail passes it but the ctor takes both
    // key and value from `entry`. Kept in the signature because the second caller
    // (TradeUI_impl_55a0, 0x1400255a0) passes it too.
    if (!fn_MemAllocStatic) return nullptr;
    uint64_t* n = static_cast<uint64_t*>(fn_MemAllocStatic(0x30, 8));
    if (!n) return nullptr;                  // retail checks this too
    n[0] = entry[0];                         // key
    n[1] = entry[1];                         // value
    n[2] = reinterpret_cast<uint64_t>(parent);
    n[3] = 0;                                // left
    n[4] = 0;                                // right
    *reinterpret_cast<uint32_t*>(n + 5) = 0; // balance + height
    return n;
}

extern "C" void* TradeUI_StorageTree_Insert_e1c0(int64_t* tree,
                                                 void**   out,
                                                 const uint64_t* entry) {
    if (!tree || !out || !entry) return out;   // standalone-only
    int32_t* count = reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(tree) + 0x08);

    if (*tree == 0) {
        // Empty tree: the root node is built inline rather than through e2c0 (retail
        // duplicates the ctor here), with a null parent and no rebalance.
        uint64_t* n = fn_MemAllocStatic
            ? static_cast<uint64_t*>(fn_MemAllocStatic(0x30, 8)) : nullptr;
        if (!n) {
            *tree = 0;
            *out  = nullptr;
        } else {
            n[0] = entry[0];
            n[1] = entry[1];
            n[2] = 0;
            n[3] = 0;
            n[4] = 0;
            *reinterpret_cast<uint32_t*>(n + 5) = 0;
            *out  = n;
            *tree = reinterpret_cast<int64_t>(n);
        }
        ++*count;                            // retail bumps the count even when the
        return out;                          // allocation failed — kept faithful
    }

    const uint64_t key = entry[0];
    uint64_t* node = reinterpret_cast<uint64_t*>(*tree);
    void* inserted = nullptr;
    for (;;) {
        if (key < node[0]) {                 // unsigned, see note above
            if (node[3] == 0) {
                inserted = TradeUI_StorageTreeNodeNew_e2c0(key, entry, node);
                node[3] = reinterpret_cast<uint64_t>(inserted);
                break;
            }
            node = reinterpret_cast<uint64_t*>(node[3]);
        } else if (node[0] < key) {
            if (node[4] == 0) {
                inserted = TradeUI_StorageTreeNodeNew_e2c0(key, entry, node);
                node[4] = reinterpret_cast<uint64_t>(inserted);
                break;
            }
            node = reinterpret_cast<uint64_t*>(node[4]);
        } else {
            *out = node;                     // key already present: hand back the
            return out;                      // existing node, count UNCHANGED
        }
    }

    // Rebalance from the insertion point upward. `node` is the PARENT of the new
    // node, not the new node — retail passes the address of the local holding it and
    // Helper_e360 walks it up via +0x10 to the root, rotating as it goes. That helper
    // is 0x14000e360, already ported and registered by
    // standalone/reg_citizenai_housing_idle.cpp; resolving it through the registry
    // rather than calling it directly keeps this port honest about which exe function
    // it is invoking.
    if (fn_Rebalance_e360_t reb = RebalanceE360()) {
        int64_t walk = reinterpret_cast<int64_t>(node);
        reb(tree, &walk);
    }
    *out = inserted;
    ++*count;
    return out;
}

// ── REMAINING WAVE A LEAVES (not ported here) ────────────────────────────
// The TradeUI gate stays closed until this resolves too. Sized from its decompile;
// the note says what closure it needs, because that is the part that makes a
// "small" leaf not small.
//
//   0x140059670  fn_Building_GetBuyEntries     249 decompiled lines — the largest of
//                the original set, and the one that must be read from the
//                DISASSEMBLY: Ghidra renders a joined_r0x000140059767 back-edge that
//                looks like an infinite loop and is not one. Needs 4 type-id globals
//                (g_TypeInfo_ValueDescription, DWORD_1401c7cb8, DWORD_1401c7318,
//                DWORD_1401c48a8), sub-callees FUN_140001bb0 + FUN_14000ee40,
//                fn_Sprite_IsValid and a _singleton_exref global. It feeds the buy
//                table, so a wrong port yields a POPULATED-but-wrong panel rather
//                than an empty or crashing one.
//
// Landed elsewhere in this wave, recorded here so the gate list and the tree agree:
//   0x14005a520 + 0x14000ea80  → code/Game/Dialog/tradeui_price.cpp
//   0x1400162d0 0x1400165d0 0x140015ad0 0x140016920 0x1400845d0 0x140084430
//                              → code/Game/Dialog/tradeui_sort.cpp
// The sort cluster is the cautionary tale of this wave: the gate named two
// addresses, and porting them turned out to mean porting six, because neither
// driver's comparator nor its small-range sort existed either. What the gate can
// see is not the same as what has to exist.
//
// NOT a gate dependency, deliberately: 0x140081420 fn_TradeUI_OnTradeButton is the
// Wave D checkout, reachable only from the slot-9 buttonTrade branch, which
// null-guards it at the call site in tradeui.cpp.
