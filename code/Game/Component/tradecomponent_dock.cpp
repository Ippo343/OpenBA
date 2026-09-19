// code/Game/Component/tradecomponent_dock.cpp
// TradeComponent controller wave, LAYER 3 — the DOCKED state (0x13).
//
// Layer 1 (tradecomponent_states.cpp) built the machine and the idle state; layer 2
// (boatcomponent_route.cpp) made the merchant arrive. This file is what happens once
// the boat is tied up: the merchant's stock list is generated, the stay timer is armed,
// and — if the post is configured for it — the post trades with the merchant by itself.
//
//   0x14005b580  TradeComponent_BuildMerchantManifest  the merchant's stock list
//   0x140059ad0  TradeComponent_AutoTrade              buy against the standing orders
//   0x14005a800  TradeCtrl_State13_Enter               state 0x13 ENTER
//   0x14005a6d0  TradeCtrl_State13_Update              state 0x13 UPDATE (leaving)
//
// ── The two TradeComponent arrays these functions live on ─────────────────────────
// They are easy to swap, and swapping them silently produces plausible garbage:
//
//   +0x40 base / +0x48 count, stride 0x0c — the player's STANDING BUY ORDERS.
//       +0x00 uint32 raw-material category MASK (ANDed with RawMaterialDescription+0x5c)
//       +0x08 int32  how many units the post wants
//   +0x50 base / +0x58 count / +0x5c capacity, stride 0x10 — the docked merchant's
//       MANIFEST, generated fresh on every arrival and freed on every departure.
//       +0x00 entity/description pointer   +0x08 int32 unit price   +0x0c int32 stock
//   +0x30 base / +0x38 count, stride 0x20 — the post's per-merchant-slot records
//       (built by TradeComponent_Setup / 0x14000e760).
//       +0x00 String merchant name   +0x08 int32 order kind
//       +0x10 ptr / +0x18 count / +0x1c cap — a hand-authored goods list
//   +0x2c int32 index of the DOCKED merchant, -1 when none
//   +0x28 int32 auto-trade policy: 1 = trade on arrival, 2 = trade on departure
//   +0x71 "mid-trade" flag (checked by the stay timer, not written here)
//   +0x78 Time  stay countdown        +0x80 merchant entity reference
//
// The description side: desc+0x48 merchant-record array / desc+0x50 count, stride 0x48;
// desc+0x58 tier array / desc+0x60 count, stride 0x14.
//
// ── Deviations, all deliberate ────────────────────────────────────────────────────
//  * 0x140059ad0's AVL insert is written out THREE times by the compiler (a probe pass,
//    a duplicate-locate pass and an insert pass, all with identical comparisons). It is
//    collapsed here into one search-then-insert. The tree is deterministic and the
//    comparisons are unchanged, so the resulting tree is the same one.
//  * Retail's new tree node writes eight bytes at +0x08 whose upper four are an
//    uninitialised stack slot; only the lower int32 is ever read. This port writes a
//    clean 1.
//  * 0x140088f90 is called with the unit PRICE in RCX where its other callers pass a
//    StorageComponent. That is retail's own code and it is harmless — the function
//    ignores its first parameter (see ActRm_GetResourceCategory_88f90) — so the price
//    is passed through rather than "corrected".
//  * The candidate cursor is kept in one variable. Retail keeps two (R12D and a
//    pointer-sized copy) that are incremented and reset together.
//
// ── Where Ghidra's decompile of 0x140059ad0 cannot be trusted ─────────────────────
// The function has a 0x98-byte frame and the decompiler folds distinct variables onto
// one name. Everything here is read off the disassembly. In particular:
//   [RBP+0x77] is the buy-order byte offset for the first three instructions of each
//              outer iteration and then becomes `spent` for the rest of it;
//   R13D is the storage volume in the purchase loop and `paid so far` in the sell loop;
//   the committed storage volume lives in [RBP+0x7f], NOT in R13 — the purchase loop
//              works on a copy and writes it back only when a unit is actually bought.
//
// GATED: reached only through BANISHED_LIVE_TRADE_CTRL. Nothing else in the build calls
// any of these four addresses.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// Layer 1 leaf, used by the manifest generator to pick the merchant's price tier.
extern "C" int TradeComponent_PickMerchantTier(void* self);   // 0x14005b4f0

