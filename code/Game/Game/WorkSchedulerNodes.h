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

// The transient cost-ordered candidate tree the citizen's work walk builds
// (0x1400f79b0) from one scheduler id tree: its two builders 0x1400f8000 /
// 0x1400f7c90 allocate the node (MemAlloc(0x30, 8): 0x1400f807e / 0x1400f8163 /
// 0x1400f81aa), its walk 0x1400f7b60 visits it in order, 0x14000e320 frees the
// children and MemFree the root (0x1400f7b12..0x1400f7b2f). NOT a
// WorkSchedulerIdNode: the key is the int path cost at +0x08 (ties broken on
// Work +0x20), not a work id at +0x00.
//
// The node, from the builders' stores (0x1400f809d..0x1400f80bb):
//   qword +0x00 = 0, then the Work (the InternalPtr copy: zero, assign; the
//   CheckPtr on the source element, 0x1400f8037, is a bare RET), dword +0x08 =
//   the cost, qwords +0x10 / +0x18 / +0x20 = parent / 0 / 0, ONE dword store of
//   0 at +0x28 over balance and height. +0x0C and +0x2C are never written.
// Balance / height are signed int16 (0x14000e360: MOVZX word +0x2a, INC,
// CMOVG; word +0x28 compared with JLE 1 / JGE -1). The rebalance 0x14000e360 is
// shared with every other 0x30 node whose links are at +0x10 / +0x18 / +0x20.
struct WorkCandidateNode {
    Object::InternalPtr<Work> work;      // +0x00 the candidate Work
    int32_t                   cost;      // +0x08 the sort key (path cost / Work +0x24)
    uint8_t                   _pad0C[4]; // +0x0C never written
    WorkCandidateNode*        parent;    // +0x10
    WorkCandidateNode*        left;      // +0x18
    WorkCandidateNode*        right;     // +0x20
    int16_t                   balance;   // +0x28 right height - left height
    int16_t                   height;    // +0x2A
    uint8_t                   _pad2C[4]; // +0x2C never written
};
static_assert(sizeof(WorkCandidateNode) == 0x30, "MemAlloc(0x30, 8) in 0x1400f8000 / 0x1400f7c90");
static_assert(offsetof(WorkCandidateNode, work) == 0x00, "WorkCandidateNode::work @ +0x00");
static_assert(offsetof(WorkCandidateNode, cost) == 0x08, "WorkCandidateNode::cost @ +0x08");
static_assert(offsetof(WorkCandidateNode, _pad0C) == 0x0C, "WorkCandidateNode +0x0C");
static_assert(offsetof(WorkCandidateNode, parent) == 0x10, "WorkCandidateNode::parent @ +0x10");
static_assert(offsetof(WorkCandidateNode, left) == 0x18, "WorkCandidateNode::left @ +0x18");
static_assert(offsetof(WorkCandidateNode, right) == 0x20, "WorkCandidateNode::right @ +0x20");
static_assert(offsetof(WorkCandidateNode, balance) == 0x28, "WorkCandidateNode::balance @ +0x28");
static_assert(offsetof(WorkCandidateNode, height) == 0x2A, "WorkCandidateNode::height @ +0x2A");
static_assert(offsetof(WorkCandidateNode, _pad2C) == 0x2C, "WorkCandidateNode +0x2C");

// The candidate tree's {root, count} header: the stack local of 0x1400f79b0
// (qword [RBP-0x18] = 0, dword [RBP-0x10] = 0 at 0x1400f7a07 / 0x1400f7a0b);
// the builders INC dword +0x08 per insert (0x1400f81f8).
struct WorkCandidateTree {
    WorkCandidateNode* root;      // +0x00
    int32_t            count;     // +0x08
    uint8_t            _pad0C[4]; // +0x0C
};
static_assert(sizeof(WorkCandidateTree) == 0x10, "the {root, count} local of 0x1400f79b0");
static_assert(offsetof(WorkCandidateTree, root) == 0x00, "WorkCandidateTree::root @ +0x00");
static_assert(offsetof(WorkCandidateTree, count) == 0x08, "WorkCandidateTree::count @ +0x08");
static_assert(offsetof(WorkCandidateTree, _pad0C) == 0x0C, "WorkCandidateTree +0x0C");
