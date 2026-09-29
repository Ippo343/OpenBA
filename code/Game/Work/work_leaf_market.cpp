// code/Game/Work/work_leaf_market.cpp
// Wave (Game-gap / Work-leaf) — Market/Trade leaf helpers.
//
// These are exe sub-helpers resolved via ResolveAddr(Module::Exe, 0xADDR) from
#include "Work/exe_bodies.h"
// code/Game/Work/marketwork.cpp and tradework.cpp. They resolve to 0 (silently
// skipped) unless registered in standalone/address_registry.cpp. This file provides faithful 1:1
// reimplementations so the standalone exe can fire real logic.
//
// SCOPE: this file currently contains ONLY the closure-safe recursive tree cleanup
// helper 0x14002af40 (its unreferenced sibling port of 0x140014b00 went in B.4: the
// live port is TradeWork_helper_14b00, Player/). Its ENTIRE exe sub-callee closure is:
//   - recursive self-calls (self-contained)
//   - System::MemoryAllocator::MemFree (a Runtime export) — reached here via
//     fn_MemFreeStatic, the canonical static wrapper, so no exe registration is needed.
// Both are therefore safe to register.
//
// The other six gap addresses in this batch are NOT ported (see batch report):
//   0x1400fb290 / 0x1400fac40 / 0x1400faaa0 / 0x1400fa9f0 — CALLER-BUGs: these land
//     MID-FUNCTION inside BridgeTool_OnRemove (entry 0x1400fadc0) and BridgeTool_Slot13
//     (entry 0x1400f9cd0); the marketwork.cpp typedefs point at wrong addresses.
//   0x1400eff80 — MarketWork_Serialize (the Slot4 reimpl target itself, 1408 lines);
//     huge unverified closure incl. LEAD-handled 0x140115e60 → DEFER.
//   0x14002c8a0 — TradeWork_helper_c8a0: calls unregistered TradeWork_helper_c710
//     (0x14002c710) → DEFER.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "system/MemoryAllocator.h"
#include "collection/Collection.h"
#include <cstdint>

// ============================================================
// 0x14002af40 — recursive market-tree node cleanup (frees node + its data buffer)
// ============================================================
// __fastcall FUN_14002af40(allocCtx param_1 [RCX], void* node [RDX], ...)
// Post-order recursion: free left child (node+0x20), free right child (node+0x28),
// then MemFree the data buffer at node+0x08, zero node+0x08 and node+0x10, and
// finally MemFree the node itself.  param_1 (the alloc context in RCX) is threaded
// through the recursion but is unused once we route frees through fn_MemFreeStatic
// (which fetches the singleton internally) — matches the exe's System::MemoryAllocator
// ::MemFree, called via dispatch slot [0x140152b68] in the original.
// Verified offset-by-offset against disassembly @0x14002af40.
// The node is a Collection::Set node over MarketWork's demand record (0x18 bytes:
// a key, then an inner Collection::Array {data, count, capacity}). LEFT: the
// caller (MarketWork, WORK-b) hands the node as void*.
namespace {
struct MarketDemand {
    void*    key;        // +0x00
    void*    data;       // +0x08 the inner array's data (freed here)
    int32_t  count;      // +0x10
    int32_t  capacity;   // +0x14
};
using MarketDemandNode = Collection::Set<MarketDemand>::Node;
static_assert(sizeof(MarketDemandNode) == 0x38, "left +0x20, right +0x28");
}  // namespace

void WorkLeaf_MarketTreeCleanup_2af40(void* ctx, void* node) {
    (void)ctx;
    if (node == nullptr) return;
    MarketDemandNode* n = reinterpret_cast<MarketDemandNode*>(node);
    WorkLeaf_MarketTreeCleanup_2af40(ctx, n->left);  // left
    WorkLeaf_MarketTreeCleanup_2af40(ctx, n->right); // right
    System::MemoryAllocator::MemFree(n->element.data); // data buffer
    n->element.data = nullptr;
    n->element.count = 0;      // retail: one qword store over count and capacity
    n->element.capacity = 0;
    System::MemoryAllocator::MemFree(node); // node
}
