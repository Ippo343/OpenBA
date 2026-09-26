#pragma once
// ============================================================================
// code/Game/Game/Profession.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `Profession` (0x60): a citizen profession definition (name, description,
// priority, skill multipliers), a real `Object::External` subclass. Layout from
// ctor 0x1400a9030 + factory 0x14001e530.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "Profession", parent "Object::External", table 0x14015efa0,
//   9 slots, sizeof 0x60.
//   [0] 0x14001e6f0 scalar deleting dtor   [1] TypeName  [2] Type
//   [3] GetFileVersion `MOV EAX,3`         [4] IMPORT Object::External::Compile
//   [5] 0x1400a8f10 Serialize              [6] 0x14001e580 Reconstruct
//   [7] 0x140001640 Destruct               [8] 0x14001e570 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class Profession final : public Object::External {
    DECLARE_EXE_TYPE(Profession)

public:
    Profession();                                                 // 0x1400a9030

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x14001e6f0
    int32_t GetFileVersion() const override { return 3; }         // [3]
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(void* stream) override;                        // [5] 0x1400a8f10
    Object::External* Reconstruct() override;                     // [6] 0x14001e580
    void Destruct() override;                                     // [7] 0x140001640 DeletingDtor(0)
    virtual Object::External* Construct();                        // [8] 0x14001e570

protected:
    ~Profession();

public:
    void*    externalPtr1;       // +0x20: ExternalPtr (serialized via 0x14000bf40)
    void*    externalPtr2;       // +0x28: ExternalPtr (serialized via 0x14000bd30)
    uint64_t stringName;         // +0x30: System::String (profession name)
    uint64_t stringDesc;         // +0x38: System::String (profession description)
    uint64_t stringTool;         // +0x40: System::String (tool type / animation set)
    int32_t  field48;            // +0x48: int (serialized)
    uint8_t  field4C;            // +0x4C: bool (serialized)
    uint8_t  field4D;            // +0x4D: bool (serialized)
    uint8_t  isAvailable;        // +0x4E: bool, default 1 (serialized)
    uint8_t  _pad4F;             // +0x4F: padding
    int32_t  priority;           // +0x50: 100 (default priority, serialized)
    float    skillMult1;         // +0x54: 1.0f (primary skill mult, serialized)
    float    skillMult2;         // +0x58: 1.0f (secondary skill mult, serialized)
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(Profession) == 0x60, "Profession is 0x60");
static_assert(offsetof(Profession, externalPtr1) == 0x20, "+0x20");
static_assert(offsetof(Profession, externalPtr2) == 0x28, "+0x28");
static_assert(offsetof(Profession, stringName) == 0x30, "+0x30");
static_assert(offsetof(Profession, stringDesc) == 0x38, "+0x38");
static_assert(offsetof(Profession, stringTool) == 0x40, "+0x40");
static_assert(offsetof(Profession, field48) == 0x48, "+0x48");
static_assert(offsetof(Profession, field4C) == 0x4C, "+0x4C");
static_assert(offsetof(Profession, field4D) == 0x4D, "+0x4D");
static_assert(offsetof(Profession, isAvailable) == 0x4E, "+0x4E");
static_assert(offsetof(Profession, priority) == 0x50, "+0x50");
static_assert(offsetof(Profession, skillMult1) == 0x54, "+0x54");
static_assert(offsetof(Profession, skillMult2) == 0x58, "+0x58");
