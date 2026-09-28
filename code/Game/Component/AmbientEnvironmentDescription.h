#pragma once
// ============================================================================
// code/Game/Component/AmbientEnvironmentDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AmbientEnvironmentDescription", parent "Description", ti 0x1401c3f08
//   create 0x140001f40: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140001fb0.
//   table 0x140158bf0, 10 slots:
//     [0] 0x140002100 scalar deleting dtor   [1] 0x140001f30 TypeName   [2] 0x140001f20 Type
//     [3] 0x140043a70 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x140043950 Serialize (`MOV AL,1`)
//     [6] 0x140002010 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140002070 GetComponentType: AmbientEnvironmentComponent's TypeId
//     [9] 0x140001fb0 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class AmbientEnvironmentDescription final : public Description {
    DECLARE_EXE_TYPE(AmbientEnvironmentDescription)

public:
    AmbientEnvironmentDescription();

    ~AmbientEnvironmentDescription() override;                             // [0] 0x140002100
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140043a70
    bool              Serialize(void* stream) override;                    // [5] 0x140043950
    Object::External* Reconstruct() override;                              // [6] 0x140002010
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140002070
    virtual Object::External* Construct();                                 // [9] 0x140001fb0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _entries;                             // +0x20 entry array data (0x28-byte elements: enum, 3 ExternalPtr, int)
    int32_t _entryCount;                        // +0x28
    int32_t _entryCap;                          // +0x2c
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
};

static_assert(sizeof(AmbientEnvironmentDescription) == 0x38, "AmbientEnvironmentDescription is 0x38 (create 0x140001f40 MemAlloc(0x38))");
static_assert(alignof(AmbientEnvironmentDescription) == 8, "AmbientEnvironmentDescription alignment");
static_assert(offsetof(AmbientEnvironmentDescription, _entries) == 0x20, "AmbientEnvironmentDescription::_entries @ +0x20");
static_assert(offsetof(AmbientEnvironmentDescription, _entryCount) == 0x28, "AmbientEnvironmentDescription::_entryCount @ +0x28");
static_assert(offsetof(AmbientEnvironmentDescription, _entryCap) == 0x2c, "AmbientEnvironmentDescription::_entryCap @ +0x2c");
static_assert(offsetof(AmbientEnvironmentDescription, _f30) == 0x30, "AmbientEnvironmentDescription::_f30 @ +0x30");
static_assert(offsetof(AmbientEnvironmentDescription, _f34) == 0x34, "AmbientEnvironmentDescription::_f34 @ +0x34");
