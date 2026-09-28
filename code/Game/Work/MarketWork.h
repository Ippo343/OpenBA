#pragma once
// ============================================================================
// code/Game/Work/MarketWork.h — ENDGAME B.3.8 block WRK
//
// MarketWork — the market vendor's work (walk the town's storage, take goods the
// houses need and bring them to the market): a tree of the citizens assigned
// (+0x48, count +0x50; nodes carry a demand array at +0x08). A REAL class with a
// compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "MarketWork", parent "Work", ti 0x1401c6b70,
//   create 0x14002a8d0 (MemAlloc(0x58) + ctor 0x1400f1f70); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161710, 13 slots:
//   [0] 0x14002a910 scalar deleting dtor (the dtor inlined + MemFree on &1)
//   [1] TypeName  [2] Type  [3] 0x1400f1d70 Persist  [4] 0x1400eff80 Execute
//   [5] = Work (0x1400f7540)  [6] 0x1400eff60 OnComplete  [7] 0x1400eff50 OnCancel
//   = `ADD RCX,0x48; JMP 0x14002aaa0` (a member forwarder: the tree's erase-by-key)
//   [8] 0x1400f1ca0 Cleanup  [9] 0x1400efd20 Validate  [10] 0x1400f1d10 GetTarget
//   [11] 0x140029860 GetPriority (`+0x50 > 0`, shared by 13 tables)
//   [12] = Work (0x140006b10)
//   MarketComponent's CreateWork (resourceproduce_creatework.cpp, block MI-B) builds
//   a MarketWork BY HAND and stamps 0x140161710; that registration serves
//   MarketWork::Vtable() (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

namespace MarketWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400f1f70;
    constexpr uintptr_t Factory = 0x14002a8d0;
    constexpr uintptr_t Vtable  = 0x140161710;
}

class MarketWork final : public Work {
    DECLARE_EXE_TYPE(MarketWork)

public:
    MarketWork();                                                   // 0x1400f1f70

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a910
    void  Persist(void* stream) override;                           // [3]  0x1400f1d70
    int   Execute(void* citizenHolder, void* ctx) override;  // [4]  0x1400eff80
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400eff60
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400eff50
    int   Cleanup() override;                                       // [8]  0x1400f1ca0
    int   Validate() override;                          // [9]  0x1400efd20
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400f1d10
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- MarketWork fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding

protected:
    ~MarketWork();                                                // frees the +0x48 demand tree
};

static_assert(sizeof(MarketWork) == 0x58, "MarketWork is 0x58 (retail create MemAlloc)");
static_assert(alignof(MarketWork) == 8, "MarketWork is 8-aligned");
static_assert(offsetof(MarketWork, _field48) == 0x48, "MarketWork +0x48");
static_assert(offsetof(MarketWork, _field50) == 0x50, "MarketWork +0x50");
