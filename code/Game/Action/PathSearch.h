#pragma once
// ============================================================================
// code/Game/Action/PathSearch.h — ENDGAME B.4 W1 block ACT-a
//
// The record structs the pathfinder bodies (`action_pf_*.cpp`) read: the node
// pool, the pooled cell queue, the A* open set (an AVL tree), the per-cell search
// node, the search context both A* runs share, the grid search the worker thread
// owns (PathfinderThread +0x78), its command and result records, and the coarse
// region graph (Map +0x70).
//
// NAMES. Retail has no symbols for any of these and the profile build lists no
// pathfinder source file of the exe; every struct and field name below is
// DESCRIPTIVE and UNCONFIRMED, taken from what retail does with the field. Offsets,
// widths and signedness are retail's (the instruction that reads / writes each
// field is named on it).
//
// OWNERSHIP. None of these records is constructed or destroyed by a C++
// constructor: retail builds them in raw memory with field stores (the grid
// search in the pathfinder's ctor 0x1400a67f0 / 0x1400a6110, the region graph in
// 0x1400a8670, the nodes by the pool allocators below). The members that are
// embedded in `PathfinderThread` are therefore trivially constructible and
// destructible (its dtor 0x1400a6720 frees them by hand; a member destructor
// would free them twice). Arrays there are declared as retail's three raw fields
// {data, size, capacity} of `Collection::Array<T>`.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "collection/Collection.h"

class Map;

// ---------------------------------------------------------------------------
// The node pool: nodes carved out of chunks, a free list threaded through the
// first qword of each free node. Retail's three allocators 0x14001a9b0 (queue
// node, stride 0x18), 0x14001df40 (queue node, stride 0x18) and 0x14001d730
// (open-set node, stride 0x30) grow it identically: growCount doubles, one chunk
// of growCount * stride bytes (+ the 8-byte chunk link) is allocated and every
// node of it is pushed on the free list.
// ---------------------------------------------------------------------------
struct PathPoolLink {
    PathPoolLink* next;          // +0x00 chunk list / free list link
};
static_assert(sizeof(PathPoolLink) == 8, "PathPoolLink is one qword");

struct PathNodePool {
    PathPoolLink* chunks;        // +0x00 chunk list (0x14001a9b0: `*chunk = pool->chunks`)
    PathPoolLink* freeList;      // +0x08 free nodes
    int32_t  growCount;          // +0x10 nodes per new chunk, doubled before each grow (ctor 8)
    int32_t  stride;             // +0x14 node size; 0 until the first grow sets it
    int32_t  total;              // +0x18 nodes ever carved
    int32_t  inUse;              // +0x1C +1 on a pop, -1 on a push back
    int32_t  available;          // +0x20 -1 on a pop, +1 on a push back
    uint8_t  _pad24[4];          // +0x24
};
static_assert(sizeof(PathNodePool) == 0x28, "PathNodePool is 0x28");
static_assert(offsetof(PathNodePool, chunks) == 0x00, "PathNodePool::chunks @ +0x00");
static_assert(offsetof(PathNodePool, freeList) == 0x08, "PathNodePool::freeList @ +0x08");
static_assert(offsetof(PathNodePool, growCount) == 0x10, "PathNodePool::growCount @ +0x10");
static_assert(offsetof(PathNodePool, stride) == 0x14, "PathNodePool::stride @ +0x14");
static_assert(offsetof(PathNodePool, total) == 0x18, "PathNodePool::total @ +0x18");
static_assert(offsetof(PathNodePool, inUse) == 0x1C, "PathNodePool::inUse @ +0x1C");
static_assert(offsetof(PathNodePool, available) == 0x20, "PathNodePool::available @ +0x20");
static_assert(std::is_trivially_copyable<PathNodePool>::value, "raw record");

