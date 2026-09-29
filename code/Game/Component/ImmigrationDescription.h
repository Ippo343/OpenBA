#pragma once
// ============================================================================
// code/Game/Component/ImmigrationDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ImmigrationDescription", parent "Description", ti 0x1401c44b8
//   create 0x140007a80: MemAlloc(0x68, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140007ac0.
//   table 0x140159bc0, 10 slots:
//     [0] 0x140007b00 scalar deleting dtor   [1] 0x140007a70 TypeName   [2] 0x140007a60 Type
//     [3] 0x14004e930 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14004e7f0 Serialize (`MOV AL,1`)
//     [6] 0x140007ad0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140007af0 GetComponentType: ImmigrationComponent's TypeId
//     [9] 0x140007ac0 Construct (folded with createInPlace)
//   sizeof 0x68.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;

class ImmigrationDescription final : public Description {
    DECLARE_EXE_TYPE(ImmigrationDescription)

public:
    ImmigrationDescription();

    ~ImmigrationDescription() override;                                    // [0] 0x140007b00
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14004e930
    bool              Serialize(void* stream) override;                    // [5] 0x14004e7f0
    Object::External* Reconstruct() override;                              // [6] 0x140007ad0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140007af0
    virtual Object::External* Construct();                                 // [9] 0x140007ac0

    // ---- fields (B.4 COMP-d: names from what ImmigrationComponent does with them,
    // immigrationcomponent_states.cpp; types from Serialize 0x14004e7f0) ----
    ComponentDescription* _nomad;               // +0x20 ExternalPtr: the nomad entity each group member is built from
    int32_t _minPopulation;                     // +0x28 CanAccept: the town's citizen count floor (80)
    uint8_t _pad2c[4];                          // +0x2c never written by the ctor
    ComponentDescription** _requiredBuildings;  // +0x30 Array<ExternalPtr<ComponentDescription>> data: each must be built
    int32_t _requiredBuildingCount;             // +0x38
    int32_t _requiredBuildingCap;               // +0x3c
    float _groupFractionMin;                    // +0x40 group size as a fraction of the population (0.05)
    float _groupFractionMax;                    // +0x44 (0.15)
    float _waitMonths;                          // +0x48 wait before the arrival season, fixed part (24)
    float _waitRandomMonths;                    // +0x4c random part (48)
    int32_t _arrivalMonthFirst;                 // +0x50 arrival month window (1..3)
    int32_t _arrivalMonthLast;                  // +0x54
    int32_t _leaveMonth;                        // +0x58 unanswered groups leave from this month (7)
    int32_t _spawnEdgeDistance;                 // +0x5c spawn distance from the map edge (10)
    float _diseaseChance;                       // +0x60 disease seed written to the town on accept (1.0)
    int32_t _diseaseMonths;                     // +0x64 (6)
};

static_assert(sizeof(ImmigrationDescription) == 0x68, "ImmigrationDescription is 0x68 (create 0x140007a80 MemAlloc(0x68))");
static_assert(alignof(ImmigrationDescription) == 8, "ImmigrationDescription alignment");
static_assert(offsetof(ImmigrationDescription, _nomad) == 0x20, "ImmigrationDescription::_nomad @ +0x20");
static_assert(offsetof(ImmigrationDescription, _minPopulation) == 0x28, "ImmigrationDescription::_minPopulation @ +0x28");
static_assert(offsetof(ImmigrationDescription, _requiredBuildings) == 0x30, "ImmigrationDescription::_requiredBuildings @ +0x30");
static_assert(offsetof(ImmigrationDescription, _requiredBuildingCount) == 0x38, "ImmigrationDescription::_requiredBuildingCount @ +0x38");
static_assert(offsetof(ImmigrationDescription, _requiredBuildingCap) == 0x3c, "ImmigrationDescription::_requiredBuildingCap @ +0x3c");
static_assert(offsetof(ImmigrationDescription, _groupFractionMin) == 0x40, "ImmigrationDescription::_groupFractionMin @ +0x40");
static_assert(offsetof(ImmigrationDescription, _groupFractionMax) == 0x44, "ImmigrationDescription::_groupFractionMax @ +0x44");
static_assert(offsetof(ImmigrationDescription, _waitMonths) == 0x48, "ImmigrationDescription::_waitMonths @ +0x48");
static_assert(offsetof(ImmigrationDescription, _waitRandomMonths) == 0x4c, "ImmigrationDescription::_waitRandomMonths @ +0x4c");
static_assert(offsetof(ImmigrationDescription, _arrivalMonthFirst) == 0x50, "ImmigrationDescription::_arrivalMonthFirst @ +0x50");
static_assert(offsetof(ImmigrationDescription, _arrivalMonthLast) == 0x54, "ImmigrationDescription::_arrivalMonthLast @ +0x54");
static_assert(offsetof(ImmigrationDescription, _leaveMonth) == 0x58, "ImmigrationDescription::_leaveMonth @ +0x58");
static_assert(offsetof(ImmigrationDescription, _spawnEdgeDistance) == 0x5c, "ImmigrationDescription::_spawnEdgeDistance @ +0x5c");
static_assert(offsetof(ImmigrationDescription, _diseaseChance) == 0x60, "ImmigrationDescription::_diseaseChance @ +0x60");
static_assert(offsetof(ImmigrationDescription, _diseaseMonths) == 0x64, "ImmigrationDescription::_diseaseMonths @ +0x64");
