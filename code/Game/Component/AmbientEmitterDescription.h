#pragma once
// ============================================================================
// code/Game/Component/AmbientEmitterDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AmbientEmitterDescription", parent "Description", ti 0x1401c3e98
//   create 0x140001510: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140001580.
//   table 0x140158ae0, 10 slots:
//     [0] 0x140001660 scalar deleting dtor   [1] 0x140001500 TypeName   [2] 0x1400014f0 Type
//     [3] 0x140043250 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x1400431b0 Serialize (`MOV AL,1`)
//     [6] 0x1400015e0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140001650 GetComponentType: AmbientEmitterComponent's TypeId
//     [9] 0x140001580 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

namespace Audio { class SoundEffect; }

class AmbientEmitterDescription final : public Description {
    DECLARE_EXE_TYPE(AmbientEmitterDescription)

public:
    AmbientEmitterDescription();

    ~AmbientEmitterDescription() override;                                 // [0] 0x140001660
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140043250
    bool              Serialize(void* stream) override;                    // [5] 0x1400431b0
    Object::External* Reconstruct() override;                              // [6] 0x1400015e0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140001650
    virtual Object::External* Construct();                                 // [9] 0x140001580

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    bool _enabled;                              // +0x20
    uint8_t _pad21[7];                          // +0x21 never written by the ctor
    Audio::SoundEffect** _emitters;             // +0x28 ExternalPtr<SoundEffect> array data
    int32_t _emitterCount;                      // +0x30
    int32_t _emitterCap;                        // +0x34
};

static_assert(sizeof(AmbientEmitterDescription) == 0x38, "AmbientEmitterDescription is 0x38 (create 0x140001510 MemAlloc(0x38))");
static_assert(alignof(AmbientEmitterDescription) == 8, "AmbientEmitterDescription alignment");
static_assert(offsetof(AmbientEmitterDescription, _enabled) == 0x20, "AmbientEmitterDescription::_enabled @ +0x20");
static_assert(offsetof(AmbientEmitterDescription, _emitters) == 0x28, "AmbientEmitterDescription::_emitters @ +0x28");
static_assert(offsetof(AmbientEmitterDescription, _emitterCount) == 0x30, "AmbientEmitterDescription::_emitterCount @ +0x30");
static_assert(offsetof(AmbientEmitterDescription, _emitterCap) == 0x34, "AmbientEmitterDescription::_emitterCap @ +0x34");
