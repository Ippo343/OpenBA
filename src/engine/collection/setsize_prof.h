#pragma once
// ============================================================================
// src/engine/collection/setsize_prof.h -- DRAWSORT target 1: SetSizeCore
// ============================================================================
//
// NOT RETAIL. Faithful perf work (docs/extensions/handins/DRAWSORT.md section 1).
//
// The kill switch of SetSizeCore's exact fast arm, and the COMPPROF counter that
// attributes SetSizeCore's element-loop iterations by spec and by
// (oldSize, newSize) class.
//
// THE FAST ARM. SetSizeCore's element loop runs i over [start, newSize). An
// iteration with i >= oldSize, no `fill` and no `spec->init` computes the slot
// address, tests it against null, and does nothing else: no load from the
// buffer, no store, no call. When fill and init are both null the loop can stop
// at min(newSize, oldSize) and the array holds the same bytes, the header the
// same values, and the allocator saw the same calls. The hot case is
// `Collection::Array<Geometry*>::Reserve(16384)` (retail 0x18005f980) on the
// fresh visible list of every `Scene::AddCulledDrawItems` call: 16384 empty
// iterations per cull pass. Retail's own instantiation of that body has no tail
// loop at all (array.cpp's banner on RtColl_ArrayReserve_18005f980: "the copy
// loop is bounded by the OLD size").
//
// THE COUNTER. Off unless the stress harness turns it on (COMPPROF >= 1), and
// then only on the thread the harness marks (the main thread, from its draw
// hook): SetSizeCore also runs on the loader and pathfinding threads, and the
// table below is not shared-safe. The record call is out of line and taken only
// when `g_setSizeProfOn` is set, so the faithful path pays one load and one
// predictable branch per SetSizeCore call.
// ============================================================================

#include <cstdint>

namespace rtcoll {

struct ArraySpec;

namespace SetSizeFast {
// The kill switch. false = the loop runs every iteration of [start, newSize)
// exactly as before this block.
inline constexpr bool kSkipEmptyTail = true;
}  // namespace SetSizeFast

// The (oldSize, newSize) classes of one SetSizeCore call that did not take the
// equal-size early return.
enum SetSizeClass : int {
    kSetSizeAppend = 0,   // newSize == oldSize + 1
    kSetSizeClear  = 1,   // newSize == 0
    kSetSizeGrow0  = 2,   // oldSize == 0, newSize > 1
    kSetSizeGrow   = 3,   // any other growth
    kSetSizeShrink = 4,   // any other shrink
    kSetSizeClasses = 5
};

struct SetSizeProfRow {
    const ArraySpec* spec;
    uint64_t calls[kSetSizeClasses];
    uint64_t iters[kSetSizeClasses];   // iterations of retail's loop [start, newSize)
    uint64_t emptyIters;               // of those, the ones that do nothing (the fast arm's skip)
    uint64_t reallocs;
    int32_t  maxNewSize;
};

// Set by the stress harness (COMPPROF >= 1). Read once per SetSizeCore call.
extern int g_setSizeProfOn;

// The harness calls this on the thread whose calls are to be counted.
void SetSizeProf_MarkThisThread();

// Out of line, cold: SetSizeCore calls it only when g_setSizeProfOn is set.
void SetSizeProf_Record(const ArraySpec* spec, int32_t oldSize, int32_t newSize,
                        int64_t iters, int64_t emptyIters, bool realloced);

// Copies the window's rows (largest iteration count first) into `out`, at most
// `maxRows`, returns how many, and resets the window. `totalCalls` /
// `totalIters` / `totalEmpty` get the window totals over every spec.
int SetSizeProf_Take(SetSizeProfRow* out, int maxRows, uint64_t* totalCalls,
                     uint64_t* totalIters, uint64_t* totalEmpty);

// The spec's debug name ("Array<ptr>", ...), for the report.
const char* SetSizeProf_SpecName(const ArraySpec* spec);

}  // namespace rtcoll
