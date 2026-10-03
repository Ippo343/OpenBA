#pragma once
// ============================================================================
// code/Game/Work/BuildRoad.h — ENDGAME B.3.8 block WRK
//
// BuildRoad — the road-construction work (one per road cell to build). A REAL
// class with a compiler-generated vtable. Source: Game/Work/BuildRoad.cpp
// (profile-build strings).
//
// RETAIL (B38_SLOTS.md §6): reflected "BuildRoad", parent "Work", ti 0x1401c6860,
//   create 0x140029560 (MemAlloc(0x70) + ctor 0x1400e4ad0), createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161020, 13 slots:
//   [0] 0x1400295b0 scalar deleting dtor   [1] 0x140029550 TypeName  [2] 0x140029540 Type
//   [3] 0x1400e4980 Persist   [4] 0x1400e4190 Execute   [5] = Work (0x1400f7540)
//   [6] 0x1400e4150 OnComplete   [7] 0x1400e4120 OnCancel   [8] 0x1400e4910 Cleanup
//   [9] 0x1400e40c0 Validate   [10] 0x1400e4960 GetTarget   [11] 0x1400295a0 GetPriority
//   [12] = Work (0x140006b10)
//   ctors: 0x1400e4ad0 (default) and 0x1400e4b50 (the 8-argument one RoadTool's
//   commit path calls by address, FUN_14009c960).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class CitizenComponent;


class BuildRoad final : public Work {
    DECLARE_EXE_TYPE(BuildRoad)

public:
    BuildRoad();                                                        // 0x1400e4ad0
    BuildRoad(uint64_t* ownerIbp, uint32_t cellIdx, uint32_t mapDescVal,
              uint64_t roadDescExtra, uint32_t roadTypeExisting,
              uint32_t roadState, uint32_t terrainType);                // 0x1400e4b50

    // retail's dtor only re-stamps the tables
    ~BuildRoad() override = default;                                    // [0]  0x1400295b0
    void  Persist(IO::Stream& stream) override;                               // [3]  0x1400e4980
    int   Execute(CitizenComponent** entityIBP, System::Time* context) override;  // [4]  0x1400e4190
    int   OnComplete(CitizenComponent** citizen, ComponentEntity** arg2, int param4) override;   // [6]  0x1400e4150
    int   OnCancel(CitizenComponent** citizen, ComponentEntity** arg2, int arg3) override;       // [7]  0x1400e4120
    int   Cleanup() override;                                           // [8]  0x1400e4910
    int   Validate() override;                              // [9]  0x1400e40c0
    Profession** GetTarget(Profession** retbuf) override;                             // [10] 0x1400e4960
    int   GetPriority() override;                                       // [11] 0x1400295a0

    // --- BuildRoad fields (+0x48) ---
    CitizenComponent* field_48;  // +0x48: InternalPtr<CitizenComponent> — the assigned citizen (zeroed;
                                 //        Persist 0x1400e4980 validates it against CitizenComponent)
    void*    field_50;           // +0x50: ExternalPtr — building description (zeroed)
    int32_t  field_58;           // +0x58: map cell index (-1 sentinel)
    int32_t  field_5c;           // +0x5C: road type (2 = default)
    int32_t  field_60;           // +0x60: material type (0xff default if version < 1)
    int32_t  field_64;           // +0x64: field (serialized, version >= 1)
    int32_t  field_68;           // +0x68: material count (2 default if version < 1)
    uint8_t  _pad6c[4];          // +0x6C: padding
};

static_assert(sizeof(BuildRoad) == 0x70, "BuildRoad is 0x70 (create 0x140029560 MemAlloc(0x70))");
static_assert(alignof(BuildRoad) == 8, "BuildRoad is 8-aligned");
static_assert(offsetof(BuildRoad, field_48) == 0x48, "BuildRoad +0x48");
static_assert(offsetof(BuildRoad, field_50) == 0x50, "BuildRoad +0x50");
static_assert(offsetof(BuildRoad, field_58) == 0x58, "BuildRoad +0x58");
static_assert(offsetof(BuildRoad, field_5c) == 0x5c, "BuildRoad +0x5c");
static_assert(offsetof(BuildRoad, field_60) == 0x60, "BuildRoad +0x60");
static_assert(offsetof(BuildRoad, field_64) == 0x64, "BuildRoad +0x64");
static_assert(offsetof(BuildRoad, field_68) == 0x68, "BuildRoad +0x68");
