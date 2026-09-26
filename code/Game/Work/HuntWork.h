#pragma once
// ============================================================================
// code/Game/Work/HuntWork.h — ENDGAME B.3.8 block WRK
//
// HuntWork — the hunting cabin work (hunt the game animals in the work
// radius): a BST of the citizens assigned (+0x48, count +0x50). A REAL
// class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "HuntWork", parent "AutoPickupWork",
//   create 0x14002a690 (MemAlloc(0x58) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161620, 13 slots:
//   [0] 0x14002a700 scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400ef600 Persist  [4] 0x1400eeff0 Execute  [5] = Work (0x1400f7540)
//   [6] 0x1400eefd0 OnComplete  [7] 0x1400eefc0 OnCancel = `ADD RCX,0x48; JMP
//   0x140025520` (a member forwarder)  [8] 0x1400ef530 Cleanup  [9] 0x1400eed90
//   Validate  [10] 0x1400ef5a0 GetTarget  [11] 0x140029860 GetPriority (`+0x50 > 0`,
//   shared by 13 tables)  [12] = Work (0x140006b10)
//   HuntComponent's CreateWork (resourceproduce_creatework.cpp, block MI-B) builds
//   the work BY HAND (retail 0x1401226a0) and stamps 0x140161620; that registration
//   serves HuntWork::Vtable() (B38 D5). The default ctor here builds what the tree's
//   factory built (Work_GenericCtor: base init, leaf fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/AutoPickupWork.h"
#include "Object/exe_type.h"

namespace HuntWorkAddr {
    constexpr uintptr_t Factory = 0x14002a690;
    constexpr uintptr_t Vtable  = 0x140161620;
}

class HuntWork final : public AutoPickupWork {
    DECLARE_EXE_TYPE(HuntWork)

public:
    HuntWork();                                                 // (Work_GenericCtor shape)

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a700
    void  Persist(void* stream) override;                           // [3]  0x1400ef600
    int   Execute(void* entityIBP, void* context, uint64_t arg3) override;  // [4]  0x1400eeff0
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400eefd0
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400eefc0
    int   Cleanup() override;                                       // [8]  0x1400ef530
    int   Validate(void* param2) override;                          // [9]  0x1400eed90
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400ef5a0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- HuntWork fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding

protected:
    ~HuntWork();                                               // frees the +0x48 citizen BST
};

static_assert(sizeof(HuntWork) == 0x58, "HuntWork is 0x58 (retail create MemAlloc)");
static_assert(alignof(HuntWork) == 8, "HuntWork is 8-aligned");
static_assert(offsetof(HuntWork, _field48) == 0x48, "HuntWork +0x48");
static_assert(offsetof(HuntWork, _field50) == 0x50, "HuntWork +0x50");
