#pragma once
// ============================================================================
// code/Game/Game/MusicList.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `MusicList` (0x30): the music-track playlist resource (Game\MusicTracks.rsc),
// a real `Object::External` subclass. Layout from factory 0x14001c260, dtor
// 0x14001c360, Serialize 0x1400a3e90.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "MusicList", parent "Object::External", table 0x14015ee40,
//   9 slots, sizeof 0x30.
//   [0] 0x14001c360 scalar deleting dtor   [1] 0x14001c250 TypeName
//   [2] 0x14001c240 Type                   [3] 0x1400a3f80 GetFileVersion `MOV EAX,1`
//   [4] IMPORT Object::External::Compile   [5] 0x1400a3e90 Serialize
//   [6] 0x14001c310 Reconstruct            [7] 0x140001640 Destruct
//   [8] 0x14001c2c0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class MusicList final : public Object::External {
    DECLARE_EXE_TYPE(MusicList)

public:
    MusicList();                                                  // factory 0x14001c260 body

    ~MusicList() override;                                        // [0] 0x14001c360
    int32_t GetFileVersion() const override { return 1; }         // [3] 0x1400a3f80
    // [4] Compile: inherited (retail's slot is the import; the old table's was null)
    bool Serialize(void* stream) override;                        // [5] 0x1400a3e90
    Object::External* Reconstruct() override;                     // [6] 0x14001c310
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x14001c2c0

public:
    // Track entries: a {data,count,cap} array of 0x18-byte entries
    //   +0x00 ExternalPtr (the track's audio resource)
    //   +0x08 float   +0x0c float   +0x10 int   +0x14 int
    void*    _entries;           // +0x20: data
    int32_t  _entryCount;        // +0x28: count
    int32_t  _entryCapacity;     // +0x2C: capacity
};

static_assert(sizeof(MusicList) == 0x30, "MusicList is 0x30");
static_assert(offsetof(MusicList, _entries) == 0x20, "+0x20");
static_assert(offsetof(MusicList, _entryCount) == 0x28, "+0x28");
static_assert(offsetof(MusicList, _entryCapacity) == 0x2C, "+0x2C");