// ---------------------------------------------------------------------------
// The pooled cell queue: a doubly linked FIFO whose nodes come from a pool.
// Pushed at the TAIL (+0x00), popped at the HEAD (+0x08). Two instances:
//   Map +0xA8 (the flood fill 0x14009c170, value = one cell index, the pool
//             allocator 0x14001a9b0 writes only its low dword)
//   RegionGraph +0xC8 (the label flood fill 0x1400a7ad0, value = packed (x, y),
//             allocator 0x14001df40 writes the whole qword)
// ---------------------------------------------------------------------------
template <class T>
struct PathCellQueue {
    struct Node {
        T     value;             // +0x00
        Node* next;              // +0x08 toward the tail
        Node* prev;              // +0x10 toward the head
    };
    Node*    tail;               // +0x00
    Node*    head;               // +0x08
    int32_t  count;              // +0x10
    uint8_t  _pad14[4];          // +0x14
    PathNodePool pool;           // +0x18
};

// The (x, y) pair 0x1400a7ad0 queues: x in the low dword, y in the high dword.
struct PathCellXY {
    int32_t x;                   // +0x00
    int32_t y;                   // +0x04
};
static_assert(sizeof(PathCellXY) == 8, "PathCellXY is one qword");

// Map's queue holds one cell index; the node's high dword of the first qword is
// never written (0x14001a9b0 stores a dword).
struct PathCellIndex {
    int32_t  cell;               // +0x00
    uint8_t  _pad04[4];          // +0x04 not written by 0x14001a9b0
};
static_assert(sizeof(PathCellIndex) == 8, "PathCellIndex is one qword");

using MapCellQueue    = PathCellQueue<PathCellIndex>;
using RegionCellQueue = PathCellQueue<PathCellXY>;

static_assert(sizeof(MapCellQueue) == 0x40, "the pooled queue is 0x40 (Map +0xA8..+0xE8)");
static_assert(offsetof(MapCellQueue, tail) == 0x00, "queue tail @ +0x00");
static_assert(offsetof(MapCellQueue, head) == 0x08, "queue head @ +0x08");
static_assert(offsetof(MapCellQueue, count) == 0x10, "queue count @ +0x10");
static_assert(offsetof(MapCellQueue, pool) == 0x18, "queue pool @ +0x18");
static_assert(sizeof(MapCellQueue::Node) == 0x18, "queue node stride 0x18 (0x14001a9b0)");
static_assert(offsetof(MapCellQueue::Node, next) == 0x08, "queue node next @ +0x08");
static_assert(offsetof(MapCellQueue::Node, prev) == 0x10, "queue node prev @ +0x10");
static_assert(sizeof(RegionCellQueue) == 0x40, "the pooled queue is 0x40 (RegionGraph +0xC8)");
static_assert(sizeof(RegionCellQueue::Node) == 0x18, "queue node stride 0x18 (0x14001df40)");
static_assert(std::is_trivially_copyable<MapCellQueue>::value, "raw record");

// ---------------------------------------------------------------------------
// The A* open set: an AVL tree ordered by (fcost, seq), nodes from its own pool.
// Node layout from the allocator 0x14001d730 and the delete 0x14001d550.
// ---------------------------------------------------------------------------
struct PathOpenKey {             // the 3-int key 0x14001d450 / 0x14001d730 read
    int32_t cell;                // +0x00
    int32_t seq;                 // +0x04 tie-break (the search's sequence counter)
    int32_t fcost;               // +0x08 sort key
};
static_assert(sizeof(PathOpenKey) == 0x0C, "PathOpenKey is three ints");

struct PathOpenNode {
    int32_t       cell;          // +0x00
    int32_t       seq;           // +0x04
    int32_t       fcost;         // +0x08
    uint8_t       _pad0C[4];     // +0x0C not written by 0x14001d730
    PathOpenNode* parent;        // +0x10
    PathOpenNode* left;          // +0x18
    PathOpenNode* right;         // +0x20
    int16_t       balance;       // +0x28 height(right) - height(left)
    int16_t       height;        // +0x2A
    uint8_t       _pad2C[4];     // +0x2C
};
static_assert(sizeof(PathOpenNode) == 0x30, "open-set node stride 0x30 (0x14001d730)");
static_assert(offsetof(PathOpenNode, cell) == 0x00, "PathOpenNode::cell @ +0x00");
static_assert(offsetof(PathOpenNode, seq) == 0x04, "PathOpenNode::seq @ +0x04");
static_assert(offsetof(PathOpenNode, fcost) == 0x08, "PathOpenNode::fcost @ +0x08");
static_assert(offsetof(PathOpenNode, parent) == 0x10, "PathOpenNode::parent @ +0x10");
static_assert(offsetof(PathOpenNode, left) == 0x18, "PathOpenNode::left @ +0x18");
static_assert(offsetof(PathOpenNode, right) == 0x20, "PathOpenNode::right @ +0x20");
static_assert(offsetof(PathOpenNode, balance) == 0x28, "PathOpenNode::balance @ +0x28");
static_assert(offsetof(PathOpenNode, height) == 0x2A, "PathOpenNode::height @ +0x2A");

