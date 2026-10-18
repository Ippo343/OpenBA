#pragma once
// src/game/Entity/path_cost_index.h -- NOT RETAIL (HITCH_PAIRSPREAD). The PathGraph's
// region-pair cost map, stored as a flat hash table instead of retail's AVL tree at
// PathGraph+0x18 (PathCostTreeInsert 0x140025a10, nodes of 0x30 bytes).
//
// WHY IT IS THE SAME. Retail uses the tree as a map and nothing else: it is searched by
// (high, low) (ProcessPaths 0x1400d6f00, BuildPathGraph 0x1400d70c0's de-duplication,
// RegionPairCost 0x1400d6060), inserted into when the key is absent, its cost overwritten
// (ProcessPaths), and freed whole (BuildPathGraph's step 2, ArrayPlusTree_Clear 0x1400171b0).
// No body walks it in order, reads its count, or saves it. The keys are unique under a
// strict (high, low) order, so a map that answers the same membership and the same value
// for every key is the same state. The AVL costs a MemAlloc and a rebalance per insert,
// ~14 dependent node reads per lookup and a MemFree per node at every rebuild (9,902 pairs
// in the stress town: ~2 ms in one tick at the rebuild, ~2 ms of lookups at the drain).
//
// With the index on, PathGraph::m_bst stays empty (root null, count 0). At COMPPROF=4
// (g_verifyOn) every operation also runs on the retail tree in m_bst and every lookup is
// compared (StressHarness_Verify "PathCostIndex").
#include <cstdint>

struct PathGraph;

namespace PathCostIndex {
// The cost slot of (high, low), or nullptr when the pair is absent.
int32_t* Find(const PathGraph* pg, int high, int low);
// Insert-or-find, as PathCostTreeInsert: an absent key gets `value`, a present one keeps its
// own. Returns the slot.
int32_t* Insert(const PathGraph* pg, int high, int low, int32_t value);
// Every pair gone (O(1)).
void Clear(const PathGraph* pg);
int  Count();
}  // namespace PathCostIndex