namespace {

// ── Resolved leaves, bound by ADDRESS ─────────────────────────────────────────────
// Every one of these is already ported and registered elsewhere in the tree. Binding
// by address rather than through the C++ names keeps this file immune to the naming
// question — two of these carry names that are wrong (see the notes below) and one
// retail function in this codebase has already been ported twice under two names.
typedef void (*fn_GetBuyEntries_t)(void* tradeComp, void* outArray);   // 0x140059670
typedef int  (*fn_ResourcePrice_t)(void* tradeComp, int64_t entity);   // 0x14005a520
typedef void (*fn_ExtendStay_t)(void* tradeComp);                      // 0x14005a490
typedef void*(*fn_FindStorage_t)(int64_t entity, void* outIBP);        // 0x140006270
typedef void*(*fn_FindStateMachine_t)(int64_t entity, void* outIBP);   // 0x140003ea0
typedef int  (*fn_QueryCount_t)(void* storageComp);                    // 0x14012cf10
typedef void (*fn_StorageAdd_t)(void* storageComp, int64_t entity, int qty);  // 0x14012d000
typedef int  (*fn_ResourceCategory_t)(void* ignored, int64_t entity);  // 0x140088f90
typedef void (*fn_RegisterRawMat_t)(void* townMgr, int64_t entity);    // 0x140091e60
typedef int64_t (*fn_AvlRotate_t)(int64_t* root, int64_t node);        // 0x14000e450 / e580
typedef void (*fn_TreeFree_t)(void* rootHolder, void* node);           // 0x14000e320

fn_GetBuyEntries_t    s_GetBuyEntries    = nullptr;
fn_ResourcePrice_t    s_ResourcePrice    = nullptr;   // NOT a count: a520 returns a PRICE
fn_ExtendStay_t       s_ExtendStay       = nullptr;
fn_FindStorage_t      s_FindStorage      = nullptr;
fn_FindStateMachine_t s_FindStateMachine = nullptr;
fn_QueryCount_t       s_QueryCount       = nullptr;
fn_StorageAdd_t       s_StorageAdd       = nullptr;
fn_ResourceCategory_t s_ResourceCategory = nullptr;
fn_RegisterRawMat_t   s_RegisterRawMat   = nullptr;   // 91e60 is NOT "Game_DestroyEntity"
fn_AvlRotate_t        s_RotateLeft       = nullptr;   // e450 — used when right-heavy
fn_AvlRotate_t        s_RotateRight      = nullptr;   // e580 — used when left-heavy
fn_TreeFree_t         s_TreeFree         = nullptr;
bool                  s_resolved         = false;

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_GetBuyEntries    = reinterpret_cast<fn_GetBuyEntries_t>(ResolveAddr(Module::Exe, 0x140059670));
    s_ResourcePrice    = reinterpret_cast<fn_ResourcePrice_t>(ResolveAddr(Module::Exe, 0x14005a520));
    s_ExtendStay       = reinterpret_cast<fn_ExtendStay_t>(ResolveAddr(Module::Exe, 0x14005a490));
    s_FindStorage      = reinterpret_cast<fn_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_FindStateMachine = reinterpret_cast<fn_FindStateMachine_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_QueryCount       = reinterpret_cast<fn_QueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_StorageAdd       = reinterpret_cast<fn_StorageAdd_t>(ResolveAddr(Module::Exe, 0x14012d000));
    s_ResourceCategory = reinterpret_cast<fn_ResourceCategory_t>(ResolveAddr(Module::Exe, 0x140088f90));
    s_RegisterRawMat   = reinterpret_cast<fn_RegisterRawMat_t>(ResolveAddr(Module::Exe, 0x140091e60));
    s_RotateLeft       = reinterpret_cast<fn_AvlRotate_t>(ResolveAddr(Module::Exe, 0x14000e450));
    s_RotateRight      = reinterpret_cast<fn_AvlRotate_t>(ResolveAddr(Module::Exe, 0x14000e580));
    s_TreeFree         = reinterpret_cast<fn_TreeFree_t>(ResolveAddr(Module::Exe, 0x14000e320));

    // Report the OUTCOME of every dependency. One null here turns the docked state into
    // a state that runs, mutates nothing, and looks exactly like a merchant that simply
    // had nothing to sell.
    std::fprintf(stderr,
        "[TradeDock] deps: buyEntries=%p price=%p extendStay=%p findStorage=%p findSM=%p "
        "queryCount=%p storageAdd=%p category=%p regRawMat=%p rotL=%p rotR=%p treeFree=%p\n",
        reinterpret_cast<void*>(s_GetBuyEntries), reinterpret_cast<void*>(s_ResourcePrice),
        reinterpret_cast<void*>(s_ExtendStay), reinterpret_cast<void*>(s_FindStorage),
        reinterpret_cast<void*>(s_FindStateMachine), reinterpret_cast<void*>(s_QueryCount),
        reinterpret_cast<void*>(s_StorageAdd), reinterpret_cast<void*>(s_ResourceCategory),
        reinterpret_cast<void*>(s_RegisterRawMat), reinterpret_cast<void*>(s_RotateLeft),
        reinterpret_cast<void*>(s_RotateRight), reinterpret_cast<void*>(s_TreeFree));
    std::fflush(stderr);
}


// The TradeDescription off component+0x18, cast exactly as retail does it.
void* TradeDesc(uintptr_t comp) {
    void* raw = *reinterpret_cast<void**>(comp + 0x18);
    if (!g_TypeInfo_TradeDescription || !fn_IsDerivedFrom) return nullptr;
    return fn_IsDerivedFrom(raw, *g_TypeInfo_TradeDescription) ? raw : nullptr;
}

uintptr_t TownMgr() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    return *reinterpret_cast<uintptr_t*>(
               reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
}

// Find the first object in a description's child array (base +0x28, count +0x30,
// stride 8) whose GetTypeId (vtable +0x10) equals `typeId`. Retail inlines this scan
// six times across the two big functions; every copy stops at the FIRST id match and
// only then consults IsDerivedFrom, breaking out rather than continuing the scan if
// that says no. A null return therefore reproduces retail's fall-through exactly.
void* FindChildByTypeId(uintptr_t owner, uint32_t typeId) {
    if (!owner || !typeId) return nullptr;
    const int count = *reinterpret_cast<int*>(owner + 0x30);
    void** objs = *reinterpret_cast<void***>(owner + 0x28);
    if (!objs) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* obj = objs[i];
        if (!obj) continue;
        uint32_t (*getTypeId)(void*) =
            *reinterpret_cast<uint32_t (**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x10);
        if (getTypeId(obj) != typeId) continue;
        return (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, typeId)) ? obj : nullptr;
    }
    return nullptr;
}

