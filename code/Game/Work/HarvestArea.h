#pragma once
// ============================================================================
// code/Game/Work/HarvestArea.h — ENDGAME B.3.8 block WRK
//
// HarvestArea — the crop-field harvest work: a BST of the cells being harvested
// (+0x48, count +0x50) and a per-cell validity byte array (+0x58 / +0x60). A REAL
// class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "HarvestArea", parent "AutoPickupWork",
//   create 0x14002a460 (MemAlloc(0x68) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x1401614a0, 13 slots:
//   [0] 0x14002a4d0 scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400ed350 Persist  [4] 0x1400eccd0 Execute  [5] = Work (0x1400f7540)
//   [6] 0x1400ecc20 OnComplete  [7] 0x1400ecb60 OnCancel  [8] 0x1400ed270 Cleanup
//   [9] 0x1400ec930 Validate  [10] 0x1400ed2f0 GetTarget
//   [11] 0x140029860 GetPriority (`+0x50 > 0`, shared by 13 tables)  [12] = Work
//   CropFieldComponent's state machine (cropfieldcomponent_states.cpp, block MI-B)
//   builds the work BY HAND and stamps 0x1401614a0; that registration serves
//   HarvestArea::Vtable() (B38 D5). The default ctor here builds what the tree's
//   factory built (Work_GenericCtor: base init, leaf fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/AutoPickupWork.h"
#include "Object/exe_type.h"

namespace HarvestAreaAddr {
    constexpr uintptr_t Factory = 0x14002a460;
    constexpr uintptr_t Vtable  = 0x1401614a0;
}

class HarvestArea final : public AutoPickupWork {
    DECLARE_EXE_TYPE(HarvestArea)

public:
    HarvestArea();                                                  // (Work_GenericCtor shape)

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a4d0
    void  Persist(void* stream) override;                           // [3]  0x1400ed350
    int   Execute(void* entityIBP, void* context, uint64_t arg3) override;  // [4]  0x1400eccd0
    int   OnComplete(void* param2, void* param3, int arg3) override;  // [6]  0x1400ecc20
    int   OnCancel(void* param2, void* param3, int arg3) override;    // [7]  0x1400ecb60
    int   Cleanup() override;                                       // [8]  0x1400ed270
    int   Validate(void* param2) override;                          // [9]  0x1400ec930
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400ed2f0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- HarvestArea fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed

protected:
    ~HarvestArea();                                                // frees the validity array and the cell BST
};

static_assert(sizeof(HarvestArea) == 0x68, "HarvestArea is 0x68 (retail create MemAlloc)");
static_assert(alignof(HarvestArea) == 8, "HarvestArea is 8-aligned");
static_assert(offsetof(HarvestArea, _field48) == 0x48, "HarvestArea +0x48");
static_assert(offsetof(HarvestArea, _field50) == 0x50, "HarvestArea +0x50");
static_assert(offsetof(HarvestArea, _field58) == 0x58, "HarvestArea +0x58");
static_assert(offsetof(HarvestArea, _field60) == 0x60, "HarvestArea +0x60");
