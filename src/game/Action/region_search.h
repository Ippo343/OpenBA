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
