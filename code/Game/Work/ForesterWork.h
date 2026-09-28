#pragma once
// ============================================================================
// code/Game/Work/ForesterWork.h — ENDGAME B.3.8 block WRK
//
// ForesterWork — the forester's lodge work (plant saplings, cut grown trees in
// the work radius): a BST of the citizens assigned (+0x48, count +0x50). A REAL
// class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "ForesterWork", parent "AutoPickupWork",
//   create 0x14002a310 (MemAlloc(0x60) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x1401613a0, 13 slots:
//   [0] 0x14002a370 scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400eb970 Persist  [4] 0x1400eaec0 Execute  [5] = Work (0x1400f7540)
//   [6] 0x1400eaea0 OnComplete  [7] 0x1400eae90 OnCancel = `ADD RCX,0x48; JMP
//   0x140025520` (a member forwarder)  [8] 0x1400eb8a0 Cleanup  [9] 0x1400eac60
//   Validate  [10] 0x1400eb910 GetTarget  [11] 0x140029860 GetPriority (`+0x50 > 0`,
//   shared by 13 tables)  [12] = Work (0x140006b10)
//   ForesterComponent's CreateWork (resourceproduce_creatework.cpp, block MI-B) builds
//   the work BY HAND (retail 0x14011f780) and stamps 0x1401613a0; that registration
//   serves ForesterWork::Vtable() (B38 D5). The default ctor here builds what the tree's
//   factory built (Work_GenericCtor: base init, leaf fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/AutoPickupWork.h"
#include "Object/exe_type.h"

namespace ForesterWorkAddr {
    constexpr uintptr_t Factory = 0x14002a310;
    constexpr uintptr_t Vtable  = 0x1401613a0;
}

class ForesterWork final : public AutoPickupWork {
    DECLARE_EXE_TYPE(ForesterWork)

public:
    ForesterWork();                                                 // (Work_GenericCtor shape)

    // frees the +0x48 citizen BST
    ~ForesterWork() override;                                       // [0]  0x14002a370
    void  Persist(void* stream) override;                           // [3]  0x1400eb970
    int   Execute(void* entityIBP, void* context) override;  // [4]  0x1400eaec0
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400eaea0
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400eae90
    int   Cleanup() override;                                       // [8]  0x1400eb8a0
    int   Validate() override;                          // [9]  0x1400eac60
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400eb910
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- ForesterWork fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    uint32_t _field58;           // +0x58: zeroed
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(ForesterWork) == 0x60, "ForesterWork is 0x60 (retail create MemAlloc)");
static_assert(alignof(ForesterWork) == 8, "ForesterWork is 8-aligned");
static_assert(offsetof(ForesterWork, _field48) == 0x48, "ForesterWork +0x48");
static_assert(offsetof(ForesterWork, _field50) == 0x50, "ForesterWork +0x50");
static_assert(offsetof(ForesterWork, _field58) == 0x58, "ForesterWork +0x58");
