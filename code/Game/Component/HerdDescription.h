#pragma once
// ============================================================================
// code/Game/Component/HerdDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HerdDescription", parent "Description", ti 0x1401c7858
//   create 0x140034d30: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140034d90.
//   table 0x140163700, 10 slots:
//     [0] 0x140034e40 scalar deleting dtor   [1] 0x140034d20 TypeName   [2] 0x140034d10 Type
//     [3] 0x1401222a0 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x1401221e0 Serialize (`MOV AL,1`)
//     [6] 0x140034de0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140034e30 GetComponentType: HerdComponent's TypeId
//     [9] 0x140034d90 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;

class HerdDescription final : public Description {
    DECLARE_EXE_TYPE(HerdDescription)

public:
    HerdDescription();

    ~HerdDescription() override = default;                                 // [0] 0x140034e40
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x1401222a0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x1401221e0
    Object::External* Reconstruct() override;                              // [6] 0x140034de0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140034e30
    virtual Object::External* Construct();                                 // [9] 0x140034d90

    // ---- fields (B.4 COMP-d: names from what HerdComponent does with them,
    // Update 0x140121bf0, AdjustSize 0x140121450, Populate 0x140120f50) ----
    ComponentDescription* _animal;              // +0x20 ExternalPtr: the entity description each animal is built from
    int32_t _maxSize;                           // +0x28 herd size cap (scaled by the food score)
    int32_t _minSize;                           // +0x2c herd size floor (PopulationComponent Startup: Rand(max - min) + min)
    float _grazeMinutes;                        // +0x30 graze time, fixed part (x 300 s)
    float _grazeRandomMinutes;                  // +0x34 graze time, random part
    int32_t _spread;                            // +0x38 side of the wander box / spawn radius, tiles
    float _growMinutes;                         // +0x3c growth interval (x 300 s)
};

static_assert(sizeof(HerdDescription) == 0x40, "HerdDescription is 0x40 (create 0x140034d30 MemAlloc(0x40))");
static_assert(alignof(HerdDescription) == 8, "HerdDescription alignment");
static_assert(offsetof(HerdDescription, _animal) == 0x20, "HerdDescription::_animal @ +0x20");
static_assert(offsetof(HerdDescription, _maxSize) == 0x28, "HerdDescription::_maxSize @ +0x28");
static_assert(offsetof(HerdDescription, _minSize) == 0x2c, "HerdDescription::_minSize @ +0x2c");
static_assert(offsetof(HerdDescription, _grazeMinutes) == 0x30, "HerdDescription::_grazeMinutes @ +0x30");
static_assert(offsetof(HerdDescription, _grazeRandomMinutes) == 0x34, "HerdDescription::_grazeRandomMinutes @ +0x34");
static_assert(offsetof(HerdDescription, _spread) == 0x38, "HerdDescription::_spread @ +0x38");
static_assert(offsetof(HerdDescription, _growMinutes) == 0x3c, "HerdDescription::_growMinutes @ +0x3c");
