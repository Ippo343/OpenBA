#pragma once
// ============================================================================
// code/Game/Work/FishingWork.h — ENDGAME B.3.8 block WRK
//
// FishingWork — the fishing-dock work: the citizens holding a fishing spot
// (+0x48 array) and the per-spot state (+0x58 array). A REAL class with a
// compiler-generated vtable. Extends Work directly (not AutoPickupWork).
//
// RETAIL (B38_SLOTS.md §6): reflected "FishingWork", parent "Work", ti 0x1401c69b0,
//   create 0x14002a280 (MemAlloc(0x70) + ctor 0x1400eabe0); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161320, 13 slots:
//   [0] 0x14002a2c0 scalar deleting dtor (the dtor 0x1400eaa50 + MemFree on &1)
//   [1] 0x14002a270 TypeName  [2] 0x14002a260 Type  [3] 0x1400ea910 Persist
//   [4] 0x1400e9f20 Execute   [5] = Work (0x1400f7540)  [6] 0x1400e9eb0 OnComplete
//   [7] 0x1400e9e80 OnCancel  [8] 0x1400ea830 Cleanup   [9] 0x1400e9c30 Validate
//   [10] 0x1400ea8b0 GetTarget  [11] 0x140029860 GetPriority (`+0x50 > 0`, shared
//   by 13 tables)  [12] = Work (0x140006b10)
//   FishingComponent's CreateWork (resourceproduce_creatework.cpp, block MI-B) builds
//   the work BY HAND (retail ctor 0x1400eaac0) and stamps 0x140161320; that
//   registration serves FishingWork::Vtable() (B38 D5). The default ctor here builds
//   what the tree's factory built (Work_GenericCtor: base init, leaf fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class CitizenComponent;


class FishingWork final : public Work {
    DECLARE_EXE_TYPE(FishingWork)

public:
    FishingWork();                                                  // (Work_GenericCtor shape)

    // PORT GAP (retail 0x1400eaa50) — B38 D9: retail's dtor (reached from slot 0
    // 0x14002a2c0) is its own body and very likely frees the two arrays; the tree's
    // slot 0 was Work_GenericScalarDtor (stamp Work, ~Internal), which is what this
    // empty dtor + the compiler chain still does (the arrays leak, as before).
    ~FishingWork() override = default;                              // [0]  0x14002a2c0
    void  Persist(void* stream) override;                           // [3]  0x1400ea910
    int   Execute(void* citizenIBP, void* time) override;  // [4]  0x1400e9f20
    int   OnComplete(void* param2, void* param3, int arg3) override;  // [6]  0x1400e9eb0
    int   OnCancel(void* param2, void* param3, int arg3) override;    // [7]  0x1400e9e80
    int   Cleanup() override;                                       // [8]  0x1400ea830
    int   Validate() override;                          // [9]  0x1400e9c30
    void* GetTarget(void* param2) override;                         // [10] 0x1400ea8b0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- FishingWork fields (+0x48) ---
    CitizenComponent** citizenRefs; // +0x48: Array<InternalPtr<CitizenComponent>> data (Persist
                                    //        0x1400ea910 validates each against CitizenComponent)
    int32_t  citizenCount;       // +0x50: count of citizenRefs array
    int32_t  citizenCapacity;    // +0x54: capacity of citizenRefs array
    int32_t* spotStates;         // +0x58: Array<int> data (0=avail, 1=assigned, 2=done)
    int32_t  spotCount;          // +0x60: count of spotStates array
    int32_t  spotCapacity;       // +0x64: capacity of spotStates array
    int32_t  retryCount;         // +0x68: retry counter for fishing spot search
    uint8_t  _pad6C[4];          // +0x6C: padding
};

static_assert(sizeof(FishingWork) == 0x70, "FishingWork is 0x70 (retail create MemAlloc)");
static_assert(alignof(FishingWork) == 8, "FishingWork is 8-aligned");
static_assert(offsetof(FishingWork, citizenRefs) == 0x48, "FishingWork +0x48");
static_assert(offsetof(FishingWork, citizenCount) == 0x50, "FishingWork +0x50");
static_assert(offsetof(FishingWork, citizenCapacity) == 0x54, "FishingWork +0x54");
static_assert(offsetof(FishingWork, spotStates) == 0x58, "FishingWork +0x58");
static_assert(offsetof(FishingWork, spotCount) == 0x60, "FishingWork +0x60");
static_assert(offsetof(FishingWork, spotCapacity) == 0x64, "FishingWork +0x64");
static_assert(offsetof(FishingWork, retryCount) == 0x68, "FishingWork +0x68");
