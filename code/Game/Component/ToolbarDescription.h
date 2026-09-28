#pragma once
// ============================================================================
// code/Game/Component/ToolbarDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ToolbarDescription", parent "Description", ti 0x1401c7f58
//   create 0x140039490: MemAlloc(0x68, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400394d0.
//   table 0x140164980, 10 slots:
//     [0] 0x140039500 scalar deleting dtor   [1] 0x140039480 TypeName   [2] 0x140039470 Type
//     [3] 0x14012db60 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x14012da30 Serialize (`MOV AL,1`)
//     [6] 0x1400394e0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140004950 GetComponentType: 0 (data-only: shared body 0x140004950 `MOV dword [RDX],0`)
//     [9] 0x1400394d0 Construct (folded with createInPlace)
//   sizeof 0x68.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/String.h"

class ToolbarDescription final : public Description {
    DECLARE_EXE_TYPE(ToolbarDescription)

public:
    ToolbarDescription();

    ~ToolbarDescription() override;                                        // [0] 0x140039500
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x14012db60
    bool              Serialize(void* stream) override;                    // [5] 0x14012da30
    Object::External* Reconstruct() override;                              // [6] 0x1400394e0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140004950
    virtual Object::External* Construct();                                 // [9] 0x1400394d0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _descRef;                             // +0x20 ExternalPtr (Description)
    System::String _name;                       // +0x28 String
    void* _iconRef;                             // +0x30 ExternalPtr (StringTable variant)
    System::String _str1;                       // +0x38 String
    System::String _str2;                       // +0x40 String
    System::String _str3;                       // +0x48 String
    void* _entries;                             // +0x50 System::String array data
    int32_t _entryCount;                        // +0x58
    int32_t _entryCap;                          // +0x5c
    int32_t _val0;                              // +0x60
    int32_t _val1;                              // +0x64
};

static_assert(sizeof(ToolbarDescription) == 0x68, "ToolbarDescription is 0x68 (create 0x140039490 MemAlloc(0x68))");
static_assert(alignof(ToolbarDescription) == 8, "ToolbarDescription alignment");
static_assert(offsetof(ToolbarDescription, _descRef) == 0x20, "ToolbarDescription::_descRef @ +0x20");
static_assert(offsetof(ToolbarDescription, _name) == 0x28, "ToolbarDescription::_name @ +0x28");
static_assert(offsetof(ToolbarDescription, _iconRef) == 0x30, "ToolbarDescription::_iconRef @ +0x30");
static_assert(offsetof(ToolbarDescription, _str1) == 0x38, "ToolbarDescription::_str1 @ +0x38");
static_assert(offsetof(ToolbarDescription, _str2) == 0x40, "ToolbarDescription::_str2 @ +0x40");
static_assert(offsetof(ToolbarDescription, _str3) == 0x48, "ToolbarDescription::_str3 @ +0x48");
static_assert(offsetof(ToolbarDescription, _entries) == 0x50, "ToolbarDescription::_entries @ +0x50");
static_assert(offsetof(ToolbarDescription, _entryCount) == 0x58, "ToolbarDescription::_entryCount @ +0x58");
static_assert(offsetof(ToolbarDescription, _entryCap) == 0x5c, "ToolbarDescription::_entryCap @ +0x5c");
static_assert(offsetof(ToolbarDescription, _val0) == 0x60, "ToolbarDescription::_val0 @ +0x60");
static_assert(offsetof(ToolbarDescription, _val1) == 0x64, "ToolbarDescription::_val1 @ +0x64");