// ── The purchase tally: an AVL map from bought entity -> unit count ───────────────
// Node layout (0x30 bytes), read off the disassembly at 0x140059ef1:
//   +0x00 uint64 key (the purchased entity)    +0x08 int32 units bought
//   +0x10 parent   +0x18 left   +0x20 right    +0x28 int16 balance  +0x2a int16 height
// The rotations are retail's own (0x14000e450 / 0x14000e580, "layout B" in
// action_pf_avl.cpp) and operate on the ROOT HOLDER, so `root` is passed by address.
struct AvlNode;
inline int64_t&  NLink(uintptr_t n, uintptr_t off) { return *reinterpret_cast<int64_t*>(n + off); }
inline int16_t&  NS16(uintptr_t n, uintptr_t off)  { return *reinterpret_cast<int16_t*>(n + off); }

int16_t ChildHeight(int64_t child) {
    return child ? static_cast<int16_t>(NS16(static_cast<uintptr_t>(child), 0x2a) + 1) : 0;
}

// Walk from `from` to the root recomputing height/balance and rotating where the AVL
// invariant broke. This is the tail of retail's insert (0x140059f90 / 0x14005a081),
// which runs it twice verbatim — once per insert side.
void AvlRebalance(int64_t* root, int64_t from) {
    for (int64_t cur = from; cur != 0; ) {
        const uintptr_t c = static_cast<uintptr_t>(cur);
        const int64_t up = NLink(c, 0x10);            // retail saves the parent FIRST
        const int16_t lh = ChildHeight(NLink(c, 0x18));
        const int16_t rh = ChildHeight(NLink(c, 0x20));
        NS16(c, 0x2a) = (rh < lh) ? lh : rh;
        NS16(c, 0x28) = static_cast<int16_t>(rh - lh);

        const int64_t right = NLink(c, 0x20);
        if (static_cast<int16_t>(rh - lh) > 1 && right) {
            if (NS16(static_cast<uintptr_t>(right), 0x28) < 0 && s_RotateRight)
                s_RotateRight(root, right);
            if (s_RotateLeft) s_RotateLeft(root, cur);
        }
        // Re-read the balance: the rotation above may have rewritten it. Retail does.
        const int64_t left = NLink(c, 0x18);
        if (NS16(c, 0x28) < -1 && left) {
            if (NS16(static_cast<uintptr_t>(left), 0x28) > 0 && s_RotateLeft)
                s_RotateLeft(root, left);
            if (s_RotateRight) s_RotateRight(root, cur);
        }
        cur = up;
    }
}

// Insert `key`, or bump the count of an existing node. Returns true if a NODE was
// added (retail increments its node counter only then).
//
// Retail emits the same descent three times — a probe, a duplicate-locate and an
// insert. Collapsed here to one descent; the comparisons and the resulting shape are
// identical because the tree is not mutated between the copies.
bool AvlInsert(int64_t* root, uint64_t key) {
    uintptr_t parent = 0;
    bool goLeft = false;
    for (uintptr_t cur = static_cast<uintptr_t>(*root); cur; ) {
        const uint64_t k = *reinterpret_cast<uint64_t*>(cur);
        if (key == k) {                                   // already bought: one more unit
            *reinterpret_cast<int32_t*>(cur + 8) += 1;
            return false;
        }
        parent = cur;
        goLeft = key < k;
        cur = static_cast<uintptr_t>(NLink(cur, goLeft ? 0x18 : 0x20));
    }

    uintptr_t node = reinterpret_cast<uintptr_t>(
        fn_MemAllocStatic ? fn_MemAllocStatic(0x30, 8) : nullptr);
    if (node) {
        *reinterpret_cast<uint64_t*>(node) = key;
        // Retail stores an 8-byte stack qword here whose upper half is uninitialised;
        // only the lower int32 is ever read back (`INC dword [n+8]` / `MOV R8D,[n+8]`).
        *reinterpret_cast<int64_t*>(node + 8) = 1;
        NLink(node, 0x10) = static_cast<int64_t>(parent);
        NLink(node, 0x18) = 0;
        NLink(node, 0x20) = 0;
        *reinterpret_cast<int32_t*>(node + 0x28) = 0;      // balance and height together
    }
    if (!parent) {
        *root = static_cast<int64_t>(node);                // first node: no rebalance
        return true;
    }
    NLink(parent, goLeft ? 0x18 : 0x20) = static_cast<int64_t>(node);
    AvlRebalance(root, static_cast<int64_t>(parent));
    return true;
}

int64_t AvlLeftmost(int64_t n) {
    while (n && NLink(static_cast<uintptr_t>(n), 0x18)) n = NLink(static_cast<uintptr_t>(n), 0x18);
    return n;
}

// In-order successor, 1:1 with 0x14005a3f5.
int64_t AvlNext(int64_t n) {
    const uintptr_t c = static_cast<uintptr_t>(n);
    if (int64_t r = NLink(c, 0x20)) return AvlLeftmost(r);
    int64_t node = n;
    int64_t up = NLink(static_cast<uintptr_t>(node), 0x10);
    if (!up) return NLink(static_cast<uintptr_t>(node), 0x10);
    if (NLink(static_cast<uintptr_t>(up), 0x18) == node) return up;
    while (up && NLink(static_cast<uintptr_t>(up), 0x20) == node) {
        node = up;
        up = NLink(static_cast<uintptr_t>(node), 0x10);
    }
    return NLink(static_cast<uintptr_t>(node), 0x10);
}

