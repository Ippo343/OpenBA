#pragma once
// ============================================================================
// code/Game/Component/TornadoDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TornadoDescription", parent "Description", ti 0x1401c4aa0
//   create 0x14000d560: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000d5a0.
//   table 0x14015ab00, 10 slots:
//     [0] 0x14000d5e0 scalar deleting dtor   [1] 0x14000d550 TypeName   [2] 0x14000d540 Type
//     [3] 0x140058e30 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140058d90 Serialize (`MOV AL,1`)
//     [6] 0x14000d5b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000d5d0 GetComponentType: TornadoComponent's TypeId
//     [9] 0x14000d5a0 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/Value.h"

class TornadoDescription final : public Description {
    DECLARE_EXE_TYPE(TornadoDescription)

public:
    TornadoDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14000d5e0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140058e30
    bool              Serialize(void* stream) override;                    // [5] 0x140058d90
    Object::External* Reconstruct() override;                              // [6] 0x14000d5b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000d5d0
    virtual Object::External* Construct();                                 // [9] 0x14000d5a0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _f20;                                 // +0x20
    int32_t _i24;                               // +0x24
    System::Time _time28;                       // +0x28 System::Time, zeroed
    int32_t _i30;                               // +0x30
    int32_t _i34;                               // +0x34

protected:
    ~TornadoDescription() = default;
};

static_assert(sizeof(TornadoDescription) == 0x38, "TornadoDescription is 0x38 (create 0x14000d560 MemAlloc(0x38))");
static_assert(alignof(TornadoDescription) == 8, "TornadoDescription alignment");
static_assert(offsetof(TornadoDescription, _f20) == 0x20, "TornadoDescription::_f20 @ +0x20");
static_assert(offsetof(TornadoDescription, _i24) == 0x24, "TornadoDescription::_i24 @ +0x24");
static_assert(offsetof(TornadoDescription, _time28) == 0x28, "TornadoDescription::_time28 @ +0x28");
static_assert(offsetof(TornadoDescription, _i30) == 0x30, "TornadoDescription::_i30 @ +0x30");
static_assert(offsetof(TornadoDescription, _i34) == 0x34, "TornadoDescription::_i34 @ +0x34");
