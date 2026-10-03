#pragma once
// ============================================================================
// code/Game/Component/OrchardDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "OrchardDescription", parent "Description", ti 0x1401c46e8
//   create 0x1400098d0: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140009940.
//   table 0x14015a210, 10 slots:
//     [0] 0x140009a10 scalar deleting dtor   [1] 0x1400098c0 TypeName   [2] 0x1400098b0 Type
//     [3] 0x1400523a0 GetFileVersion = 3   [4] External::Compile (import, inherited)
//     [5] 0x140052300 Serialize (`MOV AL,1`)
//     [6] 0x1400099a0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140009a00 GetComponentType: OrchardComponent's TypeId
//     [9] 0x140009940 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class OrchardDescription final : public Description {
    DECLARE_EXE_TYPE(OrchardDescription)

public:
    OrchardDescription();

    ~OrchardDescription() override = default;                              // [0] 0x140009a10
    int32_t           GetFileVersion() const override { return 3; }    // [3] 0x1400523a0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x140052300
    Object::External* Reconstruct() override;                              // [6] 0x1400099a0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140009a00
    virtual Object::External* Construct();                                 // [9] 0x140009940

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    uint32_t _type;                             // +0x20 enum (serialized through the uint door)
    int32_t _spacingX;                          // +0x24 cells between tree columns (state 9 enter)
    int32_t _spacingY;                          // +0x28 cells between tree rows
    float _boostRate;                           // +0x2c growth added per boost (OnMessage 2); ctor stores int 0
};

static_assert(sizeof(OrchardDescription) == 0x30, "OrchardDescription is 0x30 (create 0x1400098d0 MemAlloc(0x30))");
static_assert(alignof(OrchardDescription) == 8, "OrchardDescription alignment");
static_assert(offsetof(OrchardDescription, _type) == 0x20, "OrchardDescription::_type @ +0x20");
static_assert(offsetof(OrchardDescription, _spacingX) == 0x24, "OrchardDescription::_spacingX @ +0x24");
static_assert(offsetof(OrchardDescription, _spacingY) == 0x28, "OrchardDescription::_spacingY @ +0x28");
static_assert(offsetof(OrchardDescription, _boostRate) == 0x2c, "OrchardDescription::_boostRate @ +0x2c");