// ── The two inline dynamic arrays 0x14005b580 builds on its stack ─────────────────
// Both are the engine's {ptr, int32 count, int32 capacity} header. The goods array is
// handed to 0x140059670, which writes it through that exact layout, so it must stay
// binary-compatible with it.
struct PtrArray { int64_t base; int32_t count; int32_t cap; };
static_assert(sizeof(PtrArray) == 0x10, "PtrArray must match the engine's array header");

void PtrArrayPush(PtrArray& a, void* value) {
    const int32_t oldN = a.count;
    const int32_t newN = oldN + 1;
    void* oldBuf = reinterpret_cast<void*>(a.base);
    a.count = newN;
    if (a.cap < newN) {
        a.cap = (oldN != 0) ? newN * 3 : newN;
        a.base = reinterpret_cast<int64_t>(
            fn_MemAllocStatic ? fn_MemAllocStatic(static_cast<uint64_t>(a.cap) * 8, 8) : nullptr);
    }
    const int32_t start = (oldBuf == reinterpret_cast<void*>(a.base)) ? oldN : 0;
    for (int32_t i = start; i < newN; ++i) {
        void** dst = reinterpret_cast<void**>(a.base + static_cast<int64_t>(i) * 8);
        if (!dst) continue;
        *dst = (i < oldN) ? reinterpret_cast<void**>(oldBuf)[i] : value;
    }
    if (oldBuf != reinterpret_cast<void*>(a.base) && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
}

// The candidate index array in 0x140059ad0 — same header, int32 elements.
struct IntArray { int64_t base; int32_t count; int32_t cap; };

void IntArrayPush(IntArray& a, int32_t value) {
    const int32_t oldN = a.count;
    const int32_t newN = oldN + 1;
    void* oldBuf = reinterpret_cast<void*>(a.base);
    a.count = newN;
    if (a.cap < newN) {
        a.cap = (oldN != 0) ? newN * 3 : newN;
        a.base = reinterpret_cast<int64_t>(
            fn_MemAllocStatic ? fn_MemAllocStatic(static_cast<uint64_t>(a.cap) * 4, 4) : nullptr);
    }
    const int32_t start = (oldBuf == reinterpret_cast<void*>(a.base)) ? oldN : 0;
    for (int32_t i = start; i < newN; ++i) {
        int32_t* dst = reinterpret_cast<int32_t*>(a.base + static_cast<int64_t>(i) * 4);
        if (!dst) continue;
        *dst = (i < oldN) ? reinterpret_cast<int32_t*>(oldBuf)[i] : value;
    }
    if (oldBuf != reinterpret_cast<void*>(a.base) && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
}

}  // namespace

// ============================================================
// 0x14005b580 — generate the docked merchant's MANIFEST
// ============================================================
// Runs once, on arrival. It throws away any previous manifest, rolls which of the
// description's merchants turned up, and fills +0x50 with the goods that merchant is
// offering and what it wants for them.
//
// Where the goods come from depends on the post's per-merchant record at +0x30:
//   order kind (+0x08) is 0, or its list is empty
//        -> the generic buy list from 0x140059670 (every raw material the town knows
//           about, plus the merchant's own goods list and select-type matches), at the
//           ValueDescription's base price;
//   otherwise
//        -> exactly the goods on that record's list, at 1.25x (DAT_140172158) the base
//           price. Retail then FREES that list unless the kind is 2, so a kind-1 order
//           is consumed by the arrival it was placed for.
//
// The number of distinct rolls is merchant[+0x40] / tier[+0x0c], at least 1. Each roll
// picks a good at random; a repeat roll adds to the existing manifest line's stock
// rather than adding a second line.
//
// Per-good figures come from the good's ValueDescription:
//   +0x40 base price  +0x44 stock numerator (divided by tier[+0x10])  +0x48 stock floor
extern "C" void TradeComponent_BuildMerchantManifest(void* selfV) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    // Drop the previous manifest. +0x58 is cleared as one qword: count AND capacity.
    if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(comp + 0x50));
    *reinterpret_cast<uint64_t*>(comp + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
    *reinterpret_cast<int32_t*>(comp + 0x2c) = -1;

    void* rng = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8);
    const uintptr_t desc = reinterpret_cast<uintptr_t>(TradeDesc(comp));

    // Roll which merchant arrived, and remember it: everything below indexes on it, and
    // so does the panel.
    const int merchantIdx = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(
        static_cast<uint32_t>(*reinterpret_cast<uint32_t*>(desc + 0x50))));   // B.3.4b bridge
    *reinterpret_cast<int32_t*>(comp + 0x2c) = merchantIdx;

    const uintptr_t merchant =
        static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(desc + 0x48)) +
        static_cast<uintptr_t>(merchantIdx) * 0x48;
    const int tierIdx = TradeComponent_PickMerchantTier(selfV);
    const uintptr_t tier =
        static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(desc + 0x58)) +
        static_cast<uintptr_t>(tierIdx) * 0x14;

    PtrArray goods  = {0, 0, 0};   // the goods on offer
    PtrArray values = {0, 0, 0};   // their ValueDescriptions, index-aligned with `goods`

    // Where the goods list comes from, and whether it is marked up.
    const uintptr_t rec = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(comp + 0x30)) +
                          static_cast<uintptr_t>(merchantIdx) * 0x20;
    bool fromOrder = false;
    if (*reinterpret_cast<int32_t*>(rec + 0x08) == 0 ||
        *reinterpret_cast<int32_t*>(rec + 0x18) < 1) {
        if (s_GetBuyEntries) s_GetBuyEntries(selfV, &goods);
    } else {
        fromOrder = true;
        const int32_t n = *reinterpret_cast<int32_t*>(rec + 0x18);
        goods.count = n;
        goods.cap   = n;
        goods.base  = reinterpret_cast<int64_t>(
            fn_MemAllocStatic ? fn_MemAllocStatic(static_cast<uint64_t>(n) * 8, 8) : nullptr);
        void** src = *reinterpret_cast<void***>(rec + 0x10);
        for (int32_t i = 0; i < n && goods.base; ++i)
            reinterpret_cast<void**>(goods.base)[i] = src[i];
    }

    // Consume the order unless it is the persistent kind.
    if (*reinterpret_cast<int32_t*>(rec + 0x08) != 2) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(rec + 0x10));
        *reinterpret_cast<uint64_t*>(rec + 0x10) = 0;
        *reinterpret_cast<uint64_t*>(rec + 0x18) = 0;    // count and capacity together
        *reinterpret_cast<int32_t*>(rec + 0x08) = 0;
    }

    // Each good's ValueDescription, pushed even when absent so the two arrays stay
    // index-aligned — the roll below indexes both with the same number.
    const uint32_t tidValue = g_TypeInfo_ValueDescription ? *g_TypeInfo_ValueDescription : 0;
    for (int32_t i = 0; i < goods.count; ++i) {
        void* good = reinterpret_cast<void**>(goods.base)[i];
        PtrArrayPush(values, FindChildByTypeId(reinterpret_cast<uintptr_t>(good), tidValue));
    }

    // STANDALONE-ONLY guard, not a softened retail deref. Retail rolls at least once and
    // indexes `values` without checking, because its data guarantees both a non-empty buy
    // list and a ValueDescription on every entry. In standalone both can be empty for a
    // reason that has nothing to do with the data: the ComponentDescription and
    // ValueDescription type-ids (0x1401c7318 / 0x1401c4bf0) sit behind
    // BANISHED_LIVE_TRADE_TIDS, and with that gate off 0x140059670's ExternalFactory walk
    // matches nothing and every ValueDescription lookup returns null. Faulting on a flag
    // the player can turn off reproduces nothing, so name the flag instead.
    if (values.count <= 0) {
        std::fprintf(stderr, "[TradeDock] manifest: NO GOODS to offer (buy list empty). "
                             "If BANISHED_LIVE_TRADE_TIDS is off, that is why — the "
                             "ComponentDescription type-id 0x1401c7318 reads 0 and the "
                             "ExternalFactory walk matches nothing.\n");
        std::fflush(stderr);
        if (fn_MemFreeStatic) { fn_MemFreeStatic(reinterpret_cast<void*>(values.base));
                                fn_MemFreeStatic(reinterpret_cast<void*>(goods.base)); }
        return;
    }

    int rolls = *reinterpret_cast<int32_t*>(merchant + 0x40) /
                *reinterpret_cast<int32_t*>(tier + 0x0c);
    if (rolls < 1) rolls = 1;

    for (int r = 0; r < rolls; ++r) {
        const int pick = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(values.count)));   // B.3.4b bridge
        const uintptr_t vd = reinterpret_cast<uintptr_t>(
            reinterpret_cast<void**>(values.base)[pick]);
        if (!vd) {
            // Same standalone-only reasoning as above: with the type-id gate off EVERY
            // entry is null. Say so once rather than faulting on vd+0x44.
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[TradeDock] manifest: good #%d has NO ValueDescription "
                                     "(type-id 0x1401c4bf0 reads %u) — skipping the roll\n",
                             pick, tidValue);
                std::fflush(stderr);
            }
            continue;
        }

        int stock = *reinterpret_cast<int32_t*>(vd + 0x44) /
                    *reinterpret_cast<int32_t*>(tier + 0x10);
        if (stock < *reinterpret_cast<int32_t*>(vd + 0x48))
            stock = *reinterpret_cast<int32_t*>(vd + 0x48);
        if (stock < 1) stock = 1;

        int price = *reinterpret_cast<int32_t*>(vd + 0x40);
        if (fromOrder) price = static_cast<int>(static_cast<float>(price) * 1.25f);  // DAT_140172158
        if (price < 1) price = 1;

        void* good = reinterpret_cast<void**>(goods.base)[pick];

        // Already on the manifest? Add to its stock and move on.
        bool merged = false;
        const int lineCount = *reinterpret_cast<int32_t*>(comp + 0x58);
        for (int i = 0; i < lineCount; ++i) {
            const uintptr_t line = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(comp + 0x50)) +
                                   static_cast<uintptr_t>(i) * 0x10;
            if (*reinterpret_cast<void**>(line) != good) continue;
            *reinterpret_cast<int32_t*>(line + 0x0c) += stock;
            merged = true;
            break;
        }
        if (merged) continue;

        // Append a new manifest line {good, price, stock}, growing +0x50 in place.
        int64_t entry[2];
        reinterpret_cast<void**>(entry)[0] = good;
        reinterpret_cast<int32_t*>(entry)[2] = price;
        reinterpret_cast<int32_t*>(entry)[3] = stock;

        const int oldN = *reinterpret_cast<int32_t*>(comp + 0x58);
        const int newN = oldN + 1;
        void* oldBuf = *reinterpret_cast<void**>(comp + 0x50);
        *reinterpret_cast<int32_t*>(comp + 0x58) = newN;
        if (*reinterpret_cast<int32_t*>(comp + 0x5c) < newN) {
            *reinterpret_cast<int32_t*>(comp + 0x5c) = (oldN != 0) ? newN * 3 : newN;
            *reinterpret_cast<void**>(comp + 0x50) = fn_MemAllocStatic
                ? fn_MemAllocStatic(
                      static_cast<uint64_t>(*reinterpret_cast<int32_t*>(comp + 0x5c)) << 4, 8)
                : nullptr;
        }
        const int start = (oldBuf == *reinterpret_cast<void**>(comp + 0x50)) ? oldN : 0;
        for (int i = start; i < newN; ++i) {
            int64_t* dst = reinterpret_cast<int64_t*>(
                *reinterpret_cast<uintptr_t*>(comp + 0x50) + static_cast<uintptr_t>(i) * 0x10);
            if (!dst) continue;
            const int64_t* src = (i < oldN)
                ? reinterpret_cast<const int64_t*>(reinterpret_cast<uintptr_t>(oldBuf) +
                                                   static_cast<uintptr_t>(i) * 0x10)
                : entry;
            dst[0] = src[0];
            dst[1] = src[1];
        }
        if (oldBuf != *reinterpret_cast<void**>(comp + 0x50) && fn_MemFreeStatic)
            fn_MemFreeStatic(oldBuf);
    }

    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(values.base));
    values.base = 0; values.count = 0; values.cap = 0;
    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(goods.base));

    // OUTCOME, not intent: the line count is the only thing that says the merchant has
    // anything to sell. "The manifest was generated" and "the manifest has goods in it"
    // are different claims and only the readback proves the second.
}

