#pragma once
// ============================================================================
// code/Game/Component/StorageLocationDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StorageLocationDescription", parent "Description", ti 0x1401c7f20
//   create 0x140038e50: MemAlloc(0x28, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140038eb0.
//   table 0x140164910, 10 slots:
//     [0] 0x140038f60 scalar deleting dtor   [1] 0x140038e40 TypeName   [2] 0x140038e30 Type
//     [3] 0x14012d4c0 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14012d470 Serialize (`MOV AL,1`)
//     [6] 0x140038f00 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140038f50 GetComponentType: StorageLocationComponent's TypeId
//     [9] 0x140038eb0 Construct (folded with createInPlace)
//   sizeof 0x28.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class StorageLocationDescription final : public Description {
    DECLARE_EXE_TYPE(StorageLocationDescription)

public:
    StorageLocationDescription();

    ~StorageLocationDescription() override = default;                      // [0] 0x140038f60
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14012d4c0
    bool              Serialize(void* stream) override;                    // [5] 0x14012d470
    Object::External* Reconstruct() override;                              // [6] 0x140038f00
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140038f50
    virtual Object::External* Construct();                                 // [9] 0x140038eb0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    uint32_t _locationType;                     // +0x20 enum (serialized through the uint door)
    uint8_t _pad24[4];                          // +0x24 never written by the ctor
};

static_assert(sizeof(StorageLocationDescription) == 0x28, "StorageLocationDescription is 0x28 (create 0x140038e50 MemAlloc(0x28))");
static_assert(alignof(StorageLocationDescription) == 8, "StorageLocationDescription alignment");
static_assert(offsetof(StorageLocationDescription, _locationType) == 0x20, "StorageLocationDescription::_locationType @ +0x20");
