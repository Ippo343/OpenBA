#pragma once
// EntityList — binary-compatible struct for the entity management system.
//
// EntityList manages all ComponentEntity instances in the game world.
// It maintains a master array of entity IBPs, 5 update buckets for round-robin
// component ticking, two binary trees (update tree and post-update tree),
// and staging/removal arrays for deferred add/remove operations.
//
// Size: 0xE0 bytes (verified from field access patterns in decompiled functions)
//
// Key functions:
//   PreUpdate  (0x1400c3bd0) — process staging arrays into trees
//   Update     (0x1400c4090) — main tick: tree walk + bucket round-robin
//   PostUpdate (0x1400c43d0) — post-update tree walk
//   Serialize  (0x1400c44a0) — save/load
//   RemoveEntity (0x1400c38e0) — remove entity from buckets + trees
//   FlagAggregation (0x1400c3470) — per-entity flag OR across components
//
// Decompiled from Application-x64.exe using Ghidra.

#include <cstdint>
#include <cstddef>

// Forward declaration
struct EntityList;

// IBPArray — dynamic array of InternalBasePtr (8-byte elements)
// Layout: +0x00 = void* data, +0x08 = int32_t count, +0x0C = int32_t capacity
struct IBPArray {
    void*    data;      // +0x00: pointer to array of 8-byte IBP entries
    int32_t  count;     // +0x08: current element count
    int32_t  capacity;  // +0x0C: allocated capacity
};
static_assert(sizeof(IBPArray) == 0x10, "IBPArray must be 0x10 bytes");
static_assert(offsetof(IBPArray, data) == 0x00, "IBPArray::data at +0x00");
static_assert(offsetof(IBPArray, count) == 0x08, "IBPArray::count at +0x08");
static_assert(offsetof(IBPArray, capacity) == 0x0C, "IBPArray::capacity at +0x0C");

// EntityTreeNode — AVL binary tree node for entity iteration
// Layout: +0x00 = entity IBP, +0x08 = parent, +0x10 = left, +0x18 = right, +0x20 = balance info
// Allocated as 0x28 bytes by the tree insert function (0x140021a30).
struct EntityTreeNode {
    uint64_t     entityIBP;  // +0x00: InternalBasePtr to ComponentEntity
    EntityTreeNode* parent;  // +0x08: parent node
    EntityTreeNode* left;    // +0x10: left child
    EntityTreeNode* right;   // +0x18: right child
    int16_t      balance;    // +0x20: AVL balance factor
    int16_t      height;     // +0x22: AVL subtree height
    uint32_t     _pad24;     // +0x24: padding to 0x28
};
static_assert(sizeof(EntityTreeNode) == 0x28, "EntityTreeNode must be 0x28 bytes");
static_assert(offsetof(EntityTreeNode, entityIBP) == 0x00, "EntityTreeNode::entityIBP at +0x00");
static_assert(offsetof(EntityTreeNode, parent) == 0x08, "EntityTreeNode::parent at +0x08");
static_assert(offsetof(EntityTreeNode, left) == 0x10, "EntityTreeNode::left at +0x10");
static_assert(offsetof(EntityTreeNode, right) == 0x18, "EntityTreeNode::right at +0x18");

// UpdateBucket — one of 5 round-robin update buckets
// Each bucket is 16 bytes: pointer to IBP array + count (int64 for alignment)
struct UpdateBucket {
    int64_t* data;     // +0x00: pointer to array of entity IBPs (8-byte each)
    int32_t  count;    // +0x08: number of entities in this bucket
    int32_t  _pad0C;   // +0x0C: padding/capacity
};
static_assert(sizeof(UpdateBucket) == 0x10, "UpdateBucket must be 0x10 bytes");

// EntityList — main entity management structure
// Size: 0xE0 bytes
struct EntityList {
    int64_t         accumulatedTime;    // +0x00: accumulated simulation time
    IBPArray        masterArray;        // +0x08: master array of all entity IBPs {data, count, cap}
    UpdateBucket    buckets[5];         // +0x18: 5 round-robin update buckets (each 0x10)
    EntityTreeNode* updateTreeRoot;     // +0x68: root of binary tree for per-tick entity processing
    int64_t         updateTreeCount;    // +0x70: number of nodes in update tree
    EntityTreeNode* postUpdateTreeRoot; // +0x78: root of binary tree for post-update processing
    int64_t         postUpdateTreeCount;// +0x80: number of nodes in post-update tree
    IBPArray        stagingUpdate;      // +0x88: staging array for update tree (added by FlagAggregation)
    IBPArray        stagingPostUpdate;  // +0x98: staging array for post-update tree
    uint32_t        processCount[5];    // +0xA8: per-bucket entities to process per tick
    uint32_t        cursor[5];          // +0xBC: per-bucket round-robin cursor position
    IBPArray        removalList;        // +0xD0: entities pending removal
};
static_assert(offsetof(EntityList, accumulatedTime) == 0x00, "EntityList::accumulatedTime at +0x00");
static_assert(offsetof(EntityList, masterArray) == 0x08, "EntityList::masterArray at +0x08");
static_assert(offsetof(EntityList, buckets) == 0x18, "EntityList::buckets at +0x18");
static_assert(offsetof(EntityList, updateTreeRoot) == 0x68, "EntityList::updateTreeRoot at +0x68");
static_assert(offsetof(EntityList, postUpdateTreeRoot) == 0x78, "EntityList::postUpdateTreeRoot at +0x78");
static_assert(offsetof(EntityList, stagingUpdate) == 0x88, "EntityList::stagingUpdate at +0x88");
static_assert(offsetof(EntityList, stagingPostUpdate) == 0x98, "EntityList::stagingPostUpdate at +0x98");
static_assert(offsetof(EntityList, processCount) == 0xA8, "EntityList::processCount at +0xA8");
static_assert(offsetof(EntityList, cursor) == 0xBC, "EntityList::cursor at +0xBC");
static_assert(offsetof(EntityList, removalList) == 0xD0, "EntityList::removalList at +0xD0");
static_assert(sizeof(EntityList) == 0xE0, "EntityList must be 0xE0 bytes");
