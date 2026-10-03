#pragma once
// ============================================================================
// code/Game/Component/ClearAreaDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ClearAreaDescription", parent "Description", ti 0x1401c7200
//   create 0x140030f00: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140030f80.
//   table 0x140162760, 10 slots:
//     [0] 0x140031060 scalar deleting dtor   [1] 0x140030ef0 TypeName   [2] 0x140030ee0 Type
//     [3] 0x1401181c0 GetFileVersion = 6   [4] External::Compile (import, inherited)
//     [5] 0x1401180f0 Serialize (`MOV AL,1`)
//     [6] 0x140030fe0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140031050 GetComponentType: ClearAreaComponent's TypeId
//     [9] 0x140030f80 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;   // Component/ComponentDescription.h

class ClearAreaDescription final : public Description {
    DECLARE_EXE_TYPE(ClearAreaDescription)

public:
    ClearAreaDescription();

    ~ClearAreaDescription() override = default;                            // [0] 0x140031060
    int32_t           GetFileVersion() const override { return 6; }    // [3] 0x1401181c0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x1401180f0
    Object::External* Reconstruct() override;                              // [6] 0x140030fe0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140031050
    virtual Object::External* Construct();                                 // [9] 0x140030f80

    // ---- fields (PUBLIC: the components read them by name) ----
    // Serialize 0x1401180f0: ExternalPtr<ComponentDescription> (0x140002340), six bools, int.
    ComponentDescription* _target;  // +0x20 the resource kind to clear (compared against an
                                    //       occupant's ComponentEntity::_description; 0 = any)
    bool _clearTrees;               // +0x28 ClearArea work +0x70; IsDone: 0 => every cell empty
    bool _clearGround;              // +0x29 ClearArea work +0x71; IsDone: ground must be clear
    bool _clearBuildings;           // +0x2a ClearArea work +0x72; IsDone: 1 => every cell empty
    bool _priority;                 // +0x2b the work-scheduler post flag (0x14012ec60 arg 3)
    bool _regrowTerrain;            // +0x2c FinishClear 0x140118270: refresh the region, event 5
    bool _instant;                  // +0x2d State0Enter 0x140117960: sweep the footprint at once
    uint8_t _pad2e[2];              // +0x2e never written by the ctor
    int32_t _value;                             // +0x30
    uint8_t _pad34[4];                          // +0x34 never written by the ctor
};

static_assert(sizeof(ClearAreaDescription) == 0x38, "ClearAreaDescription is 0x38 (create 0x140030f00 MemAlloc(0x38))");
static_assert(alignof(ClearAreaDescription) == 8, "ClearAreaDescription alignment");
static_assert(offsetof(ClearAreaDescription, _target) == 0x20, "ClearAreaDescription::_target @ +0x20");
static_assert(offsetof(ClearAreaDescription, _clearTrees) == 0x28, "ClearAreaDescription::_clearTrees @ +0x28");
static_assert(offsetof(ClearAreaDescription, _clearGround) == 0x29, "ClearAreaDescription::_clearGround @ +0x29");
static_assert(offsetof(ClearAreaDescription, _clearBuildings) == 0x2a, "ClearAreaDescription::_clearBuildings @ +0x2a");
static_assert(offsetof(ClearAreaDescription, _priority) == 0x2b, "ClearAreaDescription::_priority @ +0x2b");
static_assert(offsetof(ClearAreaDescription, _regrowTerrain) == 0x2c, "ClearAreaDescription::_regrowTerrain @ +0x2c");
static_assert(offsetof(ClearAreaDescription, _instant) == 0x2d, "ClearAreaDescription::_instant @ +0x2d");
static_assert(offsetof(ClearAreaDescription, _value) == 0x30, "ClearAreaDescription::_value @ +0x30");
