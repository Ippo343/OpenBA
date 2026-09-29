#pragma once
// ============================================================================
// code/Game/Work/ConsumeProduce.h — ENDGAME B.3.8 block WRK
//
// ConsumeProduce — the production work of a ConsumeProduceComponent building
// (blacksmith, tailor, …): the citizens assigned to its work slots (+0x58 array)
// and the per-slot state (+0x68 array). Reflected "ConsumeProduce" (the tree's
// POD was named ConsumeProduceAction). A REAL class with a compiler-generated
// vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "ConsumeProduce", parent "Work", ti 0x1401c6940,
//   create 0x14002a150 (MemAlloc(0x78) + ctor 0x1400e9340); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161220, 13 slots:
//   [0] 0x14002a190 scalar deleting dtor (the dtor 0x1400e9190 + MemFree on &1)
//   [1] 0x14002a140 TypeName  [2] 0x14002a130 Type  [3] 0x1400e9020 Persist
//   [4] 0x1400e7840 Execute   [5] = Work (0x1400f7540)  [6] 0x1400e77c0 OnComplete
//   [7] 0x1400e7780 OnCancel  [8] 0x1400e8f30 Cleanup   [9] 0x1400e7530 Validate
//   [10] 0x1400e8fb0 GetTarget  [11] 0x14002a0f0 GetPriority (`+0x60 > 0`, shared
//   with ClearStorage)  [12] 0x1400e9010 its OWN `RET 0` (not Work's 0x140006b10)
//   The component's CreateWork (resourceproduce_creatework.cpp, block MI-B) builds a
//   ConsumeProduce BY HAND (retail 0x1400e9200) and stamps 0x140161220; that
//   registration serves ConsumeProduce::Vtable() (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class CitizenComponent;


class ConsumeProduce final : public Work {
    DECLARE_EXE_TYPE(ConsumeProduce)

public:
    ConsumeProduce();                                               // 0x1400e9340

    // 0x1400e9190: frees both arrays
    ~ConsumeProduce() override;                                     // [0]  0x14002a190
    void  Persist(void* stream) override;                           // [3]  0x1400e9020
    int   Execute(void* entityIBP, void* context) override;  // [4]  0x1400e7840
    int   OnComplete(void* param2, void* param3, int arg3) override;  // [6]  0x1400e77c0
    int   OnCancel(void* param2, void* param3, int arg3) override;    // [7]  0x1400e7780
    int   Cleanup() override;                                       // [8]  0x1400e8f30
    int   Validate() override;                          // [9]  0x1400e7530
    void* GetTarget(void* retbuf) override;                         // [10] 0x1400e8fb0
    int   GetPriority() override;                                   // [11] 0x14002a0f0
    void  Slot12(int version) override;                                      // [12] 0x1400e9010 (own RET 0)

    // --- ConsumeProduce fields (+0x48) ---
    uint8_t  field_48;           // +0x48: bool (serialized)
    uint8_t  _pad49[3];          // +0x49: padding
    int32_t  field_4c;           // +0x4C: int (serialized)
    int32_t  field_50;           // +0x50: Type (zeroed, serialized)
    int32_t  field_54;           // +0x54: int (serialized)
    CitizenComponent** field_58; // +0x58: Array<InternalPtr<CitizenComponent>> data (zeroed; Persist
                                 //        0x1400e9020 validates each element against CitizenComponent)
    int32_t  field_60;           // +0x60: DynArray count (zeroed)
    int32_t  field_64;           // +0x64: DynArray capacity (zeroed)
    int32_t* field_68;           // +0x68: Array<int> data (zeroed; Persist's int-array overload)
    int32_t  field_70;           // +0x70: DynArray count (zeroed)
    int32_t  field_74;           // +0x74: DynArray capacity (zeroed)
};

static_assert(sizeof(ConsumeProduce) == 0x78, "ConsumeProduce is 0x78 (retail create MemAlloc)");
static_assert(alignof(ConsumeProduce) == 8, "ConsumeProduce is 8-aligned");
static_assert(offsetof(ConsumeProduce, field_48) == 0x48, "ConsumeProduce +0x48");
static_assert(offsetof(ConsumeProduce, field_4c) == 0x4c, "ConsumeProduce +0x4c");
static_assert(offsetof(ConsumeProduce, field_50) == 0x50, "ConsumeProduce +0x50");
static_assert(offsetof(ConsumeProduce, field_54) == 0x54, "ConsumeProduce +0x54");
static_assert(offsetof(ConsumeProduce, field_58) == 0x58, "ConsumeProduce +0x58");
static_assert(offsetof(ConsumeProduce, field_60) == 0x60, "ConsumeProduce +0x60");
static_assert(offsetof(ConsumeProduce, field_64) == 0x64, "ConsumeProduce +0x64");
static_assert(offsetof(ConsumeProduce, field_68) == 0x68, "ConsumeProduce +0x68");
static_assert(offsetof(ConsumeProduce, field_70) == 0x70, "ConsumeProduce +0x70");
static_assert(offsetof(ConsumeProduce, field_74) == 0x74, "ConsumeProduce +0x74");
