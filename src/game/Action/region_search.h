#pragma once
// src/game/Action/region_search.h -- read-outs of RegionGraph::FindPath 0x14001dcd0's
// bucket open set (action_pf_region_search.cpp; PATHDIST). Not retail: retail's open set
// is the AVL PathOpenSet and keeps no counters. For the unit tests and the COMPPROF=4
// verification shadow only; the search never reads them.
#include <cstddef>

namespace RegionSearch {
// Vertices the last RegionGraph::FindPath closed (pops other than the goal's).
int    LastExpanded();
// The open set's retained scratch (bucket heads + entry pool capacity), in bytes.
size_t ScratchBytes();
}  // namespace RegionSearch

// ── HITCH_PAIRSPREAD: the region path-length memo (NOT RETAIL) ─────────────────────────
// RegionGraph::PathLength 0x1400a8ba0 is a pure function of (start, goal) and the region
// graph (centroids x / y and neighbour lists): the search's stamps are compared for
// equality with a fresh generation only, so nothing a previous search left changes the
// result. Every write to the graph happens inside RegionGraph::Flush 0x1400a8a90 (Init only
// marks the blocks dirty), so a result stays valid until the next Flush that rebuilds
// something. The memo keeps PathLength's sum per (graph, start, goal) and is dropped
// whole at each such Flush (NoteGraphChanged) or when another graph asks. PathLength
// consults it after its own Flush and before the search; at COMPPROF=4 a hit still runs
// the search and compares (StressHarness_Verify "PathLengthMemo").
namespace RegionSearch {
struct MemoStats {
    long long lookups;        // PathLength calls (after the Flush)
    long long hits;           // answered from the memo
    long long graphChanges;   // Flushes that rebuilt something (each drops the memo)
    long long full;           // drops because the table reached its cap
};
const MemoStats& Stats();
void NoteGraphChanged();
bool MemoFind(const void* graph, int start, int goal, int* sum);
void MemoStore(const void* graph, int start, int goal, int sum);
}  // namespace RegionSearch
