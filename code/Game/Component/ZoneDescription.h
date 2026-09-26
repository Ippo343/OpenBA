#pragma once
// ============================================================================
// code/Game/Component/ZoneDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ZoneDescription", parent "Description", ti 0x1401c8188
//   create 0x14003a700: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x14003a760.
//   table 0x140164eb0, 10 slots:
//     [0] 0x14003a810 scalar deleting dtor   [1] 0x14003a6f0 TypeName   [2] 0x14003a6e0 Type
//     [3] 0x140130470 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140130430 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x14003a7b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14003a800 GetComponentType: ZoneComponent's TypeId
//     [9] 0x14003a760 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ZoneDescription final : public Description {
    DECLARE_EXE_TYPE(ZoneDescription)

public:
    ZoneDescription() = default;

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14003a810
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140130470
    bool              Serialize(void* stream) override;                    // [5] 0x140130430
    Object::External* Reconstruct() override;                              // [6] 0x14003a7b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14003a800
    virtual Object::External* Construct();                                 // [9] 0x14003a760

protected:
    ~ZoneDescription() = default;
};

static_assert(sizeof(ZoneDescription) == 0x20, "ZoneDescription is 0x20 (create 0x14003a700 MemAlloc(0x20))");
static_assert(alignof(ZoneDescription) == 8, "ZoneDescription alignment");
