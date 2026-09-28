#pragma once
// ============================================================================
// code/Game/Component/UIDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "UIDescription", parent "Description", ti 0x1401c7f90
//   create 0x1400395b0: MemAlloc(0x48, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140039620.
//   table 0x140164a70, 10 slots:
//     [0] 0x140039700 scalar deleting dtor   [1] 0x1400395a0 TypeName   [2] 0x140039590 Type
//     [3] 0x14012dfa0 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x14012de80 Serialize (`MOV AL,1`)
//     [6] 0x140039680 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400396e0 GetComponentType: UIComponent's TypeId
//     [9] 0x140039620 Construct (folded with createInPlace)
//   sizeof 0x48.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class UIDescription final : public Description {
    DECLARE_EXE_TYPE(UIDescription)

public:
    UIDescription();

    ~UIDescription() override;                                             // [0] 0x140039700
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x14012dfa0
    bool              Serialize(void* stream) override;                    // [5] 0x14012de80
    Object::External* Reconstruct() override;                              // [6] 0x140039680
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400396e0
    virtual Object::External* Construct();                                 // [9] 0x140039620

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    int32_t _valTail;                           // +0x20 serialized LAST; slot 8 builds a UIComponent only when > 0
    uint8_t _pad24[4];                          // +0x24 never written by the ctor
    void* _descRef;                             // +0x28 ExternalPtr (Description)
    void* _entries;                             // +0x30 element array data (0x28 stride: Type, 3 ExternalPtr, String)
    int32_t _entryCount;                        // +0x38
    int32_t _entryCap;                          // +0x3c
    int32_t _valHead;                           // +0x40
    uint8_t _pad44[4];                          // +0x44 never written by the ctor
};

static_assert(sizeof(UIDescription) == 0x48, "UIDescription is 0x48 (create 0x1400395b0 MemAlloc(0x48))");
static_assert(alignof(UIDescription) == 8, "UIDescription alignment");
static_assert(offsetof(UIDescription, _valTail) == 0x20, "UIDescription::_valTail @ +0x20");
static_assert(offsetof(UIDescription, _descRef) == 0x28, "UIDescription::_descRef @ +0x28");
static_assert(offsetof(UIDescription, _entries) == 0x30, "UIDescription::_entries @ +0x30");
static_assert(offsetof(UIDescription, _entryCount) == 0x38, "UIDescription::_entryCount @ +0x38");
static_assert(offsetof(UIDescription, _entryCap) == 0x3c, "UIDescription::_entryCap @ +0x3c");
static_assert(offsetof(UIDescription, _valHead) == 0x40, "UIDescription::_valHead @ +0x40");
