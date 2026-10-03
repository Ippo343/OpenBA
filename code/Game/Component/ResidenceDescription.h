#pragma once
// ============================================================================
// code/Game/Component/ResidenceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ResidenceDescription", parent "Description", ti 0x1401c7d28
//   create 0x1400374a0: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140037500.
//   table 0x140164350, 10 slots:
//     [0] 0x1400375b0 scalar deleting dtor   [1] 0x140037490 TypeName   [2] 0x140037480 Type
//     [3] 0x14012a460 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14012a3d0 Serialize (`MOV AL,1`)
//     [6] 0x140037550 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400375a0 GetComponentType: ResidenceComponent's TypeId
//     [9] 0x140037500 Construct (folded with createInPlace)
//   retail's ctor leaves +0x20..+0x2c uninitialised; the tree zeroes them (kept)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ResidenceDescription final : public Description {
    DECLARE_EXE_TYPE(ResidenceDescription)

public:
    ResidenceDescription();

    ~ResidenceDescription() override = default;                            // [0] 0x1400375b0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14012a460
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14012a3d0
    Object::External* Reconstruct() override;                              // [6] 0x140037550
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400375a0
    virtual Object::External* Construct();                                 // [9] 0x140037500

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    int32_t _maxFamilies;                       // +0x20 family-slot count
    int32_t _maxResidentsPerFamily;             // +0x24
    float _coldThreshold;                       // +0x28 temp below which the house burns fuel
    float _heatInterval;                        // +0x2c fuel-consumption period
};

static_assert(sizeof(ResidenceDescription) == 0x30, "ResidenceDescription is 0x30 (create 0x1400374a0 MemAlloc(0x30))");
static_assert(alignof(ResidenceDescription) == 8, "ResidenceDescription alignment");
static_assert(offsetof(ResidenceDescription, _maxFamilies) == 0x20, "ResidenceDescription::_maxFamilies @ +0x20");
static_assert(offsetof(ResidenceDescription, _maxResidentsPerFamily) == 0x24, "ResidenceDescription::_maxResidentsPerFamily @ +0x24");
static_assert(offsetof(ResidenceDescription, _coldThreshold) == 0x28, "ResidenceDescription::_coldThreshold @ +0x28");
static_assert(offsetof(ResidenceDescription, _heatInterval) == 0x2c, "ResidenceDescription::_heatInterval @ +0x2c");
