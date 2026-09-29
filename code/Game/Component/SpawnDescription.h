#pragma once
// ============================================================================
// code/Game/Component/SpawnDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "SpawnDescription", parent "Description", ti 0x1401c4918
//   create 0x14000c1d0: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000c230.
//   table 0x14015a6f0, 10 slots:
//     [0] 0x14000c2e0 scalar deleting dtor   [1] 0x14000c1c0 TypeName   [2] 0x14000c1b0 Type
//     [3] 0x140056180 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x1400560b0 Serialize (`MOV AL,1`)
//     [6] 0x14000c280 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000c2d0 GetComponentType: SpawnComponent's TypeId
//     [9] 0x14000c230 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;   // Component/ComponentDescription.h

class SpawnDescription final : public Description {
    DECLARE_EXE_TYPE(SpawnDescription)

public:
    SpawnDescription();

    ~SpawnDescription() override;                                          // [0] 0x14000c2e0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140056180
    bool              Serialize(void* stream) override;                    // [5] 0x1400560b0
    Object::External* Reconstruct() override;                              // [6] 0x14000c280
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000c2d0
    virtual Object::External* Construct();                                 // [9] 0x14000c230

    // One spawn-table entry, 0x10 bytes (Serialize 0x1400560b0; the array's element
    // init 0x14000c520 sets the ages to 0 / 10).
    struct Entry {
        ComponentDescription* _template;        // +0x00 ExternalPtr<ComponentDescription> (0x140002340): the entity spawned
        float _minSpawnAge;                     // +0x08 the owner's resource age window
        float _maxSpawnAge;                     // +0x0c
    };

    // ---- fields (PUBLIC while the components read them) ----
    Entry* _entries;                            // +0x20 Array<Entry> data
    int32_t _entryCount;                        // +0x28
    int32_t _entryCap;                          // +0x2c
};

static_assert(sizeof(SpawnDescription) == 0x30, "SpawnDescription is 0x30 (create 0x14000c1d0 MemAlloc(0x30))");
static_assert(alignof(SpawnDescription) == 8, "SpawnDescription alignment");
static_assert(sizeof(SpawnDescription::Entry) == 0x10, "SpawnDescription::Entry stride 0x10 (0x14000c520)");
static_assert(offsetof(SpawnDescription, _entries) == 0x20, "SpawnDescription::_entries @ +0x20");
static_assert(offsetof(SpawnDescription, _entryCount) == 0x28, "SpawnDescription::_entryCount @ +0x28");
static_assert(offsetof(SpawnDescription, _entryCap) == 0x2c, "SpawnDescription::_entryCap @ +0x2c");
