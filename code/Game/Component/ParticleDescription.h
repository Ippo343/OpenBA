#pragma once
// ============================================================================
// code/Game/Component/ParticleDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ParticleDescription", parent "Description", ti 0x1401c4758
//   create 0x14000a3c0: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000a430.
//   table 0x14015a310, 10 slots:
//     [0] 0x14000a500 scalar deleting dtor   [1] 0x14000a3b0 TypeName   [2] 0x14000a3a0 Type
//     [3] 0x1400535b0 GetFileVersion = 7   [4] External::Compile (import, inherited)
//     [5] 0x140053460 Serialize (`MOV AL,1`)
//     [6] 0x14000a490 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000a4f0 GetComponentType: ParticleComponent's TypeId
//     [9] 0x14000a430 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ParticleDescription final : public Description {
    DECLARE_EXE_TYPE(ParticleDescription)

public:
    ParticleDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14000a500
    int32_t           GetFileVersion() const override { return 7; }    // [3] 0x1400535b0
    bool              Serialize(void* stream) override;                    // [5] 0x140053460
    Object::External* Reconstruct() override;                              // [6] 0x14000a490
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000a4f0
    virtual Object::External* Construct();                                 // [9] 0x14000a430

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _materialRef;                         // +0x20 ExternalPtr<Pathfinding::PointList> (the name is historical)
    void* _emitters;                            // +0x28 emitter array data (0x28 stride)
    int32_t _emitterCount;                      // +0x30
    int32_t _emitterCap;                        // +0x34

protected:
    ~ParticleDescription();
};

static_assert(sizeof(ParticleDescription) == 0x38, "ParticleDescription is 0x38 (create 0x14000a3c0 MemAlloc(0x38))");
static_assert(alignof(ParticleDescription) == 8, "ParticleDescription alignment");
static_assert(offsetof(ParticleDescription, _materialRef) == 0x20, "ParticleDescription::_materialRef @ +0x20");
static_assert(offsetof(ParticleDescription, _emitters) == 0x28, "ParticleDescription::_emitters @ +0x28");
static_assert(offsetof(ParticleDescription, _emitterCount) == 0x30, "ParticleDescription::_emitterCount @ +0x30");
static_assert(offsetof(ParticleDescription, _emitterCap) == 0x34, "ParticleDescription::_emitterCap @ +0x34");
