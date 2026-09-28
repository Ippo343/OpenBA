#pragma once
// ============================================================================
// code/Game/Component/CreateRemovalDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CreateRemovalDescription", parent "Description", ti 0x1401c74a0
//   create 0x140032b70: MemAlloc(0x48, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140032be0.
//   table 0x140162d80, 10 slots:
//     [0] 0x140032ca0 scalar deleting dtor   [1] 0x140032b60 TypeName   [2] 0x140032b50 Type
//     [3] 0x140119bc0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140119ac0 Serialize (`MOV AL,1`)
//     [6] 0x140032c40 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140004950 GetComponentType: 0 (data-only: shared body 0x140004950 `MOV dword [RDX],0`)
//     [9] 0x140032be0 Construct (folded with createInPlace)
//   sizeof 0x48.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class CreateRemovalDescription final : public Description {
    DECLARE_EXE_TYPE(CreateRemovalDescription)

public:
    CreateRemovalDescription();

    ~CreateRemovalDescription() override;                                  // [0] 0x140032ca0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140119bc0
    bool              Serialize(void* stream) override;                    // [5] 0x140119ac0
    Object::External* Reconstruct() override;                              // [6] 0x140032c40
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140004950
    virtual Object::External* Construct();                                 // [9] 0x140032be0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    bool _b20;                                  // +0x20
    uint8_t _pad21[7];                          // +0x21 never written by the ctor
    void* _refs;                                // +0x28 heap ExternalPtr array (8-byte elements)
    int32_t _refCount;                          // +0x30
    int32_t _refCap;                            // +0x34
    void* _ref38;                               // +0x38 ExternalPtr (a raw pointer)
    bool _b40;                                  // +0x40
    bool _b41;                                  // +0x41
    bool _b42;                                  // +0x42
    bool _b43;                                  // +0x43
    uint8_t _pad44[4];                          // +0x44 never written by the ctor
};

static_assert(sizeof(CreateRemovalDescription) == 0x48, "CreateRemovalDescription is 0x48 (create 0x140032b70 MemAlloc(0x48))");
static_assert(alignof(CreateRemovalDescription) == 8, "CreateRemovalDescription alignment");
static_assert(offsetof(CreateRemovalDescription, _b20) == 0x20, "CreateRemovalDescription::_b20 @ +0x20");
static_assert(offsetof(CreateRemovalDescription, _refs) == 0x28, "CreateRemovalDescription::_refs @ +0x28");
static_assert(offsetof(CreateRemovalDescription, _refCount) == 0x30, "CreateRemovalDescription::_refCount @ +0x30");
static_assert(offsetof(CreateRemovalDescription, _refCap) == 0x34, "CreateRemovalDescription::_refCap @ +0x34");
static_assert(offsetof(CreateRemovalDescription, _ref38) == 0x38, "CreateRemovalDescription::_ref38 @ +0x38");
static_assert(offsetof(CreateRemovalDescription, _b40) == 0x40, "CreateRemovalDescription::_b40 @ +0x40");
static_assert(offsetof(CreateRemovalDescription, _b41) == 0x41, "CreateRemovalDescription::_b41 @ +0x41");
static_assert(offsetof(CreateRemovalDescription, _b42) == 0x42, "CreateRemovalDescription::_b42 @ +0x42");
static_assert(offsetof(CreateRemovalDescription, _b43) == 0x43, "CreateRemovalDescription::_b43 @ +0x43");
