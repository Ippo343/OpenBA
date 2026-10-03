#pragma once
// ============================================================================
// code/Game/Component/StatusIconDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StatusIconDescription", parent "Description", ti 0x1401c7e40
//   create 0x140038920: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140038990.
//   table 0x1401646a0, 10 slots:
//     [0] 0x140038a60 scalar deleting dtor   [1] 0x140038910 TypeName   [2] 0x140038900 Type
//     [3] 0x14012c1f0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14012c180 Serialize (`MOV AL,1`)
//     [6] 0x1400389f0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140038a50 GetComponentType: StatusIconComponent's TypeId
//     [9] 0x140038990 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

namespace UI { class SpriteSheet; }

class StatusIconDescription final : public Description {
    DECLARE_EXE_TYPE(StatusIconDescription)

public:
    StatusIconDescription();

    ~StatusIconDescription() override = default;                           // [0] 0x140038a60
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14012c1f0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14012c180
    Object::External* Reconstruct() override;                              // [6] 0x1400389f0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140038a50
    virtual Object::External* Construct();                                 // [9] 0x140038990

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    UI::SpriteSheet* _spriteSheet;              // +0x20 ExternalPtr<UI::SpriteSheet> (Serialize 0x14000bd30, typeInfo 0x140151480)
    float _scale;                               // +0x28 the icon size (Setup 0x1400aa440's size)
    float _scale2;                              // +0x2c not serialized; the icon's height above its anchor (Tick 0x14012bbd0)
    bool _flag;                                 // +0x30 the camera-constant screen size flag (Setup's `constant`)
    uint8_t _pad31[3];                          // +0x31 zeroed by the ctor's 4-byte store
    uint8_t _pad34[4];                          // +0x34 never written by the ctor
};

static_assert(sizeof(StatusIconDescription) == 0x38, "StatusIconDescription is 0x38 (create 0x140038920 MemAlloc(0x38))");
static_assert(alignof(StatusIconDescription) == 8, "StatusIconDescription alignment");
static_assert(offsetof(StatusIconDescription, _spriteSheet) == 0x20, "StatusIconDescription::_spriteSheet @ +0x20");
static_assert(offsetof(StatusIconDescription, _scale) == 0x28, "StatusIconDescription::_scale @ +0x28");
static_assert(offsetof(StatusIconDescription, _scale2) == 0x2c, "StatusIconDescription::_scale2 @ +0x2c");
static_assert(offsetof(StatusIconDescription, _flag) == 0x30, "StatusIconDescription::_flag @ +0x30");