struct PathOpenSet {
    PathOpenNode* root;          // +0x00
    int32_t       count;         // +0x08
    uint8_t       _pad0C[4];     // +0x0C
    PathNodePool  pool;          // +0x10
};
static_assert(sizeof(PathOpenSet) == 0x38, "PathOpenSet is 0x38");
static_assert(offsetof(PathOpenSet, root) == 0x00, "PathOpenSet::root @ +0x00");
static_assert(offsetof(PathOpenSet, count) == 0x08, "PathOpenSet::count @ +0x08");
static_assert(offsetof(PathOpenSet, pool) == 0x10, "PathOpenSet::pool @ +0x10");

// ---------------------------------------------------------------------------
// One search node per graph vertex (grid cell or region), stride 0x18; the array
// is resized by 0x14001d830, whose new-element fill is {0, -1, 0, 0, 0}.
// A node is OPEN while openStamp == the search's generation and CLOSED while
// closedStamp == it (0x14001d040 / 0x14001d320 decrement a stamp to clear it).
// ---------------------------------------------------------------------------
struct PathSearchNode {
    PathOpenNode* openNode;      // +0x00 this vertex's open-set entry
    int32_t       parent;        // +0x08 predecessor vertex, -1 for none
    int32_t       openStamp;     // +0x0C
    int32_t       closedStamp;   // +0x10
    int32_t       gCost;         // +0x14 cost so far
};
static_assert(sizeof(PathSearchNode) == 0x18, "search node stride 0x18 (0x14001d830)");
static_assert(offsetof(PathSearchNode, openNode) == 0x00, "PathSearchNode::openNode @ +0x00");
static_assert(offsetof(PathSearchNode, parent) == 0x08, "PathSearchNode::parent @ +0x08");
static_assert(offsetof(PathSearchNode, openStamp) == 0x0C, "PathSearchNode::openStamp @ +0x0C");
static_assert(offsetof(PathSearchNode, closedStamp) == 0x10, "PathSearchNode::closedStamp @ +0x10");
static_assert(offsetof(PathSearchNode, gCost) == 0x14, "PathSearchNode::gCost @ +0x14");

// ---------------------------------------------------------------------------
// The search context both A* variants share: the grid search (0x14001d040, run
// by 0x14001ce00) and the region search (0x14001dcd0, run by 0x14001dc00). Both
// owners start with these 0x50 bytes (ctors 0x1400a6110 and 0x1400a8670 write
// the same stores: generation 1, the pool's growCount 8).
// ---------------------------------------------------------------------------
struct PathSearch {
    int32_t         generation;    // +0x00 bumped before each run
    int32_t         sequence;      // +0x04 open-set tie-break counter
    PathSearchNode* nodes;         // +0x08 Collection::Array<PathSearchNode> data
    int32_t         nodeCount;     // +0x10 ... size
    int32_t         nodeCapacity;  // +0x14 ... capacity
    PathOpenSet     open;          // +0x18
};
static_assert(sizeof(PathSearch) == 0x50, "PathSearch is 0x50");
static_assert(offsetof(PathSearch, generation) == 0x00, "PathSearch::generation @ +0x00");
static_assert(offsetof(PathSearch, sequence) == 0x04, "PathSearch::sequence @ +0x04");
static_assert(offsetof(PathSearch, nodes) == 0x08, "PathSearch::nodes @ +0x08");
static_assert(offsetof(PathSearch, nodeCount) == 0x10, "PathSearch::nodeCount @ +0x10");
static_assert(offsetof(PathSearch, nodeCapacity) == 0x14, "PathSearch::nodeCapacity @ +0x14");
static_assert(offsetof(PathSearch, open) == 0x18, "PathSearch::open @ +0x18");
static_assert(std::is_trivially_copyable<PathSearch>::value, "raw record");

