#pragma once
// ============================================================================
// code/Game/Work/TradeWork.h — ENDGAME B.3.8 block WRK
//
// TradeWork — the trading post's work (carry goods between the post's storage and
// the docked trader): a BST of the citizens assigned (+0x48, count +0x50; extended
// 0x30-byte nodes: key, target, quantity, parent, left, right). A REAL class with a
// compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "TradeWork", parent "Work", ti 0x1401c6cc0,
//   create 0x14002c620 (MemAlloc(0x58) + ctor 0x1400f6da0); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161a10, 13 slots:
//   [0] 0x14002c660 scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400f6cb0 Persist  [4] 0x1400f5d40 Execute  [5] = Work (0x1400f7540)
//   [6] 0x1400f5d20 OnComplete  [7] 0x1400f5d10 OnCancel = `ADD RCX,0x48; JMP
//   0x14002c690` (a member forwarder)  [8] 0x1400f6be0 Cleanup  [9] 0x1400f5ae0
//   Validate  [10] 0x1400f6c50 GetTarget  [11] 0x140029860 GetPriority (`+0x50 > 0`,
//   shared by 13 tables)  [12] = Work (0x140006b10)
//   TradeComponent (tradecomponent.cpp / tradecomponent_states.cpp, outside this
//   block) builds a TradeWork BY HAND (the from-entity ctor 0x1400f6ea0) and stamps
//   0x140161a10; that registration serves TradeWork::Vtable() (B38 D5). The default
//   ctor here builds what the tree's factory built (Work_GenericCtor: base init, leaf
//   fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class CitizenComponent;
class ComponentDescription;
namespace IO { class Serializer; }

class TradeWork final : public Work {
    DECLARE_EXE_TYPE(TradeWork)

public:
    TradeWork();                                                    // (Work_GenericCtor shape)

    // frees the +0x48 citizen BST
    ~TradeWork() override;                                          // [0]  0x14002c660
    void  Persist(IO::Stream& stream) override;                           // [3]  0x1400f6cb0
    int   Execute(void* citizenIBP, void* context) override;  // [4]  0x1400f5d40
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400f5d20
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400f5d10
    int   Cleanup() override;                                       // [8]  0x1400f6be0
    int   Validate() override;                          // [9]  0x1400f5ae0
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400f6c50
    int   GetPriority() override;                                   // [11] 0x140029860

    // One carry the post has handed out: the element of the AVL set at +0x48,
    // keyed on the citizen (insert 0x14002c710 copies these 0x18 bytes into the
    // node; serialize 0x14002c8a0 streams them).
    struct Assignment {
        CitizenComponent*     citizen;   // +0x00 InternalPtr, the key (0x14002c8a0 validates it
                                         //       against CitizenComponent)
        ComponentDescription* resource;  // +0x08 ExternalPtr (serialized by 0x140002340)
        int32_t               quantity;  // +0x10 < 0 carried out of the post, > 0 carried in
                                         //       (0x1400f5d40)
        uint32_t              _pad14;    // +0x14 copied with the record, never read
    };
    // The set's node (0x38 bytes: MemAlloc(0x38) in 0x14002c830).
    struct Node {
        Assignment value;    // +0x00
        Node*      parent;   // +0x18
        Node*      left;     // +0x20
        Node*      right;    // +0x28
        int16_t    balance;  // +0x30 right height - left height (one dword store zeroes both)
        int16_t    height;   // +0x32
        uint32_t   _pad34;   // +0x34
    };

    // --- TradeWork fields (+0x48) ---
    Node*    bstRoot;            // +0x48: the assignment set's root
    int32_t  workerCount;        // +0x50: its count (0x140029860 `CMP [RCX+0x50]; SETG`: signed)
    uint8_t  _pad54[4];          // +0x54: padding

private:
    // 0x14002c710: insert `rec` keyed on its citizen; returns the new node, or the
    // node already holding that citizen (the count is not bumped then).
    Node* InsertAssignment(const Assignment& rec);
    // 0x14002c830: the insert's node allocation (RCX unused by the body).
    static Node* AllocNode(const Assignment& rec, Node* parent);
    // 0x14002c8a0: stream the set (count, then each record in order).
    void SerializeAssignments(IO::Serializer& ser);
    // The assignment tail Execute (0x1400f5d40) inlines on both passes: the
    // citizen's task-state stores and the insert of {citizen, resource, quantity}.
    int CommitAssignment(CitizenComponent* const* citizenRef, ComponentDescription* resource, int quantity);
};

static_assert(sizeof(TradeWork) == 0x58, "TradeWork is 0x58 (retail create MemAlloc)");
static_assert(alignof(TradeWork) == 8, "TradeWork is 8-aligned");
static_assert(offsetof(TradeWork, bstRoot) == 0x48, "TradeWork +0x48");
static_assert(offsetof(TradeWork, workerCount) == 0x50, "TradeWork +0x50");
static_assert(sizeof(TradeWork::Assignment) == 0x18, "the record 0x14002c710 copies is 0x18 bytes");
static_assert(offsetof(TradeWork::Assignment, citizen) == 0x00, "");
static_assert(offsetof(TradeWork::Assignment, resource) == 0x08, "");
static_assert(offsetof(TradeWork::Assignment, quantity) == 0x10, "");
static_assert(sizeof(TradeWork::Node) == 0x38, "MemAlloc(0x38) in 0x14002c830");
static_assert(offsetof(TradeWork::Node, parent) == 0x18, "");
static_assert(offsetof(TradeWork::Node, left) == 0x20, "");
static_assert(offsetof(TradeWork::Node, right) == 0x28, "");
static_assert(offsetof(TradeWork::Node, balance) == 0x30, "");
static_assert(offsetof(TradeWork::Node, height) == 0x32, "");
