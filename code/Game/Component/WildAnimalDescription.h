#pragma once
// ============================================================================
// code/Game/Component/WildAnimalDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "WildAnimalDescription", parent "Description", ti 0x1401c8038
//   create 0x140039c80: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140039ce0.
//   table 0x140164b70, 10 slots:
//     [0] 0x140039d90 scalar deleting dtor   [1] 0x140039c70 TypeName   [2] 0x140039c60 Type
//     [3] 0x14012e480 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14012e440 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x140039d30 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140039d80 GetComponentType: WildAnimalComponent's TypeId
//     [9] 0x140039ce0 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class WildAnimalDescription final : public Description {
    DECLARE_EXE_TYPE(WildAnimalDescription)

public:
    WildAnimalDescription() = default;

    ~WildAnimalDescription() override = default;                           // [0] 0x140039d90
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14012e480
    bool              Serialize(void* stream) override;                    // [5] 0x14012e440
    Object::External* Reconstruct() override;                              // [6] 0x140039d30
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140039d80
    virtual Object::External* Construct();                                 // [9] 0x140039ce0
};

static_assert(sizeof(WildAnimalDescription) == 0x20, "WildAnimalDescription is 0x20 (create 0x140039c80 MemAlloc(0x20))");
static_assert(alignof(WildAnimalDescription) == 8, "WildAnimalDescription alignment");