// ============================================================
// 0x140059ad0 — AUTO-TRADE against the standing buy orders
// ============================================================
// The post trades with the docked merchant with no player involvement. For each
// standing buy order, in order:
//
//   1. price up everything unreserved in the post's own storage — that sum is the
//      post's spending limit for this order;
//   2. collect the manifest lines whose raw-material flags (+0x5c) hit the order's
//      category mask, and how many units of them the merchant has;
//   3. buy ONE unit at a time, round-robin over those lines, until the order is
//      filled, the money runs out, the merchant runs out, or the next unit would
//      not fit in the post's storage volume (StorageComponent+0x38). Each purchase
//      decrements the merchant's stock and tallies the entity in an AVL map;
//   4. pay for it by selling the post's own stored goods, unit by unit, crediting
//      the town's trade total (townMgr+0x1ac) and per-category counter
//      (townMgr+0x170[cat]+0x0c) as each unit goes.
//
// Only then, once every order has been processed, are the purchases DELIVERED: the
// tally is walked in key order and each entity is registered as available to the town
// (0x140091e60 — which is NOT "Game_DestroyEntity", see tradeui_checkout.cpp) and added
// to the post's storage.
//
// Deliveries are deferred to the end on purpose: the sell phase walks the same storage
// array, and adding the purchases first would make the post pay for its new goods with
// its new goods.
extern "C" void TradeComponent_AutoTrade(void* selfV) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    if (s_ExtendStay) s_ExtendStay(selfV);

    uint64_t storageIBP[2] = {0, 0};
    if (s_FindStorage) s_FindStorage(*reinterpret_cast<int64_t*>(comp + 0x10), storageIBP);

    const uintptr_t townMgr = TownMgr();

    int64_t treeRoot = 0;      // the purchase tally
    int     treeCount = 0;     // retail keeps it; nothing reads it back

    const uintptr_t storage = static_cast<uintptr_t>(storageIBP[0]);
    int volume = s_QueryCount ? s_QueryCount(reinterpret_cast<void*>(storage)) : 0;
    const int volumeCap = *reinterpret_cast<int32_t*>(storage + 0x38);

    const uint32_t tidRawMat = g_TypeId_RawMaterialDescription ? *g_TypeId_RawMaterialDescription : 0;

    // What the merchant is actually offering, with the raw-material flag word that each
    // order mask is tested against. Without this, a "0 delivered" outcome cannot be told
    // apart from "the merchant brought one good that nobody had ordered".

    const int orderCount = *reinterpret_cast<int32_t*>(comp + 0x48);
    for (int o = 0; o < orderCount; ++o) {
        const uintptr_t order = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(comp + 0x40)) +
                                static_cast<uintptr_t>(o) * 0x0c;
        const int orderAmount = *reinterpret_cast<int32_t*>(order + 0x08);
        if (orderAmount == 0) {
            // OUTCOME, not intent: an order row with amount 0 is a category the player
            // never asked for. Retail skips it, and a post with 14 all-zero rows buying
            // nothing is CORRECT behaviour, not a broken buy loop. Print both arms so the
            // two cannot be confused.
            continue;
        }
        const uint32_t orderMask = *reinterpret_cast<uint32_t*>(order + 0x00);

        // ── 1. what the post can afford: everything unreserved in its storage ──────
        int sellValue = 0;
        {
            const int n = *reinterpret_cast<int32_t*>(storage + 0x30);
            for (int j = 0; j < n; ++j) {
                const uintptr_t e = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(storage + 0x28)) +
                                    static_cast<uintptr_t>(j) * 0x10;
                const int have = *reinterpret_cast<int32_t*>(e + 0x08) -
                                 *reinterpret_cast<int32_t*>(e + 0x0c);
                const int unit = s_ResourcePrice
                    ? s_ResourcePrice(selfV, *reinterpret_cast<int64_t*>(e)) : 0;
                sellValue += unit * have;
            }
        }

        // ── 2. which manifest lines this order matches ────────────────────────────
        IntArray candidates = {0, 0, 0};
        int availQty = 0;
        {
            const int lines = *reinterpret_cast<int32_t*>(comp + 0x58);
            for (int k = 0; k < lines; ++k) {
                const uintptr_t line = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(comp + 0x50)) +
                                       static_cast<uintptr_t>(k) * 0x10;
                void* rawMat = FindChildByTypeId(
                    static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(line)), tidRawMat);
                if (!rawMat) continue;
                if ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x5c) &
                     orderMask) == 0) continue;
                IntArrayPush(candidates, k);
                availQty += *reinterpret_cast<int32_t*>(line + 0x0c);
            }
        }

        // ── 3. buy, one unit per round-robin step ─────────────────────────────────
        int bought = 0, spent = 0, cursor = 0;
        if (availQty > 0) {
            for (;;) {
                if (bought >= orderAmount) break;      // order filled
                if (spent >= sellValue) break;         // out of money

                const int lineIdx = reinterpret_cast<int32_t*>(candidates.base)[cursor];
                const uintptr_t line = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(comp + 0x50)) +
                                       static_cast<uintptr_t>(lineIdx) * 0x10;
                // Every candidate was selected BY having a RawMaterialDescription, so
                // this second lookup on the same entity cannot miss. Retail derefs its
                // result unguarded for exactly that reason.
                const uintptr_t rawMat = reinterpret_cast<uintptr_t>(FindChildByTypeId(
                    static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(line)), tidRawMat));

                if (*reinterpret_cast<int32_t*>(line + 0x0c) > 0) {
                    const int next = volume + *reinterpret_cast<int32_t*>(rawMat + 0x48);
                    if (next > volumeCap) break;       // no room for the next unit
                    volume = next;
                    spent += *reinterpret_cast<int32_t*>(line + 0x08);
                    if (AvlInsert(&treeRoot,
                                  static_cast<uint64_t>(*reinterpret_cast<int64_t*>(line))))
                        ++treeCount;
                    *reinterpret_cast<int32_t*>(line + 0x0c) -= 1;
                    ++bought;
                }

                if (++cursor >= candidates.count) cursor = 0;
                if (bought >= availQty) break;         // merchant is out of stock
            }
        }

        // Which arm actually stopped this order. "The buy loop ran" and "the buy loop
        // bought something" are separate claims; only the readback settles the second.

        // ── 4. sell to cover `spent` ──────────────────────────────────────────────
        {
            int paid = 0;
            for (int idx = 0; idx < *reinterpret_cast<int32_t*>(storage + 0x30); ++idx) {
                const uintptr_t e = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(storage + 0x28)) +
                                    static_cast<uintptr_t>(idx) * 0x10;
                const int64_t resource = *reinterpret_cast<int64_t*>(e);
                const int unit = s_ResourcePrice ? s_ResourcePrice(selfV, resource) : 0;
                const int avail = *reinterpret_cast<int32_t*>(e + 0x08) -
                                  *reinterpret_cast<int32_t*>(e + 0x0c);
                if (avail == 0) continue;

                int sold = 0;
                while (paid < spent) {
                    if (sold >= avail) break;
                    paid += unit;
                    ++sold;

                    *reinterpret_cast<int32_t*>(townMgr + 0x1ac) += unit;
                    // Retail passes the unit price where every other caller of 0x140088f90
                    // passes a StorageComponent. The function ignores its first parameter,
                    // so this is harmless — passed through rather than "corrected".
                    const int cat = s_ResourceCategory
                        ? s_ResourceCategory(reinterpret_cast<void*>(static_cast<uintptr_t>(unit)),
                                             resource) : -1;
                    if (cat >= 0) {
                        const uintptr_t catArray = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
                        *reinterpret_cast<int32_t*>(catArray + static_cast<uintptr_t>(cat) * 0x20
                                                    + 0x0c) += 1;
                    }

                    // Take the unit out of storage, by identity. An entry that reaches
                    // zero is erased and the tail shifted down.
                    const int n = *reinterpret_cast<int32_t*>(storage + 0x30);
                    const uintptr_t base = static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(storage + 0x28));
                    for (int j = 0; j < n; ++j) {
                        const uintptr_t slot = base + static_cast<uintptr_t>(j) * 0x10;
                        if (*reinterpret_cast<int64_t*>(slot) != resource) continue;
                        *reinterpret_cast<int32_t*>(slot + 0x08) -= 1;
                        if (*reinterpret_cast<int32_t*>(
                                static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(storage + 0x28)) +
                                static_cast<uintptr_t>(j) * 0x10 + 0x08) == 0) {
                            for (int m = j; m + 1 < *reinterpret_cast<int32_t*>(storage + 0x30); ++m) {
                                uint64_t* p = reinterpret_cast<uint64_t*>(
                                    static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(storage + 0x28)) +
                                    static_cast<uintptr_t>(m) * 0x10);
                                if (!p) continue;
                                p[0] = p[2];
                                p[1] = p[3];
                            }
                            *reinterpret_cast<int32_t*>(storage + 0x30) -= 1;
                        }
                        break;
                    }

                    // The volume freed by that unit. Same "cannot miss" reasoning as the
                    // purchase loop: the entry is in storage, so it is a raw material.
                    const uintptr_t soldRaw = reinterpret_cast<uintptr_t>(
                        FindChildByTypeId(static_cast<uintptr_t>(resource), tidRawMat));
                    volume -= *reinterpret_cast<int32_t*>(soldRaw + 0x48);
                }

                // The entry was drained to nothing and therefore erased: re-test this
                // index, because a different entry now occupies it.
                if (avail == sold) --idx;
            }
        }

        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(candidates.base));
    }

    // ── DELIVER the purchases ─────────────────────────────────────────────────────
    int delivered = 0, deliveredLines = 0;
    for (int64_t n = AvlLeftmost(treeRoot); n != 0; n = AvlNext(n)) {
        const uintptr_t node = static_cast<uintptr_t>(n);
        const int64_t entity = *reinterpret_cast<int64_t*>(node);
        const int units = *reinterpret_cast<int32_t*>(node + 8);
        if (s_RegisterRawMat) s_RegisterRawMat(reinterpret_cast<void*>(townMgr), entity);
        if (s_StorageAdd) s_StorageAdd(reinterpret_cast<void*>(storage), entity, units);
        delivered += units;
        ++deliveredLines;
    }

    if (treeRoot) {
        const uintptr_t root = static_cast<uintptr_t>(treeRoot);
        if (s_TreeFree) {
            s_TreeFree(&treeRoot, reinterpret_cast<void*>(NLink(root, 0x18)));
            s_TreeFree(&treeRoot, reinterpret_cast<void*>(NLink(root, 0x20)));
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(root));
    }
    treeRoot = 0;
    treeCount = 0;

    // OUTCOME, not intent: "auto-trade ran" and "the post bought something" are separate
    // claims. A run with orders but zero deliveries is the interesting case — it means
    // the post could not afford, could not fit, or was offered nothing that matched.
}

