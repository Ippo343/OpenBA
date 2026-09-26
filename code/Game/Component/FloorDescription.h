#pragma once
// ============================================================================
// code/Game/Component/FloorDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FloorDescription", parent "Description", ti 0x1401c7708
//   create 0x140034660: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400346d0.
//   table 0x1401633e0, 10 slots:
//     [0] 0x140034790 scalar deleting dtor   [1] 0x140034650 TypeName   [2] 0x140034640 Type
//     [3] 0x14011f6e0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14011f680 Serialize (`MOV AL,1`)
//     [6] 0x140034720 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140034780 GetComponentType: FloorComponent's TypeId
//     [9] 0x1400346d0 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class FloorDescription final : public Description {
    DECLARE_EXE_TYPE(FloorDescription)

public:
    FloorDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140034790
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14011f6e0
    bool              Serialize(void* stream) override;                    // [5] 0x14011f680
    Object::External* Reconstruct() override;                              // [6] 0x140034720
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140034780
    virtual Object::External* Construct();                                 // [9] 0x1400346d0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    bool _b28;                                  // +0x28
    uint8_t _pad29[7];                          // +0x29 never written by the ctor

protected:
    ~FloorDescription() = default;
};

static_assert(sizeof(FloorDescription) == 0x30, "FloorDescription is 0x30 (create 0x140034660 MemAlloc(0x30))");
static_assert(alignof(FloorDescription) == 8, "FloorDescription alignment");
static_assert(offsetof(FloorDescription, _ref20) == 0x20, "FloorDescription::_ref20 @ +0x20");
static_assert(offsetof(FloorDescription, _b28) == 0x28, "FloorDescription::_b28 @ +0x28");
