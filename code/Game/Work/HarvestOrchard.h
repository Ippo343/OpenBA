#pragma once
// ============================================================================
// code/Game/Work/HarvestOrchard.h — ENDGAME B.3.8 block WRK
//
// HarvestOrchard — the orchard harvest work: a BST of the trees being picked
// (+0x48, count +0x50) and a per-orchard-slot byte array (+0x58 / +0x60: 1 = a
// tree to pick). A REAL class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "HarvestOrchard", parent "AutoPickupWork",
//   create 0x14002a520 (MemAlloc(0x68) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161520, 13 slots: [0] scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400edea0 Persist  [4] Execute  [5] = Work (0x1400f7540)  [6] OnComplete
//   [7] OnCancel  [8] Cleanup  [9] Validate  [10] GetTarget  [11] 0x140029860
//   GetPriority (`+0x50 > 0`, shared by 13 tables)  [12] = Work (0x140006b10)
//   ctors: the default one (the tree built it with Work_GenericCtor: base init, leaf
//   fields zeroed — kept) and the from-entity ctor 0x1400ee060 (harvestorchard_ctor.cpp),
//   which OrchardComponent's state 11 (orchardcomponent_states.cpp, block MI-B) calls
//   through the door HarvestOrchard_CtorFromEntity. Nothing outside this block stamps
//   0x140161520.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/AutoPickupWork.h"
#include "Object/exe_type.h"


class HarvestOrchard final : public AutoPickupWork {
    DECLARE_EXE_TYPE(HarvestOrchard)

public:
    HarvestOrchard();                                               // (Work_GenericCtor shape)
    explicit HarvestOrchard(void** entityIBP);                      // 0x1400ee060

    // frees the slot array and the BST
    ~HarvestOrchard() override;                                     // [0]
    void  Persist(void* stream) override;                           // [3]  0x1400edea0
    int   Execute(void* entityIBP, void* context) override;  // [4]
    int   OnComplete(void* param2, void* param3, int param4) override;  // [6]
    int   OnCancel(void* param2, void* param3, int param4) override;    // [7]
    int   Cleanup() override;                                       // [8]
    int   Validate() override;                          // [9]
    void* GetTarget(void* outPtr) override;                         // [10]
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- HarvestOrchard fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
};

static_assert(sizeof(HarvestOrchard) == 0x68, "HarvestOrchard is 0x68 (retail create MemAlloc)");
static_assert(alignof(HarvestOrchard) == 8, "HarvestOrchard is 8-aligned");
static_assert(offsetof(HarvestOrchard, _field48) == 0x48, "HarvestOrchard +0x48");
static_assert(offsetof(HarvestOrchard, _field50) == 0x50, "HarvestOrchard +0x50");
static_assert(offsetof(HarvestOrchard, _field58) == 0x58, "HarvestOrchard +0x58");
static_assert(offsetof(HarvestOrchard, _field60) == 0x60, "HarvestOrchard +0x60");
