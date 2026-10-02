#pragma once
// ============================================================================
// code/Game/Game/WorkSchedulerNodes.h — ENDGAME B.4 GAME-b
//
// The three node types of the work scheduler's trees (WorkScheduler.h, the
// object at GameUpdateLoop+0x118). Retail inlines Collection::Set / Map over
// each; the layouts are read from its serializer 0x1400f9240 and the bodies it
// calls (0x14002d060, 0x14002d340, 0x14002d5f0, 0x14002ccf0 / 0x14002ceb0,
// 0x14002cfb0) and from the rebalance drivers the inserts end in (the 0x30 node
// 0x140013480, the 0x28 node 0x14001a8c0, the 0x38 node 0x140014b40).
// Link and balance fields follow Action/PathAvl.h's table.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"   // Object::InternalPtr<T>
#include "Game/WorkScheduler.h"   // WorkSchedulerTree

class Work;
class Profession;

// The 0x30 node of the fifteen +0x30 trees (and of a bucket's own tree): the
// work id and the Work, an InternalPtr (the serializer writes it through
// InternalBasePtr::Serialize and nulls a non-Work, 0x14002d0ee / 0x14002d104).
struct WorkSchedulerIdNode {
    int32_t                     key;       // +0x00 the work id (Work +0x20)
    uint8_t                     _pad04[4]; // +0x04
    Object::InternalPtr<Work>   work;      // +0x08
    WorkSchedulerIdNode*        parent;    // +0x10
    WorkSchedulerIdNode*        left;      // +0x18
    WorkSchedulerIdNode*        right;     // +0x20
    int16_t                     balance;   // +0x28
    int16_t                     height;    // +0x2A
    uint8_t                     _pad2C[4]; // +0x2C
};
static_assert(sizeof(WorkSchedulerIdNode) == 0x30, "MemAlloc(0x30, 8) in 0x14002d060");
static_assert(offsetof(WorkSchedulerIdNode, key) == 0x00, "WorkSchedulerIdNode::key @ +0x00");
static_assert(offsetof(WorkSchedulerIdNode, _pad04) == 0x04, "WorkSchedulerIdNode +0x04");
static_assert(offsetof(WorkSchedulerIdNode, work) == 0x08, "WorkSchedulerIdNode::work @ +0x08");
static_assert(offsetof(WorkSchedulerIdNode, parent) == 0x10, "WorkSchedulerIdNode::parent @ +0x10");
static_assert(offsetof(WorkSchedulerIdNode, left) == 0x18, "WorkSchedulerIdNode::left @ +0x18");
static_assert(offsetof(WorkSchedulerIdNode, right) == 0x20, "WorkSchedulerIdNode::right @ +0x20");
static_assert(offsetof(WorkSchedulerIdNode, balance) == 0x28, "WorkSchedulerIdNode::balance @ +0x28");
static_assert(offsetof(WorkSchedulerIdNode, height) == 0x2A, "WorkSchedulerIdNode::height @ +0x2A");
static_assert(offsetof(WorkSchedulerIdNode, _pad2C) == 0x2C, "WorkSchedulerIdNode +0x2C");

// The 0x28 node of the +0x10 tree (rebuilt on load from version-2 saves, keyed
// by the running counter at +0x18) and of the serializer's local dead-id set:
// one 8-byte element, retail writes it as one qword (counter | workId << 32).
struct WorkSchedulerCountNode {
    int32_t                 key;       // +0x00 the counter (the dead-id set: the work id)
    uint32_t                workId;    // +0x04 Work +0x20 (0 in the dead-id set)
    WorkSchedulerCountNode* parent;    // +0x08
    WorkSchedulerCountNode* left;      // +0x10
    WorkSchedulerCountNode* right;     // +0x18
    int16_t                 balance;   // +0x20
    int16_t                 height;    // +0x22
    uint8_t                 _pad24[4]; // +0x24
};
static_assert(sizeof(WorkSchedulerCountNode) == 0x28, "MemAlloc(0x28, 8)");
static_assert(offsetof(WorkSchedulerCountNode, key) == 0x00, "WorkSchedulerCountNode::key @ +0x00");
static_assert(offsetof(WorkSchedulerCountNode, workId) == 0x04, "WorkSchedulerCountNode::workId @ +0x04");
static_assert(offsetof(WorkSchedulerCountNode, parent) == 0x08, "WorkSchedulerCountNode::parent @ +0x08");
static_assert(offsetof(WorkSchedulerCountNode, left) == 0x10, "WorkSchedulerCountNode::left @ +0x10");
static_assert(offsetof(WorkSchedulerCountNode, right) == 0x18, "WorkSchedulerCountNode::right @ +0x18");
static_assert(offsetof(WorkSchedulerCountNode, balance) == 0x20, "WorkSchedulerCountNode::balance @ +0x20");
static_assert(offsetof(WorkSchedulerCountNode, height) == 0x22, "WorkSchedulerCountNode::height @ +0x22");
static_assert(offsetof(WorkSchedulerCountNode, _pad24) == 0x24, "WorkSchedulerCountNode +0x24");

// The 0x18-byte element of the +0x120 tree: the key, an ExternalPtr<Profession>
// (written through 0x140018020, the serializer ProfessionList's four profession
// references use; compared as an unsigned qword against the Profession every
// Work's slot 10, GetTarget, writes), and the bucket's own id tree.
struct WorkSchedulerBucket {
    Profession*       key;       // +0x00
    WorkSchedulerTree entries;   // +0x08 {root, count}
};
static_assert(sizeof(WorkSchedulerBucket) == 0x18, "the record 0x14002d5f0 builds on its stack");
static_assert(offsetof(WorkSchedulerBucket, key) == 0x00, "WorkSchedulerBucket::key @ +0x00");
static_assert(offsetof(WorkSchedulerBucket, entries) == 0x08, "WorkSchedulerBucket::entries @ +0x08");

// The 0x38 node of the +0x120 tree (0x14002ceb0 builds it).
struct WorkSchedulerBucketNode {
    WorkSchedulerBucket      bucket;    // +0x00
    WorkSchedulerBucketNode* parent;    // +0x18
    WorkSchedulerBucketNode* left;      // +0x20
    WorkSchedulerBucketNode* right;     // +0x28
    int16_t                  balance;   // +0x30
    int16_t                  height;    // +0x32
    uint8_t                  _pad34[4]; // +0x34
};
static_assert(sizeof(WorkSchedulerBucketNode) == 0x38, "MemAlloc(0x38, 8) in 0x14002ccf0");
static_assert(offsetof(WorkSchedulerBucketNode, bucket) == 0x00, "WorkSchedulerBucketNode::bucket @ +0x00");
static_assert(offsetof(WorkSchedulerBucketNode, parent) == 0x18, "WorkSchedulerBucketNode::parent @ +0x18");
static_assert(offsetof(WorkSchedulerBucketNode, left) == 0x20, "WorkSchedulerBucketNode::left @ +0x20");
static_assert(offsetof(WorkSchedulerBucketNode, right) == 0x28, "WorkSchedulerBucketNode::right @ +0x28");
static_assert(offsetof(WorkSchedulerBucketNode, balance) == 0x30, "WorkSchedulerBucketNode::balance @ +0x30");
static_assert(offsetof(WorkSchedulerBucketNode, height) == 0x32, "WorkSchedulerBucketNode::height @ +0x32");
