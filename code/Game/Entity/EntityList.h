#pragma once
// ============================================================================
// code/Game/Entity/EntityList.h — ENDGAME B.4 W0-H1 (moved from engine/types)
//
// EntityList manages all ComponentEntity instances in the game world.
// It maintains a master array of entity IBPs, 5 update buckets for round-robin
// component ticking, two binary trees (update tree and post-update tree),
// and staging/removal arrays for deferred add/remove operations. No vtable,
// no base. Lives at GameUpdateLoop+0xE8 (m_entityList).
//
// RETAIL (Application-x64.exe):
//   alloc  GameUpdateLoop::Load 0x140096b00: MemAlloc(0xe8, 4)
//   ctor   0x1400c3f90: +0x00/+0x08/+0x10 = 0; the five bucket Arrays at +0x18
//          (vector ctor 0x14003accc); the trees +0x68 / +0x78 as {qword root,
//          dword count}; +0x88 / +0x98 / +0xD0 Arrays = 0; +0xE0 (byte) = 0;
//          +0xA8 = 50, +0xAC = 15, +0xB0 = 8, +0xB4 = 5, +0xB8 = 2 (a qword
//          store: +0xBC = 0), +0xC0 / +0xC8 = 0
//   PreUpdate  0x1400c3bd0 · Update 0x1400c4090 · PostUpdate 0x1400c43d0 ·
//   Serialize 0x1400c44a0 · RemoveEntity 0x1400c38e0 · Shutdown 0x1400c3d30 ·
//   FlagAggregation 0x1400c3470
//
// B.4 W0-H1: the size is 0xE8, not 0xE0: the +0xE0 byte is retail's shutdown
// flag (the tree read and wrote it past the old sizeof). The Arrays stay the
// trivial IBPArray / UpdateBucket shapes (H5: Collection::Array has a
// destructor that no tree code runs on this object); the bodies read them by
// these names. engine/types/EntityList.h forwards here.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/Value.h"   // System::Time (8 bytes, trivial)

class ComponentEntity;
namespace IO { class Stream; }

// IBPArray — dynamic array of InternalBasePtr (8-byte elements)
// Layout: +0x00 = void* data, +0x08 = int32_t count, +0x0C = int32_t capacity
struct IBPArray {
    ComponentEntity** data;  // +0x00: InternalPtr<ComponentEntity> slots (8 bytes each; retail
                             //        CheckPtr's a slot, then reads the bare pointer: 0x1400c38e0)
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
    ComponentEntity* entityIBP;  // +0x00: InternalBasePtr to ComponentEntity (the key: retail
                                 //        compares it unsigned, 0x1400c38e0)
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
static_assert(offsetof(EntityTreeNode, balance) == 0x20, "EntityTreeNode::balance at +0x20");
static_assert(offsetof(EntityTreeNode, height) == 0x22, "EntityTreeNode::height at +0x22");
static_assert(offsetof(EntityTreeNode, _pad24) == 0x24, "EntityTreeNode::_pad24 at +0x24");

// UpdateBucket — one of 5 round-robin update buckets: a Collection::Array of
// entity IBPs (retail constructs the five with the vector-ctor iterator
// 0x14003accc: element 0x10, count 5, ctor 0x140008080, dtor 0x140001740).
struct UpdateBucket {
    ComponentEntity** data;  // +0x00: InternalPtr<ComponentEntity> slots (8-byte each)
    int32_t  count;    // +0x08: number of entities in this bucket
    int32_t  _pad0C;   // +0x0C: Array capacity
};
static_assert(sizeof(UpdateBucket) == 0x10, "UpdateBucket must be 0x10 bytes");
static_assert(offsetof(UpdateBucket, data) == 0x00, "UpdateBucket::data at +0x00");
static_assert(offsetof(UpdateBucket, count) == 0x08, "UpdateBucket::count at +0x08");
static_assert(offsetof(UpdateBucket, _pad0C) == 0x0C, "UpdateBucket::_pad0C at +0x0C");

// EntityList — main entity management structure
// Size: 0xE8 bytes (GameUpdateLoop::Load 0x140096b00: MemAlloc(0xe8, 4) + ctor 0x1400c3f90)
struct EntityList {
    System::Time    accumulatedTime;    // +0x00: accumulated simulation time (Serialize 0x1400c44a0:
                                        //        System::Time::Serialize on +0x00)
    IBPArray        masterArray;        // +0x08: master array of all entity IBPs {data, count, cap}
    UpdateBucket    buckets[5];         // +0x18: 5 round-robin update buckets (each 0x10)
    EntityTreeNode* updateTreeRoot;     // +0x68: root of binary tree for per-tick entity processing
    int32_t         updateTreeCount;    // +0x70: number of nodes in update tree (ctor 0x1400c3fe8
                                        //        `MOV dword ptr [RAX+0x8],EDI`; the Set count dword)
    uint8_t         _pad74[4];          // +0x74
    EntityTreeNode* postUpdateTreeRoot; // +0x78: root of binary tree for post-update processing
    int32_t         postUpdateTreeCount;// +0x80: number of nodes in post-update tree (ctor 0x1400c3ff7)
    uint8_t         _pad84[4];          // +0x84
    IBPArray        stagingUpdate;      // +0x88: staging array for update tree (added by FlagAggregation)
    IBPArray        stagingPostUpdate;  // +0x98: staging array for post-update tree
    int32_t         processCount[5];    // +0xA8: per-bucket entities to process per tick (int: Serialize
                                        //        0x1400c45d0 writes it through the int-array overload)
    int32_t         cursor[5];          // +0xBC: per-bucket round-robin cursor position (int: 0x1400c45e8)
    IBPArray        removalList;        // +0xD0: entities pending removal
    bool            shuttingDown;       // +0xE0: ctor 0; 0x1400c3d30 (the shutdown drain) sets it
                                        //        first; RegisterEntity drops an entity while it is set
    uint8_t         _padE1[7];          // +0xE1