// ---------------------------------------------------------------------------
// The grid search the worker thread owns (PathfinderThread +0x78, 0xE8 bytes,
// ctor 0x1400a6110). The command dispatcher 0x1400a6430 writes every field after
// `search`; 0x1400a5ec0 (neighbour), 0x1400a6030 (heuristic) and 0x14001d040
// (A* core) read them.
// ---------------------------------------------------------------------------
struct GridPathSearch {
    PathSearch search;            // +0x00
    int32_t    stepCost[0x20];    // +0x50 per terrain type (command 1)
    uint32_t   passableTypes;     // +0xD0 bit per terrain type (command 0; 0x1400a5ec0 tests it)
    uint8_t    allowDiagonal;     // +0xD4 command 4 arg 1: eight neighbours, octile heuristic
    uint8_t    manhattan;         // +0xD5 command 4 arg 2: Manhattan heuristic
    uint8_t    respectBlocked;    // +0xD6 command 4 arg 3: skip cells whose flag bit 7 is set
    uint8_t    _padD7;            // +0xD7
    int32_t    width;             // +0xD8 grid edge in cells
    uint8_t    _padDC[4];         // +0xDC
    uint8_t*   cellFlags;         // +0xE0 per cell: terrain type (bits 0-6), blocked (bit 7)
};
static_assert(sizeof(GridPathSearch) == 0xE8, "GridPathSearch is 0xE8 (PathfinderThread +0x78..+0x160)");
static_assert(offsetof(GridPathSearch, search) == 0x00, "GridPathSearch::search @ +0x00");
static_assert(offsetof(GridPathSearch, stepCost) == 0x50, "GridPathSearch::stepCost @ +0x50");
static_assert(offsetof(GridPathSearch, passableTypes) == 0xD0, "GridPathSearch::passableTypes @ +0xD0");
static_assert(offsetof(GridPathSearch, allowDiagonal) == 0xD4, "GridPathSearch::allowDiagonal @ +0xD4");
static_assert(offsetof(GridPathSearch, manhattan) == 0xD5, "GridPathSearch::manhattan @ +0xD5");
static_assert(offsetof(GridPathSearch, respectBlocked) == 0xD6, "GridPathSearch::respectBlocked @ +0xD6");
static_assert(offsetof(GridPathSearch, width) == 0xD8, "GridPathSearch::width @ +0xD8");
static_assert(offsetof(GridPathSearch, cellFlags) == 0xE0, "GridPathSearch::cellFlags @ +0xE0");
static_assert(std::is_trivially_copyable<GridPathSearch>::value, "raw record");

// ---------------------------------------------------------------------------
// A worker command: the 0x10 bytes every enqueuer (0x1400a6fe0, 0x1400a6a30,
// 0x1400a6b20, 0x1400a68b0, 0x1400a6c10) builds on its stack and hands to the
// dispatcher 0x1400a6430 by address (RDX), and the queued node (MemAlloc(0x20,
// 8)) that carries a copy of it until the worker 0x1400a6360 pops and frees it.
// type 0..5, see 0x1400a6430.
// ---------------------------------------------------------------------------
struct PathCommandArgs {
    int32_t type;                // +0x00
    int32_t arg1;                // +0x04
    int32_t arg2;                // +0x08
    int32_t arg3;                // +0x0C
};
static_assert(sizeof(PathCommandArgs) == 0x10, "PathCommandArgs is four ints");
static_assert(offsetof(PathCommandArgs, type) == 0x00, "PathCommandArgs::type @ +0x00");
static_assert(offsetof(PathCommandArgs, arg1) == 0x04, "PathCommandArgs::arg1 @ +0x04");
static_assert(offsetof(PathCommandArgs, arg2) == 0x08, "PathCommandArgs::arg2 @ +0x08");
static_assert(offsetof(PathCommandArgs, arg3) == 0x0C, "PathCommandArgs::arg3 @ +0x0C");

