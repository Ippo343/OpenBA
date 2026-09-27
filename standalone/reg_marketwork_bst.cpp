// standalone/reg_marketwork_bst.cpp
// Wave (MarketWork BST family) — registers the 12 exe helpers that MarketWork's
// vtable slots and its Slot4 trading logic reach.
//
// WHY THIS WAVE EXISTS
// Until 2026-08-22 every address below resolved to 0 in standalone. That was benign
// only for as long as nothing minted a MarketWork: MarketComponent's slot 18
// (CreateWork, 0x14004ea40) was itself unported, so state 13 never created one. Once
// slot 18 landed, MarketWork objects started being posted to the citizen reservation
// ledger — and MarketWork's vtable slots 6/7/9 and Serialize call three of these
// helpers through UNGUARDED function pointers. Registering them is what keeps those
// live slots from calling address 0.
//
// ⚠ THE "7-FUNCTION AVL/BST BLOCK" NOTE WAS WRONG IN TWO WAYS.
//  1. It UNDERCOUNTED. The family is 12 functions, not 7: aaa0 / ac40 / aee0 (erase)
//     and b290 / b5f0 (v1 serialize) were omitted, and those five are precisely the
//     ones reachable from already-live vtable slots.
//  2. It MISNAMED. Only about half of this "AVL block" is tree code at all:
//       0x14002afb0            — dynamic-array GROW (0x20-byte elements), not a tree
//       0x14002b680 / 0x14002bd80 — MSVC CRT qsort + shortsort, 0x20-byte records,
//                                 ascending by the float at record+0x14
//       0x14002ba70            — the same qsort instantiated for 0x18-byte records,
//                                 ascending by the SIGNED int32 at record+0x08
//     Ghidra named them all "MarketWork_helper_*" purely because they sit in the
//     address range between the genuine tree helpers. The export names below keep the
//     MarketBst_ prefix so this file binds unchanged; treat it as a label, not a
//     description, and read each header for what the function really is.
//
// It was never actually blocked. The whole support closure — 0x14000d9d0,
// 0x14000da70, 0x140025920, 0x140002340, 0x140014ed0, 0x140015000 — was already
// ported and registered by earlier waves, and MemAlloc/MemFree/CheckPtr/IsDerivedFrom
// come from the Runtime. The only thing missing was the 12 bodies themselves.
//
// Node layout shared by the genuine tree helpers (0x38 bytes):
//   +0x00 key (InternalBasePtr, also the unsigned sort key)
//   +0x08 payload array { ptr@+0x08, count@+0x10 int32, cap@+0x14 int32 }
//   +0x18 parent   +0x20 left   +0x28 right   +0x30 balance
// Tree header (MarketWork+0x48): +0x00 root, +0x08 count (int32).

#include "resolve.h"
#include <cstdint>
#include "Work/marketwork_bst_a9f0.h"
#include "Work/marketwork_bst_aaa0.h"
#include "Work/marketwork_bst_afb0.h"
#include "Work/marketwork_bst_b290.h"
#include "Work/marketwork_bst_b680.h"
#include "Work/marketwork_bst_ba70.h"

void RegisterMarketWorkBstBatch() {
    // ── genuine AVL tree: insert-with-payload cluster ────────────────────────────
    // a9f0 takes FOUR args (tree, out, key, vec) — the old typedef in marketwork.cpp
    // declared two, leaving R8/R9 garbage. Fixed alongside this wave.
    RegisterExeAddr(0x14002a9f0, reinterpret_cast<uintptr_t>(&MarketBst_a9f0));

    // ── genuine AVL tree: erase cluster ──────────────────────────────────────────
    // ⚠ aaa0 is ERASE-BY-KEY, not insert. Retail vtable slot 6 (0x1400eff60) AND
    // slot 7 (0x1400eff50) are both 1-line thunks onto it; our Slot7 was calling
    // ac40 (erase-by-NODE) instead, which would have read +0x18/+0x20/+0x28 off an
    // InternalBasePtr and unlinked garbage. Fixed alongside this wave.
    RegisterExeAddr(0x14002aaa0, reinterpret_cast<uintptr_t>(&MarketBst_aaa0));
    RegisterExeAddr(0x14002ac40, reinterpret_cast<uintptr_t>(&MarketBst_ac40));

    // ── v1 save format (slot 3 Serialize, 0x1400f1d70, takes this arm when
    //    IO::Serializer::GetVersion() != 0; v0 streams migrate via a9f0 instead) ──
    RegisterExeAddr(0x14002b290, reinterpret_cast<uintptr_t>(&MarketBst_b290));

    // ── NOT tree code: dynamic-array grow, 0x20-byte elements ────────────────────
    RegisterExeAddr(0x14002afb0, reinterpret_cast<uintptr_t>(&MarketBst_afb0));

    // ── NOT tree code: qsort over 0x20-byte records, key = float at +0x14 ─────────
    RegisterExeAddr(0x14002b680, reinterpret_cast<uintptr_t>(&MarketBst_b680));

    // ── NOT tree code: qsort over 0x18-byte records, key = int32 at +0x08 ─────────
    RegisterExeAddr(0x14002ba70, reinterpret_cast<uintptr_t>(&MarketBst_ba70));
}
