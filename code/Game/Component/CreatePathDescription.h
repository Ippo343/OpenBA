#pragma once
// ============================================================================
// code/Game/Component/CreatePathDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CreatePathDescription", parent "Description", ti 0x1401c4138
//   create 0x140004c30: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140004c90.
//   table 0x140159210, 10 slots:
//     [0] 0x140004d30 scalar deleting dtor   [1] 0x140004c20 TypeName   [2] 0x140004c10 Type
//     [3] 0x140047ab0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140047a70 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x140004ce0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140004950 GetComponentType: 0 (a data-only description: no component; shared body 0x140004950 `MOV dword [RDX],0`)
//     [9] 0x140004c90 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class CreatePathDescription final : public Description {
    DECLARE_EXE_TYPE(CreatePathDescription)

public:
    CreatePathDescription() = default;

    ~CreatePathDescription() override = default;                           // [0] 0x140004d30
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140047ab0
    bool              Serialize(void* stream) override;                    // [5] 0x140047a70
    Object::External* Reconstruct() override;                              // [6] 0x140004ce0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140004950
    virtual Object::External* Construct();                                 // [9] 0x140004c90
};

static_assert(sizeof(CreatePathDescription) == 0x20, "CreatePathDescription is 0x20 (create 0x140004c30 MemAlloc(0x20))");
static_assert(alignof(CreatePathDescription) == 8, "CreatePathDescription alignment");