    // ---- members (retail addresses; bodies in entitylist.cpp) ----
    void Update(void* time);                          // 0x1400c4090 (time: a System::Time*)
    void PreUpdate();                                 // 0x1400c3bd0
    void PostUpdateStaging();                         // 0x1400c3ab0
    void PostUpdate();                                // 0x1400c43d0
    void RemoveEntity(ComponentEntity** entityIBP);   // 0x1400c38e0 (an InternalPtr slot)
    void Serialize(IO::Stream& stream);               // 0x1400c44a0
    void RegisterEntity(ComponentEntity** entityIBP, int category);  // 0x1400c48a0 (body in componententity.cpp)

private:
    // The retail step-5 tail of Update (0x1400c4090), relocated to the top of the
    // NEXT frame's Update (the tree's one-frame removal deferral; see the body).
    void DrainRemovals();
};
static_assert(offsetof(EntityList, accumulatedTime) == 0x00, "EntityList::accumulatedTime at +0x00");
static_assert(offsetof(EntityList, masterArray) == 0x08, "EntityList::masterArray at +0x08");
static_assert(offsetof(EntityList, buckets) == 0x18, "EntityList::buckets at +0x18");
static_assert(offsetof(EntityList, updateTreeRoot) == 0x68, "EntityList::updateTreeRoot at +0x68");
static_assert(offsetof(EntityList, updateTreeCount) == 0x70, "EntityList::updateTreeCount at +0x70");
static_assert(offsetof(EntityList, _pad74) == 0x74, "EntityList +0x74");
static_assert(offsetof(EntityList, postUpdateTreeRoot) == 0x78, "EntityList::postUpdateTreeRoot at +0x78");
static_assert(offsetof(EntityList, postUpdateTreeCount) == 0x80, "EntityList::postUpdateTreeCount at +0x80");
static_assert(offsetof(EntityList, _pad84) == 0x84, "EntityList +0x84");
static_assert(offsetof(EntityList, stagingUpdate) == 0x88, "EntityList::stagingUpdate at +0x88");
static_assert(offsetof(EntityList, stagingPostUpdate) == 0x98, "EntityList::stagingPostUpdate at +0x98");
static_assert(offsetof(EntityList, processCount) == 0xA8, "EntityList::processCount at +0xA8");
static_assert(offsetof(EntityList, cursor) == 0xBC, "EntityList::cursor at +0xBC");
static_assert(offsetof(EntityList, removalList) == 0xD0, "EntityList::removalList at +0xD0");
static_assert(offsetof(EntityList, shuttingDown) == 0xE0, "EntityList::shuttingDown at +0xE0");
static_assert(offsetof(EntityList, _padE1) == 0xE1, "EntityList::_padE1 at +0xE1");
static_assert(sizeof(EntityList) == 0xE8, "EntityList is 0xE8 bytes (MemAlloc(0xe8) in 0x140096b00)");
