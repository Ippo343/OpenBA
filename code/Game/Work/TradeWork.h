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

namespace TradeWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400f6da0;
    constexpr uintptr_t Factory = 0x14002c620;
    constexpr uintptr_t Vtable  = 0x140161a10;
}

class TradeWork final : public Work {
    DECLARE_EXE_TYPE(TradeWork)

public:
    TradeWork();                                                    // (Work_GenericCtor shape)

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002c660
    void  Persist(void* stream) override;                           // [3]  0x1400f6cb0
    int   Execute(void* citizenIBP, void* context, uint64_t arg3) override;  // [4]  0x1400f5d40
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400f5d20
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400f5d10
    int   Cleanup() override;                                       // [8]  0x1400f6be0
    int   Validate(void* param2) override;                          // [9]  0x1400f5ae0
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400f6c50
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- TradeWork fields (+0x48) ---
    void*    bstRoot;            // +0x48: BST root for assigned trade workers
    uint32_t workerCount;        // +0x50: number of workers in BST
    uint8_t  _pad54[4];          // +0x54: padding

protected:
    ~TradeWork();                                               // frees the +0x48 citizen BST
};

static_assert(sizeof(TradeWork) == 0x58, "TradeWork is 0x58 (retail create MemAlloc)");
static_assert(alignof(TradeWork) == 8, "TradeWork is 8-aligned");
static_assert(offsetof(TradeWork, bstRoot) == 0x48, "TradeWork +0x48");
static_assert(offsetof(TradeWork, workerCount) == 0x50, "TradeWork +0x50");