struct PathCommand {
    PathCommandArgs args;        // +0x00
    PathCommand*    next;        // +0x10 toward the tail
    PathCommand*    prev;        // +0x18 toward the head
};
static_assert(sizeof(PathCommand) == 0x20, "PathCommand is MemAlloc(0x20)");
static_assert(offsetof(PathCommand, args) == 0x00, "PathCommand::args @ +0x00");
static_assert(offsetof(PathCommand, next) == 0x10, "PathCommand::next @ +0x10");
static_assert(offsetof(PathCommand, prev) == 0x18, "PathCommand::prev @ +0x18");

// ---------------------------------------------------------------------------
// A path request's result slot (PathfinderThread +0x160, stride 0x18): allocated
// by 0x1400a6240 (token != 0 while claimed), filled by command 5 (0x1400a6430),
// read and released by 0x1400a6d50. The array is resized by 0x14001d940.
// ---------------------------------------------------------------------------
struct PathResult {
    uint8_t  ready;              // +0x00 set to 1 once `path` is written
    uint8_t  _pad01[3];          // +0x01
    int32_t  token;              // +0x04 0 = free slot
    Collection::Array<int32_t> path;   // +0x08 cell indices, start first
};
static_assert(sizeof(PathResult) == 0x18, "result slot stride 0x18 (0x14001d940)");
static_assert(offsetof(PathResult, ready) == 0x00, "PathResult::ready @ +0x00");
static_assert(offsetof(PathResult, token) == 0x04, "PathResult::token @ +0x04");
static_assert(offsetof(PathResult, path) == 0x08, "PathResult::path @ +0x08");

// The {slot, token} pair 0x1400a6240 / 0x1400a6c10 hand back to the requester.
struct PathTicket {
    int32_t slot;                // +0x00
    int32_t token;               // +0x04
};
static_assert(sizeof(PathTicket) == 8, "PathTicket is two ints");

// ---------------------------------------------------------------------------
// The coarse region graph (Map +0x70; MemAlloc(0x108) + ctor 0x1400a8670). The
// map is cut into blocks of blockWidth x blockHeight cells (both 8); each block's
// cells are flood-filled into regions (0x1400a7f20 / 0x1400a7ad0), each region
// is a graph vertex with a centroid and an adjacency list, and the region A*
// (0x14001dc00) runs over them.
// ---------------------------------------------------------------------------
struct RegionNode {                          // stride 0x30 (0x14001e210)
    int32_t x;                               // +0x00 centroid, in cells
    int32_t y;                               // +0x04
    int32_t block;                           // +0x08 owning block, -1 = free (0x1400a8200)
    uint8_t _pad0C[4];                       // +0x0C
    Collection::Array<int32_t> neighbours;   // +0x10 adjacent region ids
    Collection::Array<int32_t> borderCells;  // +0x20 cells just outside the block
};
static_assert(sizeof(RegionNode) == 0x30, "RegionNode stride 0x30 (0x14001e210)");
static_assert(offsetof(RegionNode, x) == 0x00, "RegionNode::x @ +0x00");
static_assert(offsetof(RegionNode, y) == 0x04, "RegionNode::y @ +0x04");
static_assert(offsetof(RegionNode, block) == 0x08, "RegionNode::block @ +0x08");
static_assert(offsetof(RegionNode, neighbours) == 0x10, "RegionNode::neighbours @ +0x10");
static_assert(offsetof(RegionNode, borderCells) == 0x20, "RegionNode::borderCells @ +0x20");

