#pragma once
// ============================================================================
// code/Game/Component/StorageDisplayDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StorageDisplayDescription", parent "Description", ti 0x1401c4988
//   create 0x14000c7d0: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x14000c830.
//   table 0x14015a800, 10 slots:
//     [0] 0x14000c8e0 scalar deleting dtor   [1] 0x14000c7c0 TypeName   [2] 0x14000c7b0 Type
//     [3] 0x140056ac0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140056a80 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x14000c880 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000c8d0 GetComponentType: StorageDisplayComponent's TypeId
//     [9] 0x14000c830 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class StorageDisplayDescription final : public Description {
    DECLARE_EXE_TYPE(StorageDisplayDescription)

public:
    StorageDisplayDescription() = default;

    ~StorageDisplayDescription() override = default;                       // [0] 0x14000c8e0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140056ac0
    bool              Serialize(void* stream) override;                    // [5] 0x140056a80
    Object::External* Reconstruct() override;                              // [6] 0x14000c880
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000c8d0
    virtual Object::External* Construct();                                 // [9] 0x14000c830
};

static_assert(sizeof(StorageDisplayDescription) == 0x20, "StorageDisplayDescription is 0x20 (create 0x14000c7d0 MemAlloc(0x20))");
static_assert(alignof(StorageDisplayDescription) == 8, "StorageDisplayDescription alignment");