// ============================================================
// state 0x13 ENTER — 0x14005a800
// ============================================================
// The merchant has tied up. Generate what it brought, give it 900 game-seconds
// (DAT_140172244) before it leaves again, and auto-trade now if the post is set to
// trade on arrival (+0x28 == 1).
//
// Retail's tail is `MOV AL,1; RET`.
extern "C" char TradeCtrl_State13_Enter(void* selfV, void* /*sm*/) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    TradeComponent_BuildMerchantManifest(selfV);

    // The Time ctor writes EIGHT bytes through its return slot — see
    // reference_time_ctor_8byte_clobber.
    if (fn_Time_Ctor_Float) {
        int64_t stay = 0;
        void* out = fn_Time_Ctor_Float(&stay, 900.0f);
        *reinterpret_cast<int64_t*>(comp + 0x78) = *reinterpret_cast<int64_t*>(out);
    }

    if (*reinterpret_cast<int32_t*>(comp + 0x28) == 1) TradeComponent_AutoTrade(selfV);
    return 1;
}

// ============================================================
// state 0x13 UPDATE — 0x14005a6d0
// ============================================================
// The merchant is leaving. Auto-trade now if the post is set to trade on departure
// (+0x28 == 2), then post event 0 to the merchant boat's own state machine — that is
// what makes it cast off — drop the reference, and free the manifest.
//
// This is the UPDATE slot (+0x20), which the generic dispatcher only invokes when a
// transition out of the state is pending. Read off 0x14000dd70's disassembly: its
// arguments land as id -> +0x00, RDX -> +0x08 enter, R8 -> +0x20 update, and the
// FIFTH argument (on the stack at [RSP+0x170], which the decompiler renders as an
// `in_R9` phantom) -> +0x18 trans38. So c650's `dd70(0x13, a800, a6d0, _, a780)`
// means enter=a800, update=a6d0, per-frame=a780.
extern "C" char TradeCtrl_State13_Update(void* selfV, void* /*sm*/) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    if (*reinterpret_cast<int32_t*>(comp + 0x28) == 2) TradeComponent_AutoTrade(selfV);

    uint64_t smIBP[2] = {0, 0};
    void* found = s_FindStateMachine
        ? s_FindStateMachine(*reinterpret_cast<int64_t*>(comp + 0x80), smIBP) : nullptr;

    // 0x140003ea0 returns its OUT-PARAM, so `found` is non-null; the component is the
    // value inside it. Retail derefs the chain below unguarded because a merchant boat
    // always carries a StateMachineComponent. The guard is standalone-only and does NOT
    // skip the teardown that follows: if the boat is missing its machine we still have to
    // release the manifest, or the post keeps a merchant's stock list forever.
    const uintptr_t smComp = found ? *reinterpret_cast<uintptr_t*>(found) : 0;
    const uintptr_t machine = smComp ? smComp + 0x48 : 0;    // the embedded StateMachine
    const uintptr_t node = machine ? *reinterpret_cast<uintptr_t*>(machine) : 0;
    if (node) {
        // Node vtable slot 2 (+0x10) is PostEvent(node, sm, curStateIdx, eventId); the
        // current index is the machine's own byte at sm+9 (= smComp+0x51). Event 0 is
        // "you are released".
        typedef void (*PostEvent_t)(void*, void*, int, int);
        PostEvent_t post = *reinterpret_cast<PostEvent_t*>(*reinterpret_cast<uintptr_t*>(node) + 0x10);
        if (post)
            post(reinterpret_cast<void*>(node), reinterpret_cast<void*>(machine),
                 static_cast<int>(*reinterpret_cast<int8_t*>(smComp + 0x51)), 0);
    } else {
        std::fprintf(stderr, "[TradeDock] leaving merchant has NO StateMachineComponent — "
                             "the boat was not told to cast off; releasing the manifest anyway\n");
        std::fflush(stderr);
    }

    uint64_t clearIBP[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) new (clearIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *reinterpret_cast<uint64_t*>(comp + 0x80) = clearIBP[0];

    if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(comp + 0x50));
    *reinterpret_cast<uint64_t*>(comp + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
    *reinterpret_cast<int32_t*>(comp + 0x2c) = -1;
    return 1;
}