struct RegionBlock {                         // stride 0x20 (0x14001e060)
    int32_t x;                               // +0x00 in blocks
    int32_t y;                               // +0x04
    uint8_t needsRebuild;                    // +0x08 queued on RegionGraph::dirtyRebuild
    uint8_t needsAdjacency;                  // +0x09 queued on RegionGraph::dirtyAdjacency
    uint8_t _pad0A[6];                       // +0x0A
    Collection::Array<int32_t> regions;      // +0x10 region ids in this block
};
static_assert(sizeof(RegionBlock) == 0x20, "RegionBlock stride 0x20 (0x14001e060)");
static_assert(offsetof(RegionBlock, x) == 0x00, "RegionBlock::x @ +0x00");
static_assert(offsetof(RegionBlock, y) == 0x04, "RegionBlock::y @ +0x04");
static_assert(offsetof(RegionBlock, needsRebuild) == 0x08, "RegionBlock::needsRebuild @ +0x08");
static_assert(offsetof(RegionBlock, needsAdjacency) == 0x09, "RegionBlock::needsAdjacency @ +0x09");
static_assert(offsetof(RegionBlock, regions) == 0x10, "RegionBlock::regions @ +0x10");

struct RegionGraph {
    PathSearch  search;                      // +0x00
    Map*        map;                         // +0x50
    int32_t     blockWidth;                  // +0x58 cells per block (ctor 8)
    int32_t     blockHeight;                 // +0x5C (ctor 8)
    int32_t     blocksWide;                  // +0x60 blocks per map row
    int32_t     blocksHigh;                  // +0x64
    RegionNode* regions;                     // +0x68 Collection::Array<RegionNode> data
    int32_t     regionCount;                 // +0x70 ... size
    int32_t     regionCapacity;              // +0x74 ... capacity
    RegionBlock* blocks;                     // +0x78 Collection::Array<RegionBlock> data
    int32_t     blockCount;                  // +0x80
    int32_t     blockCapacity;               // +0x84
    Collection::Array<int32_t> dirtyRebuild;     // +0x88 blocks whose regions are rebuilt
    Collection::Array<int32_t> dirtyAdjacency;   // +0x98 blocks whose adjacency is rebuilt
    Collection::Array<int32_t> freeRegions;      // +0xA8 recycled region ids
    Collection::Array<int32_t> labels;           // +0xB8 per cell of one block: flood label
    RegionCellQueue queue;                   // +0xC8 the label flood fill's work queue
};
static_assert(sizeof(RegionGraph) == 0x108, "RegionGraph is MemAlloc(0x108)");
static_assert(offsetof(RegionGraph, search) == 0x00, "RegionGraph::search @ +0x00");
static_assert(offsetof(RegionGraph, map) == 0x50, "RegionGraph::map @ +0x50");
static_assert(offsetof(RegionGraph, blockWidth) == 0x58, "RegionGraph::blockWidth @ +0x58");
static_assert(offsetof(RegionGraph, blockHeight) == 0x5C, "RegionGraph::blockHeight @ +0x5C");
static_assert(offsetof(RegionGraph, blocksWide) == 0x60, "RegionGraph::blocksWide @ +0x60");
static_assert(offsetof(RegionGraph, blocksHigh) == 0x64, "RegionGraph::blocksHigh @ +0x64");
static_assert(offsetof(RegionGraph, regions) == 0x68, "RegionGraph::regions @ +0x68");
static_assert(offsetof(RegionGraph, regionCount) == 0x70, "RegionGraph::regionCount @ +0x70");
static_assert(offsetof(RegionGraph, regionCapacity) == 0x74, "RegionGraph::regionCapacity @ +0x74");
static_assert(offsetof(RegionGraph, blocks) == 0x78, "RegionGraph::blocks @ +0x78");
static_assert(offsetof(RegionGraph, blockCount) == 0x80, "RegionGraph::blockCount @ +0x80");
static_assert(offsetof(RegionGraph, blockCapacity) == 0x84, "RegionGraph::blockCapacity @ +0x84");
static_assert(offsetof(RegionGraph, dirtyRebuild) == 0x88, "RegionGraph::dirtyRebuild @ +0x88");
static_assert(offsetof(RegionGraph, dirtyAdjacency) == 0x98, "RegionGraph::dirtyAdjacency @ +0x98");
static_assert(offsetof(RegionGraph, freeRegions) == 0xA8, "RegionGraph::freeRegions @ +0xA8");
static_assert(offsetof(RegionGraph, labels) == 0xB8, "RegionGraph::labels @ +0xB8");
static_assert(offsetof(RegionGraph, queue) == 0xC8, "RegionGraph::queue @ +0xC8");
