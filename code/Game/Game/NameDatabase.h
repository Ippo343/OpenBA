#pragma once
// ============================================================================
// code/Game/Game/NameDatabase.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `NameDatabase` (0x60): the ".ndb" name-generation resource (Game/Names/
// Cities.ndb, Female.ndb, ...), a real `Object::External` subclass. Layout from
// factory 0x14001c660, dtor body 0x1400a4790, Serialize 0x1400a45e0.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "NameDatabase" (file extension "ndb"), parent "Object::External",
//   table 0x14015eea0, 9 slots, sizeof 0x60.
//   [0] 0x14001c790 scalar deleting dtor (body 0x1400a4790)
//   [1] TypeName  [2] Type
//   [3] 0x1400a4780 GetFileVersion `MOV EAX,0x401`
//   [4] Compile — OWN `MOV EAX,1` (not the import)
//   [5] 0x1400a45e0 Serialize              [6] 0x14001c730 Reconstruct
//   [7] 0x140001640 Destruct               [8] 0x14001c6d0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class NameDatabase final : public Object::External {
    DECLARE_EXE_TYPE(NameDatabase)

public:
    NameDatabase();                                               // factory 0x14001c660 body

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x14001c790
    int32_t GetFileVersion() const override { return 0x401; }     // [3] 0x1400a4780
    CompileResult Compile(const System::Filename&) override { return 1; }   // [4] own `return 1`
    bool Serialize(void* stream) override;                        // [5] 0x1400a45e0
    Object::External* Reconstruct() override;                     // [6] 0x14001c730
    void Destruct() override;                                     // [7] 0x140001640 DeletingDtor(0)
    virtual Object::External* Construct();                        // [8] 0x14001c6d0

protected:
    ~NameDatabase();                                              // body 0x1400a4790

public:
    // Four {data,count,cap} arrays.
    void*    _array1;            // +0x20: stride 0xc (4 bytes + uchar + uint)
    int32_t  _array1Count;       // +0x28
    int32_t  _array1Capacity;    // +0x2C
    void*    _array2;            // +0x30: stride 0x2 (uint16)
    int32_t  _array2Count;       // +0x38
    int32_t  _array2Capacity;    // +0x3C
    void*    _array3;            // +0x40: stride 0x1 (byte buffer)
    int32_t  _array3Count;       // +0x48
    int32_t  _array3Capacity;    // +0x4C
    void*    _array4;            // +0x50: stride 0x4 (int)
    int32_t  _array4Count;       // +0x58
    int32_t  _array4Capacity;    // +0x5C
};

static_assert(sizeof(NameDatabase) == 0x60, "NameDatabase is 0x60");
static_assert(offsetof(NameDatabase, _array1) == 0x20, "+0x20");
static_assert(offsetof(NameDatabase, _array1Count) == 0x28, "+0x28");
static_assert(offsetof(NameDatabase, _array2) == 0x30, "+0x30");
static_assert(offsetof(NameDatabase, _array2Count) == 0x38, "+0x38");
static_assert(offsetof(NameDatabase, _array3) == 0x40, "+0x40");
static_assert(offsetof(NameDatabase, _array3Count) == 0x48, "+0x48");
static_assert(offsetof(NameDatabase, _array4) == 0x50, "+0x50");
static_assert(offsetof(NameDatabase, _array4Count) == 0x58, "+0x58");
static_assert(offsetof(NameDatabase, _array4Capacity) == 0x5C, "+0x5C");
