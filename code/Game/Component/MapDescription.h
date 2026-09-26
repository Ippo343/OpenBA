#pragma once
// ============================================================================
// code/Game/Component/MapDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MapDescription", parent "Description", ti 0x1401c7a18
//   create 0x140035760: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400357a0.
//   table 0x140163b20, 10 slots:
//     [0] 0x1400357e0 scalar deleting dtor   [1] 0x140035750 TypeName   [2] 0x140035740 Type
//     [3] 0x140124610 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140124560 Serialize (`MOV AL,1`)
//     [6] 0x1400357b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400357d0 GetComponentType: MapComponent's TypeId
//     [9] 0x1400357a0 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/String.h"
#include "system/Value.h"

class MapDescription final : public Description {
    DECLARE_EXE_TYPE(MapDescription)

public:
    MapDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x1400357e0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140124610
    bool              Serialize(void* stream) override;                    // [5] 0x140124560
    Object::External* Reconstruct() override;                              // [6] 0x1400357b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400357d0
    virtual Object::External* Construct();                                 // [9] 0x1400357a0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    uint32_t _overlayType;                      // +0x20 enum (serialized through the uint door)
    uint8_t _pad24[4];                          // +0x24 never written by the ctor
    System::String _label;                      // +0x28 String (empty)
    bool _b30;                                  // +0x30
    bool _b31;                                  // +0x31
    bool _b32;                                  // +0x32
    uint8_t _pad33[1];                          // +0x33 never written by the ctor
    System::Color _color;                       // +0x34 ARGB, opaque black

protected:
    ~MapDescription() = default;
};

static_assert(sizeof(MapDescription) == 0x38, "MapDescription is 0x38 (create 0x140035760 MemAlloc(0x38))");
static_assert(alignof(MapDescription) == 8, "MapDescription alignment");
static_assert(offsetof(MapDescription, _overlayType) == 0x20, "MapDescription::_overlayType @ +0x20");
static_assert(offsetof(MapDescription, _label) == 0x28, "MapDescription::_label @ +0x28");
static_assert(offsetof(MapDescription, _b30) == 0x30, "MapDescription::_b30 @ +0x30");
static_assert(offsetof(MapDescription, _b31) == 0x31, "MapDescription::_b31 @ +0x31");
static_assert(offsetof(MapDescription, _b32) == 0x32, "MapDescription::_b32 @ +0x32");
static_assert(offsetof(MapDescription, _color) == 0x34, "MapDescription::_color @ +0x34");
